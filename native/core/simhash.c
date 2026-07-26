/* core/simhash.c — near-duplicate fingerprinting and clustering. The design,
 * the pigeonhole recall argument, the token floor, the representative-migration
 * strategy, the DDL and the orchestrator wiring all live in simhash.h; this
 * file is the mechanics.
 *
 * Shape of the ingest call: segment (MeCab, via fts.h), fold tokens into a
 * 64-bit fingerprint with no allocation beyond one scratch accumulator, then
 * one short write transaction containing 4 band upserts, one single-row UPDATE
 * and — only when the item actually matched something — the merge statements.
 * Every SQLite error is swallowed: this hangs off core/intel.c's emit() and
 * must never be able to fail the ingest that called it.
 *
 * Shape of a backfill batch: a uid page read (index range over the partial
 * idx_intel_items_simhash_todo, ascending publication order), then hashing
 * with NO transaction open (MeCab is by far the slowest thing here and holding
 * SQLite's single write lock across it would stall every httpd writer), then
 * one BEGIN IMMEDIATE holding only the writes. IMMEDIATE because the batch is
 * write-only: taking the write lock up front turns a possible mid-batch
 * upgrade failure into an ordinary wait. */
#include "simhash.h"
#include "fts.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SIMHASH_BANDS != 4 || SIMHASH_BAND_BITS != 16
#error "the candidate SQL below is written for exactly 4 x 16-bit bands"
#endif
#if SIMHASH_MAX_DISTANCE >= SIMHASH_BANDS
#error "MAX_DISTANCE must be < BANDS or the band index stops being exhaustive"
#endif

/* core/intel.c builds uids into a char[512]; matching that here means a uid can
 * never be truncated into a *different* uid. Timestamps are ISO-8601. */
#define UID_MAX 512
#define TEN_MAX 64
#define TS_MAX  64

/* Distinct-token set: power of two and >= 2 * SIMHASH_MAX_TOKENS so linear
 * probing never exceeds load 0.5 and the probe loop always terminates. */
#define SEEN_SLOTS 4096

static int cancelled(volatile int *c) { return c && *c; }

/* Truncating copy. Used instead of snprintf("%s") wherever both sides are
 * fixed-size buffers: gcc cannot see that a row of a [N][UID_MAX] array is
 * NUL-terminated within its own row and warns about the whole array
 * (-Wformat-truncation), and -Wall -Wextra is treated as errors here. */
static void ccopy(char *dst, size_t n, const char *src) {
  if (!n) return;
  size_t l = strlen(src);
  if (l >= n) l = n - 1;
  memcpy(dst, src, l);
  dst[l] = 0;
}

/* ── token stream ─────────────────────────────────────────────────────────
 * fts_segment() joins MeCab surface forms with single spaces; on the Latin
 * passthrough it returns the input untouched, so whitespace generally is the
 * separator and ASCII punctuation has to be trimmed off the ends. */

static int is_sep(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static int ascii_punct(unsigned char c) {
  if (c >= 0x80) return 0;
  return !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z'));
}

/* Japanese punctuation MeCab emits as its own morphemes. They survive the
 * ASCII trim above (they are multi-byte) and would otherwise be high-frequency
 * features carrying no information about which story this is. */
static const char *JP_PUNCT[] = {
  "、", "。", "「", "」", "『", "』", "・",
  "：", "；", "！", "？", "（", "）", "〜",
  "…", "―", "／", "　", "－"
};

/* Closed-class morphemes that appear in virtually every document in the
 * corpus, plus the English function words the Latin passthrough leaves in
 * place. With term-frequency voting these dominate the fingerprint and pull
 * every document toward one common hash — they say nothing about WHICH story
 * this is. Deliberately short: an aggressive stoplist starts deleting content,
 * and this list also gates SIMHASH_MIN_TOKENS, so over-trimming would push
 * real articles below the floor. */
static const char *STOPWORDS[] = {
  "の", "に", "は", "を", "が", "と", "で",
  "も", "や", "へ", "から", "まで", "より",
  "です", "ます", "した", "する",
  "ある", "いる", "こと", "もの",
  "この", "その", "あの", "という",
  "ため", "など",
  "the", "a", "an", "of", "and", "to", "in", "for", "on", "with", "at",
  "by", "from", "is", "are", "was", "were", "be", "been", "it", "its",
  "that", "this", "as", "or"
};

/* Length-and-first-byte prefiltered linear scan. ~50 entries against at most
 * SIMHASH_MAX_TOKENS tokens is a few hundred microseconds per document, which
 * is noise next to the MeCab pass that produced the tokens. */
static int in_list(const char **list, size_t n, const char *b, size_t len) {
  for (size_t i = 0; i < n; i++) {
    const char *w = list[i];
    if (w[0] != b[0]) continue;
    if (strlen(w) != len) continue;
    if (memcmp(w, b, len) == 0) return 1;
  }
  return 0;
}

/* FNV-1a 64 with a splitmix64 finalizer. FNV alone avalanches poorly in the
 * low bits, and a simhash votes on all 64 bits independently — a hash whose
 * low bits track the last byte of the token would make those bit positions
 * near-copies of each other and cost real bits of the 64-bit budget. */
static uint64_t tok_hash(const char *p, size_t n) {
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)p[i];
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);  /* ASCII fold */
    h ^= (uint64_t)c;
    h *= 1099511628211ULL;
  }
  h ^= h >> 30; h *= 0xbf58476d1ce4e5b9ULL;
  h ^= h >> 27; h *= 0x94d049bb133111ebULL;
  h ^= h >> 31;
  return h ? h : 1ULL;      /* 0 is the "empty slot" sentinel in seen[] */
}

