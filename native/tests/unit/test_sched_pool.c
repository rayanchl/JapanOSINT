/* test_sched_pool.c — the scheduler pool's lanes, cancel, claim and respawn.
 *
 * What is pinned, and what each one used to be:
 *   1. boot ramp: the k-th scheduled source's first run is bounded by
 *      JO_SCHED_RAMP_SEC and by its own interval. It used to be
 *      (i / workers) * 3 s over ALL 18,643 registered sources, interval-0
 *      pivots included: ~1.9 h before the last scheduled source first ran.
 *   2. maintenance lane: a pod runs while every collector worker is busy. Pods
 *      used to share the collector workers, so the WAL checkpoint queued
 *      behind the fleet exactly when writes were heaviest.
 *   3. claim: a source the pool is running cannot be claimed by a manual run,
 *      and a claimed source is not started by the pool until it is released.
 *   4. cancel is real: once a run outlasts the deadline AND its own interval
 *      the watchdog sets the flag the running collector polls (it used to
 *      point at a local nobody could reach, and only logged), and
 *      shutdown sets it for every run in flight and wakes the idle workers.
 *   5. a worker whose DB attach fails retries and is counted, instead of
 *      exiting and shrinking the pool for good.
 *
 * Includes scheduler.c to reach the pool internals; run.sh links everything
 * except obj/core/scheduler.o and obj/main.o. */
#include "../../core/scheduler.c"

#include <assert.h>

static atomic_int g_slow_started, g_slow_cancelled, g_pod_runs, g_exited;

static int run_slow(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  atomic_store(&g_slow_started, 1);
  for (int i = 0; i < 3000; i++) {                 /* 30 s ceiling */
    if (__atomic_load_n((const int *)ctx->cancel, __ATOMIC_ACQUIRE)) {
      atomic_store(&g_slow_cancelled, 1);
      return 0;
    }
    usleep(10 * 1000);
  }
  return 0;
}
static int run_pod(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; (void)sink;
  atomic_fetch_add(&g_pod_runs, 1);
  return 0;
}

/* interval 5: the watchdog cancels at max(deadline, interval). */
static const source_def D_SLOW = { .id = "UNIT_POOL_SLOW", .collector = "unit",
  .name = "unit", .update_interval_sec = 5, .run = run_slow, .category = "test" };
static const source_def D_POD = { .id = "UNIT_POOL_POD", .collector = "_test",
  .name = "unit", .update_interval_sec = 3600, .run = run_pod, .category = "test" };
REGISTER_SOURCE(D_SLOW)
REGISTER_SOURCE(D_POD)

static int wait_for(atomic_int *v, int want, int ms) {
  for (int t = 0; t < ms; t += 10) {
    if (atomic_load(v) >= want) return 1;
    usleep(10 * 1000);
  }
  return atomic_load(v) >= want;
}
static int running_of(int i) {
  pthread_mutex_lock(&g_q.mu);
  int r = g_q.running[i];
  pthread_mutex_unlock(&g_q.mu);
  return r;
}
static int wait_idle(int i, int ms) {
  for (int t = 0; t < ms; t += 10) {
    if (!running_of(i)) return 1;
    usleep(10 * 1000);
  }
  return !running_of(i);
}

static void *worker_wrap(void *arg) {
  worker_thread(arg);
  atomic_fetch_add(&g_exited, 1);
  return NULL;
}

static void test_boot_offset(void) {
  /* The live registry: 18,643 sources, 8 workers, 3 s. */
  long last = sched_boot_offset(18642, 18643, 8, 3, 900, 3600);
  printf("  boot ramp: last of 18,643 at %lds (old formula: %lds)\n",
         last, (long)(18642 / 8) * 3);
  assert(last <= 900);
  for (int k = 0; k < 18643; k += 97) {
    long o = sched_boot_offset(k, 18643, 8, 3, 900, 60);
    assert(o >= 0 && o < 60 && "no source waits longer than its interval");
    long o2 = sched_boot_offset(k, 18643, 8, 3, 900, 86400);
    long o3 = sched_boot_offset(k + 97, 18643, 8, 3, 900, 86400);
    assert(o2 <= o3 && o2 <= 900 && "spread, in order, inside the ramp");
  }
  /* A small fleet keeps the gentle per-worker spacing exactly. */
  assert(sched_boot_offset(15, 16, 8, 3, 900, 3600) == 3);
  assert(sched_boot_offset(8, 16, 8, 3, 900, 3600) == 3);
  assert(sched_boot_offset(7, 16, 8, 3, 900, 3600) == 0);
  /* ramp 0 = no cap; stagger 0 = no ramp at all. */
  assert(sched_boot_offset(800, 1000, 8, 3, 0, 86400) == 300);
  assert(sched_boot_offset(999, 1000, 8, 0, 900, 3600) == 0);
}

