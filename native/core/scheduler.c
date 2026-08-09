/* core/scheduler.c — timer-wheel cron (scheduleGuardedRun parity: per-source
 * interval, skip-if-running). P4: minimal single-thread loop; the bulk waves
 * (P5) reuse it unchanged. */
#include "scheduler.h"
#include "../source.h"
#include "intel.h"
#include "evidence.h"
#include "content_change.h"
#include "maint_detect.h"
#include "hostgate.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

/* Counting sink: wraps the real intel_sink and tallies emit() calls so the
 * scheduler knows records_fetched for fetch_log/detection. Both a NEW row
 * (emit==1) and an UPDATE (emit==0) count; only errors (<0) don't. */
typedef struct { intel_sink *inner; long n; } count_sink;
static int count_emit(intel_sink *s, const intel_item *it) {
  count_sink *cs = (count_sink *)s->ctx;
  int r = cs->inner->emit(cs->inner, it);
  if (r >= 0) cs->n++;
  return r;
}

/* ── run outcome → fetch_log.status ───────────────────────────────────────
 *
 * THIS IS THE ROOT OF "QUARANTINE ON SUCCESS". The line here used to be
 *
 *     const char *status = rc == 0 ? "ok" : "error";
 *
 * and everything downstream is built on it: status != "ok" makes
 * anomaly_detect open a `status_bad` anomaly, the repair pod triages it, and
 * three failed repair cycles trip collector_repair.c's breaker and quarantine
 * the source. Seventeen collectors were individually rewritten to stop
 * returning non-zero on an honest empty result; forty-four still do, and the
 * line that turns their return value into a verdict was never touched. Fixing
 * it here is what makes the per-collector fixes stick and stops the remaining
 * forty-four from benching working sources.
 *
 * `rc` is not, and has never been, a clean success/failure bit. Three
 * conventions are in circulation across the fleet:
 *   `return n >= 0 ? 0 : -1`   the intended one (253 files)
 *   `return n > 0 ? 0 : -1`    "found nothing" reported as failure (44 files)
 *   `return n`                 the row COUNT returned as the status code
 * so a positive rc means "n records", never an error, and is treated as
 * success outright.
 *
 * The genuinely ambiguous case is rc < 0 with zero rows: a quiet hour on a
 * `n > 0 ? 0 : -1` collector and a dead upstream look identical from the
 * return value alone. They do not look identical to the HTTP client the run
 * was handed: it records every distinct host contacted and whether any
 * response from it was 2xx/3xx. If the collector talked to hosts and every one
 * of them answered, the fetch worked and the feed had nothing to give — an
 * empty result, not an outage. If no host answered, or none was contacted at
 * all, we have no evidence the run succeeded and it stays an error.
 *
 * "ok" with records_fetched = 0 is exactly what the seventeen already-fixed
 * collectors produce, so this classifies the other forty-four the same way
 * rather than inventing a new state. A source that normally returns rows and
 * suddenly returns none is still caught — by anomaly_detect's records_drop
 * baseline, which is the check designed for it. */
static const char *run_status(int rc, long records, int hosts, int hosts_ok,
                              const char **why) {
  *why = NULL;
  if (rc >= 0) return "ok";
  if (records > 0) return "ok";        /* rows landed; the code is the count-idiom misfiring */
  if (hosts > 0 && hosts_ok == hosts) return "ok";   /* fetched fine, feed was empty */
  *why = (hosts > 0) ? "run returned non-zero; no host answered"
                     : "run returned non-zero";
  return "error";
}