typedef struct {
  long     v[64];
  uint64_t seen[SEEN_SLOTS];
  int      distinct;
  int      total;
} sh_acc;

static void acc_push(sh_acc *a, const char *b, size_t n, int w) {
  while (n && ascii_punct((unsigned char)b[0])) { b++; n--; }
  while (n && ascii_punct((unsigned char)b[n - 1])) n--;
  if (!n) return;
  if (in_list(JP_PUNCT, sizeof JP_PUNCT / sizeof *JP_PUNCT, b, n)) return;
  if (in_list(STOPWORDS, sizeof STOPWORDS / sizeof *STOPWORDS, b, n)) return;

  uint64_t h = tok_hash(b, n);
  for (int i = 0; i < 64; i++)
    a->v[i] += ((h >> i) & 1ULL) ? w : -w;
  a->total++;

  size_t j = (size_t)(h & (uint64_t)(SEEN_SLOTS - 1));
  while (a->seen[j]) {
    if (a->seen[j] == h) return;                 /* repeat: voted, not new */
    j = (j + 1) & (SEEN_SLOTS - 1);
  }
  a->seen[j] = h;
  a->distinct++;
}

static void acc_text(sh_acc *a, const char *s, int w) {
  if (!s) return;
  const char *p = s;
  while (*p) {
    while (*p && is_sep((unsigned char)*p)) p++;
    if (!*p) break;
    const char *b = p;
    while (*p && !is_sep((unsigned char)*p)) p++;
    acc_push(a, b, (size_t)(p - b), w);
    if (a->total >= SIMHASH_MAX_TOKENS) return;
  }
}

uint64_t simhash_text(const char *seg_title, const char *seg_body,
                      int *out_tokens) {
  /* Heap, not stack: this runs on collector threads whose stack size is not
   * ours to assume, and the accumulator is ~33 KB. */
  sh_acc *a = calloc(1, sizeof *a);
  if (!a) { if (out_tokens) *out_tokens = 0; return 0; }
  acc_text(a, seg_title, SIMHASH_TITLE_WEIGHT);
  acc_text(a, seg_body, 1);

  uint64_t out = 0;
  for (int i = 0; i < 64; i++)
    if (a->v[i] > 0) out |= (uint64_t)1 << i;    /* ties (v==0) → 0 bit */
  int d = a->distinct;
  free(a);
  if (out_tokens) *out_tokens = d;
  return (d >= SIMHASH_MIN_TOKENS) ? out : 0;
}

int simhash_distance(uint64_t a, uint64_t b) {
  return __builtin_popcountll(a ^ b);
}

/* ── schema ───────────────────────────────────────────────────────────────
 * Order matters: the two intel_items indexes NAME the new columns, so they can
 * only be created after ensure_column() has added them. That is the whole
 * reason this lives here instead of in schema.sql (see the DDL note in the
 * header). */
