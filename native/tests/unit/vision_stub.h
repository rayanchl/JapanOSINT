/* tests/unit/vision_stub.h — the REAL vision sidecar in stub mode
 * (scripts/vision_embed_server.py --stub) for the image-index tests.
 * Header-only; include it from one test file.
 *
 * --stub hashes each input into a deterministic unit vector (model
 * "stub-sha256-v1"): the same bytes always give the same vector, different
 * bytes different ones, and a body without an image signature comes back in
 * `errors` as "not_an_image". It is the production wire format with no ML
 * dependencies, so what these tests drive is the real client against the real
 * server, not a lookalike. The server's per-request log line goes to a file so
 * a test can count what was actually sent.
 *
 *   if (vstub_start(18600, 64) != 0) { skip; }   // sets JO_VISION_EMBED_URL
 *   ... vstub_posts("/embed/image") ...
 *   vstub_stop();                                // also unsets it */
#ifndef JO_TEST_VISION_STUB_H
#define JO_TEST_VISION_STUB_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "../../core/httpclient.h"

static pid_t vstub_pid = 0;
static int   vstub_port = 0;
static char  vstub_log[256];

static int vstub_answers(int port) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d/health", port);
  http_client *h = http_client_new();
  http_response r = {0};
  int ok = h && http_request(h, "GET", url, NULL, NULL, 0, 500, 0, &r) == 0 &&
           r.status == 200;
  http_response_free(&r);
  if (h) http_client_free(h);
  return ok;
}

static void vstub_stop(void) {
  if (vstub_pid > 0) { kill(vstub_pid, SIGKILL); waitpid(vstub_pid, NULL, 0); }
  vstub_pid = 0;
  unsetenv("JO_VISION_EMBED_URL");
}

/* Lines of the server's request log naming `path` ("POST /embed/image"). */
__attribute__((unused)) static int vstub_posts(const char *path) {
  FILE *f = fopen(vstub_log, "r");
  if (!f) return 0;
  char line[1024], want[128];
  snprintf(want, sizeof want, "\"POST %s ", path);
  int n = 0;
  while (fgets(line, sizeof line, f)) if (strstr(line, want)) n++;
  fclose(f);
  return n;
}

static int vstub_start(int port, int dim) {
  vstub_stop();
  if (access("../scripts/vision_embed_server.py", R_OK) != 0) return -1;
  for (int tries = 0; tries < 40 && vstub_answers(port); tries++) port++;
  if (vstub_answers(port)) return -1;
  snprintf(vstub_log, sizeof vstub_log, "/tmp/jo_vstub_%d.log", (int)getpid());
  FILE *lf = fopen(vstub_log, "w");
  if (lf) fclose(lf);
  fflush(stdout);
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ds[16], ps[16];
    snprintf(ds, sizeof ds, "%d", dim);
    snprintf(ps, sizeof ps, "%d", port);
    setenv("VISION_STUB_DIM", ds, 1);
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (!freopen("/dev/null", "w", stdout)) { /* harmless */ }
    if (!freopen(vstub_log, "a", stderr)) { /* then nothing is counted */ }
    execlp("python3", "python3", "../scripts/vision_embed_server.py", "--stub",
           "--host", "127.0.0.1", "--port", ps, (char *)NULL);
    _exit(127);
  }
  vstub_pid = p;
  vstub_port = port;
  for (int i = 0; i < 100; i++) {
    if (vstub_answers(port)) {
      char url[128];
      snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
      setenv("JO_VISION_EMBED_URL", url, 1);
      return 0;
    }
    struct timespec ts = { 0, 50 * 1000000L };
    nanosleep(&ts, NULL);
  }
  vstub_stop();
  return -1;
}

#endif
