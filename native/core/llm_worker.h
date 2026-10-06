/* core/llm_worker.h — global per-server LLM execution worker.
 *
 * Every llama-server generation call funnels through one dedicated worker
 * thread per target base_url (lazily created on first use). Exactly one request
 * is in flight per server (FIFO), so all callers — the OSINT search pipeline,
 * the entity enricher, the maintenance triage/repair pods, the suggest path —
 * never race one another onto the single-slot server. This supersedes the old
 * per-call mutex: instead of "lock → call → unlock", a caller enqueues a job
 * and parks until the worker runs it.
 *
 * One worker per base_url (not one global): the main 20B server (:8080) and the
 * fast suggest server (:8081) get independent threads, so a slow 20B job never
 * delays typeahead. Each worker owns its own http_client for stable keep-alive
 * and prompt-cache locality to that server. Because the caller blocks on its
 * job, all input pointers may be borrowed (no copies needed). */
#ifndef JO_LLM_WORKER_H
#define JO_LLM_WORKER_H

#include "httpclient.h"

/* Perform a generation HTTP exchange through the worker that owns `base_url`.
 * Blocking. Fills *out (caller frees out->body via http_response_free).
 * Returns 0 on a completed exchange (any status), non-zero on hard failure —
 * identical contract to http_request(). Falls back to a direct one-off request
 * if a worker can't be created, so LLM calls degrade gracefully, never hang.
 *
 * `high_priority` picks the lane: interactive requests (the live OSINT search:
 * analyze + its phase-2 rounds) go high so they jump ahead of background pod
 * work (enricher backfill, triage, repair) on the same server. A generation
 * already in flight is never preempted, so a high job waits at most one
 * in-progress low job — never the whole backlog. */
int llm_worker_request(const char *base_url, const char *method, const char *url,
                       const char *const *headers, const char *body,
                       size_t body_len, int timeout_ms, int retries,
                       int high_priority, http_response *out);

/* llm_worker_request with a bound on the WHOLE wait, queue included.
 * `max_wait_ms` > 0: if the worker has not started this job within that many
 * ms (it is queued behind another caller's in-flight request), the job is
 * withdrawn unsent and the call returns non-zero with out->status 0; once
 * started, the exchange gets what is left of `max_wait_ms` (at least 1 s) as
 * its timeout. 0 = the unbounded wait above. Request-path callers (a user's
 * semantic query) need this: their own timeout otherwise bounds only the HTTP
 * exchange, and the queue in front of it can hold them for a background
 * batch's whole duration. */
int llm_worker_request_ex(const char *base_url, const char *method,
                          const char *url, const char *const *headers,
                          const char *body, size_t body_len, int timeout_ms,
                          int retries, int high_priority, int max_wait_ms,
                          http_response *out);

#endif