void simhash_ensure_schema(db_handle *db) {
  if (!db || !db->h) return;
  ensure_column(db, "intel_items", "simhash",    "INTEGER");
  ensure_column(db, "intel_items", "cluster_id", "TEXT");

  static const char *DDL =
    "CREATE INDEX IF NOT EXISTS idx_intel_items_cluster"
    " ON intel_items(cluster_id);"
    "CREATE INDEX IF NOT EXISTS idx_intel_items_simhash_todo"
    " ON intel_items(COALESCE(published_at,fetched_at), uid)"
    " WHERE simhash IS NULL;"
    "CREATE TABLE IF NOT EXISTS simhash_bands ("
    " band_idx INTEGER NOT NULL, band_val INTEGER NOT NULL, uid TEXT NOT NULL,"
    " PRIMARY KEY (band_idx, band_val, uid)) WITHOUT ROWID;"
    "CREATE INDEX IF NOT EXISTS idx_simhash_bands_lookup"
    " ON simhash_bands(band_idx, band_val);"
    "CREATE INDEX IF NOT EXISTS idx_simhash_bands_uid ON simhash_bands(uid);";
  char *e = NULL;
  if (sqlite3_exec(db->h, DDL, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "[simhash] schema: %s\n", e ? e : "?");
    sqlite3_free(e);
  }
}

/* One DDL pass per process. The unlocked read of g_ready is a benign race:
 * losing it costs one mutex acquisition, and the DDL itself is idempotent. */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_ready = 0;
static void ensure_once(db_handle *db) {
  if (g_ready) return;
  pthread_mutex_lock(&g_lock);
  if (!g_ready) { simhash_ensure_schema(db); g_ready = 1; }
  pthread_mutex_unlock(&g_lock);
}

/* ── band index ───────────────────────────────────────────────────────────*/

static int band_val(uint64_t h, int i) {
  return (int)((h >> (SIMHASH_BAND_BITS * i)) & 0xFFFFu);
}

/* Delete-then-insert rather than upsert-the-four: on re-ingest the fingerprint
 * may have moved to entirely different band values, and the STALE rows are the
 * dangerous ones — they would keep offering this uid as a candidate for text
 * it no longer contains. Keyed by uid, which is what idx_simhash_bands_uid is
 * for. */
static void bands_write(sqlite3 *h, const char *uid, uint64_t fp) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "DELETE FROM simhash_bands WHERE uid=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
  if (!fp) return;                        /* unclusterable: index nothing */
  if (sqlite3_prepare_v2(h,
        "INSERT OR REPLACE INTO simhash_bands(band_idx,band_val,uid)"
        " VALUES(?1,?2,?3)", -1, &s, NULL) != SQLITE_OK) return;
  for (int i = 0; i < SIMHASH_BANDS; i++) {
    sqlite3_reset(s);
    sqlite3_bind_int(s, 1, i);
    sqlite3_bind_int(s, 2, band_val(fp, i));
    sqlite3_bind_text(s, 3, uid, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
  }
  sqlite3_finalize(s);
}

/* ── clustering ───────────────────────────────────────────────────────────*/

/* Candidates = every row sharing ANY band with `fp`, joined to intel_items for
 * the authoritative tenant and fingerprint. The join is not overhead: the
 * fingerprint has to be read anyway to confirm the true Hamming distance, and
 * putting the tenant predicate here rather than denormalising a tenant column
 * into simhash_bands means there is exactly one place it can be wrong.
 * The UNION arms are four independent seeks on the band primary key. */
static const char *CAND_SQL =
  "SELECT i.uid,i.simhash,i.cluster_id FROM intel_items i"
  " WHERE i.uid IN ("
  "  SELECT uid FROM simhash_bands WHERE band_idx=0 AND band_val=?1"
  "  UNION SELECT uid FROM simhash_bands WHERE band_idx=1 AND band_val=?2"
  "  UNION SELECT uid FROM simhash_bands WHERE band_idx=2 AND band_val=?3"
  "  UNION SELECT uid FROM simhash_bands WHERE band_idx=3 AND band_val=?4)"
  " AND i.tenant_id=?5 AND i.uid<>?6"
  " AND i.simhash IS NOT NULL AND i.simhash<>0"
  " LIMIT ?7";

