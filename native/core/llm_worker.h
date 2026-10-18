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

/* One generation/embedding exchange, as handed to the worker. Every pointer is
 * borrowed for the duration of the call (the caller blocks). */
typedef struct {
  const char *method, *url, *body;
  const char *const *headers;
  size_t body_len;
  int timeout_ms;      /* the exchange's own timeout (<=0: 30 s) */
  /* 1: ONE more attempt after a failure that cost nothing — nothing answered
   * the connect, or the server said 5xx/429. Never after a TIMEOUT: a 120 s
   * generation that ran out its budget is retried as a second 120 s the
   * caller never granted, which is how one call held a worker for ~240 s. */
  int retry;
  /* Lane. Interactive requests (the live OSINT search, a user's semantic
   * query, suggest) go high and jump ahead of background pod work on the same
   * server. A job already in flight is never preempted. */
  int high_priority;
  /* > 0: a bound on the WHOLE wait, queue included. If the worker has not
   * started the job within that many ms it is withdrawn unsent; once started,
   * the exchange gets what is left (at least 1 s). 0: the queue wait is
   * bounded by llm_worker_queue_wait_ms(timeout_ms) instead and the exchange
   * gets its full timeout. */
  int max_wait_ms;
} llm_worker_req;

typedef enum {
  LLM_Q_RAN = 0,       /* the worker ran it; rc / out describe the exchange   */
  LLM_Q_TIMEOUT,       /* withdrawn unsent: it waited out its queue deadline  */
  LLM_Q_FULL,          /* refused unsent: its lane already held JO_LLM_QUEUE_MAX */
  LLM_Q_DIRECT,        /* no worker could be created; ran unserialized here   */
} llm_queue_verdict;

typedef struct {
  llm_queue_verdict verdict;
  long queued_ms;      /* enqueue → picked up (or → withdrawn) */
  long http_ms;        /* inside http_request, all attempts together */
  int attempts;        /* exchanges actually started (0 when never sent) */
  int transport;       /* HTTP_TE_* of the last attempt (httpclient.h) */
} llm_worker_info;

/* Run one exchange through the worker that owns `base_url` (lazily created).
 * Blocking. Fills *out (caller frees out->body via http_response_free) and,
 * when `info` is non-NULL, *info. Returns 0 on a completed exchange (any
 * status), non-zero when no exchange completed — including a job that was
 * never sent (info->verdict says which). Falls back to a direct one-off
 * request if a worker can't be created, so LLM calls degrade, never hang.
 *
 * THE QUEUE IS BOUNDED TWICE. In length: each lane holds at most
 * JO_LLM_QUEUE_MAX waiting jobs (default 64) and the next is refused at once
 * with LLM_Q_FULL — a caller that cannot be served soon learns it now rather
 * than parking a thread on a backlog that only grows. In time: every job has
 * a queue deadline, so a caller's own timeout is never the only bound on a
 * wait that its timeout does not cover. */
int llm_worker_call(const char *base_url, const llm_worker_req *req,
                    http_response *out, llm_worker_info *info);

/* The queue deadline a job without max_wait_ms gets: JO_LLM_QUEUE_WAIT_FACTOR
 * (default 4) times its own timeout — long enough to sit behind a few jobs of
 * its own size, never forever. */
int llm_worker_queue_wait_ms(int timeout_ms);

/* The per-lane length bound (JO_LLM_QUEUE_MAX, default 64, minimum 1). */
int llm_worker_queue_max(void);

#endif
