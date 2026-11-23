#include "intel.h"
#include "fts.h"
#include "fts_schema.h"
#include "alert_eval.h"
#include "simhash.h"
#include "../third_party/sqlite3.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "../lib/jocore.h"   /* jo_iso_now */
#include "lib/joenv.h"
#include "../lib/seenset.h"  /* seen_hset: the distinct-uid set of rule 4b */

/* ── `stored`: how many DISTINCT rows a run actually left behind ──────────
 *
 * HOUSE RULE 4b. `records=N` in the scheduler's run line counts emit() CALLS.
 * This sink upserts on uid, so a source whose records key onto each other
 * reports a healthy N and leaves ONE row behind. Measured over a 1,197-source
 * sweep: ECDC_RESPIRATORY emitted 12,648 and stored 31; 46 hpengine rows were
 * losing 114,795 records a pass between them. Every check in the tree was
 * blind to it, because emit() really was called 12,648 times and really did
 * return >= 0 every time. Nothing lied — nothing was counting the right thing.
 *
 * WHAT `stored` COUNTS, and why it is this and not something else:
 *
 *     stored = the number of DISTINCT uids this run upserted successfully.
 *
 * The tempting definition is "rows INSERTed", because emit() already works
 * that out for free (`is_new` below). It is also useless. A scheduled source
 * re-fetching an unchanged feed inserts nothing and updates everything, so
 * every ordinary re-run would report `records=948 stored=0` and read as total
 * loss. A metric that cries wolf on every ordinary re-run is one nobody reads
 * when a real discard happens — the same argument docs/SOURCE_EXHAUSTIVENESS
 * makes for reporting truncation as data instead of as a log line.
 *
 * Distinct-uid is stable across re-runs: 948 records keyed on 948 uids report
 * stored=948 on the first pass and on the thousandth. It moves ONLY when a
 * run's own records collapse onto each other, which is exactly the defect —
 * and it is the number `SELECT COUNT(*) FROM intel_items WHERE source_id=…`
 * returns on a fresh database, so the run line and the DB agree by
 * construction rather than by coincidence.
 *
 * Failed upserts are not counted: a row we could not write is not stored.
 * Items skipped for having no uid/remote_key never reach the counter at all
 * (emit returns -1 above it), which is right — they are not stored either.
 *
 * COST. One 64-bit FNV-1a over the uid and one open-addressed probe per emit,
 * against a table that doubles from 1024 slots at 70% load. The largest
 * source in the fleet (IRS exempt-orgs, 278,014 records) peaks at 512K slots
 * = 4 MB, freed with the sink when the run ends. Two distinct uids whose
 * 64-bit hashes collide would under-count by one; at 278K keys that is a
 * ~2e-9 chance, and it errs towards reporting loss that is not there, never
 * towards hiding loss that is.
 *
 * The table stops growing at SEEN_MAX slots (~33 MB, ~2.9M distinct uids) so
 * one runaway source cannot eat the host. Past that the count becomes a FLOOR
 * and SAYS SO (`stored>=N`) rather than quietly becoming wrong — the same
 * discipline as the rest of this tree: never report a number you did not
 * measure. */
#define SEEN_MAX  (1u << 22)

/* `magic` exists so intel_sink_rebind() can prove the sink it was handed is
 * really one of ours before reading ctx as a sink_state. Several sinks in this
 * tree are NOT intel sinks — dataapi.c's capture sink and lib/unified.c's both
 * put an unrelated struct in ctx — and misreading one would be a wild pointer
 * dereference, not a wrong answer. */
#define SINK_MAGIC 0x53494E4Bu   /* 'SINK' */

/* The FTS mirror's statements. Owned by a sink for its lifetime (see
 * PREPARED ONCE below); intel_fts_remirror() uses a stack copy and frees it. */
typedef struct {
  sqlite3_stmt *map_sel, *ins, *del, *map_put;
  const char   *ins_sql;      /* which fts_insert_sql() `ins` was built from */
} fts_stmts;

/* PREPARED ONCE PER SINK. emit() used to sqlite3_prepare_v2 six to eight
 * statements per record — the PK probe, the 20-column upsert, the uid_map
 * lookup, the FTS insert, the FTS delete and the uid_map upsert — and finalize
 * each straight away, so a 20,000-record run parsed and planned ~140,000 SQL
 * statements to execute the same seven texts. They live here now, keyed to the
 * connection they were prepared on (`stmt_db`), and are finalized by
 * intel_sink_free(). Each is RESET immediately after its step: a statement
 * left mid-step holds a read snapshot open, and on this WAL database a
 * snapshot that never closes is a checkpoint that never completes. */
typedef struct {
  unsigned magic;
  db_handle *db; char source_id[128]; char tenant_id[64];
  seen_hset seen;             /* distinct uids stored this run (lib/seenset); n is a
                               * floor once .inexact is set                    */
  size_t seen_notice_n;       /* ...of which collector-*-notice records       */
  sqlite3      *stmt_db;      /* the connection the statements below belong to */
  sqlite3_stmt *q_probe, *q_upsert, *q_touch;
  fts_stmts     fts;
  EVP_MD_CTX   *md;           /* content-hash context, reused across emits */
  /* BATCHED WRITES (see the block above emit()). */
  struct pend **buf;          /* records accepted, not yet written          */
  int    nbuf, bufcap;
  size_t nbytes;              /* rough size of what is buffered             */
  long long t0_ms;            /* when the oldest buffered record arrived    */
  int    batch_max;           /* records per transaction; <=1 = per-record  */
  int    batch_ms;            /* oldest buffered record's age that flushes  */
  size_t batch_bytes;
  long   owed_rec, owed_notice;  /* refusals not yet returned by an emit()  */
  long   failed_total;        /* every record the database refused          */
  int    trace;               /* JO_SINK_TRACE: report each flush's cost    */
} sink_state;

static void fts_stmts_free(fts_stmts *f) {
  sqlite3_finalize(f->map_sel); sqlite3_finalize(f->ins);
  sqlite3_finalize(f->del);     sqlite3_finalize(f->map_put);
  memset(f, 0, sizeof *f);
}

static void sink_stmts_free(sink_state *st) {
  sqlite3_finalize(st->q_probe); sqlite3_finalize(st->q_upsert);
  sqlite3_finalize(st->q_touch);
  st->q_probe = st->q_upsert = st->q_touch = NULL;
  fts_stmts_free(&st->fts);
  st->stmt_db = NULL;
}

/* The cached statement in `*slot`, prepared on first use. NULL if it cannot
 * be prepared — every caller treats that exactly as it treated a failed
 * prepare_v2 before the cache existed. */