static void set_cluster(sqlite3 *h, const char *uid, const char *cid) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "UPDATE intel_items SET cluster_id=?1 WHERE uid=?2",
                         -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, cid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, uid, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* COALESCE(published_at,fetched_at) of a row, or a 0xFF sentinel when the row
 * is missing — strcmp compares as unsigned char, so that sorts after every ISO
 * timestamp and an orphaned cluster_id can never win the representative slot
 * and drag a live cluster onto a uid with no row. */
static void row_pub(sqlite3 *h, const char *uid, const char *tenant,
                    char *out, size_t n) {
  snprintf(out, n, "\xff");
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "SELECT COALESCE(published_at,fetched_at) FROM intel_items"
        " WHERE uid=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW &&
      sqlite3_column_type(s, 0) != SQLITE_NULL) {
    const char *p = (const char *)sqlite3_column_text(s, 0);
    if (p && *p) snprintf(out, n, "%s", p);
  }
  sqlite3_finalize(s);
}

static long cluster_members(sqlite3 *h, const char *cid, const char *tenant) {
  long n = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "SELECT COUNT(*) FROM intel_items WHERE cluster_id=?1 AND tenant_id=?2",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, cid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

/* Relabel a whole cluster in ONE indexed statement. Followers are addressed BY
 * cluster_id, so they migrate without being enumerated — this is what keeps a
 * representative move O(members) instead of O(members) round trips. */
static void relabel(sqlite3 *h, const char *from, const char *to,
                    const char *tenant) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "UPDATE intel_items SET cluster_id=?1"
        " WHERE cluster_id=?2 AND tenant_id=?3", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s, 1, to,     -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, from,   -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, tenant, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* Decide `uid`'s cluster. Called inside the caller's write transaction, after
 * the bands for `fp` are already in place. */
static void join_cluster(sqlite3 *h, const char *uid, const char *tenant,
                         uint64_t fp) {
  /* 1. candidate superset from the 4 bands, confirmed by true distance. */
  char reps[SIMHASH_MAX_MERGE_CLUSTERS][UID_MAX];
  int nreps = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, CAND_SQL, -1, &s, NULL) != SQLITE_OK) {
    set_cluster(h, uid, uid);
    return;
  }
  for (int i = 0; i < SIMHASH_BANDS; i++)
    sqlite3_bind_int(s, i + 1, band_val(fp, i));
  sqlite3_bind_text(s, 5, tenant, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 6, uid,    -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 7, SIMHASH_MAX_CANDIDATES);
  while (sqlite3_step(s) == SQLITE_ROW) {
    uint64_t ch = (uint64_t)sqlite3_column_int64(s, 1);
    if (simhash_distance(fp, ch) > SIMHASH_MAX_DISTANCE) continue;  /* FP */
    const char *cid = (sqlite3_column_type(s, 2) == SQLITE_NULL)
                        ? (const char *)sqlite3_column_text(s, 0)
                        : (const char *)sqlite3_column_text(s, 2);
    if (!cid || !*cid || strcmp(cid, uid) == 0) continue;
    int dup = 0;
    for (int i = 0; i < nreps; i++)
      if (strcmp(reps[i], cid) == 0) { dup = 1; break; }
    if (dup) continue;
    if (nreps >= SIMHASH_MAX_MERGE_CLUSTERS) break;   /* drift/cost cap */
    ccopy(reps[nreps++], UID_MAX, cid);
  }
  sqlite3_finalize(s);

  if (nreps == 0) { set_cluster(h, uid, uid); return; }   /* own cluster */

  /* 2. representative = earliest-published member of the union. Each existing
   * cluster's representative already IS its earliest member (the invariant),
   * so comparing the representatives plus this item is sufficient — there is
   * no need to look at followers at all. */
  char best[UID_MAX], bestp[TS_MAX], p[TS_MAX];
  ccopy(best, sizeof best, uid);
  row_pub(h, uid, tenant, bestp, sizeof bestp);
  for (int i = 0; i < nreps; i++) {
    row_pub(h, reps[i], tenant, p, sizeof p);
    int c = strcmp(p, bestp);
    if (c < 0 || (c == 0 && strcmp(reps[i], best) < 0)) {
      ccopy(best,  sizeof best,  reps[i]);
      ccopy(bestp, sizeof bestp, p);
    }
  }

  /* 3. write-amplification ceiling. Count first (indexed, and 0 rows in the
   * overwhelmingly common case where only this item has to move), and refuse
   * the merge outright rather than let one ingest rewrite the corpus. The
   * clusters simply stay separate; a later backfill in publication order can
   * always rebuild them. */
  long moving = 0;
  for (int i = 0; i < nreps; i++)
    if (strcmp(reps[i], best) != 0) moving += cluster_members(h, reps[i], tenant);
  if (strcmp(uid, best) != 0) moving += cluster_members(h, uid, tenant);
  if (moving > SIMHASH_MAX_RELABEL) {
    fprintf(stderr, "[simhash] merge for %s skipped: %ld rows would move\n",
            uid, moving);
    set_cluster(h, uid, uid);
    return;
  }

  /* 4. migrate. Every absorbed cluster (including this item's own, when it
   * had followers) is repointed at `best` in one statement each; then this
   * item explicitly, because its cluster_id may still be NULL and would not
   * be caught by a WHERE cluster_id=? predicate. All inside the caller's
   * transaction, so no reader ever sees a half-migrated cluster. */
  for (int i = 0; i < nreps; i++)
    if (strcmp(reps[i], best) != 0) relabel(h, reps[i], best, tenant);
  if (strcmp(uid, best) != 0) relabel(h, uid, best, tenant);
  set_cluster(h, uid, best);
}

