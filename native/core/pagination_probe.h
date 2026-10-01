/* core/pagination_probe.h — prove which sources actually paginate.
 *
 * THE PROBLEM. 2,898 registered sources carry a hardcoded page-size parameter
 * in their URL — `limit=20`, `rows=100`, `$top=50` — and no offset, so
 * lib/pagewalk.c fetches page 1 and stops. Those caps sum to ~531,000 records
 * against upstreams that hold far more: ROR answers `limit=20` out of ~110,000
 * organisations. The bound is disclosed as a collector-truncation-notice, which
 * satisfies house rule 2's reporting half and not its collection half. We are
 * not getting the data.
 *
 * WHY IT IS NOT JUST "ADD &offset=". An endpoint that IGNORES an unknown query
 * parameter returns page 1 again. Written in blind, that makes pagewalk
 * re-fetch the same page every run, for every source we guessed wrong about —
 * turning a stated limit into unstated waste, and getting us rate-limited by
 * the very upstreams we are trying to read more of.
 *
 * SO THIS MEASURES. Per source: fetch page 1, fetch page 2 with a candidate
 * offset, compare the RECORD SETS (not the bytes — envelopes carry timestamps
 * and request ids). A source is reported as paginating only when its page 2
 * demonstrably holds records page 1 did not.
 *
 * WHAT IT DOES WITH THE ANSWER. Nothing, on its own. A proven source gets a
 * `collector_anomaly` (verdict 'manual') plus a `collector_repair` row with
 * action='url_swap' and a patch naming old_url/new_url — which is exactly what
 * the existing POST /api/admin/repairs/:id/approve consumes. Approval inserts
 * the row into collector_url_overrides, url_override_apply() picks it up, and
 * pagewalk walks it. No new approval path, no new admin surface, and no URL
 * changes without an operator saying so.
 *
 * It runs on a detached thread with its own DB connection and http_client:
 * thousands of requests must never touch the event loop or the shared handle.
 * Requests are paced per host — this is a survey, not a stampede.
 *
 * Routes (operator-gated with the rest of /api/admin):
 *   POST /api/admin/pagination/probe            start; 202 + job id
 *   GET  /api/admin/pagination/jobs[/:job_id]   progress / results
 */
#ifndef JO_PAGINATION_PROBE_H
#define JO_PAGINATION_PROBE_H

#include "db.h"

/* Start the survey. `limit` > 0 probes only the first N candidates (a smoke
 * test); 0 means all of them. Returns a malloc'd JSON envelope and sets
 * *http_status: 202 started, 409 one already running, 503 no DB. */
char *pagination_probe_start(db_handle *shared, int limit, int *http_status);

/* Snapshot of one job, or of all jobs when `job_id` is NULL. Always malloc'd. */
char *pagination_probe_status(const char *job_id);

/* Candidate count without running anything — what the survey WOULD ask for.
 * Used by the status route so an operator can see the scope before starting. */
int pagination_probe_candidates(void);

/* Ask a running survey to stop. Returns immediately; the thread observes the
 * flag between candidates and between the two page fetches. Called from the
 * shutdown path — a detached survey holding a DB handle must not outlive the
 * server that opened it. */
void pagination_probe_stop_all(void);

#endif /* JO_PAGINATION_PROBE_H */
