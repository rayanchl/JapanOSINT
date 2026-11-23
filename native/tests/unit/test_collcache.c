/* tests/unit/test_collcache.c — core/collcache.c, the collector_cache
 * get/set shared by the sweep API (instant read) and the scheduler (write-time
 * prebuild). Pins: TTL clamping (<=0 -> 15 min, floor 1 min, ceiling 24 h),
 * expiry by age against the STORED ttl, upsert (not duplicate) on re-set, the
 * returned age, and "missing table -> NULL / silently skipped" so a cache
 * outage can never break a collector run. Uses an in-memory SQLite database
 * with the same table definition as core/schema.sql. */
#include "../../core/collcache.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

static long long ttl_of(db_handle *db, const char *key) {
  sqlite3_stmt *s; long long v = -1;
  sqlite3_prepare_v2(db->h, "SELECT ttl_ms FROM collector_cache WHERE key=?1", -1, &s, NULL);
  sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s); return v;
}
static int nrows(db_handle *db) {
  sqlite3_stmt *s; int n = -1;
  sqlite3_prepare_v2(db->h, "SELECT count(*) FROM collector_cache", -1, &s, NULL);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
  sqlite3_finalize(s); return n;
}
static void age_row(db_handle *db, const char *key, long long ms) {
  char sql[256];
  snprintf(sql, sizeof sql, "UPDATE collector_cache SET fetched_at = fetched_at - %lld WHERE key='%s'", ms, key);
  sqlite3_exec(db->h, sql, 0, 0, 0);
}

int main(void) {
  db_handle db; memset(&db, 0, sizeof db);
  sqlite3_open(":memory:", &db.h);
  long long age = -7; char *r;

  printf("-- no table\n");
  ok(collcache_get(&db, "k", &age) == NULL, "missing table: get -> NULL");
  collcache_set(&db, "k", "{}", 1000);
  ok(1, "missing table: set is silently skipped");

  sqlite3_exec(db.h, "CREATE TABLE collector_cache (key TEXT PRIMARY KEY, fc_json TEXT NOT NULL, "
                     "fetched_at INTEGER NOT NULL, ttl_ms INTEGER NOT NULL)", 0, 0, 0);

  printf("-- set / get\n");
  ok(collcache_get(&db, "absent", &age) == NULL, "absent key -> NULL");
  collcache_set(&db, "fc1", "{\"type\":\"FeatureCollection\",\"features\":[]}", 600000);
  r = collcache_get(&db, "fc1", &age);
  ok(r && !strcmp(r, "{\"type\":\"FeatureCollection\",\"features\":[]}") && age >= 0 && age < 2000, "fresh entry returns its body and a small age");
  free(r);
  r = collcache_get(&db, "fc1", NULL);
  ok(r != NULL, "age_out may be NULL");
  free(r);
  ok(collcache_get(&db, "FC1", &age) == NULL, "keys are case-sensitive");

  printf("-- upsert\n");
  collcache_set(&db, "fc1", "{\"v\":2}", 600000);
  ok(nrows(&db) == 1, "re-set of the same key updates in place (still one row)");
  r = collcache_get(&db, "fc1", &age);
  ok(r && !strcmp(r, "{\"v\":2}"), "re-set replaces the body");
  free(r);

  printf("-- ttl clamping\n");
  collcache_set(&db, "t0", "x", 0);
  ok(ttl_of(&db, "t0") == 15LL * 60 * 1000, "ttl 0 -> 15 min default");
  collcache_set(&db, "tneg", "x", -5);
  ok(ttl_of(&db, "tneg") == 15LL * 60 * 1000, "negative ttl -> 15 min default");
  collcache_set(&db, "tlow", "x", 1000);
  ok(ttl_of(&db, "tlow") == 60000, "ttl below 1 min is raised to 1 min");
  collcache_set(&db, "thigh", "x", 10LL * 24 * 3600 * 1000);
  ok(ttl_of(&db, "thigh") == 24LL * 3600 * 1000, "ttl above 24 h is cut to 24 h");
  collcache_set(&db, "tmid", "x", 123456);
  ok(ttl_of(&db, "tmid") == 123456, "ttl in range is kept");
  collcache_set(&db, "tedge", "x", 60000);
  ok(ttl_of(&db, "tedge") == 60000, "exactly 1 min is kept");

  printf("-- expiry\n");
  collcache_set(&db, "ex", "body", 60000);
  age_row(&db, "ex", 30000);
  r = collcache_get(&db, "ex", &age);
  ok(r && age >= 30000 && age < 33000, "age within ttl still hits and reports the true age");
  free(r);
  age_row(&db, "ex", 40000);                 /* now ~70 s old against a 60 s ttl */
  age = -9;
  ok(collcache_get(&db, "ex", &age) == NULL && age == -9, "older than the stored ttl: miss, age_out untouched");
  collcache_set(&db, "ex", "fresh", 60000);
  r = collcache_get(&db, "ex", &age);
  ok(r && !strcmp(r, "fresh"), "re-setting an expired key makes it fresh again");
  free(r);

  printf("-- odd inputs\n");
  collcache_set(&db, "", "emptykey", 60000);
  r = collcache_get(&db, "", &age);
  ok(r && !strcmp(r, "emptykey"), "empty-string key round-trips");
  free(r);
  { char *big = malloc(2 * 1024 * 1024 + 1); memset(big, 'j', 2 * 1024 * 1024); big[2 * 1024 * 1024] = 0;
    collcache_set(&db, "big", big, 60000);
    r = collcache_get(&db, "big", &age);
    ok(r && strlen(r) == 2u * 1024 * 1024, "a 2 MiB feature collection round-trips whole");
    free(r); free(big); }
  collcache_set(&db, "quote", "it's \"json\" \\ \n", 60000);
  r = collcache_get(&db, "quote", &age);
  ok(r && !strcmp(r, "it's \"json\" \\ \n"), "quotes/backslashes/newlines stored by bind, not interpolation");
  free(r);
  collcache_set(&db, "k'; DROP TABLE collector_cache;--", "x", 60000);
  ok(nrows(&db) > 1, "a key containing SQL is data, not SQL");
  collcache_set(&db, "nullbody", NULL, 60000);
  ok(collcache_get(&db, "nullbody", &age) == NULL, "NULL body violates NOT NULL and is not stored (no crash)");

  sqlite3_close(db.h);
  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
