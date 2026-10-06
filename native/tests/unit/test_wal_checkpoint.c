/* test_wal_checkpoint.c — the WAL pod must never cost a writer its write.
 *
 * collectors/pod/wal_checkpoint_pod.c used to run one TRUNCATE checkpoint with
 * a 20 s busy timeout. TRUNCATE takes the WRITER lock and then waits, holding
 * it, for readers whose snapshots still need the log; every connection db.c
 * opens waits only 5 s for that lock. So a reader that held a snapshot for
 * more than 5 s turned the checkpoint into "database is locked" for every
 * other writer — emit() rolled back and returned -1, the counting sink
 * ignored it, and records were lost with a clean run line. The pod also left
 * its 20 s timeout behind on the scheduler connection it borrowed.
 *
 * This reproduces that shape with the pod's own run():
 *   - a reader holds a snapshot for ~6 s (longer than a writer's 5 s),
 *   - frames are written after that snapshot, so TRUNCATE must wait for it,
 *   - the pod runs on a connection whose busy_timeout is 30000 (the scheduler
 *     worker's), and
 *   - a writer with db.c's 5000 ms timeout commits while the pod is running.
 * The writer must succeed, promptly; the pod's connection must get its own
 * timeout back; and once the reader is gone the pod must truncate the -wal
 * to zero. */
#include "../../collectors/pod/wal_checkpoint_pod.c"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static double now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static sqlite3 *open_conn(const char *path, int busy_ms) {
  sqlite3 *h = NULL;
  int rc = sqlite3_open_v2(path, &h, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                           SQLITE_OPEN_FULLMUTEX, NULL);
  assert(rc == SQLITE_OK);
  sqlite3_busy_timeout(h, busy_ms);
  return h;
}

static void exec_ok(sqlite3 *h, const char *sql) {
  char *err = NULL;
  int rc = sqlite3_exec(h, sql, NULL, NULL, &err);
  if (rc != SQLITE_OK) { fprintf(stderr, "exec failed: %s (%s)\n", sql, err ? err : "?"); }
  assert(rc == SQLITE_OK);
  sqlite3_free(err);
}

static void insert_rows(sqlite3 *h, int n) {
  exec_ok(h, "BEGIN");
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(h, "INSERT INTO t(x) VALUES (?1)", -1, &s, NULL) == SQLITE_OK);
  for (int i = 0; i < n; i++) {
    sqlite3_bind_int(s, 1, i);
    assert(sqlite3_step(s) == SQLITE_DONE);
    sqlite3_reset(s);
  }
  sqlite3_finalize(s);
  exec_ok(h, "COMMIT");
}

