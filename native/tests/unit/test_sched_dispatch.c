/* test_sched_dispatch.c - the dispatcher's due-time heap, host-aware pop, and
 * the cancel window (scheduler.c, 2026-11-16).
 *
 * What is pinned, and what each one used to be:
 *   1. THE HEAP. The dispatcher walked every registered row (93,000) once a
 *      second to find the handful that were due. It now keeps a min-heap of
 *      (due time, row); a tick with nothing due is a comparison. The behaviours
 *      the old walk had are kept: a row that came due and found no room STAYS
 *      due (and is served before newer ones), a row whose run is still in
 *      flight is looked at again one spacing on, a plan change re-plans, a
 *      stale pair (the row was re-planned after it was pushed) is dropped.
 *   2. HOST-AWARE POP. A worker used to pop whichever source ranked first and
 *      then park inside hostgate_acquire. An entry whose host is at its
 *      in-flight cap, or is not ready, now stays queued and the next eligible
 *      one is taken - but never starves: skipped too often, or older than its
 *      interval, it is taken anyway.
 *   3. THE CANCEL WINDOW is max(deadline, min(interval, JO_SCHED_CANCEL_MAX_SEC)),
 *      not the row's interval: under a 6 h cadence it was 6 h.
 *   4. A MEASUREMENT: the cost of one dispatcher tick at registry size, the
 *      old whole-registry walk (reproduced below, byte for byte in what it
 *      reads) against sched_tick(). Printed, and only loosely asserted: a
 *      timing is not an invariant, so the assertion is that the new tick is
 *      not slower, which holds by orders of magnitude.
 *
 * Includes scheduler.c to reach the internals. */
#include "../../core/scheduler.c"

#include <assert.h>

