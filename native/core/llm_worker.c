/* core/llm_worker.c — see llm_worker.h. A tiny job queue per server, each
 * drained by its own thread. The caller stack-allocates the job and parks on a
 * per-job condvar, so no input copying is needed and the result transfers by
 * value. The registry is append-only (≤ a handful of servers), guarded by one
 * mutex; worker threads live for the process lifetime. */
#include "llm_worker.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct llm_job {
  const llm_worker_req *req;  /* borrowed: the caller blocks until done */
  http_response out;          /* worker fills */
  int rc, done;
  int started;                /* set by the worker under w->mu on dequeue */
  long long enq_ms;           /* CLOCK_MONOTONIC ms at enqueue */
  long long pick_ms;          /* … and when the worker picked it up */
  long long deadline_ms;      /* whole-call bound (max_wait_ms); 0 = none */
  long http_ms;
  int attempts, transport;
  pthread_mutex_t mu;
  pthread_cond_t cv;
  struct llm_job *next;
} llm_job;

typedef struct {
  char *base_url;
  http_client *http;          /* worker-owned: one keep-alive pool per server */
  pthread_t thread;
  pthread_mutex_t mu;
  pthread_cond_t cv;          /* signaled when a job is enqueued */
  llm_job *head_hi, *tail_hi; /* interactive lane (drained first) */
  llm_job *head_lo, *tail_lo; /* background lane */
  int n_hi, n_lo;             /* jobs WAITING in each lane (not the one running) */
} llm_worker;

#define MAX_WORKERS 4
static llm_worker g_workers[MAX_WORKERS];
static int g_nworkers = 0;
static pthread_mutex_t g_reg_mu = PTHREAD_MUTEX_INITIALIZER;

static long long mono_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + t.tv_nsec / 1000000L;
}

static long env_long(const char *k, long dflt, long lo) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  if (!end || end == v || *end) return dflt;
  return n < lo ? lo : n;
}

int llm_worker_queue_max(void) {
  return (int)env_long("JO_LLM_QUEUE_MAX", 64, 1);
}

int llm_worker_queue_wait_ms(int timeout_ms) {
  long f = env_long("JO_LLM_QUEUE_WAIT_FACTOR", 4, 1);
  long long t = (long long)(timeout_ms > 0 ? timeout_ms : 30000) * f;
  return t > 0x7fffffff ? 0x7fffffff : (int)t;
}

/* The exchange, with the one retry llm_worker_req.retry allows. http_request
 * is called with retries=0 because its own retry cannot tell a refused
 * connection (free to retry: nothing was spent) from a timed-out generation
 * (retrying spends the whole budget again — a 120 s call held the worker for
 * ~240 s that way). */
static int run_job(llm_job *j, http_client *http) {
  const llm_worker_req *q = j->req;
  int base = q->timeout_ms > 0 ? q->timeout_ms : 30000;
  int rc = 1;
  for (int attempt = 0; ; attempt++) {
    /* A bounded job gets what is left of its budget, not the whole budget
     * again: the time it spent queued was part of what the caller allowed. */
    int tmo = base;
    if (j->deadline_ms > 0) {
      long long left = j->deadline_ms - mono_ms();
      tmo = left < 1000 ? 1000 : (left < (long long)tmo ? (int)left : tmo);
    }
    http_response r = {0};
    long long t0 = mono_ms();
    rc = http_request(http, q->method, q->url, q->headers, q->body,
                      q->body_len, tmo, 0, &r);
    j->http_ms += (long)(mono_ms() - t0);
    j->attempts++;
    j->transport = rc ? http_last_transport_error() : HTTP_TE_NONE;
    int again = q->retry && attempt == 0 &&
                ((rc && j->transport == HTTP_TE_CONNECT) ||
                 (!rc && (r.status == 429 || (r.status >= 500 && r.status <= 599))));
    /* A bounded job retries only if a retry still fits its budget. */
    if (again && j->deadline_ms > 0 && j->deadline_ms - mono_ms() < 1250)
      again = 0;
    if (!again) { j->out = r; break; }
    http_response_free(&r);
    usleep(250 * 1000);
  }
  return rc;
}