/* ── the store step (shared by the ingest hook and the backfill) ───────────*/

static void set_fingerprint(sqlite3 *h, const char *uid, uint64_t fp,
                            int clear_cluster) {
  sqlite3_stmt *s;
  const char *sql = clear_cluster
    ? "UPDATE intel_items SET simhash=?1, cluster_id=NULL WHERE uid=?2"
    : "UPDATE intel_items SET simhash=?1 WHERE uid=?2";
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_int64(s, 1, (sqlite3_int64)fp);
  sqlite3_bind_text (s, 2, uid, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

static void sh_store(db_handle *db, const char *tenant_hint, const char *uid,
                     uint64_t fp, int ntok) {
  sqlite3 *h = db->h;
  int clusterable = (fp != 0 && ntok >= SIMHASH_MIN_TOKENS);

  /* Read current state. The row's own tenant_id is authoritative — a caller
   * passing the wrong hint can therefore never cluster across tenants. */
  char tenant[TEN_MAX] = {0}, cluster[UID_MAX] = {0};
  int have_row = 0, have_sh = 0, have_cluster = 0;
  uint64_t cur = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "SELECT tenant_id,simhash,cluster_id FROM intel_items WHERE uid=?1",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *t = (const char *)sqlite3_column_text(s, 0);
    snprintf(tenant, sizeof tenant, "%s",
             (t && *t) ? t : (tenant_hint && *tenant_hint ? tenant_hint : "legacy"));
    if (sqlite3_column_type(s, 1) != SQLITE_NULL) {
      have_sh = 1;
      cur = (uint64_t)sqlite3_column_int64(s, 1);
    }
    if (sqlite3_column_type(s, 2) != SQLITE_NULL) {
      const char *c = (const char *)sqlite3_column_text(s, 2);
      if (c && *c) { have_cluster = 1; snprintf(cluster, sizeof cluster, "%s", c); }
    }
    have_row = 1;
  }
  sqlite3_finalize(s);
  if (!have_row) return;             /* row vanished between commit and here */

  /* If a transaction is already open on this connection (the backfill's batch,
   * or a caller that wrapped us) join it rather than nesting a BEGIN, which
   * SQLite would reject. */
  int own_txn = sqlite3_get_autocommit(h) != 0;

  if (!clusterable) {
    /* Stamped with the sentinel rather than left NULL: NULL is the backfill's
     * "not done yet" predicate, and a permanently-NULL row would be re-read on
     * every sweep forever. */
    if (have_sh && cur == 0 && !have_cluster) return;
    if (own_txn) sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    bands_write(h, uid, 0);
    set_fingerprint(h, uid, 0, 1);
    if (own_txn) sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
    return;
  }

  /* Unchanged re-ingest, already clustered: one indexed SELECT and out. This
   * is the common case — collectors re-emit their whole window every tick. */
  if (have_sh && cur == fp && have_cluster) return;

  if (own_txn) sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL);
  bands_write(h, uid, fp);
  set_fingerprint(h, uid, fp, 0);
  /* A FOLLOWER (cluster_id set and not its own uid) keeps its membership: see
   * the RE-INGEST note in the header. Its bands are still refreshed above, so
   * it remains findable by future items. */
  if (!(have_cluster && strcmp(cluster, uid) != 0))
    join_cluster(h, uid, tenant, fp);
  if (own_txn) sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
}

