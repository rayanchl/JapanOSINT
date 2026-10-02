/* core/llm_worker.c — see llm_worker.h. A tiny job queue per server, each
 * drained by its own thread. The caller stack-allocates the job and parks on a
 * per-job condvar, so no input copying is needed and the result transfers by
 * value. The registry is append-only (≤ a handful of servers), guarded by one
 * mutex; worker threads live for the process lifetime. */
#include "llm_worker.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct llm_job {
  const char *method, *url, *body;
  const char *const *headers;
  size_t body_len;
  int timeout_ms, retries;
  http_response out;          /* worker fills */
  int rc, done;
  int started;                /* set by the worker under w->mu on dequeue */
  long long deadline_ms;      /* CLOCK_MONOTONIC ms; 0 = unbounded (see
                               * llm_worker_request_ex) */
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

static void *worker_main(void *vp) {
  llm_worker *w = vp;
  for (;;) {
    pthread_mutex_lock(&w->mu);
    while (!w->head_hi && !w->head_lo) pthread_cond_wait(&w->cv, &w->mu);
    llm_job *j;
    if (w->head_hi) {                       /* interactive lane wins */
      j = w->head_hi; w->head_hi = j->next;
      if (!w->head_hi) w->tail_hi = NULL;
    } else {
      j = w->head_lo; w->head_lo = j->next;
      if (!w->head_lo) w->tail_lo = NULL;
    }
    j->started = 1;             /* under w->mu: the caller's withdraw checks it */
    pthread_mutex_unlock(&w->mu);

    /* A bounded job gets what is left of its budget, not the whole budget
     * again: the time it spent queued was part of what the caller allowed. */
    int tmo = j->timeout_ms;
    if (j->deadline_ms > 0) {
      long long left = j->deadline_ms - mono_ms();
      tmo = left < 1000 ? 1000 : (left < (long long)tmo ? (int)left : tmo);
    }

    /* the one and only generation call for this server, run serially */
    http_response r = {0};
    int rc = http_request(w->http, j->method, j->url, j->headers, j->body,
                          j->body_len, tmo, j->retries, &r);

    pthread_mutex_lock(&j->mu);
    j->out = r; j->rc = rc; j->done = 1;
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
  for (int l = 0; l < 2; l++) {
    llm_job *prev = NULL;
    for (llm_job *x = *heads[l]; x; prev = x, x = x->next) {
      if (x != j) continue;
      if (prev) prev->next = x->next; else *heads[l] = x->next;
      if (*tails[l] == x) *tails[l] = prev;
      return 1;
    }
  }
  return 0;
}

int llm_worker_request(const char *base_url, const char *method, const char *url,
                       const char *const *headers, const char *body,
                       size_t body_len, int timeout_ms, int retries,
                       int high_priority, http_response *out) {
  return llm_worker_request_ex(base_url, method, url, headers, body, body_len,
                               timeout_ms, retries, high_priority, 0, out);
}

int llm_worker_request_ex(const char *base_url, const char *method,
                          const char *url, const char *const *headers,
                          const char *body, size_t body_len, int timeout_ms,
                          int retries, int high_priority, int max_wait_ms,
                          http_response *out) {
  out->status = 0; out->body = NULL; out->body_len = 0;
  llm_worker *w = worker_for(base_url);
  if (!w) {
    /* couldn't create a worker (OOM / > MAX_WORKERS): degrade to a direct,
     * unserialized request so LLM still works rather than hanging. */
    http_client *tmp = http_client_new();
    int rc = http_request(tmp, method, url, headers, body, body_len,
                          timeout_ms, retries, out);
    http_client_free(tmp);
    return rc;
  }

  llm_job j = {0};
  j.method = method; j.url = url; j.headers = headers;
  j.body = body; j.body_len = body_len;
  j.timeout_ms = timeout_ms; j.retries = retries;
  if (max_wait_ms > 0) j.deadline_ms = mono_ms() + max_wait_ms;
  pthread_mutex_init(&j.mu, NULL);
  pthread_cond_init(&j.cv, NULL);

  pthread_mutex_lock(&w->mu);
  j.next = NULL;
  if (high_priority) {
    if (w->tail_hi) w->tail_hi->next = &j; else w->head_hi = &j;
    w->tail_hi = &j;
  } else {
    if (w->tail_lo) w->tail_lo->next = &j; else w->head_lo = &j;
    w->tail_lo = &j;
  }
  pthread_cond_signal(&w->cv);
  pthread_mutex_unlock(&w->mu);

  if (j.deadline_ms > 0) {
    /* BOUNDED WAIT. Without it, a caller's own timeout bounds only its HTTP
     * exchange, not the time it sits queued behind somebody else's: a 15 s
     * semantic query waited out a 120 s embedding batch in full. Wait until
     * the deadline; if the worker has not picked the job up by then, take it
     * back out of the queue and report a failed exchange (rc 1, status 0 —
     * post_json classifies that by the clock, so it reads as a timeout). A job
     * the worker HAS started cannot be withdrawn (its stack frame is in use),
     * so that case waits for completion, which the remaining-budget timeout
     * passed to http_request bounds. */
    struct timespec abs;
    clock_gettime(CLOCK_REALTIME, &abs);
    long long add = j.deadline_ms - mono_ms();
    if (add < 0) add = 0;
    abs.tv_sec += (time_t)(add / 1000);
    abs.tv_nsec += (long)(add % 1000) * 1000000L;
    if (abs.tv_nsec >= 1000000000L) { abs.tv_sec++; abs.tv_nsec -= 1000000000L; }
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
        return 1;                 /* never sent: nothing to free */
      }
    }
  }

  pthread_mutex_lock(&j.mu);
  while (!j.done) pthread_cond_wait(&j.cv, &j.mu);   /* predicate guards lost wakeup */
  pthread_mutex_unlock(&j.mu);

  *out = j.out;
  int rc = j.rc;
  pthread_mutex_destroy(&j.mu);
  pthread_cond_destroy(&j.cv);
  return rc;
}