static void *worker_main(void *vp) {
  llm_worker *w = vp;
  for (;;) {
    pthread_mutex_lock(&w->mu);
    while (!w->head_hi && !w->head_lo) pthread_cond_wait(&w->cv, &w->mu);
    llm_job *j;
    if (w->head_hi) {                       /* interactive lane wins */
      j = w->head_hi; w->head_hi = j->next;
      if (!w->head_hi) w->tail_hi = NULL;
      w->n_hi--;
    } else {
      j = w->head_lo; w->head_lo = j->next;
      if (!w->head_lo) w->tail_lo = NULL;
      w->n_lo--;
    }
    j->started = 1;             /* under w->mu: the caller's withdraw checks it */
    j->pick_ms = mono_ms();
    pthread_mutex_unlock(&w->mu);

    /* the one and only generation call for this server, run serially */
    int rc = run_job(j, w->http);

    pthread_mutex_lock(&j->mu);
    j->rc = rc; j->done = 1;
    pthread_cond_signal(&j->cv);
    pthread_mutex_unlock(&j->mu);
    /* j may be destroyed by the caller the instant it observes done — touch
     * it no further. */
  }
  return NULL;
}

static llm_worker *worker_for(const char *base_url) {
  if (!base_url) base_url = "";
  pthread_mutex_lock(&g_reg_mu);
  llm_worker *w = NULL;
  for (int i = 0; i < g_nworkers; i++)
    if (!strcmp(g_workers[i].base_url, base_url)) { w = &g_workers[i]; break; }
  if (!w && g_nworkers < MAX_WORKERS) {
    w = &g_workers[g_nworkers];
    w->base_url = strdup(base_url);
    w->http = http_client_new();
    pthread_mutex_init(&w->mu, NULL);
    pthread_cond_init(&w->cv, NULL);
    w->head_hi = w->tail_hi = w->head_lo = w->tail_lo = NULL;
    w->n_hi = w->n_lo = 0;
    if (w->base_url && w->http &&
        pthread_create(&w->thread, NULL, worker_main, w) == 0) {
      pthread_detach(w->thread);
      g_nworkers++;
    } else {
      free(w->base_url); w->base_url = NULL;
      if (w->http) { http_client_free(w->http); w->http = NULL; }
      w = NULL;
    }
  }
  pthread_mutex_unlock(&g_reg_mu);
  return w;
}

/* Unlink `j` from whichever lane holds it. Caller holds w->mu. Returns 1 if
 * it was found (i.e. it had not been dequeued). */
static int withdraw_locked(llm_worker *w, llm_job *j) {
  llm_job **heads[2] = { &w->head_hi, &w->head_lo };
  llm_job **tails[2] = { &w->tail_hi, &w->tail_lo };
  int *counts[2] = { &w->n_hi, &w->n_lo };
  for (int l = 0; l < 2; l++) {
    llm_job *prev = NULL;
    for (llm_job *x = *heads[l]; x; prev = x, x = x->next) {
      if (x != j) continue;
      if (prev) prev->next = x->next; else *heads[l] = x->next;
      if (*tails[l] == x) *tails[l] = prev;
      (*counts[l])--;
      return 1;
    }
  }
  return 0;
}

static void abs_after(struct timespec *abs, long long add_ms) {
  clock_gettime(CLOCK_REALTIME, abs);
  if (add_ms < 0) add_ms = 0;
  abs->tv_sec += (time_t)(add_ms / 1000);
  abs->tv_nsec += (long)(add_ms % 1000) * 1000000L;
  if (abs->tv_nsec >= 1000000000L) { abs->tv_sec++; abs->tv_nsec -= 1000000000L; }
}

