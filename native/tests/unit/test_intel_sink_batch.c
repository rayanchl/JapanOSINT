/* test_intel_sink_batch.c — the sink's BATCHED writes (core/intel.c, "BATCHED
 * WRITES"): one transaction per JO_SINK_BATCH records instead of one per
 * record, without changing what is stored.
 *
 * Holds:
 *   1. IDENTICAL CONTENT: the same emit sequence (new, Japanese, geo, notices,
 *      a uid repeated inside a batch with different text, near-duplicate texts
 *      for the simhash clusters, then an unchanged and a changed re-fetch)
 *      through JO_SINK_BATCH=1 and JO_SINK_BATCH=200 into two scratch
 *      databases leaves the same intel_items rows (every column but the two
 *      timestamps), the same FTS rows, the same simhash bands and clusters, and
 *      the same stored/notices counts;
 *   2. a record the DATABASE refuses in the middle of a batch costs only
 *      itself: its neighbours are written, `stored` and the failed counts say
 *      so, and the refusal comes back through a later emit() as -1;
 *   3. alert events fire exactly once per NEW row, batched or not, and not at
 *      all on an unchanged re-fetch;
 *   4. flush points: the record limit, the age limit (checked at the next
 *      emit), reading the stored counters, intel_sink_flush() and
 *      intel_sink_free();
 *   5. a buffered record holds NO lock: another connection can take the write
 *      lock while a sink has records parked, and sees them only after the
 *      flush;
 *   6. a sink from intel_sink_rebind() (the dispatcher's) is per-record and
 *      answers exactly: 1 new, 0 updated, -1 refused.
 *
 * Includes intel.c for its statics; everything else is linked. */
#include "../../core/intel.c"
#include "../../core/db.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void sql(db_handle *d, const char *q) {
  char *e = NULL;
  if (sqlite3_exec(d->h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", q);
    exit(1);
  }
}

static long long int1(db_handle *d, const char *q) {
  sqlite3_stmt *s; long long r = -1;
  assert(sqlite3_prepare_v2(d->h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) r = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return r;
}

/* The whole result of a query as one string, rows by '\n', columns by '|'. */
static char *dump(db_handle *d, const char *q) {
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(d->h, q, -1, &s, NULL) == SQLITE_OK);
  size_t cap = 1 << 16, n = 0;
  char *out = malloc(cap);
  int nc = sqlite3_column_count(s);
  while (sqlite3_step(s) == SQLITE_ROW) {
    for (int c = 0; c < nc; c++) {
      const char *v = (const char *)sqlite3_column_text(s, c);
      if (!v) v = "<NULL>";
      size_t l = strlen(v);
      while (n + l + 4 > cap) out = realloc(out, cap *= 2);
      memcpy(out + n, v, l); n += l;
      out[n++] = c + 1 < nc ? '|' : '\n';
    }
  }
  out[n] = 0;
  sqlite3_finalize(s);
  return out;
}

static db_handle open_db(const char *path) {
  db_handle d = {0};
  unlink(path);
  char w[1100];
  snprintf(w, sizeof w, "%s-wal", path); unlink(w);
  snprintf(w, sizeof w, "%s-shm", path); unlink(w);
  assert(db_open(&d, path, NULL) == 0);
  return d;
}

static int put(intel_sink *k, const char *key, const char *title,
               const char *body, const char *rtype, int geo) {
  intel_item it = {0};
  it.remote_key = key; it.title = title; it.body = body;
  it.summary = body; it.link = "https://example.invalid/x"; it.author = "t";
  it.record_type = rtype ? rtype : "observation";
  it.properties_json = "{\"n\":1,\"k\":\"v\"}";
  it.tags_json = "[\"a\",\"b\"]";
  it.published_at = "2026-10-01T00:00:00Z";
  if (geo) { it.has_geo = 1; it.lat = 35.5; it.lon = 139.5; }
  return k->emit(k, &it);
}

static const char *LONG_A =
  "The regional observation network reported steady hourly readings across "
  "every station in the northern prefectures during the morning, with no "
  "anomalies flagged by the automatic quality control pass this cycle.";

