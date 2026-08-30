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
#include "source_trust.h"
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
typedef struct { intel_sink *inner; long n; long notices; } count_sink;
/* A `*-notice` record (collector-truncation-notice, collector-shape-notice)
 * is data about the run, not a record OF the source: it is tallied apart so
 * a source that stored nothing but its own notice still reads records=0. */
static int is_notice_record(const intel_item *it) {
  const char *rt = it ? it->record_type : NULL;
  size_t n = rt ? strlen(rt) : 0;
  return n >= 7 && strcmp(rt + n - 7, "-notice") == 0;
}
static int count_emit(intel_sink *s, const intel_item *it) {
  count_sink *cs = (count_sink *)s->ctx;
  int r = cs->inner->emit(cs->inner, it);
  if (r >= 0) { if (is_notice_record(it)) cs->notices++; else cs->n++; }
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

/* fetch_log.stored — house rule 4b's number, kept next to records_fetched so
 * the two can be compared over history rather than only in the run a human
 * happened to be watching. The column is added by db.c's ensure_column()
 * block; it is NULL on rows written before it existed, and NULL is the honest
 * value there ("not measured"), which is why it has no DEFAULT 0.
 *
 * WHY A FOLLOW-UP UPDATE and not a sixth argument to fetch_log_write().
 * fetch_log_write() is the shared stage-0 API in core/maint_detect.c and its
 * signature is quoted in seven header files as the thing scheduler.c skips for
 * `_maint` collectors; widening it would touch every one of them for a column
 * only this caller can supply. This is one UPDATE by INTEGER PRIMARY KEY on a
 * row inserted microseconds earlier — an rowid seek, still inside the same
 * page cache. If the column is absent (a database that has not booted through
 * db_open, e.g. a unit-test fixture) the prepare fails and we say nothing:
 * the run line already carried the number, and a missing migration is not a
 * reason to make a collector run look failed.
 *
 * A floor (`exact` = 0) is stored NEGATED. There is no room in an INTEGER
 * column for "at least", and writing the floor as though it were the truth
 * would be exactly the quiet over-reporting rule 4b exists to stop; a reader
 * seeing stored < 0 knows the real value is >= -stored. */
static void fetch_log_set_stored(db_handle *db, long flid, long stored,
                                 int exact) {
  if (!db || !db->h || flid < 0 || stored < 0) return;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "UPDATE fetch_log SET stored=?1 WHERE id=?2",
                         -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_int64(s, 1, (sqlite3_int64)(exact ? stored : -stored));
  sqlite3_bind_int64(s, 2, (sqlite3_int64)flid);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* ── health-driven scheduling: backoff, quarantine, priority ─────────────
 *
 * WHY. The dispatcher used to treat every scheduled source identically: due
 * every `update_interval_sec`, forever, whatever happened last time. A source
 * whose host has been unreachable for a month was still fetched every 60 s,
 * occupying a worker slot and a hostgate token each time, and the only thing
 * that could ever bench it was the LLM repair pod's breaker — which needs an
 * anomaly, a triage call and three failed repair cycles before it acts, and
 * cannot act at all when the LLM is unconfigured.
 *
 * WHAT COUNTS AS A FAILURE. Exactly what run_status() calls "error": rc<0 with
 * no records and no host that answered. An honest empty (status "ok",
 * records=0) is NOT a failure — the fetch worked, the feed had nothing — so
 * it does not back the source off. It is counted separately
 * (consecutive_empties) because a source that has been empty for 200 runs is
 * the EMPTY_RESULTSET class from CLAUDE.md and someone should see the number.
 *
 * THE ARITHMETIC. effective_interval starts at the declared interval. Each
 * failure doubles it, capped at JO_SCHED_BACKOFF_CAP_SEC (86400): a 60 s
 * source that dies is retried at 120, 240, ... 61440, 86400 s. A success
 * resets it to the declared interval in one step — an upstream that has come
 * back does not deserve to be punished for the outage. After
 * JO_SCHED_QUARANTINE_AFTER (12) consecutive failures the source is
 * quarantined: it is probed once per JO_SCHED_QUARANTINE_PROBE_SEC (7 days)
 * and released by the first successful probe. Both counters are persisted so
 * a restart does not turn 11 failures back into 0. */

static long env_long(const char *name, long dflt, long lo) {
  const char *v = getenv(name);
  if (!v || !*v) return dflt;
  long x = atol(v);
  return x < lo ? lo : x;
}

void sched_policy_load(sched_policy *p) {
  p->backoff_cap_sec      = env_long("JO_SCHED_BACKOFF_CAP_SEC", 86400, 1);
  p->quarantine_after     = (int)env_long("JO_SCHED_QUARANTINE_AFTER", 12, 1);
  p->quarantine_probe_sec = env_long("JO_SCHED_QUARANTINE_PROBE_SEC",
                                     7L * 86400L, 1);
}

void sched_state_apply(sched_state *st, const sched_policy *p, long declared,
                       const char *status, long records, time_t now) {
  if (declared < 1) declared = 1;
  if (st->effective_interval < declared) st->effective_interval = declared;
  int failed = !(status && strcmp(status, "ok") == 0);
  if (st->quarantined) st->last_probe = now;

  if (failed) {
    st->consecutive_failures++;
    /* Double from the CURRENT effective interval, never below declared and
     * never above the cap — unless the declared interval already exceeds the
     * cap, in which case the cap must not SHORTEN a source's cadence. */
    long cap = p->backoff_cap_sec > declared ? p->backoff_cap_sec : declared;
    long next = st->effective_interval > cap / 2 ? cap : st->effective_interval * 2;
    st->effective_interval = next;
    st->backoff_until = now + next;
    if (!st->quarantined && st->consecutive_failures >= p->quarantine_after) {
      st->quarantined = 1;
      st->quarantined_at = now;
      st->last_probe = now;
    }
    return;
  }
  /* "ok": the fetch worked. That is the proof of life quarantine was waiting
   * for, whether or not the feed had rows to give. */
  st->consecutive_failures = 0;
  st->effective_interval = declared;
  st->backoff_until = 0;
  if (st->quarantined) { st->quarantined = 0; st->quarantined_at = 0; st->last_probe = 0; }
  if (records > 0) st->consecutive_empties = 0;
  else             st->consecutive_empties++;
}

long sched_state_interval(const sched_state *st, const sched_policy *p,
                          long declared) {
  if (declared < 1) declared = 1;
  if (st->quarantined) return p->quarantine_probe_sec;
  return st->effective_interval > declared ? st->effective_interval : declared;
}

/* Lower key runs first. Three bands that never interleave — healthy sources
 * always beat backed-off ones, which always beat quarantine probes — and
 * inside a band: better trust score first, then the MOST overdue first, so a
 * healthy source whose slot was stolen by a slow fleet catches up before a
 * healthy source that just became due. Unrated (score<0) sits in the middle
 * of its band: no evidence is not evidence of badness (source_trust.h). */
double sched_priority(const sched_state *st, double score, double overdue_ratio) {
  double band = st->quarantined ? 20.0 : (st->consecutive_failures > 0 ? 10.0 : 0.0);
  double s = score < 0 ? 0.5 : (score > 1.0 ? 1.0 : score);
  if (overdue_ratio < 0) overdue_ratio = 0;
  if (overdue_ratio > 4) overdue_ratio = 4;   /* bounded so it stays inside the band */
  return band + (1.0 - s) * 4.0 - overdue_ratio;
}

int sched_state_load_one(db_handle *db, const char *id, sched_state *st) {
  memset(st, 0, sizeof *st);
  if (!db || !db->h) return -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT consecutive_failures,consecutive_empties,effective_interval,"
        "backoff_until,quarantined,quarantined_at,last_probe "
        "FROM source_sched_state WHERE source_id=?1", -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  int rc = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    st->consecutive_failures = sqlite3_column_int(s, 0);
    st->consecutive_empties  = sqlite3_column_int(s, 1);
    st->effective_interval   = (long)sqlite3_column_int64(s, 2);
    st->backoff_until        = (time_t)sqlite3_column_int64(s, 3);
    st->quarantined          = sqlite3_column_int(s, 4);
    st->quarantined_at       = (time_t)sqlite3_column_int64(s, 5);
    st->last_probe           = (time_t)sqlite3_column_int64(s, 6);
    rc = 1;
  }
  sqlite3_finalize(s);
  return rc;
}

int sched_state_save(db_handle *db, const char *id, const sched_state *st,
                     long declared, time_t now) {
  if (!db || !db->h) return -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO source_sched_state(source_id,consecutive_failures,"
        "consecutive_empties,declared_interval,effective_interval,backoff_until,"
        "quarantined,quarantined_at,last_probe,updated_at) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10) ON CONFLICT(source_id) DO UPDATE SET "
        "consecutive_failures=excluded.consecutive_failures,"
        "consecutive_empties=excluded.consecutive_empties,"
        "declared_interval=excluded.declared_interval,"
        "effective_interval=excluded.effective_interval,"
        "backoff_until=excluded.backoff_until,quarantined=excluded.quarantined,"
        "quarantined_at=excluded.quarantined_at,last_probe=excluded.last_probe,"
        "updated_at=excluded.updated_at", -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text (s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int  (s, 2, st->consecutive_failures);
  sqlite3_bind_int  (s, 3, st->consecutive_empties);
  sqlite3_bind_int64(s, 4, (sqlite3_int64)declared);
  sqlite3_bind_int64(s, 5, (sqlite3_int64)st->effective_interval);
  if (st->backoff_until) sqlite3_bind_int64(s, 6, (sqlite3_int64)st->backoff_until);
  else                   sqlite3_bind_null(s, 6);
  sqlite3_bind_int  (s, 7, st->quarantined);
  if (st->quarantined_at) sqlite3_bind_int64(s, 8, (sqlite3_int64)st->quarantined_at);
  else                    sqlite3_bind_null(s, 8);
  if (st->last_probe) sqlite3_bind_int64(s, 9, (sqlite3_int64)st->last_probe);
  else                sqlite3_bind_null(s, 9);
  sqlite3_bind_int64(s, 10, (sqlite3_int64)now);
  int rc = sqlite3_step(s) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(s);
  return rc;
}

/* Fold a finished run into the persisted state. Only scheduled runs of real
 * collectors (not entity pivots — a pivot that finds nothing about one entity
 * says nothing about the upstream's health, and not the _maint/_enrich pods).
 * Failure to persist is logged, not fatal: the run itself succeeded or not
 * on its own terms, and a database without the table (a unit-test fixture
 * booted without schema.sql) must not make it look failed. */
static void sched_state_record(db_handle *db, const source_def *d,
                               const char *status, long records) {
  if (!d || d->update_interval_sec <= 0) return;
  sched_policy pol; sched_policy_load(&pol);
  sched_state st;
  time_t now = time(NULL);
  if (sched_state_load_one(db, d->id, &st) < 0) return;   /* no table: say nothing */
  int was_q = st.quarantined;
  sched_state_apply(&st, &pol, d->update_interval_sec, status, records, now);
  if (sched_state_save(db, d->id, &st, d->update_interval_sec, now) != 0)
    fprintf(stderr, "[sched] %s: could not persist sched state\n", d->id);
  if (st.consecutive_failures > 0)
    fprintf(stderr, "[sched] %s backoff: %d consecutive failure(s), next in %lds%s\n",
            d->id, st.consecutive_failures, st.effective_interval,
            st.quarantined && !was_q ? " — QUARANTINED (health)" : "");
  else if (was_q)
    fprintf(stderr, "[sched] %s released from health quarantine\n", d->id);
}

int scheduler_run_source(db_handle *db, const source_def *d,
                         const char *entity) {
  intel_sink inner = intel_sink_make(db, d->id, "legacy");
  count_sink cs = { .inner = &inner, .n = 0, .notices = 0 };
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
  /* Read the distinct-row count BEFORE the sink is freed — it lives in the
   * sink_state that free() is about to release. */
  int stored_exact = 1;
  long stored = intel_sink_stored(&inner, &stored_exact);
  intel_sink_free(&inner);        /* make() heap-allocates; nothing freed it */

  /* ── the run line, and why `stored=` sits at the END of it ───────────────
   *
   * Rule 4b: `records=` counts emit() calls, `stored=` counts the distinct
   * rows those calls actually left behind. When they differ, the difference
   * IS the finding — 12,648 emitted onto 31 rows is 12,617 records discarded
   * inside our own sink, with rc=0 and a run that looked healthy. The number
   * exists in fetch_log too, but a number that only appears in a database is
   * a number nobody reads, so it is stated here on every run.
   *
   * It is appended after the duration rather than next to `records=` for a
   * boring compatibility reason: eight parsers in tests/audit/ and tools/
   * match `records=(-?\d+) (\d+)ms` as ONE unit, and inserting a field
   * between those two groups would silently stop every one of them matching —
   * which is precisely the class of quiet failure this line exists to expose.
   * Appending is invisible to all of them.
   *
   * `stored=?` means the sink could not tell us (not an intel sink);
   * `stored>=N` means N is a floor because the counter hit its ceiling. */
  char sbuf[64], note[192];
  if (stored < 0) snprintf(sbuf, sizeof sbuf, "stored=?");
  else snprintf(sbuf, sizeof sbuf, "stored=%s%ld", stored_exact ? "" : ">=", stored);
  note[0] = '\0';
  if (stored >= 0 && stored_exact && stored < cs.n)
    snprintf(note, sizeof note,
             " UID-COLLISION: %ld of %ld emitted records collapsed onto a uid"
             " already written this run", cs.n - stored, cs.n);
  /* `notices=` trails `stored=` for the same parser-compatibility reason. */
  char nbuf[32] = "";
  if (cs.notices > 0) snprintf(nbuf, sizeof nbuf, " notices=%ld", cs.notices);
  fprintf(stderr, "[sched] %s run rc=%d records=%ld %ldms %s%s%s\n",
          d->id, rc, cs.n, duration_ms, sbuf, nbuf, note);

  /* Stage 0+1: log the run and detect anomalies — but only for real data
   * collectors. The internal pods (_maint, _enrich) emit nothing and would
   * otherwise trip duration_outlier on their own LLM calls. */
  if (d->collector && d->collector[0] != '_') {
    const char *why = NULL;
    const char *status = run_status(rc, cs.n, hosts, hosts_ok, &why);
    long flid = fetch_log_write(db, d->id, status, (int)cs.n, duration_ms, why);
    anomaly_detect(db, d->id, flid, status, (int)cs.n, duration_ms);
    fetch_log_set_stored(db, flid, stored, stored_exact);
    if (!entity) sched_state_record(db, d, status, cs.n);
  }
  return rc;
}

/* sources.quarantined_until in the future → the circuit breaker has this source
 * benched; the scheduler must not probe it (mirrors Node's quarantine skip).
 *
 * Non-static because the ENTITY-PIVOT path needs the same answer. Enforcing
 * this only in scheduler_loop() meant a source the breaker had benched stayed
 * fully reachable through POST /api/search — and since the pivot path also
 * skipped anomaly_detect, pivot traffic could never bench it in the first
 * place. Two halves of one loop; sched_is_quarantined() closes both.
 *
 * is_search_only() deliberately stays static and scheduler-only: a search-only
 * source is one that EXISTS to be dispatched, so applying it here would refuse
 * exactly the sources the pivot path is for. */
static int is_quarantined_impl(db_handle *db, const char *id) {
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

int sched_is_quarantined(db_handle *db, const char *id) {
  return is_quarantined_impl(db, id);
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

/* PRIORITY, NOT FIFO. The ring used to hand sources to workers in the order
 * the dispatcher walked the registry, so a healthy 60 s feed queued behind
 * 200 dead hosts waited for every one of their connect timeouts. The queue
 * is now an unordered set with a key per entry (sched_priority): pop takes the
 * lowest key. A linear scan of at most SCHED_QCAP entries per pop is ~2 µs
 * against runs that take seconds; a heap would buy nothing legible. */
typedef struct {
  int          idx[SCHED_QCAP];      /* registry indices awaiting a worker    */
  double       key[SCHED_QCAP];      /* sched_priority() at push time         */
  int          count;
  char        *running;              /* per-source: queued or in flight       */
  time_t      *started;              /* wall-clock start, 0 = not running     */
  pthread_mutex_t mu;
  pthread_cond_t  cv;
} sched_queue;

static sched_queue    g_q;
static const source_def **g_all;
static int            g_n;
static int            g_deadline_sec = 0;
/* Per-source health state, index-aligned with g_all. Written by the worker
 * that finished the run (under g_q.mu), read by the dispatcher. */
static sched_state   *g_state;
static sched_policy   g_pol;
/* Shutdown coordination — see scheduler_stop_background(). g_inflight counts
 * collector runs currently executing, not queued ones. */
static atomic_int     g_shutdown = 0;
static atomic_int     g_inflight = 0;

static long g_q_dropped;      /* enqueues refused because the queue was full */

static void q_push(int i, double key) {
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
    g_q.idx[g_q.count] = i;
    g_q.key[g_q.count] = key;
    g_q.count++;
    pthread_cond_signal(&g_q.cv);
  }
  pthread_mutex_unlock(&g_q.mu);
}

static int q_pop(void) {
  pthread_mutex_lock(&g_q.mu);
  while (g_q.count == 0) pthread_cond_wait(&g_q.cv, &g_q.mu);
  int best = 0;
  for (int k = 1; k < g_q.count; k++)
    if (g_q.key[k] < g_q.key[best]) best = k;
  int i = g_q.idx[best];
  /* swap-remove: order inside the array carries no meaning, the key does */
  g_q.count--;
  g_q.idx[best] = g_q.idx[g_q.count];
  g_q.key[best] = g_q.key[g_q.count];
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
    /* scheduler_run_source persisted the new health state; bring the
     * dispatcher's in-memory copy up to date before releasing the slot so the
     * next due-check already sees the backed-off interval. */
    sched_state fresh;
    if (sched_state_load_one(&own, g_all[i]->id, &fresh) >= 0) {
      pthread_mutex_lock(&g_q.mu);
      g_state[i] = fresh;
      pthread_mutex_unlock(&g_q.mu);
    }
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
  g_state     = calloc(n, sizeof(sched_state));
  if (!next || !g_q.running || !g_q.started || !g_state) {
    fprintf(stderr, "[sched] out of memory; scheduler off\n");
    return;
  }
  pthread_mutex_init(&g_q.mu, NULL);
  pthread_cond_init(&g_q.cv, NULL);
  sched_policy_load(&g_pol);
  /* Health state survives restarts: a source that was on its 11th failure
   * yesterday is on its 11th failure now, not its 0th. */
  int n_restored = 0, n_q0 = 0;
  for (int i = 0; i < n; i++)
    if (sched_state_load_one(db, g_all[i]->id, &g_state[i]) > 0) {
      n_restored++;
      if (g_state[i].quarantined) n_q0++;
    }
  fprintf(stderr, "[sched] health: restored state for %d source(s), %d in "
                  "health quarantine; backoff cap %lds, quarantine after %d "
                  "failures, probe every %lds\n",
          n_restored, n_q0, g_pol.backoff_cap_sec, g_pol.quarantine_after,
          g_pol.quarantine_probe_sec);

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
  for (int i = 0; i < n; i++) {
    next[i] = now0 + (time_t)(i / workers) * stag;
    /* A restored backoff is honoured across the restart: the outage did not
     * end because the process did. A quarantined source waits out its probe
     * cadence from the last probe, not from boot. */
    const sched_state *st = &g_state[i];
    time_t hold = 0;
    if (st->quarantined && st->last_probe > 0)
      hold = st->last_probe + (time_t)g_pol.quarantine_probe_sec;
    else if (st->backoff_until > 0)
      hold = st->backoff_until;
    if (hold > next[i]) next[i] = hold;
  }

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
      if (is_quarantined_impl(db, d->id)) { next[i] = now + d->update_interval_sec; continue; }
      /* Cadence is measured from the DUE time, not from completion: a source
       * that takes 90 s on a 60 s interval stays due immediately rather than
       * silently drifting to 150 s. q_push's skip-if-running is what stops
       * that from queueing it twice. The interval is the HEALTH-ADJUSTED one:
       * declared when healthy, doubled per failure, the probe cadence while
       * quarantined (see sched_state_apply). */
      pthread_mutex_lock(&g_q.mu);
      sched_state st = g_state[i];
      pthread_mutex_unlock(&g_q.mu);
      long declared = d->update_interval_sec;
      double overdue = (double)(now - next[i]) / (double)(declared > 0 ? declared : 1);
      q_push(i, sched_priority(&st, source_trust_score(db, d->id), overdue));
      next[i] = now + sched_state_interval(&st, &g_pol, declared);
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
      int queued = g_q.count, busy = 0, backed = 0, quar = 0;
      for (int i = 0; i < n; i++) {
        if (g_q.started[i]) busy++;
        if (g_state[i].quarantined) quar++;
        else if (g_state[i].backoff_until > now) backed++;
      }
      pthread_mutex_unlock(&g_q.mu);
      /* `backed_off`/`quarantined` are APPENDED so the existing parsers of this
       * line keep matching (same reason `stored=` trails the run line). */
      fprintf(stderr, "[sched] busy=%d/%d queued=%d | hostgate inflight=%ld "
                      "waited=%ld over_budget=%ld | backed_off=%d quarantined=%d\n",
              busy, workers, queued, inflight, waits, timeouts, backed, quar);
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