/* ── public ingest hooks ──────────────────────────────────────────────────*/

void simhash_on_item_segmented(db_handle *db, const char *tenant_id,
                               const char *uid, const char *seg_title,
                               const char *seg_body) {
  if (!db || !db->h || !uid || !*uid) return;
  ensure_once(db);
  int ntok = 0;
  uint64_t fp = simhash_text(seg_title, seg_body, &ntok);
  sh_store(db, tenant_id, uid, fp, ntok);
}

void simhash_on_item(db_handle *db, const char *tenant_id, const char *uid,
                     const char *title, const char *body) {
  if (!db || !db->h || !uid || !*uid) return;
  char *st = fts_segment(title ? title : "");
  char *sb = fts_segment(body ? body : "");
  simhash_on_item_segmented(db, tenant_id, uid, st, sb);
  free(st);
  free(sb);
}

/* ── backfill ─────────────────────────────────────────────────────────────*/

typedef struct {
  char     uid[UID_MAX];
  char     tenant[TEN_MAX];
  uint64_t fp;
  int      ntok;
} sh_row;

long simhash_backfill(db_handle *db, int limit, volatile int *cancel) {
  if (!db || !db->h) return -1;
  ensure_once(db);

  /* The ORDER BY is spelled exactly as idx_intel_items_simhash_todo indexes it
   * so SQLite matches the expression and the partial index satisfies both the
   * predicate and the ordering — otherwise every batch is a full table scan
   * plus a sort and the sweep is quadratic in the corpus.
   *
   * ASCENDING PUBLICATION ORDER IS LOAD-BEARING: it guarantees a row being
   * fingerprinted is never older than one already fingerprinted, so it can
   * never displace an existing representative. That is what makes the whole
   * migration free of write amplification. */
  static const char *PAGE =
    "SELECT uid,tenant_id FROM intel_items WHERE simhash IS NULL"
    " ORDER BY COALESCE(published_at,fetched_at) ASC, uid ASC LIMIT ?1";
  static const char *TEXT =
    "SELECT title,body FROM intel_items WHERE uid=?1";

  long done = 0;
  char last_first[UID_MAX] = {0};

  for (;;) {
    if (cancelled(cancel)) break;
    long want = SIMHASH_BATCH;
    if (limit > 0 && want > (long)limit - done) want = (long)limit - done;
    if (want <= 0) break;

    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, PAGE, -1, &s, NULL) != SQLITE_OK)
      return done ? done : -1;
    sqlite3_bind_int(s, 1, (int)want);
    sh_row *rows = calloc((size_t)want, sizeof *rows);
    if (!rows) { sqlite3_finalize(s); return done ? done : -1; }
    int n = 0;
    while (n < (int)want && sqlite3_step(s) == SQLITE_ROW) {
      const char *u = (const char *)sqlite3_column_text(s, 0);
      if (!u || !*u) continue;
      snprintf(rows[n].uid, sizeof rows[n].uid, "%s", u);
      const char *t = (const char *)sqlite3_column_text(s, 1);
      snprintf(rows[n].tenant, sizeof rows[n].tenant, "%s", (t && *t) ? t : "legacy");
      n++;
    }
    sqlite3_finalize(s);
    if (n == 0) { free(rows); break; }

    /* Liveness guard: if a batch starts on the same uid as the last one, the
     * previous batch failed to stamp it and we would spin here forever. */
    if (strcmp(rows[0].uid, last_first) == 0) {
      fprintf(stderr, "[simhash] backfill stalled at %s\n", rows[0].uid);
      free(rows);
      break;
    }
    snprintf(last_first, sizeof last_first, "%s", rows[0].uid);

    /* Phase 2 — segment and hash with NO write transaction open. MeCab is the
     * expensive part; holding SQLite's single write lock across it would stall
     * every httpd writer for the length of the batch. */
    int hashed = 0;
    sqlite3_stmt *tx;
    if (sqlite3_prepare_v2(db->h, TEXT, -1, &tx, NULL) != SQLITE_OK) {
      free(rows);
      return done ? done : -1;
    }
    for (int i = 0; i < n; i++) {
      if (cancelled(cancel)) break;
      sqlite3_reset(tx);
      sqlite3_bind_text(tx, 1, rows[i].uid, -1, SQLITE_TRANSIENT);
      char *st = NULL, *sb = NULL;
      if (sqlite3_step(tx) == SQLITE_ROW) {
        const char *ti = (const char *)sqlite3_column_text(tx, 0);
        const char *bo = (const char *)sqlite3_column_text(tx, 1);
        st = fts_segment(ti ? ti : "");
        sb = fts_segment(bo ? bo : "");
      }
      rows[i].fp = simhash_text(st, sb, &rows[i].ntok);
      free(st);
      free(sb);
      hashed++;
    }
    sqlite3_finalize(tx);

    /* Phase 3 — one short transaction holding only the writes. */
    if (hashed > 0) {
      sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL);
      for (int i = 0; i < hashed; i++)
        sh_store(db, rows[i].tenant, rows[i].uid, rows[i].fp, rows[i].ntok);
      sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL);
      done += hashed;
    }
    free(rows);
    if (hashed < n) break;                     /* cancelled mid-batch */
  }
  return done;
}