/* The fixed sequence run through both databases. Returns the stored counts. */
static void sequence(db_handle *d, const char *src, long *stored, long *notices,
                     long *failed) {
  intel_sink k = intel_sink_make(d, src, NULL);
  char key[64], title[200], body[400];
  for (int i = 0; i < 700; i++) {
    snprintf(key, sizeof key, "rec-%d", i);
    if (i % 5 == 0) {
      snprintf(title, sizeof title, "東京都の観測点 %d 号", i);
      snprintf(body, sizeof body, "気象庁が発表した観測データ。地点 %d。"
               "東京都千代田区大手町一丁目。最新の値は毎時更新されます。", i);
    } else if (i % 7 == 0) {
      /* near-duplicates: identical long text under distinct keys, so simhash
       * puts them in one cluster */
      snprintf(title, sizeof title, "Network status report");
      snprintf(body, sizeof body, "%s", LONG_A);
    } else {
      snprintf(title, sizeof title, "Observation station %d", i);
      snprintf(body, sizeof body, "Hourly observation record %d from the "
               "national network; values update on the hour.", i);
    }
    put(&k, key, title, body, i % 50 == 3 ? "collector-shape-notice" : NULL, i % 3 == 0);
  }
  /* a uid repeated INSIDE a batch with different text, twice */
  put(&k, "dup-1", "first title", "first body text", NULL, 0);
  put(&k, "dup-1", "second title", "second body text", NULL, 0);
  put(&k, "dup-1", "second title", "second body text", NULL, 0);
  intel_sink_free(&k);

  /* unchanged re-fetch of the first 300, changed re-fetch of the next 100 */
  k = intel_sink_make(d, src, NULL);
  for (int i = 0; i < 400; i++) {
    snprintf(key, sizeof key, "rec-%d", i);
    snprintf(title, sizeof title, "Observation station %d", i);
    snprintf(body, sizeof body, "Hourly observation record %d from the "
             "national network; values update on the hour.", i);
    if (i % 5 == 0) {
      snprintf(title, sizeof title, "東京都の観測点 %d 号", i);
      snprintf(body, sizeof body, "気象庁が発表した観測データ。地点 %d。"
               "東京都千代田区大手町一丁目。最新の値は毎時更新されます。", i);
    } else if (i % 7 == 0) {
      snprintf(title, sizeof title, "Network status report");
      snprintf(body, sizeof body, "%s", LONG_A);
    }
    if (i >= 300) strncat(body, " REVISED", sizeof body - strlen(body) - 1);
    put(&k, key, title, body, i % 50 == 3 ? "collector-shape-notice" : NULL, i % 3 == 0);
  }
  int ex = 1;
  *stored = intel_sink_stored(&k, &ex);
  *notices = intel_sink_stored_notices(&k);
  *failed = intel_sink_failed(&k);
  intel_sink_free(&k);
}

static const char *Q_ITEMS =
  "SELECT substr(uid, instr(uid,'|')+1), title, body, summary, link, author,"
  "       language, published_at, tags, properties, lat, lon, geom_source,"
  "       record_type, sub_source_id, geometry, hex(content_hash), simhash,"
  "       tenant_id, (last_seen_at IS NOT NULL), (cluster_id IS NOT NULL),"
  "       (cluster_id = uid) FROM intel_items ORDER BY uid";
static const char *Q_FTS =
  "SELECT substr(uid, instr(uid,'|')+1), title, body, summary, keywords, link,"
  "       author, tags, props FROM intel_items_fts ORDER BY uid";
static const char *Q_MAP =
  "SELECT count(*), count(DISTINCT rowid) FROM intel_items_fts_uid_map";
static const char *Q_BANDS =
  "SELECT band_idx, band_val, substr(uid, instr(uid,'|')+1) FROM simhash_bands"
  " ORDER BY 1,2,3";
/* Cluster membership, as the SET of keys sharing a representative. */
static const char *Q_CLUSTERS =
  "SELECT group_concat(k, ',') FROM (SELECT substr(uid, instr(uid,'|')+1) AS k,"
  " cluster_id AS c FROM intel_items WHERE cluster_id IS NOT NULL ORDER BY uid)"
  " GROUP BY substr(c, instr(c,'|')+1) ORDER BY 1";

static void same(db_handle *a, db_handle *b, const char *q, const char *what) {
  char *x = dump(a, q), *y = dump(b, q);
  if (strcmp(x, y) != 0) {
    fprintf(stderr, "MISMATCH in %s\n--- unbatched (%zu bytes)\n%.600s\n"
                    "--- batched (%zu bytes)\n%.600s\n", what, strlen(x), x,
            strlen(y), y);
    exit(1);
  }
  free(x); free(y);
}

