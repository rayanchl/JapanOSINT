/* core/llm_worker.c — see llm_worker.h. A tiny job queue per server, each
 * drained by its own thread. The caller stack-allocates the job and parks on a
 * per-job condvar, so no input copying is needed and the result transfers by
 * value. The registry is append-only (≤ a handful of servers), guarded by one
 * mutex; worker threads live for the process lifetime. */
#include "llm_worker.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct llm_job {
  const char *method, *url, *body;
  const char *const *headers;
  size_t body_len;
  int timeout_ms, retries;
  http_response out;          /* worker fills */
  int rc, done;
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
    pthread_mutex_unlock(&w->mu);

    /* the one and only generation call for this server, run serially */
    http_response r = {0};
    int rc = http_request(w->http, j->method, j->url, j->headers, j->body,
                          j->body_len, j->timeout_ms, j->retries, &r);

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

int llm_worker_request(const char *base_url, const char *method, const char *url,
                       const char *const *headers, const char *body,
                       size_t body_len, int timeout_ms, int retries,
                       int high_priority, http_response *out) {
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

  pthread_mutex_lock(&j.mu);
  while (!j.done) pthread_cond_wait(&j.cv, &j.mu);   /* predicate guards lost wakeup */
  pthread_mutex_unlock(&j.mu);

  *out = j.out;
  int rc = j.rc;
  pthread_mutex_destroy(&j.mu);
  pthread_cond_destroy(&j.cv);
  return rc;
}