static sqlite3_stmt *stmt_get(sqlite3 *h, sqlite3_stmt **slot, const char *sql) {
  if (*slot) return *slot;
  if (sqlite3_prepare_v3(h, sql, -1, SQLITE_PREPARE_PERSISTENT, slot, NULL)
      != SQLITE_OK) {
    sqlite3_finalize(*slot);
    *slot = NULL;
  }
  return *slot;
}

int intel_item_is_notice(const intel_item *it) {
  const char *rt = it ? it->record_type : NULL;
  size_t n = rt ? strlen(rt) : 0;
  return n >= 17 && strncmp(rt, "collector-", 10) == 0 &&
         strcmp(rt + n - 7, "-notice") == 0;
}

/* Record `uid` in the run's distinct set (lib/seenset hset). Returns 1 when
 * `uid` was new to this run, 0 when it was already there or could not be
 * recorded; in the latter case the set's `inexact` flag is raised and the
 * caller reports a floor instead of a wrong number. */
static int seen_note(sink_state *st, const char *uid) {
  return seen_hset_add(&st->seen, uid);
}

/* Exact Node upsert: INSERT … ON CONFLICT(uid) DO UPDATE, preserving
 * DERIVED geometry across re-runs.
 *
 * 'exif' joined 'llm' here with roadmap 27. Both are coordinates this system
 * worked out for itself — the LLM geocoder from the text, media.c from an
 * image's EXIF GPS block — for items whose collector supplies no geometry of
 * its own. Those collectors emit lat=NULL every run, so without this the very
 * next scheduled refresh would fall through to `ELSE excluded.lat` and erase
 * the coordinate, and the item would silently drop off the map again. A
 * collector that DOES supply geometry still wins: excluded.lat is checked
 * first, because a real upstream coordinate outranks an inferred one. */
/* A re-fetched record whose TRANSLATED text changed must not keep the English
 * rendering of the old text, nor stay marked translated (translate.c selects
 * only rows with translated_at IS NULL, so the stale translation would be
 * shown beside the new Japanese for good). When title, body or summary differ
 * from what is stored, clear the *_en columns and the translation state so
 * the row is pending again (translate_failed back to 0 also lifts the -1
 * "not Japanese" sentinel and a burned retry count, both of which judged the
 * OLD text). Judged on the text itself rather than content_hash: a row
 * stored before the hash existed has none, and a properties-only change leaves
 * the translation valid. fts_write() wipes `keywords` on every rewrite and
 * translate_fts_resync restores it from the *_en columns, which are now empty
 * until the retranslation lands. */
#define TR_TEXT_CHANGED \
  "(intel_items.title IS NOT excluded.title OR intel_items.body IS NOT excluded.body" \
  " OR intel_items.summary IS NOT excluded.summary)"
#define TR_STALE_SET \
 "  title_en=CASE WHEN " TR_TEXT_CHANGED " THEN NULL ELSE intel_items.title_en END,"   \
 "  body_en=CASE WHEN " TR_TEXT_CHANGED " THEN NULL ELSE intel_items.body_en END,"     \
 "  summary_en=CASE WHEN " TR_TEXT_CHANGED " THEN NULL ELSE intel_items.summary_en END," \
 "  translated_at=CASE WHEN " TR_TEXT_CHANGED " THEN NULL ELSE intel_items.translated_at END," \
 "  translate_failed=CASE WHEN " TR_TEXT_CHANGED " THEN 0 ELSE intel_items.translate_failed END,"

static const char *SQL_UPSERT =
 "INSERT INTO intel_items"
 " (uid,source_id,title,body,summary,link,author,language,published_at,"
 "  fetched_at,tags,properties,lat,lon,geom_source,geom_at,record_type,"
 "  sub_source_id,geometry,tenant_id,content_hash,last_seen_at)"
 " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,"
 "         ?21,?10)"
 " ON CONFLICT(uid) DO UPDATE SET"
 "  content_hash=excluded.content_hash, last_seen_at=excluded.last_seen_at,"
 "  title=excluded.title, body=excluded.body, summary=excluded.summary,"
 "  link=excluded.link, author=excluded.author, language=excluded.language,"
 "  published_at=excluded.published_at, fetched_at=excluded.fetched_at,"
 "  tags=excluded.tags, properties=excluded.properties,"
 TR_STALE_SET
 "  lat=CASE WHEN excluded.lat IS NOT NULL THEN excluded.lat"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.lat"
 "          ELSE excluded.lat END,"
 "  lon=CASE WHEN excluded.lon IS NOT NULL THEN excluded.lon"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.lon"
 "          ELSE excluded.lon END,"
 "  geom_source=CASE WHEN excluded.geom_source IS NOT NULL THEN excluded.geom_source"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.geom_source"
 "          ELSE excluded.geom_source END,"
 "  geom_at=CASE WHEN excluded.geom_source IS NOT NULL THEN excluded.geom_at"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.geom_at"
 "          ELSE excluded.geom_at END,"
 "  geometry=COALESCE(excluded.geometry,intel_items.geometry),"
 "  record_type=COALESCE(excluded.record_type,intel_items.record_type),"
 "  sub_source_id=COALESCE(excluded.sub_source_id,intel_items.sub_source_id);";

static void bind_txt(sqlite3_stmt *s, int i, const char *v) {
  if (v) sqlite3_bind_text(s, i, v, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, i);
}

/* ftsMirror.writeOne: DELETE old fts row via uid_map, INSERT segmented,
 * upsert uid_map(uid,rowid).
 *
 * keywords stays '' here — it is NOT an ingest field. translate.c and media.c
 * UPDATE it later with machine English and OCR text, and translate.c's resync
 * watermark exists precisely because this function re-inserts the row (with a
 * fresh rowid) and wipes their write. Do not "fix" that by carrying keywords
 * over here without reading translate.h first. (emit() no longer calls this
 * for a re-fetch whose content is unchanged — see UNCHANGED RE-FETCH — so the
 * wipe now happens only when the text really changed and the translation is
 * stale anyway.)
 *
 * link/author/tags/props are v2 columns (see fts_schema.h): before them a
 * camera row — camera_store.c writes ONLY title, everything else is properties
 * — was findable by name and by nothing else. Flattening is shared with the
 * rebuild migration so the write path and the backfill cannot drift.
 *
 * `seg_title`/`seg_body` arrive ALREADY SEGMENTED: emit() hands the same two
 * strings to simhash_on_item_segmented() after the commit, which used to run
 * MeCab over title and body a second time to get them. */
/* The five derived strings the FTS insert binds, computed WITHOUT touching the
 * database. Split out of fts_write() so the batched sink can run MeCab and the
 * JSON flattening on the collector's thread at emit() time and hold the write
 * lock only for the statements (see BATCHED WRITES below). */
typedef struct { char *ss, *sl, *sa, *sg, *sp; } fts_prep;