static void test_identical(void) {
  db_handle a = open_db("/tmp/jo_sink_batch_a.db");
  db_handle b = open_db("/tmp/jo_sink_batch_b.db");
  long sa, na, fa, sb, nb, fb;
  setenv("JO_SINK_BATCH", "1", 1);
  sequence(&a, "eq-src", &sa, &na, &fa);
  setenv("JO_SINK_BATCH", "200", 1);
  sequence(&b, "eq-src", &sb, &nb, &fb);
  unsetenv("JO_SINK_BATCH");

  assert(sa == sb && na == nb && fa == fb && fa == 0);
  assert(int1(&a, "SELECT count(*) FROM intel_items") ==
         int1(&b, "SELECT count(*) FROM intel_items"));
  assert(int1(&b, "SELECT count(*) FROM intel_items") == 701 + 0 &&
         "700 records + dup-1");
  same(&a, &b, Q_ITEMS, "intel_items");
  same(&a, &b, Q_FTS, "intel_items_fts");
  same(&a, &b, Q_MAP, "fts uid_map");
  same(&a, &b, Q_BANDS, "simhash_bands");
  same(&a, &b, Q_CLUSTERS, "simhash clusters");
  /* sanity: the comparison is not vacuous */
  assert(int1(&b, "SELECT count(*) FROM simhash_bands") > 100);
  assert(int1(&b, "SELECT count(*) FROM intel_items WHERE simhash IS NOT NULL") > 600);
  assert(int1(&b, "SELECT count(*) FROM intel_items WHERE cluster_id <> uid") > 10);
  assert(int1(&b, "SELECT count(*) FROM intel_items_fts") == 701);
  /* the repeated uid ended on its last text, indexed once */
  assert(int1(&b, "SELECT count(*) FROM intel_items WHERE title='second title'") == 1);
  assert(int1(&b, "SELECT count(*) FROM intel_items_fts WHERE title='second title'") == 1);
  printf("  identical rows, FTS, simhash bands and clusters, counts "
         "(batch=1 vs 200; stored=%ld notices=%ld): ok\n", sb, nb);
  sqlite3_close(a.h); sqlite3_close(b.h);
}

static void test_failing_record(void) {
  db_handle d = open_db("/tmp/jo_sink_batch_f.db");
  sql(&d, "CREATE TRIGGER t_bad_i BEFORE INSERT ON intel_items WHEN NEW.title='BAD'"
          " BEGIN SELECT RAISE(ABORT,'refused'); END");
  sql(&d, "CREATE TRIGGER t_bad_u BEFORE UPDATE ON intel_items WHEN NEW.title='BAD'"
          " BEGIN SELECT RAISE(ABORT,'refused'); END");
  char key[32];

  /* (a) the refusal is learned at the final flush: nothing later to carry it */
  setenv("JO_SINK_BATCH", "200", 1);
  intel_sink k = intel_sink_make(&d, "fail-a", NULL);
  int neg = 0;
  for (int i = 0; i < 10; i++) {
    snprintf(key, sizeof key, "r%d", i);
    if (put(&k, key, i == 4 ? "BAD" : "good title", "body text", NULL, 0) < 0) neg++;
  }
  assert(neg == 0 && "nothing is refused until the batch is written");
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='fail-a'") == 0 &&
         "buffered, not yet written");
  int ex = 1;
  assert(intel_sink_stored(&k, &ex) == 9 && "the neighbours were written");
  assert(intel_sink_failed(&k) == 1 && intel_sink_failed_pending(&k) == 1);
  intel_sink_free(&k);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='fail-a'") == 9);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE uid='fail-a|r3'") == 1);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE uid='fail-a|r5'") == 1);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE uid='fail-a|r4'") == 0);
  assert(int1(&d, "SELECT count(*) FROM intel_items_fts WHERE uid LIKE 'fail-a|%'") == 9);
  assert(int1(&d, "SELECT count(*) FROM intel_items_fts_uid_map WHERE uid LIKE 'fail-a|%'") == 9);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='fail-a' AND "
                  "simhash IS NULL") <= 9);

  /* (b) a small batch: the refusal is returned by a LATER emit, exactly once */
  setenv("JO_SINK_BATCH", "3", 1);
  k = intel_sink_make(&d, "fail-b", NULL);
  neg = 0;
  for (int i = 0; i < 10; i++) {
    snprintf(key, sizeof key, "r%d", i);
    if (put(&k, key, i == 4 ? "BAD" : "good title", "body text", NULL, 0) < 0) neg++;
  }
  assert(neg == 1 && "one refused record -> exactly one -1");
  assert(intel_sink_stored(&k, &ex) == 9);
  assert(intel_sink_failed(&k) == 1 && intel_sink_failed_pending(&k) == 0);
  intel_sink_free(&k);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='fail-b'") == 9);

  /* (c) a refused NOTICE is owed to a notice emit, not to a record emit */
  setenv("JO_SINK_BATCH", "2", 1);
  k = intel_sink_make(&d, "fail-c", NULL);
  intel_item it = {0};
  it.remote_key = "n1"; it.title = "BAD"; it.record_type = "collector-shape-notice";
  assert(k.emit(&k, &it) == 0);
  assert(put(&k, "a", "ok", "x", NULL, 0) == 0);           /* batch flushed here */
  assert(put(&k, "b", "ok", "x", NULL, 0) == 0 && "a record is not charged");
  it.remote_key = "n2"; it.title = "ok2";
  assert(k.emit(&k, &it) == -1 && "the notice emit carries the refusal");
  intel_sink_free(&k);
  unsetenv("JO_SINK_BATCH");
  printf("  a refused record costs only itself; failed counts and the -1 "
         "are truthful: ok\n");
  sqlite3_close(d.h);
}

