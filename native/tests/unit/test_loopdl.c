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
 *   - a lock wait (second connection holding an exclusive lock) gives up at the
 *     same deadline with SQLITE_BUSY, reported for the 503; released in time it
 *     succeeds;
 *   - the 503 body is JSON naming the deadline. */
#include "../../core/loopdl.c"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static sqlite3 *g_wr;
static void *release_later(void *a) {
  (void) a;
  usleep(200000);
  sqlite3_exec(g_wr, "COMMIT", NULL, NULL, NULL);
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

  /* Lock wait: a second connection holds an exclusive lock on a file DB. */
  {
    char path[128];
    snprintf(path, sizeof path, "/tmp/test_loopdl_lock_%d.db", (int) getpid());
    unlink(path);
    sqlite3 *loop, *wr;
    assert(sqlite3_open(path, &loop) == SQLITE_OK);
    assert(sqlite3_open(path, &wr) == SQLITE_OK);
    sqlite3_exec(loop, "PRAGMA busy_timeout=5000;", NULL, NULL, NULL);
    loopdl_install(loop);               /* replaces the busy timeout */
    assert(sqlite3_exec(wr, "CREATE TABLE t(x); BEGIN EXCLUSIVE;", NULL, NULL, NULL) == SQLITE_OK);

    loopdl_arm_ms(150);
    long long t1 = mono_ms();
    int brc = sqlite3_exec(loop, "INSERT INTO t VALUES(1)", NULL, NULL, NULL);
    long long bel = mono_ms() - t1;
    assert(brc == SQLITE_BUSY);
    assert(bel >= 100 && bel < 1500 && "lock wait gives up at the deadline");
    assert(loopdl_disarm() == 1 && "reported so the route answers 503");
    printf("  armed, lock held: gave up in %lld ms (deadline 150) and reported: ok\n", bel);

    /* not armed: falls back to a bounded wait; release the lock mid-wait */
    sqlite3_exec(wr, "COMMIT", NULL, NULL, NULL);
    assert(sqlite3_exec(loop, "INSERT INTO t VALUES(2)", NULL, NULL, NULL) == SQLITE_OK);
    assert(loopdl_disarm() == 0);
    sqlite3_exec(wr, "BEGIN EXCLUSIVE", NULL, NULL, NULL);
    sqlite3_exec(wr, "ROLLBACK", NULL, NULL, NULL);
    printf("  unarmed: free lock acquired, not reported: ok\n");

    /* armed with a generous deadline, lock released while waiting: succeeds */
    sqlite3_exec(wr, "BEGIN EXCLUSIVE", NULL, NULL, NULL);
    g_wr = wr;
    pthread_t rel;
    pthread_create(&rel, NULL, release_later, NULL);
    loopdl_arm_ms(4000);
    assert(sqlite3_exec(loop, "INSERT INTO t VALUES(3)", NULL, NULL, NULL) == SQLITE_OK);
    assert(loopdl_disarm() == 0);
    pthread_join(rel, NULL);
    printf("  armed, lock released inside the deadline: write succeeds: ok\n");

    sqlite3_close(wr); sqlite3_close(loop);
    unlink(path);
  }

  sqlite3_close(h);
  printf("test_loopdl: all passed\n");
  return 0;
}