static void fts_prep_make(fts_prep *p, const char *summary, const char *link,
                          const char *author, const char *tags_json,
                          const char *props_json) {
  p->ss = fts_segment(summary ? summary : "");
  /* Latin passes fts_segment straight through, so segmenting the URL/author
   * costs nothing on the common case and still tokenizes a Japanese byline. */
  p->sl = fts_segment(link ? link : "");
  p->sa = fts_segment(author ? author : "");
  char *ftags = fts_flatten_json(tags_json);
  char *fprops = fts_flatten_json(props_json);
  p->sg = fts_segment(ftags);
  p->sp = fts_segment(fprops);
  free(ftags); free(fprops);
}

static void fts_prep_free(fts_prep *p) {
  free(p->ss); free(p->sl); free(p->sa); free(p->sg); free(p->sp);
  memset(p, 0, sizeof *p);
}

static void fts_write_prepped(sqlite3 *h, fts_stmts *fs, const char *uid,
                              const char *seg_title, const char *seg_body,
                              const fts_prep *pp) {
  sqlite3_stmt *s;
  sqlite3_int64 old = -1;
  if ((s = stmt_get(h, &fs->map_sel,
         "SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1"))) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) old = sqlite3_column_int64(s, 0);
    sqlite3_reset(s);
  }
  /* The rowid must come from THIS insert. Reading last_insert_rowid()
   * unconditionally meant that when the FTS insert failed or was never prepared,
   * the map recorded whatever rowid the preceding intel_items upsert produced —
   * and the delete at the top of the next write then removed a DIFFERENT item's
   * search-index row, evicting it from /api/intel/search with no error. */
  int inserted = 0;
  /* THE DELETE HAPPENS ONLY IF THE INSERT PREPARED. This function used to
   * DELETE the item's existing FTS row first and only then prepare an INSERT
   * naming nine columns. Against a v1 five-column intel_items_fts that
   * prepare_v2 fails, `inserted` stays 0, and the early return below leaves a
   * committed delete with nothing put back. Scheduled collectors re-emit
   * continuously, so every write was a net delete and the index bled rows with
   * no error reaching any API — the single worst defect in the audit.
   *
   * Two independent guards, because either alone still loses data:
   *   1. fts_insert_sql() adapts to the columns the LIVE index actually has,
   *      so a v1 database indexes what it can instead of nothing;
   *   2. the DELETE is ordered after a SUCCESSFUL prepare, so any future
   *      schema drift degrades to a stale row rather than a missing one. A
   *      stale hit is findable and self-heals on the next successful write; a
   *      deleted row is invisible until a full rebuild.
   *
   * The cached INSERT is rebuilt when fts_insert_sql() changes its answer (the
   * boot rebuild widening v1 to v2 while a sink is alive). */
  const char *want = fts_insert_sql(h);
  if (fs->ins && fs->ins_sql != want) { sqlite3_finalize(fs->ins); fs->ins = NULL; }
  fs->ins_sql = want;
  if ((s = stmt_get(h, &fs->ins, want))) {
    if (old >= 0) {
      sqlite3_stmt *d = stmt_get(h, &fs->del,
                                 "DELETE FROM intel_items_fts WHERE rowid=?1");
      if (d) { sqlite3_bind_int64(d, 1, old); sqlite3_step(d); sqlite3_reset(d); }
    }
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, seg_title ? seg_title : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, seg_body ? seg_body : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, pp->ss, -1, SQLITE_TRANSIENT);
    /* ?5..?8 exist only in the v2 statement; binding an unused index on the
     * v1 one is a no-op range error, not a failure, but skip it anyway. */
    if (fts_index_is_v2(h)) {
      sqlite3_bind_text(s, 5, pp->sl, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 6, pp->sa, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 7, pp->sg, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 8, pp->sp, -1, SQLITE_TRANSIENT);
    }
    inserted = (sqlite3_step(s) == SQLITE_DONE);
    sqlite3_reset(s);
  } else {
    fprintf(stderr, "[intel] fts insert not preparable for %s (%s) — the "
                    "existing index row is left in place\n",
            uid, sqlite3_errmsg(h));
  }
  if (!inserted) {
    fprintf(stderr, "[intel] fts insert failed for %s: %s\n", uid, sqlite3_errmsg(h));
    return;                       /* leave the existing map row alone */
  }
  sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
  if ((s = stmt_get(h, &fs->map_put,
      "INSERT INTO intel_items_fts_uid_map(uid,rowid) VALUES(?1,?2)"
      " ON CONFLICT(uid) DO UPDATE SET rowid=excluded.rowid"))) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 2, rid);
    sqlite3_step(s); sqlite3_reset(s);
  }
}

static void fts_write(sqlite3 *h, fts_stmts *fs, const char *uid,
                      const char *seg_title, const char *seg_body,
                      const char *summary, const char *link, const char *author,
                      const char *tags_json, const char *props_json) {
  fts_prep pp;
  fts_prep_make(&pp, summary, link, author, tags_json, props_json);
  fts_write_prepped(h, fs, uid, seg_title, seg_body, &pp);
  fts_prep_free(&pp);
}

/* See intel.h. Deliberately routed through the same fts_write() the ingest
 * path uses rather than a second, subtly different mirror. */
int intel_fts_remirror(db_handle *db, const char *uid) {
  if (!db || !db->h || !uid || !*uid) return -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT title,body,summary,link,author,tags,properties"
        "  FROM intel_items WHERE uid=?1", -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  int rc = -1;
  if (sqlite3_step(s) == SQLITE_ROW) {
    /* Copied before fts_write() runs any statement of its own: the column
     * pointers belong to `s` and are invalidated by the next step/finalize. */
    const char *c;
    char *v[7];
    for (int i = 0; i < 7; i++) {
      c = (const char *)sqlite3_column_text(s, i);
      v[i] = c ? strdup(c) : NULL;
    }
    sqlite3_finalize(s);
    char *seg_t = fts_segment(v[0] ? v[0] : "");
    char *seg_b = fts_segment_body(v[1]);
    fts_stmts fs = {0};
    fts_write(db->h, &fs, uid, seg_t, seg_b, v[2], v[3], v[4],
              v[5] ? v[5] : "[]", v[6] ? v[6] : "{}");
    fts_stmts_free(&fs);
    free(seg_t); free(seg_b);
    for (int i = 0; i < 7; i++) free(v[i]);
    rc = 0;
  } else {
    sqlite3_finalize(s);
  }
  return rc;
}