int scheduler_run_source(db_handle *db, const source_def *d,
                         const char *entity) {
  intel_sink inner = intel_sink_make(db, d->id, "legacy");
  count_sink cs = { .inner = &inner, .n = 0 };
  intel_sink sink = { .ctx = &cs, .emit = count_emit };
  volatile int cancel = 0;
  http_client *http = http_client_new();   /* sources expect ctx->http set */
  llm_client llm; llm_init(&llm, http);
  source_ctx ctx = {0};
  ctx.source_id = d->id;
  ctx.entity = entity;
  ctx.db = db;
  ctx.http = http;
  ctx.llm = &llm;
  ctx.cancel = &cancel;

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  /* Bind the evidence scope (roadmap 17) around the collector run. httpclient.c
   * has no db_handle or source_id of its own, so its capture hook reads them
   * from this thread-local scope — and fails closed when none is bound. */
  evidence_scope_begin(db, d->id, NULL);
  /* Same thread-local pattern for content-change detection (roadmap 26): the
   * httpclient hook needs a db_handle and source_id it cannot otherwise see. */
  content_change_scope_begin(db, d->id);
  int rc = d->run(&ctx, &sink);
  content_change_scope_end();
  evidence_scope_end();
  clock_gettime(CLOCK_MONOTONIC, &t1);
  long duration_ms = (t1.tv_sec - t0.tv_sec) * 1000L +
                     (t1.tv_nsec - t0.tv_nsec) / 1000000L;
  /* Read the transport evidence BEFORE the client is freed. */
  int hosts = http_client_host_count(http), hosts_ok = 0;
  for (int i = 0; i < hosts; i++) {
    int ok = 0;
    if (http_client_host_at(http, i, NULL, &ok) && ok) hosts_ok++;
  }
  http_client_free(http);
  intel_sink_free(&inner);        /* make() heap-allocates; nothing freed it */
  fprintf(stderr, "[sched] %s run rc=%d records=%ld %ldms\n",
          d->id, rc, cs.n, duration_ms);

  /* Stage 0+1: log the run and detect anomalies — but only for real data
   * collectors. The internal pods (_maint, _enrich) emit nothing and would
   * otherwise trip duration_outlier on their own LLM calls. */
  if (d->collector && d->collector[0] != '_') {
    const char *why = NULL;
    const char *status = run_status(rc, cs.n, hosts, hosts_ok, &why);
    long flid = fetch_log_write(db, d->id, status, (int)cs.n, duration_ms, why);
    anomaly_detect(db, d->id, flid, status, (int)cs.n, duration_ms);
  }
  return rc;
}

/* sources.quarantined_until in the future → the circuit breaker has this source
 * benched; the scheduler must not probe it (mirrors Node's quarantine skip). */
static int is_quarantined(db_handle *db, const char *id) {
  sqlite3_stmt *s; int q = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sources WHERE id=?1 AND quarantined_until IS NOT NULL"
        " AND quarantined_until>datetime('now') LIMIT 1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    q = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }
  return q;
}

/* sources.schedule_mode for `id` (default 'map_cron' if row/col absent).
 * Returns 1 if search_only (scheduler must NOT run it for the map). */
static int is_search_only(db_handle *db, const char *id) {
  sqlite3_stmt *s; int so = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT schedule_mode FROM sources WHERE id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s,0)!=SQLITE_NULL)
      so = strcmp((const char *)sqlite3_column_text(s,0), "search_only") == 0;
    sqlite3_finalize(s);
  }
  return so;
}

/* ── worker pool ──────────────────────────────────────────────────────────
 *
 * WHY. This loop used to call scheduler_run_source() inline, so all 361
 * scheduled sources shared one thread. The audit measured single Overpass
 * collectors at 100-162 s and 73 portal probes at ~8 s each; one of those
 * blocked the ENTIRE fleet's cadence behind it. The 12 sources declaring
 * update_interval_sec=60 (plane-adsb, unified-flights, the EEW feeds) could
 * never actually refresh at 60 s no matter what they asked for, because the
 * dispatcher had to walk past everything else first. Freshness was
 * structurally unreachable, not merely slow.
 *
 * SHAPE. One dispatcher thread decides WHAT is due and hands ids to a bounded
 * queue; N workers run them. A slow source now occupies one of N slots
 * instead of the whole fleet.
 *
 * WHY EACH WORKER OWNS ITS DB HANDLE. Unchanged from the reason the loop got
 * its own connection in the first place: a sqlite transaction belongs to a
 * connection, not a thread, and core/intel.c wraps every emit in BEGIN/COMMIT.
 * Sharing one handle across workers would interleave their transactions and
 * let one worker's ROLLBACK discard another's committed-looking rows. WAL +
 * busy_timeout (db_attach) is what makes the concurrent writes safe; the
 * timeout is raised here because N writers contend more than one did.
 *
 * SKIP-IF-RUNNING. Serial execution gave this for free — next[i] was only
 * bumped after the run returned, so a source could not overlap itself. With a
 * pool that has to be explicit, or a 3600 s source taking 4000 s would be
 * queued again while still running, and a wedged source would flood the queue.
 *
 * THREAD SAFETY OF THE PER-RUN HOOKS. scheduler_run_source binds an evidence
 * scope and a content-change scope around run(); both are `__thread` in
 * evidence.c and content_change.c, so each worker gets its own. That was
 * already true — it is the reason this port is a pool and not a rewrite.  */