int main(void) {
  test_boot_offset();

  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  assert(sched_pool_init(registry_all(), registry_count()) == 0);
  snprintf(g_db_path, sizeof g_db_path, "%s", sqlite3_db_filename(db.h, "main"));
  g_q.lane[LANE_COLLECT].workers = 1;
  g_q.lane[LANE_MAINT].workers = 1;
  int i_slow = registry_index(D_SLOW.id), i_pod = registry_index(D_POD.id);
  assert(i_slow >= 0 && i_pod >= 0);
  assert(lane_of(&D_SLOW) == LANE_COLLECT && lane_of(&D_POD) == LANE_MAINT);

  pthread_t wc, wm, wbad;
  assert(pthread_create(&wc, NULL, worker_wrap, (void *)(intptr_t)LANE_COLLECT) == 0);
  assert(pthread_create(&wm, NULL, worker_wrap, (void *)(intptr_t)LANE_MAINT) == 0);

  /* 2. the only collector worker is busy; the pod still runs. */
  q_push(i_slow, 0.0);
  assert(wait_for(&g_slow_started, 1, 5000));
  q_push(i_pod, 0.0);
  assert(wait_for(&g_pod_runs, 1, 5000) && "pod waited behind a collector");
  printf("  maintenance lane: pod ran while the collector worker was busy\n");

  /* 3. claim is shared with the pool. */
  assert(scheduler_claim(D_SLOW.id) == 0 && "claimed a source the pool is running");
  assert(wait_idle(i_pod, 5000));
  assert(scheduler_claim(D_POD.id) == 1);
  assert(sched_cancel_slot(D_POD.id) != NULL && "a claimed run gets a cancel slot");
  q_push(i_pod, 0.0);                       /* skip-if-running: not queued */
  usleep(300 * 1000);
  pthread_mutex_lock(&g_q.mu);
  int mq = g_q.lane[LANE_MAINT].count;
  pthread_mutex_unlock(&g_q.mu);
  assert(atomic_load(&g_pod_runs) == 1 && mq == 0);
  scheduler_release(D_POD.id);
  q_push(i_pod, 0.0);
  assert(wait_for(&g_pod_runs, 2, 5000));
  assert(scheduler_claim("NO_SUCH_SOURCE") == 1);   /* nothing to share */
  scheduler_release("NO_SUCH_SOURCE");
  printf("  claim: shared skip-if-running with the pool\n");

  /* 4a. the watchdog's deadline reaches the collector — not at the deadline
   * (1 s) while the run is inside its own interval (5 s), but past both. */
  g_deadline_sec = 1;
  pthread_mutex_lock(&g_q.mu);
  time_t started = g_q.started[i_slow];
  pthread_mutex_unlock(&g_q.mu);
  assert(started > 0);
  while (time(NULL) - started <= 2) usleep(50 * 1000);
  watchdog_scan();
  usleep(200 * 1000);
  assert(atomic_load(&g_slow_cancelled) == 0 && "cancelled inside its own interval");
  while (time(NULL) - started <= 6) usleep(50 * 1000);
  watchdog_scan();
  assert(wait_for(&g_slow_cancelled, 1, 3000) && "watchdog cancel never reached the run");
  assert(wait_idle(i_slow, 5000));
  printf("  watchdog: deadline set ctx->cancel, the run stopped\n");

  /* 5. a worker that cannot attach retries, counted. */
  snprintf(g_db_path, sizeof g_db_path, "/nonexistent-jo-dir/none.db");
  assert(pthread_create(&wbad, NULL, worker_wrap, (void *)(intptr_t)LANE_COLLECT) == 0);
  for (int t = 0; t < 5000 && atomic_load(&g_attach_failures) < 1; t += 10)
    usleep(10 * 1000);
  assert(atomic_load(&g_attach_failures) >= 1);
  assert(atomic_load(&g_exited) == 0 && "a failed attach must not end the worker");
  printf("  respawn: attach failure counted, worker still alive\n");

  /* 4b. shutdown cancels what is in flight and releases idle workers. */
  atomic_store(&g_slow_started, 0);
  atomic_store(&g_slow_cancelled, 0);
  g_deadline_sec = 0;
  q_push(i_slow, 0.0);
  assert(wait_for(&g_slow_started, 1, 5000));
  scheduler_stop_background(5000);
  assert(atomic_load(&g_slow_cancelled) == 1 && "shutdown did not cancel the run");
  assert(wait_for(&g_exited, 3, 5000) && "a worker stayed parked in q_pop");
  pthread_join(wc, NULL); pthread_join(wm, NULL); pthread_join(wbad, NULL);
  printf("  shutdown: run cancelled, all 3 workers exited\n");

  db_close(&db);
  printf("test_sched_pool: ok\n");
  return 0;
}