/* ── uid: never truncated into a collision ───────────────────────────────
 *
 * The uid is "<source>|<remote_key>" (or the item's explicit uid) in a
 * 512-byte buffer, and it used to be snprintf'd straight in. A longer key was
 * cut at byte 511, so two records whose keys shared their first ~500 bytes —
 * a URL with a long common query string, a composite id_keys over long text
 * columns — got the SAME uid and the second overwrote the first: rule 4b's
 * silent collapse, produced by the sink itself, with `stored` counting one.
 *
 * Now: a key that fits is stored verbatim, exactly as before (so no existing
 * row's uid changes). One that would not fit keeps a readable prefix, cut on a
 * UTF-8 boundary, plus '#' and the SHA-256 of the WHOLE key — deterministic
 * across runs, so a re-fetch upserts the same row, and distinct for distinct
 * keys. Returns 0, or -1 when the item has neither uid nor remote_key. */
#define UID_CAP 512
#define UID_HASH_HEX 64

static int make_uid(char out[UID_CAP], const char *source_id, const intel_item *it) {
  const char *a, *sep, *b;
  if (it->uid && *it->uid)                  { a = it->uid; sep = ""; b = ""; }
  else if (it->remote_key && *it->remote_key) { a = source_id; sep = "|"; b = it->remote_key; }
  else return -1;
  size_t la = strlen(a), ls = strlen(sep), lb = strlen(b), n = la + ls + lb;
  if (n < UID_CAP) {
    memcpy(out, a, la); memcpy(out + la, sep, ls); memcpy(out + la + ls, b, lb);
    out[n] = '\0';
    return 0;
  }
  unsigned char d[SHA256_DIGEST_LENGTH];
  /* One-shot SHA256() would need the three parts joined into a heap copy of a
   * key that is, by construction, long. EVP keeps it streaming. */
  EVP_MD_CTX *md = EVP_MD_CTX_new();
  unsigned dl = 0;
  if (!md || EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1 ||
      EVP_DigestUpdate(md, a, la) != 1 || EVP_DigestUpdate(md, sep, ls) != 1 ||
      EVP_DigestUpdate(md, b, lb) != 1 || EVP_DigestFinal_ex(md, d, &dl) != 1) {
    EVP_MD_CTX_free(md);
    return -1;                    /* refuse rather than truncate into a collision */
  }
  EVP_MD_CTX_free(md);
  /* prefix + '#' + 64 hex + NUL == UID_CAP. The prefix is taken from the
   * joined key without materialising it. */
  size_t keep = UID_CAP - 1 - 1 - UID_HASH_HEX, o = 0;
  for (size_t i = 0; i < keep; i++) {
    char ch = i < la ? a[i] : i < la + ls ? sep[i - la] : b[i - la - ls];
    out[o++] = ch;
  }
  /* Back off a split multibyte sequence: a dangling lead/continuation byte
   * would make the uid invalid UTF-8 in every JSON response that carries it. */
  size_t cut = o;
  while (cut > 0 && ((unsigned char)out[cut - 1] & 0xC0) == 0x80) cut--;
  if (cut > 0 && ((unsigned char)out[cut - 1] & 0x80)) {
    unsigned char lead = (unsigned char)out[cut - 1];
    size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (o - (cut - 1) < need) o = cut - 1;     /* incomplete: drop it */
  }
  out[o++] = '#';
  static const char hx[] = "0123456789abcdef";
  for (unsigned i = 0; i < dl && o + 2 < UID_CAP; i++) {
    out[o++] = hx[d[i] >> 4]; out[o++] = hx[d[i] & 15];
  }
  out[o] = '\0';
  return 0;
}

/* ── content hash: what makes a re-fetch "the same record" ──────────────
 *
 * Exactly the fields the upsert's DO UPDATE writes from the item — title,
 * body, summary, link, author, language, published_at, tags, properties,
 * native geometry (presence, lat, lon, GeoJSON), record_type, sub_source_id.
 * fetched_at is not in it (it is the time, not the content); neither are the
 * columns the upsert never overwrites (source_id, tenant_id). Each field is
 * length-prefixed, with NULL distinct from "", so no two different items can
 * produce the same byte stream. The leading version byte retires every stored
 * hash if this list ever changes — a stale hash can only cause a rewrite,
 * never a skipped one.
 *
 * SHA-256 truncated to 128 bits: compared only against the SAME uid's previous
 * content, so a collision would need two versions of one record to meet at
 * 2^-128. 16 bytes a row (~200 MB at 12.9M rows), not 64 of hex. */
#define CONTENT_HASH_LEN 16
#define CONTENT_HASH_VER 1

static void md_field(EVP_MD_CTX *md, const char *v) {
  unsigned char hdr[9];
  unsigned long long n = v ? (unsigned long long)strlen(v) : 0;
  hdr[0] = v ? 1 : 0;
  for (int i = 0; i < 8; i++) hdr[1 + i] = (unsigned char)(n >> (8 * i));
  EVP_DigestUpdate(md, hdr, sizeof hdr);
  if (n) EVP_DigestUpdate(md, v, (size_t)n);
}

/* 1 when `out` holds the hash; 0 when it could not be computed, in which case
 * the caller writes the row in full — the pre-hash behaviour. */
static int content_hash(sink_state *st, const intel_item *it,
                        unsigned char out[CONTENT_HASH_LEN]) {
  if (!st->md && !(st->md = EVP_MD_CTX_new())) return 0;
  EVP_MD_CTX *md = st->md;
  if (EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1) return 0;
  unsigned char ver = CONTENT_HASH_VER;
  EVP_DigestUpdate(md, &ver, 1);
  md_field(md, it->title);   md_field(md, it->body);  md_field(md, it->summary);
  md_field(md, it->link);    md_field(md, it->author); md_field(md, it->lang);
  md_field(md, it->published_at);
  md_field(md, it->tags_json ? it->tags_json : "[]");
  md_field(md, it->properties_json ? it->properties_json : "{}");
  char geo[64];
  if (it->has_geo) snprintf(geo, sizeof geo, "%.17g,%.17g", it->lat, it->lon);
  md_field(md, it->has_geo ? geo : NULL);
  md_field(md, it->geometry_geojson);
  md_field(md, it->record_type);
  md_field(md, it->sub_source_id);
  unsigned char d[EVP_MAX_MD_SIZE]; unsigned dl = 0;
  if (EVP_DigestFinal_ex(md, d, &dl) != 1 || dl < CONTENT_HASH_LEN) return 0;
  memcpy(out, d, CONTENT_HASH_LEN);
  return 1;
}

