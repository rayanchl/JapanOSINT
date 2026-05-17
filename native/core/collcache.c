#include "collcache.h"
#include "../third_party/sqlite3.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static long long now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
/* collectorCache.js clampTtl: <=0 → 15min default; else [1min, 24h]. */
static long long clamp_ttl(long long ms) {
  const long long DEF = 15LL*60*1000, MIN = 60LL*1000, MAX = 24LL*60*60*1000;
  if (ms <= 0) return DEF;
  if (ms < MIN) return MIN;
  if (ms > MAX) return MAX;
  return ms;
}

char *collcache_get(db_handle *db, const char *key, long long *age_out) {
  sqlite3_stmt *s = NULL; char *out = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT fc_json, fetched_at, ttl_ms FROM collector_cache WHERE key=?1",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *fc = (const char *)sqlite3_column_text(s, 0);
      long long fetched = sqlite3_column_int64(s, 1);
      long long ttl = sqlite3_column_int64(s, 2);
      long long age = now_ms() - fetched;
      if (fc && age <= ttl) { out = strdup(fc); if (age_out) *age_out = age; }
    }
  }
  sqlite3_finalize(s);
  return out;
}

void collcache_set(db_handle *db, const char *key, const char *fc_json,
                    long long ttl_ms) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO collector_cache (key, fc_json, fetched_at, ttl_ms) "
        "VALUES (?1,?2,?3,?4) ON CONFLICT(key) DO UPDATE SET "
        "fc_json=excluded.fc_json, fetched_at=excluded.fetched_at, "
        "ttl_ms=excluded.ttl_ms", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, fc_json, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 3, now_ms());
    sqlite3_bind_int64(s, 4, clamp_ttl(ttl_ms));
    sqlite3_step(s);
  }
  sqlite3_finalize(s);
}
