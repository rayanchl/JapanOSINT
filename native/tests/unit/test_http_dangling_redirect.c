/* tests/unit/test_http_dangling_redirect.c — a redirect that names no target
 * is retried; a 304 is not.
 *
 * WHAT IS BEING PROTECTED (2026-11-11). openapi.twse.com.tw sits behind a CDN
 * that answers from cache with 200 JSON and, on a miss its origin refuses,
 * with a 307 carrying a 947-byte "FOR SECURITY REASONS" page and no Location.
 * libcurl follows every 3xx that names a Location, so a 3xx that reaches the
 * caller named none: an incomplete answer, worth the second request the
 * caller already budgets for 5xx and 429. Against a local stub
 * (python3 http.server, 127.0.0.1):
 *   1. 307-without-Location, then 200: http_request returns the 200, two calls;
 *   2. 307-without-Location every time: the 307 comes back after 1 + retries;
 *   3. 304 Not Modified: returned at once, no retry (it IS the answer). */
#include "../../core/httpclient.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *STUB_PY =
  "import os\n"
  "from http.server import BaseHTTPRequestHandler,HTTPServer\n"
  "N={}\n"
  "class H(BaseHTTPRequestHandler):\n"
  " def do_GET(self):\n"
  "  p=self.path\n"
  "  if p.startswith('/count/'):\n"
  "   b=str(N.get(p[6:],0)).encode();self.send_response(200);self.end_headers();self.wfile.write(b);return\n"
  "  N[p]=N.get(p,0)+1\n"
  "  wall=b'<html><body>FOR SECURITY REASONS, THIS PAGE CAN NOT BE ACCESSED!</body></html>'\n"
  "  if p=='/up': self.send_response(200);self.end_headers();self.wfile.write(b'up');return\n"
  "  if p=='/flaky' and N[p]>=2:\n"
  "   b=b'[{\"id\":1}]';self.send_response(200);self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b);return\n"
  "  if p in ('/flaky','/wall'):\n"
  "   self.send_response(307);self.send_header('Content-Length',str(len(wall)));self.end_headers();self.wfile.write(wall);return\n"
  "  if p=='/notmod': self.send_response(304);self.end_headers();return\n"
  "  self.send_response(404);self.end_headers()\n"
  " def log_message(self,*a):pass\n"
  "HTTPServer(('127.0.0.1',int(os.environ['P'])),H).serve_forever()\n";

static pid_t g_stub;
static char g_path[64];

static long get(http_client *hc, int port, const char *path, int retries, char *body, size_t cap) {
  char url[160];
  snprintf(url, sizeof url, "http://127.0.0.1:%d%s", port, path);
  http_response hr = {0};
  int rc = http_request(hc, "GET", url, NULL, NULL, 0, 3000, retries, &hr);
  long st = rc == 0 ? hr.status : -1;
  if (body && cap) snprintf(body, cap, "%s", hr.body ? hr.body : "");
  http_response_free(&hr);
  return st;
}

static int stub_start(int port) {
  snprintf(g_path, sizeof g_path, "/tmp/jo_redir_stub_%d.py", (int)getpid());
  FILE *f = fopen(g_path, "w");
  if (!f) return -1;
  fputs(STUB_PY, f);
  fclose(f);
  fflush(stdout);
  g_stub = fork();
  if (g_stub < 0) return -1;
  if (g_stub == 0) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    setenv("P", ps, 1);
    if (!freopen("/dev/null", "w", stderr)) { /* harmless */ }
    execlp("python3", "python3", g_path, (char *)NULL);
    _exit(127);
  }
  http_client *hc = http_client_new();
  for (int i = 0; i < 100; i++) {
    if (get(hc, port, "/up", 0, NULL, 0) == 200) { http_client_free(hc); return 0; }
    usleep(100 * 1000);
  }
  http_client_free(hc);
  return -1;
}

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
  unlink(g_path);
}

/* An assert that fires must not leave the stub serving (it holds the
 * suite's stdout open, so the run would hang rather than fail). */
static void on_abort(int sig) { (void)sig; stub_stop(); signal(SIGABRT, SIG_DFL); abort(); }

static long count(http_client *hc, int port, const char *path) {
  char p[96], b[32];
  snprintf(p, sizeof p, "/count%s", path);
  get(hc, port, p, 0, b, sizeof b);
  return atol(b);
}

int main(void) {
  http_client_global_init();
  int port = 18733;
  if (stub_start(port) != 0) {
    printf("  SKIP: no python3 http.server for the stub\n");
    stub_stop();
    return 0;
  }
  signal(SIGABRT, on_abort);
  http_client *hc = http_client_new();
  char body[256];

  long st = get(hc, port, "/flaky", 2, body, sizeof body);
  assert(st == 200 && strstr(body, "\"id\":1"));
  assert(count(hc, port, "/flaky") == 2);
  printf("  a 307 naming no target is retried; the 200 behind it is returned: ok\n");

  st = get(hc, port, "/wall", 2, body, sizeof body);
  assert(st == 307 && strstr(body, "FOR SECURITY REASONS"));
  assert(count(hc, port, "/wall") == 3);
  printf("  a 307 that never clears comes back after 1 + retries requests: ok\n");

  st = get(hc, port, "/notmod", 2, NULL, 0);
  assert(st == 304);
  assert(count(hc, port, "/notmod") == 1);
  printf("  a 304 is the answer, not retried: ok\n");

  http_client_free(hc);
  stub_stop();
  printf("all passed\n");
  return 0;
}