/* ── BATCHED WRITES ───────────────────────────────────────────────────────
 *
 * One BEGIN…COMMIT per record cost 132.9 KB of WAL per REWRITTEN record
 * (tests/bench/bench_sink_rewrite.c) and a second transaction for the simhash
 * fingerprint — a WAL frame is a whole page however few bytes changed, so the
 * per-commit page set (the row, its ten-plus index pages, the FTS shadow
 * tables, the uid_map) was paid again for every record. A scheduled run emits
 * hundreds to hundreds of thousands of records through ONE sink, so the sink
 * now groups them.
 *
 * WHAT IS BATCHED, AND WHAT IS NOT. emit() does everything that needs no
 * database on the collector's own thread — uid, timestamp, content hash,
 * MeCab segmentation of title/body/summary/link/author/tags/properties — and
 * parks the finished record in memory (`pend`). Nothing is written, and NO
 * TRANSACTION IS OPEN, while the collector goes off to fetch the next page.
 * flush() then runs the same probe / upsert / FTS statements the per-record
 * path ran, for up to JO_SINK_BATCH records inside ONE `BEGIN IMMEDIATE …
 * COMMIT`, and afterwards the side effects that always ran after the commit:
 * alert_eval_on_item for each NEW row, then simhash_on_item_segmented for each
 * written row (these in one more transaction, simhash joins an open one).
 *
 * WHY DEFER INSTEAD OF HOLDING A TRANSACTION OPEN. The collector's emit()
 * calls are interleaved with network fetches (a page, a detail hop). A write
 * transaction left open across one of those holds the WAL's single writer
 * lock for as long as the upstream takes — tens of seconds on a slow host —
 * and every other worker, the HTTP loop included, then waits on busy_timeout
 * (5 s) and fails. A buffered record holds no lock at all. The lock is held
 * only inside flush(), whose cost is statements only (the CPU-heavy
 * segmentation was done at emit() time): ~0.1 ms a record, ~20 ms a batch of
 * 200 on the bench, and it is BEGIN IMMEDIATE so the wait for the lock is
 * taken up front under busy_timeout rather than discovered halfway as an
 * un-retryable SQLITE_BUSY_SNAPSHOT.
 *
 * WHEN IT FLUSHES: the buffer reaches JO_SINK_BATCH records (default 200) or
 * JO_SINK_BATCH_BYTES (8 MB, one 5 MB body must not pin 200 of them in
 * memory), or the OLDEST buffered record is JO_SINK_BATCH_MS old (500) when an
 * emit() arrives, and always when the stored counters are read, on
 * intel_sink_flush() and on intel_sink_free(). A buffer left alone while the
 * collector waits on HTTP is not written until the next emit or the end of
 * the run: that costs VISIBILITY (the rows are not in the database yet), never
 * a lock and never a record, because the scheduler always reads the counters
 * and frees the sink. JO_SINK_BATCH=1 restores the per-record path exactly:
 * emit() returns 1/0/-1 as source.h documents and nothing is deferred. The
 * entity-pivot dispatcher's sinks (intel_sink_rebind) are always that kind: its
 * dual_emit counts emit()'s answer into record_count/any_new and reads
 * nothing back from the table, but the answer must be exact.
 *
 * WHAT A BATCHED emit() RETURNS. 0 for an accepted record (new-ness is not
 * known until the batch is written; nothing in the tree branches on 1), -1
 * for an item with no uid, exactly as before. A record the DATABASE refuses
 * is learned at flush time, so it is reported on a LATER emit() of the same
 * kind (record or notice) as -1, once per refused record, which is what the
 * scheduler's `failed=` counter reads; whatever is still owed when the run
 * ends is intel_sink_failed_pending().
 *
 * FAILURE. If anything in a batch fails — a statement refused, the COMMIT
 * refused — the batch is rolled back and its records are committed ONE BY
 * ONE, each in its own transaction, with the per-record code's own verdict: so
 * one bad record cannot cost its neighbours, and only a record that fails on
 * its own is counted failed. If two consecutive records cannot even BEGIN (a
 * lock storm, which busy_timeout already waited out twice) the rest of the
 * batch is counted failed at once instead of waiting 5 s each. `stored` counts a
 * uid only after the commit that wrote it. */
typedef struct pend {
  char uid[UID_CAP];
  char fetched[40];
  unsigned char ch[CONTENT_HASH_LEN];
  int  have_hash, is_notice, has_geo;
  double lat, lon;
  char *title, *body, *summary, *link, *author, *lang, *published_at,
       *geojson, *rtype, *subsrc, *tags, *props;
  char *seg_title, *seg_body;
  fts_prep fp;
  size_t bytes;
} pend;

static char *dup0(const char *s, size_t *bytes) {
  if (!s) return NULL;
  size_t n = strlen(s);
  *bytes += n;
  char *d = malloc(n + 1);
  if (d) memcpy(d, s, n + 1);
  return d;
}

static void pend_free(pend *p) {
  if (!p) return;
  free(p->title); free(p->body); free(p->summary); free(p->link);
  free(p->author); free(p->lang); free(p->published_at); free(p->geojson);
  free(p->rtype); free(p->subsrc); free(p->tags); free(p->props);
  free(p->seg_title); free(p->seg_body);
  fts_prep_free(&p->fp);
  free(p);
}

/* Everything about a record that needs no database. NULL when the item has no
 * uid/remote_key (the caller reports -1, as before) or on OOM. */
static pend *pend_make(sink_state *st, const intel_item *it, int *no_uid) {
  *no_uid = 0;
  pend *p = calloc(1, sizeof *p);
  if (!p) return NULL;
  if (make_uid(p->uid, st->source_id, it) != 0) {
    fprintf(stderr, "[intel] %s: item without uid/remote_key skipped\n",
            st->source_id);
    *no_uid = 1; free(p); return NULL;
  }
  jo_iso_now(p->fetched, sizeof p->fetched);
  p->have_hash = content_hash(st, it, p->ch);
  p->is_notice = intel_item_is_notice(it);
  p->has_geo = it->has_geo; p->lat = it->lat; p->lon = it->lon;
  size_t b = 0;
  p->title = dup0(it->title, &b);       p->body = dup0(it->body, &b);
  p->summary = dup0(it->summary, &b);   p->link = dup0(it->link, &b);
  p->author = dup0(it->author, &b);     p->lang = dup0(it->lang, &b);
  p->published_at = dup0(it->published_at, &b);
  p->geojson = dup0(it->geometry_geojson, &b);
  p->rtype = dup0(it->record_type, &b); p->subsrc = dup0(it->sub_source_id, &b);
  p->tags  = dup0(it->tags_json ? it->tags_json : "[]", &b);
  p->props = dup0(it->properties_json ? it->properties_json : "{}", &b);
  /* Segmented once, used twice: the FTS mirror inside the transaction and the
   * simhash fingerprint after it. */
  p->seg_title = fts_segment(it->title ? it->title : "");
  p->seg_body  = fts_segment_body(it->body);
  fts_prep_make(&p->fp, it->summary, it->link, it->author,
                p->tags ? p->tags : "[]", p->props ? p->props : "{}");
  p->bytes = b * 2 + sizeof *p;     /* the segmented copies are about as big again */
  return p;
}