/* ── ?collapse=1 ──────────────────────────────────────────────────────────*/

char *simhash_collapse(db_handle *db, const char *tenant_id,
                       const char *list_json) {
  if (!db || !db->h || !list_json) return NULL;
  ensure_once(db);

  cJSON *env = cJSON_Parse(list_json);
  if (!env) return NULL;
  cJSON *data = cJSON_GetObjectItem(env, "data");   /* borrowed */
  if (!cJSON_IsArray(data)) { cJSON_Delete(env); return NULL; }

  const int tf = tenant_id && *tenant_id;
  char q_key[192], q_cnt[224], q_mem[288];
  snprintf(q_key, sizeof q_key,
    "SELECT COALESCE(cluster_id,uid) FROM intel_items WHERE uid=?1%s",
    tf ? " AND tenant_id=?2" : "");
  snprintf(q_cnt, sizeof q_cnt,
    "SELECT COUNT(*),COUNT(DISTINCT source_id) FROM intel_items"
    " WHERE cluster_id=?1%s", tf ? " AND tenant_id=?2" : "");
  snprintf(q_mem, sizeof q_mem,
    "SELECT source_id,uid FROM intel_items WHERE cluster_id=?1%s AND uid<>?%d"
    " ORDER BY COALESCE(published_at,fetched_at) ASC, uid ASC LIMIT ?%d",
    tf ? " AND tenant_id=?2" : "", tf ? 3 : 2, tf ? 4 : 3);

  sqlite3_stmt *kst = NULL, *cst = NULL, *mst = NULL;
  if (sqlite3_prepare_v2(db->h, q_key, -1, &kst, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(db->h, q_cnt, -1, &cst, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(db->h, q_mem, -1, &mst, NULL) != SQLITE_OK) {
    sqlite3_finalize(kst); sqlite3_finalize(cst); sqlite3_finalize(mst);
    cJSON_Delete(env);
    return NULL;                       /* caller keeps the uncollapsed page */
  }

  cJSON *out = cJSON_CreateArray();
  char **seen = NULL;
  int nseen = 0, cseen = 0, rows_in = 0;
  cJSON *it;

  /* Detach-from-front rather than index iteration: items are moved, never
   * copied, and the loop cannot be confused by the shifting indices. */
  while ((it = cJSON_DetachItemFromArray(data, 0)) != NULL) {
    rows_in++;
    cJSON *ju = cJSON_GetObjectItem(it, "uid");
    const char *uid = cJSON_IsString(ju) ? ju->valuestring : NULL;
    if (!uid || !*uid) { cJSON_AddItemToArray(out, it); continue; }

    /* Cluster key. A uid with no intel_items row — breach adapter synthetics,
     * or a row from another tenant — falls back to itself, i.e. a singleton,
     * so collapse is always safe to enable. */
    char key[UID_MAX];
    snprintf(key, sizeof key, "%s", uid);
    sqlite3_reset(kst);
    sqlite3_bind_text(kst, 1, uid, -1, SQLITE_TRANSIENT);
    if (tf) sqlite3_bind_text(kst, 2, tenant_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(kst) == SQLITE_ROW) {
      const char *k = (const char *)sqlite3_column_text(kst, 0);
      if (k && *k) snprintf(key, sizeof key, "%s", k);
    }

    int dup = 0;
    for (int i = 0; i < nseen; i++)
      if (strcmp(seen[i], key) == 0) { dup = 1; break; }
    if (dup) { cJSON_Delete(it); continue; }   /* folded into the kept row */

    if (nseen == cseen) {
      int nc = cseen ? cseen * 2 : 32;
      char **ns = realloc(seen, (size_t)nc * sizeof *ns);
      if (!ns) { cJSON_AddItemToArray(out, it); continue; }
      seen = ns; cseen = nc;
    }
    size_t klen = strlen(key) + 1;
    seen[nseen] = malloc(klen);
    if (!seen[nseen]) { cJSON_AddItemToArray(out, it); continue; }
    memcpy(seen[nseen], key, klen);
    nseen++;

    /* Corpus-wide membership, NOT page-local: the whole point is that the
     * nineteen other reports count even when they are on page seven. */
    long total = 0, srcs = 0;
    sqlite3_reset(cst);
    sqlite3_bind_text(cst, 1, key, -1, SQLITE_TRANSIENT);
    if (tf) sqlite3_bind_text(cst, 2, tenant_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(cst) == SQLITE_ROW) {
      total = (long)sqlite3_column_int64(cst, 0);
      srcs  = (long)sqlite3_column_int64(cst, 1);
    }
    if (total < 1) { total = 1; srcs = 1; }    /* not-yet-fingerprinted row */

    cJSON *dups = cJSON_CreateArray();
    int listed = 0;
    sqlite3_reset(mst);
    sqlite3_bind_text(mst, 1, key, -1, SQLITE_TRANSIENT);
    if (tf) sqlite3_bind_text(mst, 2, tenant_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(mst, tf ? 3 : 2, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (mst, tf ? 4 : 3, SIMHASH_MAX_DUPLICATES_LISTED);
    while (sqlite3_step(mst) == SQLITE_ROW) {
      const char *ds = (const char *)sqlite3_column_text(mst, 0);
      const char *du = (const char *)sqlite3_column_text(mst, 1);
      cJSON *d = cJSON_CreateObject();
      cJSON_AddItemToObject(d, "source_id",
        ds ? cJSON_CreateString(ds) : cJSON_CreateNull());
      cJSON_AddItemToObject(d, "uid",
        du ? cJSON_CreateString(du) : cJSON_CreateNull());
      cJSON_AddItemToArray(dups, d);
      listed++;
    }

    cJSON_AddStringToObject(it, "cluster_id", key);
    cJSON_AddNumberToObject(it, "cluster_size", (double)total);
    cJSON_AddNumberToObject(it, "cluster_source_count", (double)srcs);
    cJSON_AddItemToObject(it, "duplicates", dups);
    cJSON_AddBoolToObject(it, "duplicates_truncated",
                          (total - 1) > (long)listed);
    cJSON_AddItemToArray(out, it);
  }

  sqlite3_finalize(kst);
  sqlite3_finalize(cst);
  sqlite3_finalize(mst);
  for (int i = 0; i < nseen; i++) free(seen[i]);
  free(seen);

  int rows_out = cJSON_GetArraySize(out);
  /* Replace in place so {data,page,meta} keeps its order; `page` is untouched
   * on purpose — the cursor belongs to the underlying scan, so paging stays
   * lossless even though `data` is now shorter than `limit`. */
  if (!cJSON_ReplaceItemInObject(env, "data", out)) {
    cJSON_Delete(out);
    cJSON_Delete(env);
    return NULL;
  }

  cJSON *meta = cJSON_GetObjectItem(env, "meta");
  if (!cJSON_IsObject(meta)) {
    cJSON_DeleteItemFromObject(env, "meta");
    meta = cJSON_CreateObject();
    cJSON_AddItemToObject(env, "meta", meta);
  }
  cJSON *cm = cJSON_CreateObject();
  cJSON_AddBoolToObject(cm, "enabled", 1);
  cJSON_AddNumberToObject(cm, "rows_in", rows_in);
  cJSON_AddNumberToObject(cm, "rows_out", rows_out);
  cJSON_AddNumberToObject(cm, "max_duplicates_listed",
                          SIMHASH_MAX_DUPLICATES_LISTED);
  cJSON_AddItemToObject(meta, "collapse", cm);

  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return js;
}
