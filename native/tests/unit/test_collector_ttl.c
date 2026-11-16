/* test_collector_ttl.c — the writer for collector_ttls (core/collector_ttl.c)
 * and the route bodies behind /api/admin/collector-ttls.
 *
 * WHAT IS BEING PROTECTED.
 *   1. the key must be a REGISTERED source id (what both readers look up): an
 *      unknown key is a 404 on GET and PUT, never a row nothing reads;
 *   2. the TTL must be an integer inside the cache's own clamp
 *      [60,000, 86,400,000] ms — outside is a 400 naming the bound, never a
 *      clamp, and a fractional or non-numeric value is refused;
 *   3. the round trip: PUT stores it (source 'user', updated_by), GET and the
 *      list return it, and the READERS see it — the same SQL dataapi.c and
 *      intelapi.c run returns the new value;
 *   4. DELETE removes the row (the default comes back) and a second DELETE is
 *      a 404, so a reset that changed nothing is not audited;
 *   5. every successful write is one audit row naming key, old and new, and
 *      no refusal writes one;
 *   6. the gate the routes sit behind (opgate_check) refuses a non-operator.
 */
#include "../../core/collector_ttl.c"
#include "core/operatorgate.h"
#include "core/auth.h"

#include <assert.h>

static db_handle g_db;
#define KEY "transport-cluster-runner"   /* a registered source */