static double now_us(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

/* Pick `k` distinct scheduled collector-lane rows, in registry order. */
static int pick_rows(int *out, int k, int skip_from) {
  int nf = 0;
  for (int i = skip_from; i < g_n && nf < k; i++)
    if (g_all[i]->update_interval_sec >= 3600 && lane_of(g_all[i]) == LANE_COLLECT) out[nf++] = i;   /* not a real-time row: those re-plan inside the test window */
  return nf;
}

/* Park every scheduled row far in the future and rebuild the heap, so a test
 * decides exactly which rows are due. */
static void park_all(time_t when) {
  pthread_mutex_lock(&g_q.mu);
  for (int i = 0; i < g_n; i++) g_q.next[i] = when;
  g_q.def[0].head = g_q.def[0].n = g_q.def[1].head = g_q.def[1].n = 0;
  memset(g_q.dflag, 0, (size_t)g_n);
  heap_rebuild();
  pthread_mutex_unlock(&g_q.mu);
}

static int lane_count(int ln) {
  pthread_mutex_lock(&g_q.mu);
  int c = g_q.lane[ln].count;
  pthread_mutex_unlock(&g_q.mu);
  return c;
}
static void drain_lanes(void) {
  pthread_mutex_lock(&g_q.mu);
  for (int ln = 0; ln < LANE_N; ln++) {
    for (int k = 0; k < g_q.lane[ln].count; k++) g_q.running[g_q.lane[ln].idx[k]] = 0;
    g_q.lane[ln].count = 0;
  }
  pthread_mutex_unlock(&g_q.mu);
}

static void test_heap_ordering(void) {
  /* heap_push/heap_pop hand pairs back in time order whatever the push order,
   * and a pair is valid only while next[i] still equals its time */
  pthread_mutex_lock(&g_q.mu);
  g_q.hn = 0;
  unsigned seed = 12345;
  for (int k = 0; k < 5000; k++) {
    seed = seed * 1103515245u + 12345u;
    heap_push((time_t)(seed >> 8) % 100000, k % g_n);
  }
  time_t prev = -1; int c = 0;
  while (g_q.hn > 0) { sched_due e = heap_pop(); assert(e.t >= prev); prev = e.t; c++; }
  assert(c == 5000);
  pthread_mutex_unlock(&g_q.mu);
  printf("  heap: 5,000 pairs come back in due order\n");
}

static void test_tick(db_handle *db) {
  int r[8];
  assert(pick_rows(r, 8, 0) == 8);
  time_t now = time(NULL);
  int *cand = malloc(sizeof(int) * SCHED_QCAP * LANE_N);
  assert(cand);

  /* nothing due: nothing queued, nothing deferred */
  park_all(now + 1000000);
  drain_lanes();
  assert(sched_tick(db, now, cand) == 0 && lane_count(LANE_COLLECT) == 0);

  /* two rows come due: both queued, both re-planned past `now`, the others untouched */
  pthread_mutex_lock(&g_q.mu);
  plan_set(r[0], now - 5); plan_set(r[1], now - 1);
  pthread_mutex_unlock(&g_q.mu);
  assert(sched_tick(db, now, cand) == 0);
  assert(lane_count(LANE_COLLECT) == 2);
  assert(g_q.next[r[0]] > now && g_q.next[r[1]] > now && g_q.due[r[0]] == now);
  assert(g_q.next[r[2]] == now + 1000000);
  assert(sched_tick(db, now + 1, cand) == 0 && lane_count(LANE_COLLECT) == 2 &&
         "a row was queued twice");
  drain_lanes();
  printf("  tick: only the rows that came due are queued, and re-planned\n");

  /* a row whose run is still in flight is looked at again one spacing on */
  pthread_mutex_lock(&g_q.mu);
  g_q.running[r[3]] = 1;
  plan_set(r[3], now - 1);
  pthread_mutex_unlock(&g_q.mu);
  assert(sched_tick(db, now, cand) == 0 && lane_count(LANE_COLLECT) == 0);
  assert(g_q.next[r[3]] >= now + sched_next_delay(&g_state[r[3]], &g_pol, g_all[r[3]]) &&
         "an in-flight row was not re-planned");
  pthread_mutex_lock(&g_q.mu); g_q.running[r[3]] = 0; pthread_mutex_unlock(&g_q.mu);

  /* DEFERRED rows stay due. Leave room for one, make three due, oldest first. */
  pthread_mutex_lock(&g_q.mu);
  g_q.lane[LANE_COLLECT].count = SCHED_QCAP - 1;      /* one slot of room */
  plan_set(r[4], now - 30); plan_set(r[5], now - 20); plan_set(r[6], now - 10);
  pthread_mutex_unlock(&g_q.mu);
  int def = sched_tick(db, now, cand);
  assert(def == 2 && "two rows had no room and were not reported deferred");
  assert(g_q.running[r[4]] && !g_q.running[r[5]] && !g_q.running[r[6]] &&
         "the most overdue row did not go first");
  assert(g_q.next[r[5]] == now - 20 && g_q.next[r[6]] == now - 10 &&
         "a deferred row lost its due time");
  assert(sched_tick(db, now + 1, cand) == 2 && "deferred rows were dropped or re-added");
  /* room appears: both go, in due order, and nothing is deferred any more */
  pthread_mutex_lock(&g_q.mu);
  g_q.lane[LANE_COLLECT].count = 0;
  g_q.running[r[4]] = 0;
  pthread_mutex_unlock(&g_q.mu);
  assert(sched_tick(db, now + 2, cand) == 0);
  assert(g_q.running[r[5]] && g_q.running[r[6]] && lane_count(LANE_COLLECT) == 2);
  assert(g_q.next[r[5]] > now && g_q.next[r[6]] > now);
  drain_lanes();
  printf("  tick: a full lane defers rows, oldest first, and they stay due\n");

  /* a stale pair is dropped: re-plan a due row into the future before the tick */
  pthread_mutex_lock(&g_q.mu);
  plan_set(r[7], now - 100);
  plan_set(r[7], now + 50000);                          /* the earlier pair is now stale */
  pthread_mutex_unlock(&g_q.mu);
  assert(sched_tick(db, now, cand) == 0 && lane_count(LANE_COLLECT) == 0 &&
         "a stale heap pair queued a row that had been re-planned");
  assert(g_q.next[r[7]] == now + 50000);
  printf("  tick: a re-planned row's stale pair is dropped\n");

  /* sched_publish re-plans a finished run's row through the heap: due again
   * measured from dispatch, and the new time is what the next tick acts on */
  const source_def *d = g_all[r[0]];
  sched_state ok; memset(&ok, 0, sizeof ok);
  time_t t0 = now + 100;
  sched_publish(d, &ok, t0);
  assert(g_q.next[r[0]] == t0 + sched_row_interval(&ok, d));
  sched_state bad; memset(&bad, 0, sizeof bad);
  bad.consecutive_failures = 2; bad.effective_interval = 40; bad.backoff_until = t0 + 7777;
  sched_publish(d, &bad, t0);
  assert(g_q.next[r[0]] == t0 + 7777);
  pthread_mutex_lock(&g_q.mu); size_t hn = g_q.hn; pthread_mutex_unlock(&g_q.mu);
  assert(hn >= 1);
  assert(sched_tick(db, t0 + 7776, cand) == 0 && lane_count(LANE_COLLECT) == 0);
  assert(sched_tick(db, t0 + 7777, cand) == 0 && lane_count(LANE_COLLECT) == 1 &&
         g_q.running[r[0]] && "the health hold did not release at its time");
  drain_lanes();
  printf("  publish: an outcome re-plans through the heap; a hold releases on time\n");

  /* a policy change re-plans: a row planned a day out is pulled in to its new spacing */
  park_all(now + 86400);
  cadence_policy gp = {600, 0, 0, 0};
  const char *err = NULL;
  assert(cadence_set(db, &gp, "unit", &err) == 0);
  long pulled = sched_replan(now);
  assert(pulled > 1000 && "the policy change re-planned nothing");
  assert(g_q.next[r[2]] == now + 600);
  assert(sched_tick(db, now + 599, cand) == 0 && lane_count(LANE_COLLECT) == 0);
  {                                                    /* the heap holds a valid pair at the new time */
    int found = 0;
    pthread_mutex_lock(&g_q.mu);
    for (size_t k = 0; k < g_q.hn; k++)
      if (g_q.heap[k].i == r[2] && g_q.heap[k].t == now + 600) found = 1;
    pthread_mutex_unlock(&g_q.mu);
    assert(found && "the re-planned time never reached the heap");
  }
  drain_lanes();
  cadence_policy none = {0, 0, 0, 0};
  assert(cadence_set(db, &none, "unit", &err) == 0);
  printf("  replan: a policy change pulls %ld rows in; the heap serves them at the new time\n", pulled);
  free(cand);
}

static void *pop_thread(void *arg) {
  int *out = arg;
  *out = q_pop(LANE_COLLECT);
  return NULL;
}

static void test_host_aware_pop(void) {
  park_all(time(NULL) + 1000000);
  drain_lanes();
  int r[6];
  assert(pick_rows(r, 6, 0) == 6);
  /* r0,r1,r2 are on host A (cap 1, as a paced host is); r3,r4 on host B */
  int ha, hb;
  pthread_mutex_lock(&g_q.mu);
  ha = host_intern("host-a.unit.example"); hb = host_intern("host-b.unit.example");
  g_hcap[ha] = 1; g_hcap[hb] = 2;
  g_hid[r[0]] = g_hid[r[1]] = g_hid[r[2]] = ha;
  g_hid[r[3]] = g_hid[r[4]] = hb;
  g_hid[r[5]] = 0;                                    /* host unknown */
  pthread_mutex_unlock(&g_q.mu);
  assert(ha > 0 && hb > 0 && ha != hb);

  /* A1 (key 0), A2 (key 1), B1 (key 2): A1 first; then A2 is blocked by A1 in
   * flight, so B1 - a worse key - is taken rather than parking on host A */
  assert(q_push(r[0], 0.0) == 1 && q_push(r[1], 1.0) == 1 && q_push(r[3], 2.0) == 1);
  assert(q_pop(LANE_COLLECT) == r[0]);
  assert(g_hinfl[ha] == 1);
  assert(q_pop(LANE_COLLECT) == r[3] && "a worker took a blocked host's entry");
  assert(lane_count(LANE_COLLECT) == 1 && "the skipped entry did not stay queued");
  assert(g_hinfl[hb] == 1);
  q_done(r[3]);
  assert(g_hinfl[hb] == 0);
  printf("  pop: a host at its cap is passed over for the next eligible entry\n");

  /* every queued entry waits on host A: the worker sleeps, and a finishing run
   * releases it (it did not park inside hostgate_acquire, and did not spin) */
  int got = -2;
  pthread_t t;
  assert(pthread_create(&t, NULL, pop_thread, &got) == 0);
  usleep(400 * 1000);
  assert(got == -2 && "a worker took an entry whose host was at its cap");
  double t0 = now_us();
  q_done(r[0]);
  pthread_join(t, NULL);
  assert(got == r[1]);
  printf("  pop: all-blocked worker slept, woke %.1f ms after the host freed\n", (now_us() - t0) / 1000.0);
  q_done(r[1]);

  /* never starve, by skip count: A2 is passed over once, then taken even though
   * host A is full again */
  drain_lanes();
  g_skip_max = 2;
  long forced0 = atomic_load(&g_forced_pops);
  assert(q_push(r[0], 0.0) == 1);
  assert(q_pop(LANE_COLLECT) == r[0]);                /* A1 holds host A */
  assert(q_push(r[2], 0.0) == 1);                     /* A3 is blocked, key 0 */
  for (int k = 0; k < 2; k++) {                       /* two pops pass it over */
    assert(q_push(r[3], 5.0) == 1);
    assert(q_pop(LANE_COLLECT) == r[3]);
    q_done(r[3]);
  }
  assert(g_q.lane[LANE_COLLECT].skips[0] == 2);
  assert(q_push(r[4], 5.0) == 1);
  int third = q_pop(LANE_COLLECT);
  assert(third == r[2] && "a starved entry was not taken after JO_SCHED_HOST_SKIP_MAX skips");
  assert(atomic_load(&g_forced_pops) == forced0 + 1);
  q_done(r[2]); q_done(r[0]);
  drain_lanes();
  g_skip_max = 1000;
  printf("  pop: an entry passed over %u times is forced (forced_pops +1)\n", 2u);

  /* never starve, by age: an entry queued longer than its interval is taken */
  assert(q_push(r[0], 0.0) == 1);
  assert(q_pop(LANE_COLLECT) == r[0]);
  assert(q_push(r[1], 0.0) == 1 && q_push(r[3], 9.0) == 1);
  pthread_mutex_lock(&g_q.mu);
  for (int k = 0; k < g_q.lane[LANE_COLLECT].count; k++)
    if (g_q.lane[LANE_COLLECT].idx[k] == r[1]) g_q.lane[LANE_COLLECT].force_at[k] = time(NULL) - 1;
  pthread_mutex_unlock(&g_q.mu);
  assert(q_pop(LANE_COLLECT) == r[1] && "an entry older than its interval was not taken");
  q_done(r[1]); q_done(r[0]);
  assert(q_pop(LANE_COLLECT) == r[3]); q_done(r[3]);
  printf("  pop: an entry older than its interval is taken regardless\n");

  /* hostgate gap: a host whose gap has not elapsed is not ready, and is passed over */
  const char *u = "https://host-gap.unit.example/x";
  int hg;
  pthread_mutex_lock(&g_q.mu);
  hg = host_intern("host-gap.unit.example");
  g_hcap[hg] = 4; g_hid[r[4]] = hg;
  pthread_mutex_unlock(&g_q.mu);
  assert(hostgate_acquire(u, 1000) == 1); hostgate_release(u);   /* opens the gap */
  assert(q_push(r[4], 0.0) == 1 && q_push(r[5], 3.0) == 1);
  assert(q_pop(LANE_COLLECT) == r[5] && "a host inside its gap was started");
  q_done(r[5]);
  usleep(400 * 1000);                                  /* gap (150 ms) elapsed */
  assert(q_pop(LANE_COLLECT) == r[4]);
  q_done(r[4]);
  printf("  pop: a host inside hostgate's gap is passed over, then taken when ready\n");

  /* learning: a run teaches the registry its host; a loopback host is not learned */
  const source_def *d = g_all[r[5]];
  sched_host_learn(d, "learned.unit.example");
  assert(g_hid[r[5]] && !strcmp(g_hname[g_hid[r[5]]], "learned.unit.example"));
  int before = g_hid[r[5]];
  sched_host_learn(d, "127.0.0.1");
  assert(g_hid[r[5]] == before);
  assert(g_hcap[host_intern("reddit.com")] == 1 && "a paced host must be capped at one worker");
  printf("  learn: first host recorded; loopback ignored; a 30 s-gap host capped at 1\n");
  drain_lanes();
}

static void test_cancel_window(void) {
  int r[1];
  assert(pick_rows(r, 1, 0) == 1);
  int i = r[0];
  int save_d = g_deadline_sec; long save_c = g_cancel_cap_sec;
  sched_state z; memset(&z, 0, sizeof z);
  g_state[i] = z;
  /* a row on a 6 h cadence: window = the cap, not 6 h */
  long iv = sched_row_interval(&g_state[i], g_all[i]);
  g_deadline_sec = 300; g_cancel_cap_sec = 900;
  long w = cancel_after(i);
  assert(iv > 0);
  if (iv >= 900) assert(w == 900 && "the cancel window followed the cadence");
  else assert(w == (iv > 300 ? iv : 300));
  g_state[i].effective_interval = 0;
  /* the cap moves, never below the deadline */
  g_cancel_cap_sec = 120;
  assert(cancel_after(i) == 300 && "the window fell below the deadline");
  g_deadline_sec = 1000; g_cancel_cap_sec = 900;
  assert(cancel_after(i) == 1000 && "an operator's longer deadline was capped");
  /* a row on a short interval is still not cancelled inside it */
  g_deadline_sec = 60; g_cancel_cap_sec = 900;
  for (int k = 0; k < g_n; k++)
    if (g_all[k]->update_interval_sec > 0 && sched_is_pod(g_all[k]) &&
        g_all[k]->update_interval_sec > 60 && g_all[k]->update_interval_sec < 900) {
      assert(cancel_after(k) == (long)g_all[k]->update_interval_sec);   /* pods keep their declared interval */
      break;
    }
  g_deadline_sec = save_d; g_cancel_cap_sec = save_c;
  printf("  cancel window: max(deadline, min(interval, cap)); a 6 h interval gives the 900 s cap\n");
}

/* The OLD dispatcher walk, reproduced: every registered row, every tick, to
 * find the due ones. Same reads, same branches, no queue push. */
static int legacy_walk(time_t *next, time_t now, int *cursor, int *cand, int room0) {
  int nc = 0, first_deferred = -1, deferred = 0;
  int room = room0;
  pthread_mutex_lock(&g_q.mu);
  for (int k = 0; k < g_n; k++) {
    int i = *cursor + k < g_n ? *cursor + k : *cursor + k - g_n;
    const source_def *d = g_all[i];
    if (d->update_interval_sec <= 0 || now < next[i]) continue;
    if (g_q.running[i]) continue;
    if (room <= 0) { deferred++; if (first_deferred < 0) first_deferred = i; continue; }
    room--;
    cand[nc++] = i;
    next[i] = now + 21600;                              /* the old loop re-planned each row it queued */
  }
  pthread_mutex_unlock(&g_q.mu);
  if (first_deferred >= 0) *cursor = first_deferred;
  return nc + deferred;
}

static void test_tick_cost(db_handle *db) {
  int *cand = malloc(sizeof(int) * SCHED_QCAP * LANE_N);
  assert(cand);
  time_t now = time(NULL);
  /* a steady state: rows spread evenly over their 6 h interval, so about
   * n / 21600 of them come due per second (about 4 at 93,000 rows) */
  pthread_mutex_lock(&g_q.mu);
  int sched = 0;
  for (int i = 0; i < g_n; i++)
    if (g_all[i]->update_interval_sec > 0) {
      g_q.next[i] = now + 1 + (time_t)((long)(sched++ % 21600));
      g_q.running[i] = 0;
    }
  g_q.def[0].head = g_q.def[0].n = g_q.def[1].head = g_q.def[1].n = 0;
  memset(g_q.dflag, 0, (size_t)g_n);
  heap_rebuild();
  pthread_mutex_unlock(&g_q.mu);
  drain_lanes();

  const int TICKS = 1000;
  int cursor = 0, due_total = 0, legacy_due = 0;
  time_t *shadow = malloc((size_t)g_n * sizeof(time_t));
  assert(shadow);
  memcpy(shadow, g_q.next, (size_t)g_n * sizeof(time_t));
  double t0 = now_us();
  for (int k = 0; k < TICKS; k++) {
    int c = legacy_walk(shadow, now + k, &cursor, cand, SCHED_QCAP);
    due_total += c; legacy_due += c;
  }
  double legacy_us = (now_us() - t0) / TICKS;

  /* the same 1,000 ticks through the real dispatcher. Its dispatch half
   * (sched_row_benched / source_trust_score) reads the database for each row
   * it queues, which the legacy walk never did, so drain the lane and time
   * the WHOLE tick - an unfavourable comparison for the new one. */
  int new_due = 0;
  double spent = 0;
  for (int k = 0; k < TICKS; k++) {
    double a = now_us();
    sched_tick(db, now + k, cand);
    spent += now_us() - a;
    new_due += lane_count(LANE_COLLECT);
    drain_lanes();
  }
  double new_us = spent / TICKS;

  /* and a tick with nothing due at all, the common case between arrivals */
  park_all(now + 10000000);
  double a = now_us();
  for (int k = 0; k < TICKS; k++) sched_tick(db, now + k, cand);
  double idle_us = (now_us() - a) / TICKS;
  for (int i = 0; i < g_n; i++) shadow[i] = now + 10000000;
  double b = now_us();
  int dummy = 0;
  for (int k = 0; k < TICKS; k++) dummy += legacy_walk(shadow, now + k, &cursor, cand, SCHED_QCAP);
  double legacy_idle_us = (now_us() - b) / TICKS;
  (void)dummy;
  free(shadow);

  printf("  tick cost at %d registered rows (%d scheduled), mean of %d ticks:\n"
         "    steady state (~%.1f rows due a tick):  old whole-registry walk %.1f us   new heap tick %.1f us (incl. queueing)\n"
         "    nothing due:                           old %.1f us   new %.2f us\n",
         g_n, sched, TICKS, (double)new_due / TICKS, legacy_us, new_us, legacy_idle_us, idle_us);
  printf("    (the old walk found %.1f rows due a tick, the new tick %.1f: same arrivals)\n",
         (double)legacy_due / TICKS, (double)new_due / TICKS);
  (void)due_total;
  assert(idle_us < legacy_idle_us && "the heap tick is not cheaper than the registry walk");
  free(cand);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  unsetenv("JO_SCHED_INTERVAL_SEC"); unsetenv("JO_SCHED_MIN_INTERVAL_SEC");
  unsetenv("JO_SCHED_MAX_INTERVAL_SEC");
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  cadence_init(&db);
  sched_policy_load(&g_pol);
  assert(sched_pool_init(registry_all(), registry_count()) == 0);
  g_q.lane[LANE_COLLECT].workers = 1;
  g_q.lane[LANE_MAINT].workers = 1;

  test_heap_ordering();
  test_tick(&db);
  test_host_aware_pop();
  test_cancel_window();
  test_tick_cost(&db);

  db_close(&db);
  printf("test_sched_dispatch: ok\n");
  return 0;
}