#define SCHED_QCAP 2048

typedef struct {
  int          idx[SCHED_QCAP];      /* registry indices awaiting a worker    */
  int          head, tail, count;
  char        *running;              /* per-source: queued or in flight       */
  time_t      *started;              /* wall-clock start, 0 = not running     */
  pthread_mutex_t mu;
  pthread_cond_t  cv;
} sched_queue;

static sched_queue    g_q;
static const source_def **g_all;
static int            g_n;
static int            g_deadline_sec = 0;
/* Shutdown coordination — see scheduler_stop_background(). g_inflight counts
 * collector runs currently executing, not queued ones. */
static atomic_int     g_shutdown = 0;
static atomic_int     g_inflight = 0;

static long g_q_dropped;      /* enqueues refused because the queue was full */

static void q_push(int i) {
  pthread_mutex_lock(&g_q.mu);
  if (g_q.running[i]) {
    /* Already queued or in flight. Serial execution gave this for free (next[i]
     * only advanced after the run returned); with a pool it has to be explicit
     * or a source slower than its own interval re-queues itself forever. Not a
     * drop — the run in flight IS this cycle's refresh. */
  } else if (g_q.count >= SCHED_QCAP) {
    /* Backlog deeper than the queue: this source loses a cycle. Counted and
     * logged rather than swallowed — a silently truncated refresh reads as
     * "the source is stale" with no way to tell it apart from a broken
     * upstream. A standing nonzero count means the pool is undersized. */
    g_q_dropped++;
    fprintf(stderr, "[sched] queue full (%d), skipping this cycle of %s "
                    "(dropped=%ld) — raise JO_SCHED_WORKERS\n",
            SCHED_QCAP, g_all[i]->id, g_q_dropped);
  } else {
    g_q.running[i] = 1;
    g_q.idx[g_q.tail] = i;
    g_q.tail = (g_q.tail + 1) % SCHED_QCAP;
    g_q.count++;
    pthread_cond_signal(&g_q.cv);
  }
  pthread_mutex_unlock(&g_q.mu);
}

static int q_pop(void) {
  pthread_mutex_lock(&g_q.mu);
  while (g_q.count == 0) pthread_cond_wait(&g_q.cv, &g_q.mu);
  int i = g_q.idx[g_q.head];
  g_q.head = (g_q.head + 1) % SCHED_QCAP;
  g_q.count--;
  g_q.started[i] = time(NULL);
  pthread_mutex_unlock(&g_q.mu);
  return i;
}

static void q_done(int i) {
  pthread_mutex_lock(&g_q.mu);
  g_q.running[i] = 0;
  g_q.started[i] = 0;
  pthread_mutex_unlock(&g_q.mu);
}

static void *worker_thread(void *arg) {
  (void)arg;
  db_handle own = {0};
  if (db_attach(&own, NULL) != 0) {
    fprintf(stderr, "[sched] worker cannot open its own DB connection; exiting\n");
    return NULL;
  }
  /* N concurrent writers contend for the WAL write lock far more than one
   * did; 5 s (db_attach's default) starts returning SQLITE_BUSY on a busy
   * fleet, which surfaces as a spurious emit failure. */
  sqlite3_exec(own.h, "PRAGMA busy_timeout=30000;", NULL, NULL, NULL);
  for (;;) {
    int i = q_pop();
    if (atomic_load(&g_shutdown)) { q_done(i); break; }
    atomic_fetch_add(&g_inflight, 1);
    scheduler_run_source(&own, g_all[i], NULL);
    atomic_fetch_sub(&g_inflight, 1);
    q_done(i);
  }
  db_close(&own);
  return NULL;
}