static long count_sql(const char *q) {
  sqlite3_stmt *s; long v = -1;
  assert(sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

/* The call is made BEFORE `st` is read: as plain function arguments the
 * order of evaluation would be unspecified. */
#define EXPECT(call, want, needle) do { int st = 0; char *b_ = (call); \
    expect(b_, st, want, needle); } while (0)
static void expect(char *b, int st, int want, const char *needle) {
  assert(b);
  if (st != want || (needle && !strstr(b, needle))) {
    fprintf(stderr, "got %d (want %d, needle %s): %s\n", st, want,
            needle ? needle : "-", b);
    assert(0);
  }
  free(b);
}

static void put(const char *key, const char *body, int want, const char *needle) {
  EXPECT(collector_ttl_api_put(&g_db, key, body, body ? strlen(body) : 0,
                               "op-user", &st), want, needle);
}

/* What core/dataapi.c get_ttl_ms() reads. */
static long long reader_ttl(const char *key) {
  sqlite3_stmt *s; long long v = -1;
  assert(sqlite3_prepare_v2(g_db.h, "SELECT ttl_ms FROM collector_ttls WHERE key=?1",
                            -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

int main(void) {
  unsetenv("PLATFORM_OPERATOR_EMAILS");
  unsetenv("PLATFORM_OPERATOR_IDS");
  assert(db_open(&g_db, NULL, NULL) == 0);
  assert(registry_get(KEY) != NULL);
  collector_ttl_migrate(&g_db);
  collector_ttl_migrate(&g_db);                       /* idempotent */
  sqlite3_exec(g_db.h, "DELETE FROM collector_ttls", NULL, NULL, NULL);
  long a0 = count_sql("SELECT count(*) FROM audit_events WHERE action LIKE 'collector_ttl.%'");

  /* 1. unknown key */
  int st = 0;      /* for the calls checked inline below */
  EXPECT(collector_ttl_api_get(&g_db, "no-such-source-xyz", &st), 404, "unknown_source");
  put("no-such-source-xyz", "{\"ttl_ms\":600000}", 404, "unknown_source");
  assert(reader_ttl("no-such-source-xyz") == -1);
  /* a registered key with no row answers the default, not a 404 */
  EXPECT(collector_ttl_api_get(&g_db, KEY, &st), 200, "\"source\":\"default\"");
  printf("  unknown key 404, registered key without a row = default: ok\n");

  /* 2. bounds and shape */
  put(KEY, "{\"ttl_ms\":59999}", 400, "out_of_range");
  put(KEY, "{\"ttl_ms\":86400001}", 400, "out_of_range");
  put(KEY, "{\"ttl_ms\":0}", 400, "out_of_range");
  put(KEY, "{\"ttl_ms\":-5}", 400, "out_of_range");
  put(KEY, "{\"ttl_ms\":600000.5}", 400, "out_of_range");
  put(KEY, "{\"ttl_ms\":\"600000\"}", 400, "bad_request");
  put(KEY, "{\"ttl\":600000}", 400, "bad_request");
  put(KEY, "not json", 400, "bad_request");
  put(KEY, NULL, 400, "bad_request");
  assert(reader_ttl(KEY) == -1);                      /* nothing was stored */
  printf("  out of range / malformed refused, nothing stored: ok\n");

  /* 3. round trip, both bounds inclusive */
  put(KEY, "{\"ttl_ms\":60000}", 200, "\"ttl_ms\":60000");
  assert(reader_ttl(KEY) == 60000);
  put(KEY, "{\"ttl_ms\":86400000}", 200, "\"ttl_ms\":86400000");
  assert(reader_ttl(KEY) == 86400000);
  put(KEY, "{\"ttl_ms\":3600000}", 200, "\"updated_by\":\"op-user\"");
  assert(reader_ttl(KEY) == 3600000);
  assert(count_sql("SELECT count(*) FROM collector_ttls WHERE key='" KEY
                   "' AND source='user'") == 1);
  EXPECT(collector_ttl_api_get(&g_db, KEY, &st), 200, "\"ttl_ms\":3600000");
  EXPECT(collector_ttl_api_list(&g_db, &st), 200, "\"key\":\"" KEY "\"");
  /* a stale row for a source nobody registers is LISTED, flagged */
  sqlite3_exec(g_db.h, "INSERT INTO collector_ttls(key,ttl_ms,source,updated_at) "
               "VALUES('gone-source',5000,'registry',0)", NULL, NULL, NULL);
  {
    char *b = collector_ttl_api_list(&g_db, &st);
    assert(b && st == 200);
    assert(strstr(b, "\"key\":\"gone-source\""));
    assert(strstr(b, "\"registered\":false"));
    assert(strstr(b, "\"in_range\":false"));        /* 5000 < the cache clamp */
    free(b);
  }
  printf("  PUT round trip seen by the readers' SQL, list flags stale rows: ok\n");

  /* 4. DELETE */
  EXPECT(collector_ttl_api_delete(&g_db, KEY, "op-user", &st), 200,
         "\"source\":\"default\"");
  assert(reader_ttl(KEY) == -1);
  EXPECT(collector_ttl_api_delete(&g_db, KEY, "op-user", &st), 404, "no_override");
  /* a stale row can be cleared although its key is not registered */
  EXPECT(collector_ttl_api_delete(&g_db, "gone-source", "op-user", &st), 200, NULL);
  printf("  DELETE resets, a second DELETE is 404, stale rows clearable: ok\n");

  /* 5. audit: 3 sets + 2 resets, nothing for the refusals */
  long na = count_sql("SELECT count(*) FROM audit_events WHERE action LIKE 'collector_ttl.%'") - a0;
  assert(na == 5);
  assert(count_sql("SELECT count(*) FROM audit_events WHERE action='collector_ttl.set' "
                   "AND target='" KEY "' AND payload_json LIKE '%\"new_ttl_ms\":3600000%' "
                   "AND payload_json LIKE '%\"old_ttl_ms\":86400000%'") == 1);
  printf("  one audit row per change, none for refusals: ok\n");

  /* 6. the gate */
  auth_user anon; memset(&anon, 0, sizeof anon);
  assert(opgate_check(&anon) == -401);
  auth_user u; memset(&u, 0, sizeof u);
  snprintf(u.id, sizeof u.id, "op-user");
  assert(opgate_check(&u) != 0);
  setenv("PLATFORM_OPERATOR_IDS", "someone-else", 1);
  assert(opgate_check(&u) != 0);
  setenv("PLATFORM_OPERATOR_IDS", "op-user", 1);
  assert(opgate_check(&u) == 0);
  unsetenv("PLATFORM_OPERATOR_IDS");
  printf("  operator gate refuses a non-operator: ok\n");

  db_close(&g_db);
  printf("test_collector_ttl: all passed\n");
  return 0;
}
