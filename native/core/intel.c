#include "intel.h"
#include "fts.h"
#include "fts_schema.h"
#include "alert_eval.h"
#include "simhash.h"
#include "../third_party/sqlite3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

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
#define SEEN_INIT 1024u
#define SEEN_MAX  (1u << 22)

typedef struct {
  db_handle *db; char source_id[128]; char tenant_id[64];
  unsigned long long *seen;   /* open-addressed set of uid hashes, 0 = empty  */
  size_t seen_cap;            /* slots, always a power of two                 */
  size_t seen_n;              /* distinct uids stored this run                */
  int    seen_exact;          /* 0 once the ceiling or an ENOMEM made it a floor */
} sink_state;

static unsigned long long uid_hash(const char *s) {
  unsigned long long h = 1469598103934665603ULL;      /* FNV-1a 64 offset */
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h ? h : 1ULL;        /* 0 is the empty-slot marker, never a key */
}

static void seen_put(unsigned long long *tab, size_t cap, unsigned long long h) {
  size_t i = (size_t)(h & (cap - 1));
  while (tab[i]) {
    if (tab[i] == h) return;
    i = (i + 1) & (cap - 1);
  }
  tab[i] = h;
}

/* Record `uid` in the run's distinct set. Silent about everything except the
 * one thing that matters: if it cannot grow, seen_exact drops to 0 and the
 * caller reports a floor instead of a wrong number. */
static void seen_add(sink_state *st, const char *uid) {
  unsigned long long h = uid_hash(uid);
  if (st->seen && st->seen_n * 10 >= st->seen_cap * 7) {
    size_t ncap = st->seen_cap * 2;
    if (ncap > SEEN_MAX) { st->seen_exact = 0; return; }
    unsigned long long *nt = calloc(ncap, sizeof *nt);
    if (!nt) { st->seen_exact = 0; return; }
    for (size_t i = 0; i < st->seen_cap; i++)
      if (st->seen[i]) seen_put(nt, ncap, st->seen[i]);
    free(st->seen); st->seen = nt; st->seen_cap = ncap;
  } else if (!st->seen) {
    st->seen = calloc(SEEN_INIT, sizeof *st->seen);
    if (!st->seen) { st->seen_exact = 0; return; }
    st->seen_cap = SEEN_INIT;
  }
  size_t i = (size_t)(h & (st->seen_cap - 1));
  while (st->seen[i]) {
    if (st->seen[i] == h) return;               /* already stored this run */
    i = (i + 1) & (st->seen_cap - 1);
  }
  st->seen[i] = h;
  st->seen_n++;
}

static void iso_now(char *b, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  /* The %0Nd widths are minimums, not caps: to -Wformat-truncation
   * `tm_year + 1900` is a plain int worth up to 11 characters, so this
   * fixed 24-char stamp "may be truncated". The modulos are identity for
   * every value gmtime_r can return and make the 24 provable, not merely
   * true. */
  snprintf(b, n, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
           (unsigned)(tm.tm_year+1900) % 10000u, (unsigned)(tm.tm_mon+1) % 100u,
           (unsigned)tm.tm_mday % 100u, (unsigned)tm.tm_hour % 100u,
           (unsigned)tm.tm_min % 100u, (unsigned)tm.tm_sec % 100u,
           (unsigned)(tv.tv_usec/1000) % 1000u);
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
static const char *SQL_UPSERT =
 "INSERT INTO intel_items"
 " (uid,source_id,title,body,summary,link,author,language,published_at,"
 "  fetched_at,tags,properties,lat,lon,geom_source,geom_at,record_type,"
 "  sub_source_id,geometry,tenant_id)"
 " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20)"
 " ON CONFLICT(uid) DO UPDATE SET"
 "  title=excluded.title, body=excluded.body, summary=excluded.summary,"
 "  link=excluded.link, author=excluded.author, language=excluded.language,"
 "  published_at=excluded.published_at, fetched_at=excluded.fetched_at,"
 "  tags=excluded.tags, properties=excluded.properties,"
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
 * fresh rowid) on every collector run and wipes their write. Do not "fix" that
 * by carrying keywords over here without reading translate.h first.
 *
 * link/author/tags/props are v2 columns (see fts_schema.h): before them a
 * camera row — camera_store.c writes ONLY title, everything else is properties
 * — was findable by name and by nothing else. Flattening is shared with the
 * rebuild migration so the write path and the backfill cannot drift. */
