/* tests/unit/test_http_egress_proxy.c — private addresses are refused by
 * default, and the operator's own egress proxy is not one of them.
 *
 * WHAT IS BEING PROTECTED (2026-11-14). Since 2026-11-12 every outbound fetch
 * refuses loopback and RFC1918 peers unless JO_HTTP_BLOCK_PRIVATE=0. Behind a
 * forward proxy the peer libcurl connects to is the PROXY, and an egress proxy
 * commonly listens on loopback: in this tree's own cloud container
 * HTTPS_PROXY=http://127.0.0.1:<port>, and the first strict build refused every
 * fetch the engine made ("blocked connection to 127.0.0.1") while
 * JO_HTTP_BLOCK_PRIVATE=0 stored 429 records for the same run. Against a local
 * forward-proxy stub (python3 http.server answering absolute-URI requests):
 *   1. with http_proxy naming the stub and the private block at its DEFAULT,
 *      a fetch of an outside URL goes through the proxy and answers 200;
 *   2. the stub saw the absolute URI, so it really was used as the proxy;
 *   3. a DIRECT fetch of a loopback URL is still refused (the block holds for
 *      everything that is not the configured proxy endpoint). */
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
  "class H(BaseHTTPRequestHandler):\n"
  " def do_GET(self):\n"
  "  b=('proxied:'+self.path).encode()\n"
  "  self.send_response(200);self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)\n"
  " def log_message(self,*a):pass\n"
  "HTTPServer(('127.0.0.1',int(os.environ['P'])),H).serve_forever()\n";

static pid_t g_stub;
static char g_path[64];

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
  unlink(g_path);
}
static void on_abort(int sig) { (void)sig; stub_stop(); signal(SIGABRT, SIG_DFL); abort(); }

static int stub_start(int port) {
  snprintf(g_path, sizeof g_path, "/tmp/jo_proxy_stub_%d.py", (int)getpid());
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
  /* readiness: the stub answers any absolute-URI request; probe it through
   * the lenient service call so the probe does not depend on the proxy rule */
  http_client *hc = http_client_new();
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d/up", port);
  for (int i = 0; i < 100; i++) {
    http_response r = {0};
    int ok = http_request_service(hc, "GET", url, NULL, NULL, 0, 2000, 0, &r) == 0
             && r.status == 200;
    http_response_free(&r);
    if (ok) { http_client_free(hc); return 0; }
    usleep(100 * 1000);
  }
  http_client_free(hc);
  return -1;
}

int main(void) {
  int port = 18741;
  char proxy[64];
  snprintf(proxy, sizeof proxy, "http://127.0.0.1:%d", port);
  /* Strict default, a proxy named in the environment BEFORE the first
   * request (libcurl and the engine both read it from there). */
  unsetenv("JO_HTTP_BLOCK_PRIVATE");
  unsetenv("no_proxy"); unsetenv("NO_PROXY");
  unsetenv("https_proxy"); unsetenv("HTTPS_PROXY");
  unsetenv("all_proxy"); unsetenv("ALL_PROXY");
  setenv("http_proxy", proxy, 1);
  http_client_global_init();
  if (stub_start(port) != 0) {
    printf("  SKIP: no python3 http.server for the stub\n");
    stub_stop();
    return 0;
  }
  signal(SIGABRT, on_abort);
  http_client *hc = http_client_new();

  http_response r = {0};
  int rc = http_request(hc, "GET", "http://upstream.jo-test.invalid/x", NULL,
                        NULL, 0, 3000, 0, &r);
  assert(rc == 0 && r.status == 200);
  assert(r.body && strstr(r.body, "proxied:http://upstream.jo-test.invalid/x"));
  http_response_free(&r);
  printf("  a fetch through the operator's loopback egress proxy is allowed by default: ok\n");

  /* The same loopback address, asked for DIRECTLY, is still a private target.
   * no_proxy makes libcurl skip the proxy for it. */
  setenv("no_proxy", "127.0.0.1", 1);
  char direct[64];
  snprintf(direct, sizeof direct, "http://127.0.0.1:%d/direct", port);
  r = (http_response){0};
  rc = http_request(hc, "GET", direct, NULL, NULL, 0, 3000, 0, &r);
  assert(rc != 0 || r.status != 200);
  http_response_free(&r);
  printf("  a direct loopback fetch is still refused under the default: ok\n");

  http_client_free(hc);
  stub_stop();
  printf("all passed\n");
  return 0;
}