/* Shutdown drain.
 *
 * Workers are detached and scheduler_loop never returns, so there is nothing
 * to join. That was survivable until process exit started racing them:
 * atexit's OPENSSL_cleanup frees the CRYPTO lock objects while ~8-16 workers
 * are still inside curl_easy_perform, and TSan caught it as four
 * heap-use-after-frees ending in a SEGV in CRYPTO_THREAD_write_lock. A clean
 * shutdown could therefore exit by signal instead of 0, and a worker could be
 * torn down mid-emit().
 *
 * This does not pretend to join. It stops handing out new work and waits a
 * bounded time for in-flight fetches to land, which is the part that matters:
 * an emit() that has begun gets to finish its transaction. A worker parked in
 * a multi-second network call may still be running when the wait expires —
 * that is why main() must also skip the atexit teardown rather than rely on
 * this alone. */
void scheduler_stop_background(int wait_ms) {
  atomic_store(&g_shutdown, 1);
  const int step = 25;
  for (int waited = 0; waited < wait_ms; waited += step) {
    if (atomic_load(&g_inflight) == 0) break;
    struct timespec ts = { step / 1000, (long)(step % 1000) * 1000000L };
    nanosleep(&ts, NULL);
  }
  int left = atomic_load(&g_inflight);
  if (left > 0)
    fprintf(stderr, "[sched] shutdown: %d collector run(s) still in flight "
                    "after %d ms; exiting without waiting further\n",
            left, wait_ms);
}

/* Reports sources that have outrun the deadline. There is no safe hard kill:
 * only 25 of 585 sources poll ctx->cancel, and no lib/ helper does, so a
 * cooperative cancel cannot be claimed as a guarantee — and cancelling a
 * thread mid-transaction would corrupt exactly what the per-worker connection
 * is there to protect. The real containment is the pool itself (an overrun
 * costs one slot, not the fleet); this makes the overrun visible and feeds
 * the operator the id to quarantine. Per-request ceilings still come from
 * httpclient's CURLOPT_TIMEOUT_MS and lib/overpass.c's own budget. */
static void watchdog_scan(void) {
  if (g_deadline_sec <= 0) return;
  time_t now = time(NULL);
  pthread_mutex_lock(&g_q.mu);
  for (int i = 0; i < g_n; i++) {
    if (!g_q.started[i]) continue;
    long over = (long)(now - g_q.started[i]);
    if (over > g_deadline_sec)
      fprintf(stderr, "[sched] WARN %s has held a worker slot for %lds "
                      "(deadline %ds)\n", g_all[i]->id, over, g_deadline_sec);
  }
  pthread_mutex_unlock(&g_q.mu);
}

