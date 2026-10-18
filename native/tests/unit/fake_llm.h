/* tests/unit/fake_llm.h — a llama-server stand-in for pod tests.
 *
 * Just enough HTTP/1.1 on 127.0.0.1:<ephemeral> for core/llm.c: GET /health
 * answers 200, POST /completion answers 200 {"content": <fake_llm_content>}.
 * One connection at a time, Connection: close. The point is to give a test
 * the one case it cannot get from an unreachable port: a server that is UP
 * and answers, badly. Header-only; include it from one test file.
 *
 *   int port = fake_llm_start();          // also sets LLM_BASE_URL
 *   fake_llm_set_content("not json");
 *   ...
 *   fake_llm_point_at_nothing();          // LLM_BASE_URL -> a closed port */
#ifndef JO_TEST_FAKE_LLM_H
#define JO_TEST_FAKE_LLM_H
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int             fake_llm_fd = -1;
static pthread_mutex_t fake_llm_mu = PTHREAD_MUTEX_INITIALIZER;
static char            fake_llm_content[512] = "";
static atomic_int      fake_llm_completions = 0;

static void fake_llm_set_content(const char *c) {
  pthread_mutex_lock(&fake_llm_mu);
  snprintf(fake_llm_content, sizeof fake_llm_content, "%s", c ? c : "");
  pthread_mutex_unlock(&fake_llm_mu);
}

/* JSON-escape `in` into `out` (quotes and backslashes only; test content). */
static void fake_llm_esc(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (; *in && o + 2 < cap; in++) {
    if (*in == '"' || *in == '\\') out[o++] = '\\';
    out[o++] = *in;
  }
  out[o] = 0;
}

static void fake_llm_serve(int c) {
  char req[65536];
  size_t got = 0;
  long want = -1;
  for (;;) {
    ssize_t r = recv(c, req + got, sizeof req - 1 - got, 0);
    if (r <= 0) break;
    got += (size_t)r;
    req[got] = 0;
    char *hdr_end = strstr(req, "\r\n\r\n");
    if (!hdr_end) continue;
    if (want < 0) {
      const char *cl = strstr(req, "Content-Length:");
      if (!cl) cl = strstr(req, "content-length:");
      want = cl ? atol(cl + 15) : 0;
    }
    if ((long)(got - (size_t)(hdr_end + 4 - req)) >= want) break;
    if (got >= sizeof req - 1) break;
  }
  char body[1200], esc[1100];
  if (strncmp(req, "GET /health", 11) == 0) {
    snprintf(body, sizeof body, "{\"status\":\"ok\"}");
  } else {
    pthread_mutex_lock(&fake_llm_mu);
    fake_llm_esc(fake_llm_content, esc, sizeof esc);
    pthread_mutex_unlock(&fake_llm_mu);
    snprintf(body, sizeof body, "{\"content\":\"%s\"}", esc);
    atomic_fetch_add(&fake_llm_completions, 1);
  }
  char resp[1600];
  int n = snprintf(resp, sizeof resp, "HTTP/1.1 200 OK\r\nContent-Type: "
                   "application/json\r\nContent-Length: %zu\r\nConnection: "
                   "close\r\n\r\n%s", strlen(body), body);
  if (send(c, resp, (size_t)n, 0) < 0) { /* the client went away */ }
  close(c);
}

static void *fake_llm_loop(void *arg) {
  (void)arg;
  for (;;) {
    int c = accept(fake_llm_fd, NULL, NULL);
    if (c < 0) return NULL;
    fake_llm_serve(c);
  }
}

static int fake_llm_start(void) {
  fake_llm_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fake_llm_fd < 0) return -1;
  int one = 1;
  setsockopt(fake_llm_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  struct sockaddr_in a; memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = 0;
  if (bind(fake_llm_fd, (struct sockaddr *)&a, sizeof a) != 0) return -1;
  if (listen(fake_llm_fd, 16) != 0) return -1;
  socklen_t len = sizeof a;
  getsockname(fake_llm_fd, (struct sockaddr *)&a, &len);
  int port = ntohs(a.sin_port);
  pthread_t t;
  pthread_create(&t, NULL, fake_llm_loop, NULL);
  pthread_detach(t);
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  setenv("LLM_BASE_URL", url, 1);
  /* Loopback must not go through the sandbox's egress proxy. */
  setenv("NO_PROXY", "127.0.0.1,localhost", 1);
  setenv("no_proxy", "127.0.0.1,localhost", 1);
  return port;
}

/* LLM_BASE_URL back at the fake server (after fake_llm_point_at_nothing). */
static void fake_llm_point_back(void) {
  struct sockaddr_in a; socklen_t len = sizeof a;
  getsockname(fake_llm_fd, (struct sockaddr *)&a, &len);
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", ntohs(a.sin_port));
  setenv("LLM_BASE_URL", url, 1);
}

/* A port nothing listens on: bind one, read its number, close it. */
static void fake_llm_point_at_nothing(void) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in a; memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  bind(s, (struct sockaddr *)&a, sizeof a);
  socklen_t len = sizeof a;
  getsockname(s, (struct sockaddr *)&a, &len);
  close(s);
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", ntohs(a.sin_port));
  setenv("LLM_BASE_URL", url, 1);
}

#endif
