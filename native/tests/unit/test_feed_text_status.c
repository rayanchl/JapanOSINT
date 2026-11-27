/* test_feed_text_status.c — the TEXT fetch path records what it met.
 *
 * Until 2026-11-16 feed_get_text() returned NULL on any non-2xx and kept
 * neither the status nor the body, so a bespoke collector behind a
 * maintenance 503 (JAIRO Cloud's 690 bytes, measured 2026-11-12, whose remedy
 * is WAIT) could not tell it from a moved endpoint. CLAUDE.md recorded that as
 * "written down, not half-fixed". The slot is now set from the text path and
 * the refused body is kept for the one question it answers.
 *
 * Against the old lib/feedlib.c this cannot pass and was not run as a
 * failing build: the text path wrote nothing to the status slot (assertion 1
 * would read FEED_ST_UNKNOWN) and feed_last_failed_body() did not exist.
 *
 * Controls: a 200 leaves NO failed body (3), a later success CLEARS an earlier
 * refusal's body (4) — a stale notice attributed to the next fetch would be
 * the disclosure inventing its evidence — and a plain 404 is not read as
 * maintenance (5). */
#include "../../lib/feedlib.h"
#include "../../lib/jocore.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* The measured JAIRO Cloud body's shape: no <title>, the publisher's own
 * misspelling kept. */
static const char MAINT[] =
  "<html><head><meta charset=\"utf-8\"></head><body>"
  "<p>\xe7\x8f\xbe\xe5\x9c\xa8\xe3\x82\xb7\xe3\x82\xb9\xe3\x83\x86\xe3\x83\xa0"
  "\xe3\x83\xa1\xe3\x83\xb3\xe3\x83\x86\xe3\x83\x8a\xe3\x83\xb3\xe3\x82\xb9"
  "</p><p>Repository is temporary unavaliable due to system maintenance.</p>"
  "</body></html>";

static void *serve_one(void *vp) {
  int fd = (int)(intptr_t)vp;
  char buf[4096]; size_t got = 0;
  for (;;) {
    ssize_t r = recv(fd, buf + got, sizeof buf - 1 - got, 0);
    if (r <= 0) break;
    got += (size_t)r; buf[got] = 0;
    if (strstr(buf, "\r\n\r\n")) break;
  }
  const char *st = "200 OK", *body = "hello";
  if (strstr(buf, "GET /maint")) { st = "503 Service Unavailable"; body = MAINT; }
  else if (strstr(buf, "GET /gone")) { st = "404 Not Found"; body = "<html><body>Not Found</body></html>"; }
  char resp[2048];
  int n = snprintf(resp, sizeof resp,
                   "HTTP/1.1 %s\r\nContent-Type: text/html\r\n"
                   "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
                   st, strlen(body), body);
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

int main(void) {
  http_client_global_init();
  int s = socket(AF_INET, SOCK_STREAM, 0);
  assert(s >= 0);
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  struct sockaddr_in a = { .sin_family = AF_INET,
                           .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
  assert(bind(s, (struct sockaddr *)&a, sizeof a) == 0);
  socklen_t al = sizeof a;
  getsockname(s, (struct sockaddr *)&a, &al);
  assert(listen(s, 16) == 0);
  pthread_t at;
  assert(pthread_create(&at, NULL, acceptor, (void *)(intptr_t)s) == 0);
  pthread_detach(at);
  char url[128];
  int port = ntohs(a.sin_port);

  /* 1+2: a maintenance 503 — status recorded, body kept, and it reads as
   * maintenance although it carries no <title>. */
  feed_last_json_status_reset();
  snprintf(url, sizeof url, "http://127.0.0.1:%d/maint", port);
  char *t = feed_get_text(NULL, url, 5000);
  assert(t == NULL);
  assert(feed_last_json_status() == 503);                         /* 1 */
  assert(feed_last_failed_body() != NULL);
  assert(jo_body_says_maintenance(feed_last_failed_body()));      /* 2 */

  /* 3+4: a 200 returns its body, records 200, and clears the refused body. */
  snprintf(url, sizeof url, "http://127.0.0.1:%d/ok", port);
  t = feed_get_text(NULL, url, 5000);
  assert(t && strcmp(t, "hello") == 0);
  free(t);
  assert(feed_last_json_status() == 200);                         /* 3 */
  assert(feed_last_failed_body() == NULL);                        /* 4 */

  /* 5: an ordinary 404 is recorded and is NOT maintenance. */
  snprintf(url, sizeof url, "http://127.0.0.1:%d/gone", port);
  t = feed_get_text_max(NULL, url, 5000, 0);
  assert(t == NULL);
  assert(feed_last_json_status() == 404);
  assert(feed_last_failed_body() != NULL);
  assert(!jo_body_says_maintenance(feed_last_failed_body()));     /* 5 */

  printf("test_feed_text_status: all ok\n");
  return 0;
}