void scheduler_loop(db_handle *db) {
  g_all = registry_all();
  g_n   = registry_count();
  int n = g_n;
  time_t *next = calloc(n, sizeof(time_t));
  g_q.running = calloc(n, 1);
  g_q.started = calloc(n, sizeof(time_t));
  if (!next || !g_q.running || !g_q.started) {
    fprintf(stderr, "[sched] out of memory; scheduler off\n");
    return;
  }
  pthread_mutex_init(&g_q.mu, NULL);
  pthread_cond_init(&g_q.cv, NULL);

  /* Measured on this fleet, 60 s window, boot stagger disabled so worker count
   * was the only variable: 1 worker → 5 runs (a single 55.8 s `5g-coverage`
   * consumed the whole window and the fleet never got past "a"); 8 → 63 runs;
   * 16 → 97 runs. The work is network-bound, not CPU-bound, so the ceiling is
   * upstream politeness (hostgate) and sqlite write contention, not cores. 8
   * is the conservative default for a first deploy; raise it once the
   * heartbeat below shows a standing backlog. */
  int workers = 8;
  const char *w = getenv("JO_SCHED_WORKERS");
  if (w) workers = atoi(w);
  if (workers < 1)  workers = 1;      /* 1 = the old serial behaviour, exactly */
  if (workers > 32) workers = 32;
  const char *dl = getenv("JO_SCHED_DEADLINE_SEC");
  g_deadline_sec = dl ? atoi(dl) : 300;

  time_t now0 = time(NULL);
  /* Stagger first runs so the fleet doesn't storm upstreams at boot (many do
   * nationwide tiled Overpass). With `workers` running at once the old 3 s
   * spacing would still admit `workers` sources per 3 s window, so scale the
   * step: one source per worker per 3 s keeps boot as gentle as it was.
   *
   * The old flat 3 s-per-source ramp was its own freshness bug: at 1999
   * registered sources it took ~100 minutes for the fleet to become due even
   * ONCE after a restart, so a 60 s source was 60 s only in principle. The
   * per-host gate (core/hostgate.h) is now what enforces politeness, which is
   * what makes a tighter ramp safe — it throttles by upstream rather than by
   * position in the registry. JO_SCHED_STAGGER_SEC=0 disables the ramp. */
  int stag = 3;
  const char *sg = getenv("JO_SCHED_STAGGER_SEC");
  if (sg) stag = atoi(sg);
  if (stag < 0) stag = 0;
  for (int i = 0; i < n; i++) next[i] = now0 + (time_t)(i / workers) * stag;

  for (int i = 0; i < workers; i++) {
    pthread_t t;
    if (pthread_create(&t, NULL, worker_thread, NULL) == 0) pthread_detach(t);
    else fprintf(stderr, "[sched] worker %d failed to start\n", i);
  }
  fprintf(stderr, "[sched] %d sources registered, %d workers\n", n, workers);

  unsigned long tick = 0;
  for (;;) {
    time_t now = time(NULL);
    for (int i = 0; i < n; i++) {
      const source_def *d = g_all[i];
      /* Unified: schedule any source with an interval; on-demand sources
       * (interval<=0, e.g. OSINT-pivot) are run by the pipeline, not here.
       * No source_kind branch — a source is a source. */
      if (d->update_interval_sec <= 0) continue;
      if (now < next[i]) continue;
      /* User-set per-source mode: search_only sources don't cron for the
       * map (the search tab triggers them on demand). */
      if (is_search_only(db, d->id)) { next[i] = now + d->update_interval_sec; continue; }
      /* Circuit breaker: a quarantined source is benched until the cooldown
       * lapses (a later probe past quarantined_until runs and may clear it). */
      if (is_quarantined(db, d->id)) { next[i] = now + d->update_interval_sec; continue; }
      /* Cadence is measured from the DUE time, not from completion: a source
       * that takes 90 s on a 60 s interval stays due immediately rather than
       * silently drifting to 150 s. q_push's skip-if-running is what stops
       * that from queueing it twice. */
      q_push(i);
      next[i] = now + d->update_interval_sec;
    }
    watchdog_scan();
    /* Once a minute: how much of the pool is busy, how deep the backlog is,
     * and how often the host gate had to make a worker wait. Without this the
     * only symptom of an undersized pool or a throttling upstream is "the map
     * feels stale", which is what made the serial loop survive this long. */
    if (++tick % 60 == 0) {
      long waits = 0, timeouts = 0, inflight = 0;
      hostgate_counters(&waits, &timeouts, &inflight);
      pthread_mutex_lock(&g_q.mu);
      int queued = g_q.count, busy = 0;
      for (int i = 0; i < n; i++) if (g_q.started[i]) busy++;
      pthread_mutex_unlock(&g_q.mu);
      fprintf(stderr, "[sched] busy=%d/%d queued=%d | hostgate inflight=%ld "
                      "waited=%ld over_budget=%ld\n",
              busy, workers, queued, inflight, waits, timeouts);
    }
    sleep(1);
  }
}

/* Runs on its OWN connection. A sqlite transaction belongs to a connection,
 * not a thread, so sharing the server's handle meant this loop's BEGIN/COMMIT
 * around every emit() interleaved with the HTTP thread's transactions — one
 * side's ROLLBACK could discard the other's committed-looking work. */
static void *sched_thread(void *arg) {
  (void)arg;
  db_handle own = {0};
  if (db_attach(&own, NULL) != 0) {
    fprintf(stderr, "[sched] cannot open its own DB connection; scheduler off\n");
    return NULL;
  }
  scheduler_loop(&own);               /* infinite; lives for process lifetime */
  db_close(&own);
  return NULL;
}

void scheduler_start_background(db_handle *db) {
  if (getenv("JO_NO_SCHED")) {
    fprintf(stderr, "[sched] background scheduler disabled (JO_NO_SCHED)\n");
    return;
  }
  pthread_t t;
  if (pthread_create(&t, NULL, sched_thread, db) == 0) {
    pthread_detach(t);
    fprintf(stderr, "[sched] background scheduler started (serve + refresh)\n");
  } else {
    fprintf(stderr, "[sched] pthread_create failed; serving without scheduler\n");
  }
}