enum { RW_FAIL = -1, RW_UNCHANGED = 0, RW_WRITTEN = 1 };

/* The per-record statements, with NO transaction handling — the caller owns
 * BEGIN/COMMIT. Returns RW_FAIL, RW_UNCHANGED (only last_seen_at moved) or
 * RW_WRITTEN with *is_new set. Counts nothing and runs no side effect: those
 * belong to what the caller knows about the COMMIT. */
static int rec_write(sink_state *st, sqlite3 *h, const pend *p, int *is_new_out) {
  const char *uid = p->uid;
  *is_new_out = 0;
  /* Is this genuinely a new row? source.h documents emit() as "1 if a NEW
   * row, 0 if updated", but that was never true: after
   * "INSERT ... ON CONFLICT(uid) DO UPDATE" sqlite3_changes() is 1 either
   * way, so emit() always returned 1. One indexed PK probe makes the
   * documented contract real.
   *
   * This matters now that alert_eval hangs off ingest: without it, a rule
   * with dedup_window_sec=0 would re-fire on every scheduled refresh of an
   * unchanged item — a collector on a 60s interval would emit an alert a
   * minute, forever.
   *
   * The same probe reads the row's content hash and whether it has a search
   * index row, for the unchanged-re-fetch decision below. Inside a batch it
   * also sees the batch's own earlier writes (same connection), so a uid that
   * repeats within a batch is probed exactly as it would have been when each
   * record had its own commit. */
  int is_new = 1, unchanged = 0;
  {
    sqlite3_stmt *ex = stmt_get(h, &st->q_probe,
      "SELECT i.content_hash,"
      "       EXISTS(SELECT 1 FROM intel_items_fts_uid_map m WHERE m.uid=i.uid)"
      "  FROM intel_items i WHERE i.uid=?1");
    if (ex) {
      sqlite3_bind_text(ex, 1, uid, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(ex) == SQLITE_ROW) {
        is_new = 0;
        const void *old = sqlite3_column_blob(ex, 0);
        unchanged = p->have_hash && old &&
                    sqlite3_column_bytes(ex, 0) == CONTENT_HASH_LEN &&
                    memcmp(old, p->ch, CONTENT_HASH_LEN) == 0 &&
                    sqlite3_column_int(ex, 1);
      }
      sqlite3_reset(ex);
    }
  }

  /* UNCHANGED RE-FETCH. Most scheduled runs re-read a feed that has not
   * moved, and every one of those records used to be rewritten in full: the
   * row and its ten-plus indexes (fetched_at is in four of them), a DELETE and
   * re-INSERT of its FTS row — which also wiped `keywords` and sent
   * translate.c back to re-sync it — the uid_map upsert, and a MeCab pass for
   * simhash. Measured (tests/bench/bench_sink_rewrite.c, 20,000 records): the
   * second, identical pass appended 2.66 GB to the WAL, 132.9 KB per record.
   *
   * When the stored hash says nothing changed — and the row is in the search
   * index, so a row that lost its FTS row still self-heals here as it always
   * did — the only write is `last_seen_at`, an unindexed column. fetched_at is
   * deliberately NOT moved: it now means "when this content was written", which
   * is what its readers want. The embedding pod re-examines any row whose
   * fetched_at passes its last look (embed_pod.c, DELTA), so bumping it would
   * re-queue every unchanged row of every scheduled feed; the time-window and
   * date-fallback filters (COALESCE(published_at, fetched_at)) would otherwise
   * show an undated, unchanged record as new on every refresh. The one reader
   * that means "last time the source was fetched" — the per-source freshness
   * in intel_sources_view() — reads last_seen_at.
   *
   * `stored` counts it exactly as before (distinct uids left behind), and it
   * is not new, so no alert fires — the same answers the full path gives. */
  if (unchanged) {
    sqlite3_stmt *t = stmt_get(h, &st->q_touch,
      "UPDATE intel_items SET last_seen_at=?2 WHERE uid=?1");
    int trc = SQLITE_ERROR;
    if (t) {
      sqlite3_bind_text(t, 1, uid, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(t, 2, p->fetched, -1, SQLITE_TRANSIENT);
      trc = sqlite3_step(t);
      sqlite3_reset(t);
    }
    if (trc != SQLITE_DONE) {
      fprintf(stderr, "[intel] %s: last_seen_at update failed for %s (%s) — "
                      "row not counted\n", st->source_id, uid, sqlite3_errmsg(h));
      return RW_FAIL;
    }
    return RW_UNCHANGED;
  }

  sqlite3_stmt *s = stmt_get(h, &st->q_upsert, SQL_UPSERT);
  if (!s) return RW_FAIL;
  bind_txt(s, 1, uid);
  bind_txt(s, 2, st->source_id);
  bind_txt(s, 3, p->title);
  bind_txt(s, 4, p->body);
  bind_txt(s, 5, p->summary);
  bind_txt(s, 6, p->link);
  bind_txt(s, 7, p->author);
  bind_txt(s, 8, p->lang);
  bind_txt(s, 9, p->published_at);
  bind_txt(s, 10, p->fetched);
  bind_txt(s, 11, p->tags);
  bind_txt(s, 12, p->props);
  if (p->has_geo) { sqlite3_bind_double(s, 13, p->lat); sqlite3_bind_double(s, 14, p->lon); }
  else { sqlite3_bind_null(s, 13); sqlite3_bind_null(s, 14); }
  bind_txt(s, 15, p->has_geo ? "native" : NULL);
  bind_txt(s, 16, p->has_geo ? p->fetched : NULL);
  bind_txt(s, 17, p->rtype);
  bind_txt(s, 18, p->subsrc);
  bind_txt(s, 19, p->geojson);
  bind_txt(s, 20, st->tenant_id[0] ? st->tenant_id : "legacy");
  if (p->have_hash) sqlite3_bind_blob(s, 21, p->ch, CONTENT_HASH_LEN, SQLITE_TRANSIENT);
  else              sqlite3_bind_null(s, 21);
  int rc = sqlite3_step(s);
  sqlite3_reset(s);
  if (rc != SQLITE_DONE) return RW_FAIL;

  int changes = sqlite3_changes(h); /* 1 insert, or update */
  fts_write_prepped(h, &st->fts, uid, p->seg_title, p->seg_body, &p->fp);
  *is_new_out = (changes && is_new);
  return RW_WRITTEN;
}

/* One transaction around a run of records. Returns 1 when every record in
 * [lo,hi) was written and the COMMIT took; 0 after a ROLLBACK otherwise (a
 * record refused, BEGIN or COMMIT failed — *begin_failed says which of the
 * last two was BEGIN). res[] is filled for the records written. */
static int txn_write(sink_state *st, sqlite3 *h, pend **b, int lo, int hi,
                     int *res, int *isnew, int *begin_failed) {
  *begin_failed = 0;
  if (sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
    fprintf(stderr, "[intel] %s: BEGIN failed (%s)\n", st->source_id,
            sqlite3_errmsg(h));
    *begin_failed = 1;
    return 0;
  }
  for (int i = lo; i < hi; i++) {
    res[i] = rec_write(st, h, b[i], &isnew[i]);
    if (res[i] == RW_FAIL) {
      fprintf(stderr, "[intel] %s: write failed for %s (%s)\n", st->source_id,
              b[i]->uid, sqlite3_errmsg(h));
      sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
      return 0;
    }
  }
  /* THE COMMIT IS THE WRITE. Its return code used to be ignored, so a COMMIT
   * that failed (SQLITE_FULL on a full volume, SQLITE_IOERR, a lock that
   * could not be had) left emit() returning success for a row that was never
   * written, and `stored` counting it. Roll back explicitly — some failures
   * leave the transaction open on this connection, and the next BEGIN would
   * then fail and fold it into an unrelated row's transaction. */
  if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
    fprintf(stderr, "[intel] %s: COMMIT failed for %d record(s) starting at %s "
                    "(%s) — not stored\n", st->source_id, hi - lo, b[lo]->uid,
            sqlite3_errmsg(h));
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
    return 0;
  }
  return 1;
}

