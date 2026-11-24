/* test_loopdl.c — API-07: a SQLite query on the event loop has a deadline.
 *
 * core/loopdl.c installs a progress handler on the loop's connection and arms
 * it per request. Holds:
 *   - unarmed (startup, migrations, timers, worker threads): an expensive query
 *     runs to completion;
 *   - armed with a small deadline: the same query is interrupted
 *     (SQLITE_INTERRUPT), loopdl_disarm() reports it, and the next statement
 *     after disarm works on the same connection;
 *   - a cheap query under the deadline is untouched and disarm reports 0;
 *   - armed on THIS thread does not reach a query on another thread using the
 *     same connection (a worker that fell back to the shared handle);
 *   - JO_LOOP_QUERY_MS: default, override, 0 = disabled, garbage = default;
 *   - the 503 body is JSON naming the deadline. */
#include "../../core/loopdl.c"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* Counts to N with a recursive CTE: cost scales with N, no table needed. */
static int run_count(sqlite3 *h, long n, long long *out) {
  char sql[200];
  snprintf(sql, sizeof sql,
    "WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM c WHERE x<%ld) "
    "SELECT count(*) FROM c", n);
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(h, sql, -1, &st, NULL) != SQLITE_OK) return -1;
  int rc = sqlite3_step(st);
  if (rc == SQLITE_ROW && out) *out = sqlite3_column_int64(st, 0);
  sqlite3_finalize(st);
  return rc;
}

static sqlite3 *g_shared;
static int g_thread_rc;
static void *worker(void *arg) {
  (void) arg;
  long long n = 0;
  g_thread_rc = run_count(g_shared, 3000000, &n);
  return NULL;
}

int main(void) {
  sqlite3 *h;
  assert(sqlite3_open(":memory:", &h) == SQLITE_OK);
  loopdl_install(h);
  long long n = 0;

  assert(run_count(h, 2000000, &n) == SQLITE_ROW && n == 2000000);
  printf("  unarmed: expensive query completes: ok\n");

  loopdl_arm_ms(5000);
  assert(run_count(h, 100, &n) == SQLITE_ROW && n == 100);
  assert(loopdl_disarm() == 0);
  printf("  armed, cheap query: not interrupted: ok\n");

  long long t0 = mono_ms();
  loopdl_arm_ms(50);
  int rc = run_count(h, 2000000000L, &n);
  long long el = mono_ms() - t0;
  assert(rc == SQLITE_INTERRUPT);
  assert(el < 3000 && "interrupted promptly, not after the full recursion");
  /* a further big query in the same request aborts at its first check */
  t0 = mono_ms();
  assert(run_count(h, 2000000000L, &n) == SQLITE_INTERRUPT);
  assert(mono_ms() - t0 < 500);
  assert(loopdl_disarm() == 1);
  printf("  armed, 50 ms deadline: interrupted in %lld ms and reported: ok\n", el);

  assert(run_count(h, 1000, &n) == SQLITE_ROW && n == 1000);
  assert(loopdl_disarm() == 0);
  printf("  connection healthy after an overrun: ok\n");

  g_shared = h;
  loopdl_arm_ms(1);
  pthread_t th;
  pthread_create(&th, NULL, worker, NULL);
  pthread_join(th, NULL);
  assert(g_thread_rc == SQLITE_ROW && "worker thread is never interrupted");
  loopdl_disarm();
  printf("  other thread on the shared handle: not interrupted: ok\n");

  loopdl_arm_ms(0);
  assert(run_count(h, 1500000, &n) == SQLITE_ROW);
  assert(loopdl_disarm() == 0);

  unsetenv("JO_LOOP_QUERY_MS");
  assert(loopdl_ms() == LOOPDL_DEFAULT_MS);
  setenv("JO_LOOP_QUERY_MS", "250", 1);
  assert(loopdl_ms() == 250);
  setenv("JO_LOOP_QUERY_MS", "0", 1);
  assert(loopdl_ms() == 0);
  loopdl_arm();
  assert(run_count(h, 1500000, &n) == SQLITE_ROW && loopdl_disarm() == 0);
  setenv("JO_LOOP_QUERY_MS", "soon", 1);
  assert(loopdl_ms() == LOOPDL_DEFAULT_MS && "garbage reads as the default");
  setenv("JO_LOOP_QUERY_MS", "40", 1);
  loopdl_arm();
  assert(run_count(h, 2000000000L, &n) == SQLITE_INTERRUPT);
  assert(loopdl_disarm() == 1);
  printf("  JO_LOOP_QUERY_MS: default, override, 0, garbage: ok\n");

  const char *b = loopdl_body();
  assert(strstr(b, "\"error\":\"query_deadline_exceeded\""));
  assert(strstr(b, "\"deadline_ms\":40"));
  printf("  503 body names the deadline: ok\n");

  sqlite3_close(h);
  printf("test_loopdl: all passed\n");
  return 0;
}
