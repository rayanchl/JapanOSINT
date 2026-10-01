#ifndef JO_SCHEDULER_H
#define JO_SCHEDULER_H
#include "db.h"
#include "../source.h"
#include <time.h>
int  scheduler_run_source(db_handle *db, const source_def *d, const char *entity);
void scheduler_loop(db_handle *db);
/* Run scheduler_loop on a detached background thread so httpd_serve both
 * serves and refreshes collectors (parity with Node's in-process scheduler).
 * No-op if JO_NO_SCHED is set (serve-only / multi-instance / migration). */
void scheduler_start_background(db_handle *db);
/* Stop handing collectors to the worker pool and wait up to `wait_ms` for the
 * runs already executing to finish, so an emit() in progress completes its
 * transaction. Workers are detached and scheduler_loop never returns, so this
 * is a drain, not a join: it can return with runs still in flight, and it says
 * so on stderr when it does. Safe to call when the scheduler never started. */
void scheduler_stop_background(int wait_ms);

/* ── health-driven scheduling (no LLM) ───────────────────────────────────
 *
 * Per-source state persisted in `source_sched_state` so a restart does not
 * forget that a source has been dead for a week. The arithmetic is pure
 * (sched_state_apply / sched_state_interval / sched_priority take `now` as an
 * argument) so tests/unit/test_sched_backoff.c can drive it with a fake clock.
 *
 * This quarantine is DISTINCT from the repair pod's (sources.quarantined_until,
 * set by collectors/pod/collector_repair.c after failed LLM repair cycles and
 * cleared by an operator or by time). That one is a verdict on the collector
 * CODE; this one is a verdict on the UPSTREAM's recent behaviour and clears
 * itself on the first successful probe. /api/status reports both. */
typedef struct {
  int    consecutive_failures;   /* error runs since the last ok run          */
  int    consecutive_empties;    /* ok runs with 0 records since the last >0  */
  long   effective_interval;     /* seconds; == declared when healthy          */
  time_t backoff_until;          /* 0 = not backed off                         */
  int    quarantined;            /* health quarantine (not the repair pod's)   */
  time_t quarantined_at;
  time_t last_probe;             /* last run attempted while quarantined       */
} sched_state;

typedef struct {
  long backoff_cap_sec;          /* JO_SCHED_BACKOFF_CAP_SEC,   default 86400 */
  int  quarantine_after;         /* JO_SCHED_QUARANTINE_AFTER,  default 12    */
  long quarantine_probe_sec;     /* JO_SCHED_QUARANTINE_PROBE_SEC, default 7d */
} sched_policy;

/* Policy from the environment (defaults above); clamps nonsense. */
void sched_policy_load(sched_policy *p);
/* Fold one run outcome into `st`. `status` is the fetch_log verdict ("ok" /
 * "error"); `records` the emit count. */
void sched_state_apply(sched_state *st, const sched_policy *p, long declared,
                       const char *status, long records, time_t now);
/* Seconds until the NEXT attempt after a run that ended at `now`: the
 * declared interval when healthy, the backed-off one after failures, the
 * probe cadence while quarantined. */
long sched_state_interval(const sched_state *st, const sched_policy *p,
                          long declared);
/* Ordering key for the run queue; lower runs first. `score` is the trust
 * score in 0..1 or <0 for unrated; `overdue_ratio` = seconds past due /
 * declared interval. */
double sched_priority(const sched_state *st, double score, double overdue_ratio);
/* Persistence. load_one leaves *st zeroed when there is no row. */
int  sched_state_load_one(db_handle *db, const char *id, sched_state *st);
int  sched_state_save(db_handle *db, const char *id, const sched_state *st,
                      long declared, time_t now);

/* 1 when the circuit breaker has this source benched (sources.quarantined_until
 * in the future). Shared with the entity-pivot path in core/osint_dispatch.c so
 * both entry points honour one answer — see the note at the definition. */
int sched_is_quarantined(db_handle *db, const char *id);

#endif