int llm_worker_call(const char *base_url, const llm_worker_req *req,
                    http_response *out, llm_worker_info *info) {
  out->status = 0; out->body = NULL; out->body_len = 0;
  llm_worker_info dummy;
  if (!info) info = &dummy;
  memset(info, 0, sizeof *info);
  llm_worker *w = worker_for(base_url);
  if (!w) {
    /* couldn't create a worker (OOM / > MAX_WORKERS): degrade to a direct,
     * unserialized request so LLM still works rather than hanging. */
    http_client *tmp = http_client_new();
    llm_job j = {0};
    j.req = req;
    if (req->max_wait_ms > 0) j.deadline_ms = mono_ms() + req->max_wait_ms;
    int rc = run_job(&j, tmp);
    http_client_free(tmp);
    *out = j.out;
    info->verdict = LLM_Q_DIRECT;
    info->http_ms = j.http_ms;
    info->attempts = j.attempts;
    info->transport = j.transport;
    return rc;
  }

  llm_job j = {0};
  j.req = req;
  j.enq_ms = mono_ms();
  if (req->max_wait_ms > 0) j.deadline_ms = j.enq_ms + req->max_wait_ms;
  /* When the job is withdrawn if still queued: the whole-call bound for a
   * bounded caller, its derived queue deadline for everyone else. */
  long long queue_deadline = j.deadline_ms > 0
      ? j.deadline_ms : j.enq_ms + llm_worker_queue_wait_ms(req->timeout_ms);
  pthread_mutex_init(&j.mu, NULL);
  pthread_cond_init(&j.cv, NULL);

  int qmax = llm_worker_queue_max();
  pthread_mutex_lock(&w->mu);
  int *n = req->high_priority ? &w->n_hi : &w->n_lo;
  if (*n >= qmax) {
    /* LANE FULL. Refused before it is queued, so nothing is withdrawn and
     * nothing was sent: the caller hears "busy" at once rather than after
     * waiting out a backlog its own queue deadline would expire behind. */
    pthread_mutex_unlock(&w->mu);
    pthread_mutex_destroy(&j.mu);
    pthread_cond_destroy(&j.cv);
    info->verdict = LLM_Q_FULL;
    fprintf(stderr, "[llm] %s: %s lane full (%d waiting, JO_LLM_QUEUE_MAX) — "
                    "request refused unsent\n", w->base_url,
            req->high_priority ? "interactive" : "background", qmax);
    return 1;
  }
  j.next = NULL;
  if (req->high_priority) {
    if (w->tail_hi) w->tail_hi->next = &j; else w->head_hi = &j;
    w->tail_hi = &j;
  } else {
    if (w->tail_lo) w->tail_lo->next = &j; else w->head_lo = &j;
    w->tail_lo = &j;
  }
  (*n)++;
  pthread_cond_signal(&w->cv);
  pthread_mutex_unlock(&w->mu);

  /* BOUNDED QUEUE WAIT. Without it, a caller's own timeout bounds only its
   * HTTP exchange, not the time it sits queued behind somebody else's: a 15 s
   * semantic query waited out a 120 s embedding batch in full, and nothing at
   * all bounded a background caller parked behind a backlog. Wait until the
   * queue deadline; if the worker has not picked the job up by then, take it
   * back out of the queue and report LLM_Q_TIMEOUT. A job the worker HAS
   * started cannot be withdrawn (its stack frame is in use), so that case
   * waits for completion, which the exchange's own timeout bounds. */
  struct timespec abs;
  abs_after(&abs, queue_deadline - mono_ms());
  pthread_mutex_lock(&j.mu);
  int to = 0;
  while (!j.done && !to)
    to = (pthread_cond_timedwait(&j.cv, &j.mu, &abs) == ETIMEDOUT);
  int done = j.done;
  pthread_mutex_unlock(&j.mu);
  if (!done) {
    pthread_mutex_lock(&w->mu);
    int withdrawn = !j.started && withdraw_locked(w, &j);
    pthread_mutex_unlock(&w->mu);
    if (withdrawn) {
      pthread_mutex_destroy(&j.mu);
      pthread_cond_destroy(&j.cv);
      info->verdict = LLM_Q_TIMEOUT;
      info->queued_ms = (long)(mono_ms() - j.enq_ms);
      return 1;                   /* never sent: nothing to free */
    }
  }

  pthread_mutex_lock(&j.mu);
  while (!j.done) pthread_cond_wait(&j.cv, &j.mu);   /* predicate guards lost wakeup */
  pthread_mutex_unlock(&j.mu);

  *out = j.out;
  info->verdict = LLM_Q_RAN;
  info->queued_ms = (long)(j.pick_ms - j.enq_ms);
  info->http_ms = j.http_ms;
  info->attempts = j.attempts;
  info->transport = j.transport;
  int rc = j.rc;
  pthread_mutex_destroy(&j.mu);
  pthread_cond_destroy(&j.cv);
  return rc;
}