static long long events(db_handle *d, const char *rule) {
  char q[256];
  snprintf(q, sizeof q, "SELECT count(*) FROM alert_events WHERE rule_id='%s'", rule);
  return int1(d, q);
}

static void test_alerts(void) {
  db_handle d = open_db("/tmp/jo_sink_batch_alert.db");
  sql(&d, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A')");
  sql(&d, "INSERT INTO alert_rules(id,tenant_id,name,predicate_json,dedup_window_sec,"
          "storm_cap_per_hour) VALUES"
          " ('rU','tA','one','{\"source_ids\":[\"alert-unbatched\"]}',0,100000),"
          " ('rB','tA','two','{\"source_ids\":[\"alert-batched\"]}',0,100000)");
  char key[32];
  for (int pass = 0; pass < 2; pass++) {
    setenv("JO_SINK_BATCH", pass ? "200" : "1", 1);
    intel_sink k = intel_sink_make(&d, pass ? "alert-batched" : "alert-unbatched", NULL);
    for (int i = 0; i < 120; i++) {
      snprintf(key, sizeof key, "r%d", i);
      put(&k, key, "alert title", "alert body", NULL, 0);
    }
    intel_sink_free(&k);
  }
  assert(events(&d, "rU") == 120 && "unbatched: one event per new row");
  assert(events(&d, "rB") == 120 && "batched: one event per new row");
  /* an unchanged re-fetch and a changed one: neither is NEW, neither fires */
  for (int pass = 0; pass < 2; pass++) {
    setenv("JO_SINK_BATCH", pass ? "200" : "1", 1);
    intel_sink k = intel_sink_make(&d, pass ? "alert-batched" : "alert-unbatched", NULL);
    for (int i = 0; i < 120; i++) {
      snprintf(key, sizeof key, "r%d", i);
      put(&k, key, i < 60 ? "alert title" : "alert title CHANGED", "alert body", NULL, 0);
    }
    intel_sink_free(&k);
  }
  assert(events(&d, "rU") == 120 && events(&d, "rB") == 120);
  unsetenv("JO_SINK_BATCH");
  printf("  alert events: once per new row, batched == unbatched, none on "
         "re-fetch: ok\n");
  sqlite3_close(d.h);
}

static void test_flush_points(void) {
  db_handle d = open_db("/tmp/jo_sink_batch_p.db");
  char key[32]; int ex = 1;
  /* record limit */
  setenv("JO_SINK_BATCH", "5", 1);
  setenv("JO_SINK_BATCH_MS", "600000", 1);
  intel_sink k = intel_sink_make(&d, "pt", NULL);
  for (int i = 0; i < 4; i++) { snprintf(key, sizeof key, "r%d", i); put(&k, key, "t", "b", NULL, 0); }
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='pt'") == 0);
  put(&k, "r4", "t", "b", NULL, 0);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='pt'") == 5 &&
         "the 5th record writes the batch");
  /* explicit flush */
  put(&k, "r5", "t", "b", NULL, 0);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='pt'") == 5);
  intel_sink_flush(&k);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='pt'") == 6);
  /* reading the counters */
  put(&k, "r6", "t", "b", NULL, 0);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='pt'") == 6);
  assert(intel_sink_stored(&k, &ex) == 7 && ex == 1);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='pt'") == 7);
  put(&k, "r7", "t", "b", NULL, 0);
  assert(intel_sink_stored_notices(&k) == 0);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='pt'") == 8);
  /* free */
  put(&k, "r8", "t", "b", NULL, 0);
  intel_sink_free(&k);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='pt'") == 9 &&
         "intel_sink_free writes what is buffered");

  /* age limit: checked at the next emit */
  setenv("JO_SINK_BATCH", "1000", 1);
  setenv("JO_SINK_BATCH_MS", "60", 1);
  k = intel_sink_make(&d, "age", NULL);
  put(&k, "a1", "t", "b", NULL, 0);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='age'") == 0);
  usleep(120 * 1000);
  put(&k, "a2", "t", "b", NULL, 0);
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='age'") == 2 &&
         "a batch older than JO_SINK_BATCH_MS is written at the next emit");
  intel_sink_free(&k);

  /* no uid: refused immediately, as before */
  setenv("JO_SINK_BATCH", "200", 1);
  k = intel_sink_make(&d, "nouid", NULL);
  intel_item it = {0}; it.title = "x";
  assert(k.emit(&k, &it) == -1);
  intel_sink_free(&k);
  unsetenv("JO_SINK_BATCH"); unsetenv("JO_SINK_BATCH_MS");
  printf("  flush on record limit, age, flush(), counters and free; no-uid "
         "still -1: ok\n");
  sqlite3_close(d.h);
}