static void fts_write(sqlite3 *h, const char *uid, const char *title,
                      const char *body, const char *summary,
                      const char *link, const char *author,
                      const char *tags_json, const char *props_json) {
  sqlite3_stmt *s;
  sqlite3_int64 old = -1;
  if (sqlite3_prepare_v2(h, "SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) old = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  char *st = fts_segment(title ? title : "");
  char *sb = fts_segment(body ? body : "");
  char *ss = fts_segment(summary ? summary : "");
  /* Latin passes fts_segment straight through, so segmenting the URL/author
   * costs nothing on the common case and still tokenizes a Japanese byline. */
  char *sl = fts_segment(link ? link : "");
  char *sa = fts_segment(author ? author : "");
  char *ftags = fts_flatten_json(tags_json);
  char *fprops = fts_flatten_json(props_json);
  char *sg = fts_segment(ftags);
  char *sp = fts_segment(fprops);
  free(ftags); free(fprops);
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
   *      deleted row is invisible until a full rebuild. */
  if (sqlite3_prepare_v2(h, fts_insert_sql(h), -1, &s, NULL) == SQLITE_OK) {
    if (old >= 0) {
      sqlite3_stmt *d;
      if (sqlite3_prepare_v2(h, "DELETE FROM intel_items_fts WHERE rowid=?1",
                             -1, &d, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(d, 1, old); sqlite3_step(d); sqlite3_finalize(d);
      }
    }
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, st, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, sb, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, ss, -1, SQLITE_TRANSIENT);
    /* ?5..?8 exist only in the v2 statement; binding an unused index on the
     * v1 one is a no-op range error, not a failure, but skip it anyway. */
    if (fts_index_is_v2(h)) {
      sqlite3_bind_text(s, 5, sl, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 6, sa, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 7, sg, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 8, sp, -1, SQLITE_TRANSIENT);
    }
    inserted = (sqlite3_step(s) == SQLITE_DONE);
    sqlite3_finalize(s);
  } else {
    fprintf(stderr, "[intel] fts insert not preparable for %s (%s) — the "
                    "existing index row is left in place\n",
            uid, sqlite3_errmsg(h));
  }
  free(st); free(sb); free(ss); free(sl); free(sa); free(sg); free(sp);
  if (!inserted) {
    fprintf(stderr, "[intel] fts insert failed for %s: %s\n", uid, sqlite3_errmsg(h));
    return;                       /* leave the existing map row alone */
  }
  sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
  if (sqlite3_prepare_v2(h,
      "INSERT INTO intel_items_fts_uid_map(uid,rowid) VALUES(?1,?2)"
      " ON CONFLICT(uid) DO UPDATE SET rowid=excluded.rowid",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 2, rid);
    sqlite3_step(s); sqlite3_finalize(s);
  }
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
    fts_write(db->h, uid, v[0], v[1], v[2], v[3], v[4],
              v[5] ? v[5] : "[]", v[6] ? v[6] : "{}");
    for (int i = 0; i < 7; i++) free(v[i]);
    rc = 0;
  } else {
    sqlite3_finalize(s);
  }
  return rc;
}

static int emit(struct intel_sink *self, const intel_item *it) {
  sink_state *st = self->ctx;
  sqlite3 *h = st->db->h;

  /* uid: explicit → "<source>|<remote_key>" → (hash fallback = P5 toolkit). */
  char uid[512];
  if (it->uid && *it->uid) snprintf(uid, sizeof uid, "%s", it->uid);
  else if (it->remote_key && *it->remote_key)
    snprintf(uid, sizeof uid, "%s|%s", st->source_id, it->remote_key);
  else { fprintf(stderr, "[intel] %s: item without uid/remote_key skipped\n",
                 st->source_id); return -1; }

  char fetched[40]; iso_now(fetched, sizeof fetched);
  int has_geo = it->has_geo;
  const char *geom_src = has_geo ? "native" : NULL;

  /* Is this genuinely a new row? source.h documents emit() as "1 if a NEW
   * row, 0 if updated", but that was never true: after
   * "INSERT ... ON CONFLICT(uid) DO UPDATE" sqlite3_changes() is 1 either
   * way, so emit() always returned 1. One indexed PK probe makes the
   * documented contract real.
   *
   * This matters now that alert_eval hangs off ingest: without it, a rule
   * with dedup_window_sec=0 would re-fire on every scheduled refresh of an
   * unchanged item — a collector on a 60s interval would emit an alert a
   * minute, forever. No existing caller branches on the return value
   * (checked across all 569 emit() call sites), so tightening it is safe. */
  int is_new = 1;
  {
    sqlite3_stmt *ex;
    if (sqlite3_prepare_v2(h, "SELECT 1 FROM intel_items WHERE uid=?1",
                           -1, &ex, NULL) == SQLITE_OK) {
      sqlite3_bind_text(ex, 1, uid, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(ex) == SQLITE_ROW) is_new = 0;
      sqlite3_finalize(ex);
    }
  }

  sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, SQL_UPSERT, -1, &s, NULL) != SQLITE_OK) {
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL); return -1;
  }
  bind_txt(s, 1, uid);
  bind_txt(s, 2, st->source_id);
  bind_txt(s, 3, it->title);
  bind_txt(s, 4, it->body);
  bind_txt(s, 5, it->summary);
  bind_txt(s, 6, it->link);
  bind_txt(s, 7, it->author);
  bind_txt(s, 8, it->lang);
  bind_txt(s, 9, it->published_at);
  bind_txt(s, 10, fetched);
  bind_txt(s, 11, it->tags_json ? it->tags_json : "[]");
  bind_txt(s, 12, it->properties_json ? it->properties_json : "{}");
  if (has_geo) { sqlite3_bind_double(s, 13, it->lat); sqlite3_bind_double(s, 14, it->lon); }
  else { sqlite3_bind_null(s, 13); sqlite3_bind_null(s, 14); }
  bind_txt(s, 15, geom_src);
  bind_txt(s, 16, has_geo ? fetched : NULL);
  bind_txt(s, 17, it->record_type);
  bind_txt(s, 18, it->sub_source_id);
  bind_txt(s, 19, it->geometry_geojson);
  bind_txt(s, 20, st->tenant_id[0] ? st->tenant_id : "legacy");
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) { sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL); return -1; }

  int changes = sqlite3_changes(h); /* 1 insert, or update */
  /* Rule 4b (see the block at the top of this file). Counted here, AFTER the
   * upsert stepped SQLITE_DONE and before anything can return early, so the
   * set holds exactly the uids that are now rows. */
  seen_add(st, uid);
  fts_write(h, uid, it->title, it->body, it->summary, it->link, it->author,
            it->tags_json ? it->tags_json : "[]",
            it->properties_json ? it->properties_json : "{}");
  sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);

  /* Alert matching (roadmap P0.1) — AFTER the commit, never inside it. An
   * alert write must not be able to roll back the ingest that produced it,
   * and a rule evaluation must see the row it is evaluating. Only new rows
   * are evaluated; re-ingest of an unchanged item is not news. */
  if (changes && is_new)
    alert_eval_on_item(st->db, st->tenant_id[0] ? st->tenant_id : "legacy", uid);

  /* Near-duplicate clustering (roadmap 25). Deliberately NOT gated on is_new:
   * an UPDATE can change title/body, and a stale fingerprint would cluster the
   * row by text it no longer has. */
  if (changes)
    simhash_on_item(st->db, st->tenant_id[0] ? st->tenant_id : "legacy",
                    uid, it->title, it->body);

  return (changes && is_new) ? 1 : 0;
}

intel_sink intel_sink_make(db_handle *db, const char *source_id,
                           const char *tenant_id) {
  sink_state *st = calloc(1, sizeof *st);
  st->db = db;
  st->seen_exact = 1;
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
  const sink_state *st = k->ctx;
  if (exact) *exact = st->seen_exact;
  return (long)st->seen_n;
}

/* sink_state owns exactly one allocation of its own — the distinct-uid table
 * (the db_handle is borrowed and the two char arrays are inline). */
void intel_sink_free(intel_sink *k) {
  if (!k || !k->ctx) return;
  sink_state *st = k->ctx;
  free(st->seen);
  free(st);
  k->ctx = NULL;
  k->emit = NULL;
}
