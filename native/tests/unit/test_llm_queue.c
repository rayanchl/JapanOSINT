/* test_llm_queue.c — the per-server LLM worker queue (core/llm_worker.c) and
 * how core/llm.c names what happened to a call.
 *
 * What this holds closed:
 *
 *  1. A TIMED-OUT exchange was retried. Every call went out with retries=1 and
 *     httpclient counts a timeout as retryable, so a 120 s generation that ran
 *     out its budget held the worker ~240 s — and every caller queued behind
 *     it. A timeout is now final; a refused connection (nothing spent) still
 *     gets its one retry.
 *  2. The queue had no length bound and a background caller's wait no time
 *     bound. A lane now refuses at JO_LLM_QUEUE_MAX, and a job not picked up
 *     by JO_LLM_QUEUE_WAIT_FACTOR x its timeout is withdrawn unsent.
 *  3. "Never sent" was reported as llm_timeout ("the model is slow"): the
 *     status was decided by a wall clock that included the queue wait. A
 *     withdrawn job is llm_queue_timeout, a refused one llm_queue_full, and
 *     queue time is reported apart from HTTP time.
 *
 * The upstream is an in-process loopback HTTP server with a settable delay,
 * which counts the requests it RECEIVED — the number a retry inflates. */
#include "../../core/llm_worker.c"
#include "../../core/llm.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <sys/socket.h>

static atomic_int g_delay_ms = 0;
static atomic_int g_hits = 0;
static int g_port = 0;

static void *serve_one(void *vp) {
  int fd = (int)(intptr_t)vp;
  char buf[8192];
  size_t got = 0;
  long need = -1;
  for (;;) {                       /* headers, then Content-Length bytes */
    ssize_t r = recv(fd, buf + got, sizeof buf - 1 - got, 0);
    if (r <= 0) break;
    got += (size_t)r; buf[got] = 0;
    char *eoh = strstr(buf, "\r\n\r\n");
    if (eoh && need < 0) {
      char *cl = strstr(buf, "Content-Length:");   /* libcurl's spelling */
      need = (long)(eoh + 4 - buf) + (cl ? atol(cl + 15) : 0);
    }
    if (need >= 0 && (long)got >= need) break;
  }
  atomic_fetch_add(&g_hits, 1);
  int d = atomic_load(&g_delay_ms);
  if (d > 0) usleep((useconds_t)d * 1000);
  const char *body = "{\"choices\":[{\"message\":{\"content\":\"ok\"}}]}";
  char resp[512];
  int n = snprintf(resp, sizeof resp,
                   "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                   "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
                   strlen(body), body);
  send(fd, resp, (size_t)n, MSG_NOSIGNAL);
  close(fd);
  return NULL;
}

static void *acceptor(void *vp) {
  int ls = (int)(intptr_t)vp;
  for (;;) {
    int fd = accept(ls, NULL, NULL);
    if (fd < 0) continue;
    pthread_t t;
    if (pthread_create(&t, NULL, serve_one, (void *)(intptr_t)fd) == 0)
      pthread_detach(t);
    else close(fd);
  }
  return NULL;
}