static void test_no_lock_held(void) {
  const char *path = "/tmp/jo_sink_batch_l.db";
  db_handle d = open_db(path);
  db_handle other = {0};
  assert(db_open(&other, path, NULL) == 0);
  setenv("JO_SINK_BATCH", "200", 1);
  setenv("JO_SINK_BATCH_MS", "600000", 1);
  intel_sink k = intel_sink_make(&d, "lk", NULL);
  for (int i = 0; i < 20; i++) { char key[16]; snprintf(key, sizeof key, "r%d", i); put(&k, key, "t", "b", NULL, 0); }
  /* the collector is now "waiting on HTTP": another connection must be able
   * to take the write lock at once */
  sqlite3_busy_timeout(other.h, 0);
  assert(sqlite3_exec(other.h, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK &&
         "a parked batch holds no write lock");
  assert(sqlite3_exec(other.h, "COMMIT", NULL, NULL, NULL) == SQLITE_OK);
  assert(int1(&other, "SELECT count(*) FROM intel_items WHERE source_id='lk'") == 0);
  intel_sink_flush(&k);
  assert(int1(&other, "SELECT count(*) FROM intel_items WHERE source_id='lk'") == 20 &&
         "after the flush the other connection sees the rows");
  assert(sqlite3_get_autocommit(d.h) && "no transaction left open by the sink");
  intel_sink_free(&k);
  unsetenv("JO_SINK_BATCH"); unsetenv("JO_SINK_BATCH_MS");
  printf("  a parked batch holds no write lock; visible to others after the "
         "flush: ok\n");
  sqlite3_close(other.h); sqlite3_close(d.h);
}

static void test_rebind_exact(void) {
  db_handle d = open_db("/tmp/jo_sink_batch_r.db");
  sql(&d, "CREATE TRIGGER t_bad_i BEFORE INSERT ON intel_items WHEN NEW.title='BAD'"
          " BEGIN SELECT RAISE(ABORT,'refused'); END");
  setenv("JO_SINK_BATCH", "200", 1);
  intel_sink base = intel_sink_make(&d, "osint-search", "legacy");
  intel_sink own;
  assert(intel_sink_rebind(&base, "svc", &own) == 1);
  assert(put(&own, "k1", "t", "b", NULL, 0) == 1 && "new");
  assert(int1(&d, "SELECT count(*) FROM intel_items WHERE source_id='svc'") == 1 &&
         "a pivot sink writes at once");
  assert(put(&own, "k1", "t", "b", NULL, 0) == 0 && "unchanged");
  assert(put(&own, "k2", "BAD", "b", NULL, 0) == -1 && "refused, exactly");
  int ex = 1;
  assert(intel_sink_stored(&own, &ex) == 1 && intel_sink_failed(&own) == 1 &&
         intel_sink_failed_pending(&own) == 0);
  intel_sink_free(&own);
  intel_sink_free(&base);
  unsetenv("JO_SINK_BATCH");
  printf("  rebound (dispatcher) sink: per record, exact 1/0/-1: ok\n");
  sqlite3_close(d.h);
}

int main(void) {
  test_alerts();           /* first: the rule cache loads from the first db used */
  test_identical();
  test_failing_record();
  test_flush_points();
  test_no_lock_held();
  test_rebind_exact();
  printf("\nall passed\n");
  return 0;
}