/* Write every buffered record, then run the post-commit side effects. Returns
 * the verdict of the LAST record (RW_WRITTEN with *new_out, RW_UNCHANGED or
 * RW_FAIL) — which is what emit() returns for a batch of one. */
static int flush_buf(sink_state *st, int *new_out) {
  int n = st->nbuf;
  if (new_out) *new_out = 0;
  if (n == 0) return RW_UNCHANGED;
  sqlite3 *h = st->db->h;
  if (st->stmt_db != h) { sink_stmts_free(st); st->stmt_db = h; }
  pend **b = st->buf;
  int *res = calloc((size_t)n, sizeof *res), *isnew = calloc((size_t)n, sizeof *isnew);
  if (!res || !isnew) {
    /* Cannot even hold the verdicts: refuse the batch, loudly, as failed. */
    fprintf(stderr, "[intel] %s: OOM flushing %d record(s) — counted failed\n",
            st->source_id, n);
    free(res); free(isnew);
    for (int i = 0; i < n; i++) {
      if (b[i]->is_notice) st->owed_notice++; else st->owed_rec++;
      st->failed_total++;
      pend_free(b[i]);
    }
    st->nbuf = 0; st->nbytes = 0;
    return RW_FAIL;
  }

  long long tr0 = jo_mono_ms();
  int bf = 0;
  if (!txn_write(st, h, b, 0, n, res, isnew, &bf)) {
    if (n == 1) {
      res[0] = RW_FAIL;                 /* the per-record verdict, as before */
    } else {
      /* One bad record (or one failed COMMIT) must not take its neighbours
       * with it: redo the batch a record at a time. Two BEGINs refused in a
       * row is a lock storm — busy_timeout has waited twice already — so the
       * remainder is failed at once rather than at 5 s apiece. */
      int storm = 0;
      for (int i = 0; i < n; i++) {
        if (storm >= 2) { res[i] = RW_FAIL; continue; }
        int one_bf = 0;
        int ok = txn_write(st, h, b, i, i + 1, res, isnew, &one_bf);
        if (!ok) res[i] = RW_FAIL;
        storm = one_bf ? storm + 1 : 0;
      }
    }
  }

  /* From here a record is either committed (res >= 0) or refused. */
  long long tr1 = jo_mono_ms();      /* end of the write transaction(s) */
  int last = res[n - 1], last_new = isnew[n - 1];
  const char *ten = st->tenant_id[0] ? st->tenant_id : "legacy";
  int nsimhash = 0;
  for (int i = 0; i < n; i++) {
    if (res[i] == RW_FAIL) {
      if (b[i]->is_notice) st->owed_notice++; else st->owed_rec++;
      st->failed_total++;
      continue;
    }
    /* Rule 4b (see the block at the top of this file). Counted only once the
     * COMMIT has succeeded, so the set holds exactly the uids that are now
     * rows. */
    if (seen_note(st, b[i]->uid) && b[i]->is_notice) st->seen_notice_n++;
    if (res[i] == RW_WRITTEN) nsimhash++;
  }
  /* Alert matching (roadmap P0.1) — AFTER the commit, never inside it. An
   * alert write must not be able to roll back the ingest that produced it,
   * and a rule evaluation must see the row it is evaluating. Only new rows
   * are evaluated; re-ingest of an unchanged item is not news. */
  for (int i = 0; i < n; i++)
    if (res[i] == RW_WRITTEN && isnew[i])
      alert_eval_on_item(st->db, ten, b[i]->uid);
  /* Near-duplicate clustering (roadmap 25). Deliberately NOT gated on is_new:
   * an UPDATE can change title/body, and a stale fingerprint would cluster the
   * row by text it no longer has. simhash joins a transaction that is already
   * open on the connection, so a batch of them shares one commit instead of
   * paying one each; a failure there leaves simhash NULL for the backfill, as
   * it always has, and cannot touch the ingest, which is already committed. */
  if (nsimhash > 0) {
    int wrap = nsimhash > 1 &&
               sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK;
    for (int i = 0; i < n; i++)
      if (res[i] == RW_WRITTEN)
        simhash_on_item_segmented(st->db, ten, b[i]->uid, b[i]->seg_title,
                                  b[i]->seg_body);
    if (wrap && sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
      fprintf(stderr, "[intel] %s: simhash batch COMMIT failed (%s) — "
                      "fingerprints left for the backfill\n", st->source_id,
              sqlite3_errmsg(h));
      sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
    }
  }

  if (st->trace)       /* JO_SINK_TRACE=1: what a flush costs, i.e. the lock hold */
    fprintf(stderr, "[intel-trace] %s flush n=%d write_txn=%lldms post=%lldms\n",
            st->source_id, n, tr1 - tr0, jo_mono_ms() - tr1);
  for (int i = 0; i < n; i++) pend_free(b[i]);
  st->nbuf = 0; st->nbytes = 0;
  free(res); free(isnew);
  if (new_out) *new_out = last_new;
  return last;
}