static int listen_any(int *port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  assert(s >= 0);
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  struct sockaddr_in a = { .sin_family = AF_INET,
                           .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
  assert(bind(s, (struct sockaddr *)&a, sizeof a) == 0);
  socklen_t al = sizeof a;
  getsockname(s, (struct sockaddr *)&a, &al);
  *port = ntohs(a.sin_port);
  return s;
}

static long long now_ms(void) { return mono_ms(); }

/* ---- a background job that occupies the worker ---------------------------- */
typedef struct { const char *base; int timeout, hi; int rc; llm_worker_info wi; } bg;
static char g_base[64], g_url[96];
static const char *HDRS[] = { "Content-Type: application/json", NULL };
static void *bg_call(void *vp) {
  bg *b = vp;
  llm_worker_req q = { .method = "POST", .url = g_url, .headers = HDRS,
                       .body = "{}", .body_len = 2, .timeout_ms = b->timeout,
                       .retry = 1, .high_priority = b->hi };
  http_response r = {0};
  b->rc = llm_worker_call(g_base, &q, &r, &b->wi);
  http_response_free(&r);
  return NULL;
}

static void test_no_retry_after_timeout(void) {
  atomic_store(&g_hits, 0);
  atomic_store(&g_delay_ms, 1500);
  llm_worker_req q = { .method = "POST", .url = g_url, .headers = HDRS,
                       .body = "{}", .body_len = 2, .timeout_ms = 400,
                       .retry = 1 };
  http_response r = {0};
  llm_worker_info wi;
  long long t0 = now_ms();
  int rc = llm_worker_call(g_base, &q, &r, &wi);
  long long took = now_ms() - t0;
  http_response_free(&r);
  assert(rc != 0 && wi.verdict == LLM_Q_RAN);
  assert(wi.transport == HTTP_TE_TIMEOUT);
  assert(wi.attempts == 1 && "a timed-out exchange must not be sent again");
  assert(took < 1000);                       /* one budget, not two */
  usleep(1300 * 1000);                       /* let the server finish */
  assert(atomic_load(&g_hits) == 1);
  atomic_store(&g_delay_ms, 0);
  printf("  timeout: 1 attempt, %lld ms, server saw %d request: ok\n", took,
         atomic_load(&g_hits));
}

static void test_refused_retries_once(void) {
  int p = 0;
  int s = listen_any(&p);
  close(s);                                  /* nothing listens there now */
  char base[64], url[96];
  snprintf(base, sizeof base, "http://127.0.0.1:%d", p);
  snprintf(url, sizeof url, "%s/v1/chat/completions", base);
  llm_worker_req q = { .method = "POST", .url = url, .headers = HDRS,
                       .body = "{}", .body_len = 2, .timeout_ms = 2000,
                       .retry = 1 };
  http_response r = {0};
  llm_worker_info wi;
  int rc = llm_worker_call(base, &q, &r, &wi);
  http_response_free(&r);
  assert(rc != 0 && wi.transport == HTTP_TE_CONNECT);
  assert(wi.attempts == 2 && "a refused connection cost nothing: retry once");

  /* and llm.c calls it unreachable, quickly — not a timeout */
  llm_client c = { .http = NULL, .base_url = base, .interactive = 1 };
  llm_status st = LLM_OK;
  char *out = llm_chat_ex(&c, "[{\"role\":\"user\",\"content\":\"x\"}]", NULL,
                          8, 0, 2000, &st, NULL);
  assert(!out && st == LLM_ERR_UNREACHABLE);
  printf("  refused: 2 attempts, llm_unreachable: ok\n");
}

static void test_queue_full(void) {
  setenv("JO_LLM_QUEUE_MAX", "1", 1);
  atomic_store(&g_delay_ms, 800);
  bg a = { .timeout = 5000 }, b = { .timeout = 5000 };
  pthread_t ta, tb;
  assert(pthread_create(&ta, NULL, bg_call, &a) == 0);
  usleep(200 * 1000);                        /* a is in flight */
  assert(pthread_create(&tb, NULL, bg_call, &b) == 0);
  usleep(100 * 1000);                        /* b is the one waiting */
  bg c = { .timeout = 5000 };
  long long t0 = now_ms();
  bg_call(&c);
  long long took = now_ms() - t0;
  assert(c.rc != 0 && c.wi.verdict == LLM_Q_FULL && c.wi.attempts == 0);
  assert(took < 100 && "a full lane refuses at once");
  /* the other lane has its own bound */
  bg d = { .timeout = 5000, .hi = 1 };
  bg_call(&d);
  assert(d.rc == 0 && d.wi.verdict == LLM_Q_RAN);
  pthread_join(ta, NULL);
  pthread_join(tb, NULL);
  assert(a.rc == 0 && b.rc == 0);
  /* through llm.c: its own code */
  pthread_t te;
  bg e = { .timeout = 5000 }, f = { .timeout = 5000 };
  assert(pthread_create(&te, NULL, bg_call, &e) == 0);
  usleep(200 * 1000);
  pthread_t tf;
  assert(pthread_create(&tf, NULL, bg_call, &f) == 0);
  usleep(100 * 1000);
  llm_client cl = { .http = NULL, .base_url = g_base, .interactive = 0 };
  llm_status st = LLM_OK;
  char *out = llm_chat_ex(&cl, "[{\"role\":\"user\",\"content\":\"x\"}]", NULL,
                          8, 0, 5000, &st, NULL);
  assert(!out && st == LLM_ERR_QUEUE_FULL);
  assert(!strcmp(llm_status_code(st), "llm_queue_full"));
  pthread_join(te, NULL);
  pthread_join(tf, NULL);
  unsetenv("JO_LLM_QUEUE_MAX");
  atomic_store(&g_delay_ms, 0);
  printf("  queue full: refused in %lld ms, other lane unaffected: ok\n", took);
}

static void test_queue_deadline_unbounded_caller(void) {
  setenv("JO_LLM_QUEUE_WAIT_FACTOR", "1", 1);
  atomic_store(&g_delay_ms, 1500);
  atomic_store(&g_hits, 0);
  bg a = { .timeout = 5000 };
  pthread_t ta;
  assert(pthread_create(&ta, NULL, bg_call, &a) == 0);
  usleep(200 * 1000);
  /* No max_wait_ms: before, nothing bounded this wait at all. */
  llm_client c = { .http = NULL, .base_url = g_base, .interactive = 0 };
  llm_status st = LLM_OK;
  long long t0 = now_ms();
  char *out = llm_chat_ex(&c, "[{\"role\":\"user\",\"content\":\"x\"}]", NULL,
                          8, 0, 400, &st, NULL);
  long long took = now_ms() - t0;
  long qms = -1, hms = -1;
  llm_last_call_timing(&qms, &hms);
  assert(!out && st == LLM_ERR_QUEUE_TIMEOUT);
  assert(!strcmp(llm_status_code(st), "llm_queue_timeout"));
  assert(took >= 350 && took < 1000);        /* 1 x its 400 ms timeout */
  assert(qms >= 350 && hms == 0 && "queued, never on the wire");
  pthread_join(ta, NULL);
  assert(a.rc == 0);
  assert(atomic_load(&g_hits) == 1 && "the withdrawn job was never sent");
  unsetenv("JO_LLM_QUEUE_WAIT_FACTOR");
  atomic_store(&g_delay_ms, 0);
  /* the worker still serves after a withdrawal, and timing is reported */
  out = llm_chat_ex(&c, "[{\"role\":\"user\",\"content\":\"x\"}]", NULL,
                    8, 0, 2000, &st, NULL);
  assert(out && st == LLM_OK);
  free(out);
  printf("  queue deadline: withdrawn after %lld ms (%ld queued, %ld HTTP), "
         "llm_queue_timeout: ok\n", took, qms, hms);
}

static void test_timeout_named_from_http_time(void) {
  atomic_store(&g_delay_ms, 1500);
  llm_client c = { .http = NULL, .base_url = g_base, .interactive = 1 };
  llm_status st = LLM_OK;
  char *out = llm_chat_ex(&c, "[{\"role\":\"user\",\"content\":\"x\"}]", NULL,
                          8, 0, 400, &st, NULL);
  long qms = -1, hms = -1;
  llm_last_call_timing(&qms, &hms);
  assert(!out && st == LLM_ERR_TIMEOUT);
  assert(hms >= 350 && hms < 1000);
  usleep(1300 * 1000);
  atomic_store(&g_delay_ms, 0);
  printf("  connected exchange past its budget: llm_timeout: ok\n");
}

int main(void) {
  http_client_global_init();
  int ls = listen_any(&g_port);
  assert(listen(ls, 64) == 0);
  pthread_t at;
  assert(pthread_create(&at, NULL, acceptor, (void *)(intptr_t)ls) == 0);
  pthread_detach(at);
  snprintf(g_base, sizeof g_base, "http://127.0.0.1:%d", g_port);
  snprintf(g_url, sizeof g_url, "%s/v1/chat/completions", g_base);
  unsetenv("JO_LLM_QUEUE_MAX");
  unsetenv("JO_LLM_QUEUE_WAIT_FACTOR");

  assert(llm_worker_queue_max() == 64);
  assert(llm_worker_queue_wait_ms(1000) == 4000);

  test_no_retry_after_timeout();
  test_refused_retries_once();
  test_queue_full();
  test_queue_deadline_unbounded_caller();
  test_timeout_named_from_http_time();
  printf("test_llm_queue: all ok\n");
  return 0;
}