static int count_rows(sqlite3 *h) {
  sqlite3_stmt *s; int n = -1;
  assert(sqlite3_prepare_v2(h, "SELECT count(*) FROM t", -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
  sqlite3_finalize(s);
  return n;
}

typedef struct { db_handle db; int rc; double ms; } pod_arg;

static void *pod_thread(void *vp) {
  pod_arg *a = vp;
  source_ctx ctx; memset(&ctx, 0, sizeof ctx);
  ctx.source_id = "wal-checkpoint";
  ctx.db = &a->db;
  double t0 = now_ms();
  a->rc = run(&ctx, NULL);
  a->ms = now_ms() - t0;
  return NULL;
}

int main(void) {
  char path[1024];
  const char *base = getenv("JO_DB");
  snprintf(path, sizeof path, "%s.walpod", base && *base ? base : "/tmp/jo-unit-walpod.db");
  char aux[1100];
  unlink(path);
  snprintf(aux, sizeof aux, "%s-wal", path); unlink(aux);
  snprintf(aux, sizeof aux, "%s-shm", path); unlink(aux);

  sqlite3 *init = open_conn(path, 5000);
  exec_ok(init, "PRAGMA journal_mode=WAL");
  exec_ok(init, "PRAGMA wal_autocheckpoint=0");
  exec_ok(init, "CREATE TABLE t(x)");
  insert_rows(init, 2000);

  /* A reader holding a snapshot across statements — the export walk / status
   * build shape. Taken BEFORE the second batch, so TRUNCATE must wait for it. */
  sqlite3 *reader = open_conn(path, 5000);
  exec_ok(reader, "BEGIN");
  assert(count_rows(reader) == 2000);
  insert_rows(init, 2000);

  /* The pod borrows a scheduler-worker-shaped connection (busy_timeout=30000). */
  pod_arg pa; memset(&pa, 0, sizeof pa);
  pa.db.h = open_conn(path, 30000);
  exec_ok(pa.db.h, "PRAGMA journal_mode=WAL");   /* what db_attach() runs */
  pthread_t th;
  double t_start = now_ms();
  assert(pthread_create(&th, NULL, pod_thread, &pa) == 0);
  usleep(200 * 1000);                    /* let the first TRUNCATE take the lock */

  /* A db.c-default writer (5 s) — the event loop, an audit_write, an emit on
   * a 5 s connection. It must commit, and long before its own timeout. */
  sqlite3 *w = open_conn(path, 5000);
  double t0 = now_ms();
  int wrc = sqlite3_exec(w, "BEGIN; INSERT INTO t(x) VALUES (-1); COMMIT;", NULL, NULL, NULL);
  double wms = now_ms() - t0;
  printf("  writer during checkpoint: rc=%d (%s) after %.0f ms\n", wrc, sqlite3_errstr(wrc), wms);
  assert(wrc == SQLITE_OK && "a checkpoint must never make another writer fail");
  /* Under the old 20 s TRUNCATE this writer FAILED at its own 5 s; anything
   * that commits inside that window is the fix working. The bound is the
   * writer's own timeout, not a guess at runner speed: the macOS CI runner
   * took 1,775 ms here where a laptop takes ~300. */
  assert(wms < 4500.0 && "the writer waited out most of its own busy timeout");

  /* The reader outlives the pod: it is released only after run() returns, so
   * the pod must give up on its OWN bound. Releasing it on a wall clock (6 s)
   * raced the last attempt — on a slow runner that attempt began late, the
   * reader let go mid-attempt, the TRUNCATE succeeded at 6,233 ms and the
   * bound assert failed on a pod behaving exactly as designed (CI, f6e589e).
   * The reader is still held past the 5 s a writer will wait — the case that
   * used to turn into "database is locked". */
  pthread_join(th, NULL);
  while (now_ms() - t_start < 6000.0) usleep(50 * 1000);
  exec_ok(reader, "COMMIT");
  printf("  pod run with a long reader: rc=%d in %.0f ms\n", pa.rc, pa.ms);
  assert(pa.rc == -1 && "frames the reader still needs: TRUNCATE skipped, PASSIVE only");
  /* An attempt can overrun its busy timeout by one busy-handler sleep, so
   * allow 2x per attempt, the gaps, and 3 s of scheduling slack. What this
   * guards against is one 20 s TRUNCATE, far outside it. */
  const double bound = WAL_TRUNC_TRIES * 2.0 * WAL_TRUNC_WAIT_MS
                     + (WAL_TRUNC_TRIES - 1) * (double)WAL_TRUNC_GAP_MS + 3000.0;
  assert(pa.ms < bound && "TRUNCATE attempts are bounded");

  /* The borrowed connection gets its own timeout back. */
  assert(busy_timeout_of(pa.db.h) == 30000);
  printf("  pod restored the connection's busy_timeout: ok\n");

  /* Nothing was lost: 2000 + 2000 + the writer's row. */
  assert(count_rows(init) == 4001);

  /* With the reader gone, the pod empties the -wal — run here on a FRESH
   * connection that has never read the file, where sqlite3_wal_checkpoint_v2()
   * on its own is a silent SQLITE_OK no-op (-1/-1 frames). */
  sqlite3_close(pa.db.h);
  pa.db.h = open_conn(path, 30000);
  pod_thread(&pa);
  snprintf(aux, sizeof aux, "%s-wal", path);
  struct stat st;
  long long wal = stat(aux, &st) == 0 ? (long long)st.st_size : -1;
  printf("  pod run without readers: rc=%d, -wal now %lld bytes\n", pa.rc, wal);
  assert(pa.rc == 0);
  assert(wal == 0);
  assert(busy_timeout_of(pa.db.h) == 30000);

  sqlite3_close(w); sqlite3_close(reader); sqlite3_close(pa.db.h); sqlite3_close(init);
  unlink(path);
  snprintf(aux, sizeof aux, "%s-wal", path); unlink(aux);
  snprintf(aux, sizeof aux, "%s-shm", path); unlink(aux);
  printf("test_wal_checkpoint: ok\n");
  return 0;
}