static int emit(struct intel_sink *self, const intel_item *it) {
  sink_state *st = self->ctx;
  sqlite3 *h = st->db->h;
  /* Statements belong to a connection; a sink re-pointed at another handle
   * (never done today) must not step a statement from the old one. */
  if (st->stmt_db != h) { sink_stmts_free(st); st->stmt_db = h; }

  int no_uid = 0;
  pend *p = pend_make(st, it, &no_uid);
  if (!p) return -1;

  /* A refusal learned at an earlier flush is reported here, on an emit of the
   * same kind, so the scheduler's per-call `failed=` sees it. The record being
   * emitted now is still buffered and written normally. */
  int owed = 0;
  if (p->is_notice ? st->owed_notice > 0 : st->owed_rec > 0) {
    if (p->is_notice) st->owed_notice--; else st->owed_rec--;
    owed = 1;
  }

  if (st->batch_max <= 1) {                       /* per-record: exact answer */
    if (!st->buf && !(st->buf = malloc(sizeof *st->buf))) { pend_free(p); return -1; }
    st->bufcap = 1;
    st->buf[st->nbuf++] = p;
    int isnew = 0, notice = p->is_notice;
    int r = flush_buf(st, &isnew);
    if (r == RW_FAIL) {
      /* The caller is told by the return value; it is not "owed" as well. */
      if (notice) st->owed_notice--; else st->owed_rec--;
      return -1;
    }
    return isnew ? 1 : 0;
  }

  if (st->nbuf == st->bufcap) {
    int ncap = st->bufcap ? st->bufcap * 2 : 64;
    pend **nb = realloc(st->buf, sizeof *nb * (size_t)ncap);
    if (!nb) { pend_free(p); return -1; }
    st->buf = nb; st->bufcap = ncap;
  }
  if (st->nbuf == 0) st->t0_ms = jo_mono_ms();
  st->buf[st->nbuf++] = p;
  st->nbytes += p->bytes;
  if (st->nbuf >= st->batch_max || st->nbytes >= st->batch_bytes ||
      (st->batch_ms > 0 && jo_mono_ms() - st->t0_ms >= st->batch_ms))
    flush_buf(st, NULL);
  return owed ? -1 : 0;
}

/* See intel.h. */
void intel_sink_flush(intel_sink *k) {
  if (!k || !k->ctx || k->emit != emit) return;
  sink_state *st = k->ctx;
  if (st->nbuf > 0) flush_buf(st, NULL);
}

long intel_sink_failed(const intel_sink *k) {
  if (!k || !k->ctx || k->emit != emit) return -1;
  return ((const sink_state *)k->ctx)->failed_total;
}

long intel_sink_failed_pending(const intel_sink *k) {
  if (!k || !k->ctx || k->emit != emit) return -1;
  const sink_state *st = k->ctx;
  return st->owed_rec + st->owed_notice;
}


/* emit() for a sink that failed to allocate its state. This runs on every
 * collector/dispatch worker thread once per source per cycle — under real
 * memory pressure an unchecked calloc here would take down the whole run on
 * a NULL sink_state dereference. Refusing every emit is the same "degrade,
 * don't crash" contract intel_sink_stored()/intel_sink_rebind() already give
 * a non-intel-sink ctx (their `k->emit != emit` check makes this sink fail
 * both the same way): the source's records are lost for this run, reported
 * as refused rather than losing the process. */
static int emit_oom(struct intel_sink *self, const intel_item *it) {
  (void)self; (void)it;
  return -1;
}

intel_sink intel_sink_make(db_handle *db, const char *source_id,
                           const char *tenant_id) {
  sink_state *st = calloc(1, sizeof *st);
  if (!st) {
    fprintf(stderr, "[intel] %s: OOM allocating sink state; every emit this "
                    "run will be refused\n", source_id ? source_id : "unknown");
    intel_sink k; k.ctx = NULL; k.emit = emit_oom;
    return k;
  }
  st->magic = SINK_MAGIC;
  st->db = db;
  st->seen.max_slots = SEEN_MAX;
  st->batch_max   = jo_env_int("JO_SINK_BATCH", 200, 1, 100000);
  st->trace       = getenv("JO_SINK_TRACE") != NULL;
  st->batch_ms    = jo_env_int("JO_SINK_BATCH_MS", 500, 0, 600000);
  st->batch_bytes = (size_t)jo_env_int("JO_SINK_BATCH_BYTES", 8 << 20, 1 << 16, 1 << 30);
  snprintf(st->source_id, sizeof st->source_id, "%s", source_id ? source_id : "unknown");
  if (tenant_id) snprintf(st->tenant_id, sizeof st->tenant_id, "%s", tenant_id);
  intel_sink k; k.ctx = st; k.emit = emit;
  return k;
}

/* See intel.h. The `sink->emit == emit` test is not paranoia: the scheduler
 * hands collectors a COUNTING WRAPPER around this sink, and asking the wrapper
 * for its stored count would read a foreign ctx as a sink_state. Refusing is
 * the only safe answer; a caller must pass the sink make() returned. */
long intel_sink_stored(const intel_sink *k, int *exact) {
  if (exact) *exact = 1;
  if (!k || !k->ctx || k->emit != emit) return -1;
  /* The count is of COMMITTED rows, so anything still buffered is written
   * first (the scheduler reads this before it frees the sink). */
  intel_sink_flush((intel_sink *)k);
  const sink_state *st = k->ctx;
  if (exact) *exact = !st->seen.inexact;
  return (long)st->seen.n;
}

long intel_sink_stored_notices(const intel_sink *k) {
  if (!k || !k->ctx || k->emit != emit) return -1;
  intel_sink_flush((intel_sink *)k);
  const sink_state *st = k->ctx;
  return (long)st->seen_notice_n;
}

/* The db_handle is borrowed; the sink owns the distinct-uid table, the cached
 * statements and the digest context (released by intel_sink_free). */
int intel_sink_rebind(const intel_sink *base, const char *source_id,
                      intel_sink *out) {
  if (!base || !base->ctx || !source_id || !*source_id || !out) return 0;
  const sink_state *b = (const sink_state *)base->ctx;
  if (b->magic != SINK_MAGIC) return 0;    /* not an intel sink — refuse */
  *out = intel_sink_make(b->db, source_id, b->tenant_id[0] ? b->tenant_id : NULL);
  /* The dispatcher counts emit()'s answer (refused / new) into its response,
   * so a pivot sink writes per record and answers exactly. */
  if (out->ctx) ((sink_state *)out->ctx)->batch_max = 1;
  return 1;
}

void intel_sink_free(intel_sink *k) {
  if (!k || !k->ctx) return;
  sink_state *st = k->ctx;
  /* A sink freed with records still buffered writes them first. A free() that
   * dropped them would be the silent loss this file exists to prevent. Only a
   * sink this file made has a buffer (emit_oom has no ctx at all). */
  if (k->emit == emit) intel_sink_flush(k);
  free(st->buf);
  /* Before the connection closes: sqlite3_close() refuses (SQLITE_BUSY) a
   * connection that still has prepared statements. */
  sink_stmts_free(st);
  EVP_MD_CTX_free(st->md);
  seen_hset_free(&st->seen);
  free(st);
  k->ctx = NULL;
  k->emit = NULL;
}
