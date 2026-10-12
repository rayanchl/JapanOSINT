/* tests/unit/embed_stub.h — a bag-of-words embedding server for the pipeline
 * tests (corpus hybrid, prompt selection, relevance check, few-shot, resolver,
 * suggest). Header-only; include it from one test file.
 *
 * The same stub test_service_vec.c inlines, plus a request counter: words
 * ([a-z0-9]+, lower-cased) are hashed into D buckets and the vector is
 * L2-normalised, so two texts sharing words are close and two sharing none
 * are orthogonal. That makes every ranking in these tests DETERMINISTIC and
 * query-dependent; it says nothing about embedding quality and is not claimed
 * to. Every POST appends one line to $LOG so a test can assert that an inert
 * path sent NOTHING.
 *
 *   if (estub_start(18300, 64, "stub-bow") != 0) { skip; }
 *   estub_use();                 // JO_EMBED_URL -> the stub
 *   ... estub_posts() ...
 *   estub_stop();                // also unsets JO_EMBED_URL */
#ifndef JO_TEST_EMBED_STUB_H
#define JO_TEST_EMBED_STUB_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../core/llm.h"

static pid_t estub_pid = 0;
static int   estub_port = 0;
static char  estub_log[256];

static const char *ESTUB_PY =
  "import hashlib,json,os,re\n"
  "from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer\n"
  "D=int(os.environ.get('D','64'));M=os.environ.get('M','stub-bow');L=os.environ.get('LOG')\n"
  "def vec(t):\n"
  " v=[0.0]*D\n"
  " for w in re.findall(r'[a-z0-9]+',(t or '').lower()):\n"
  "  v[int(hashlib.sha1(w.encode()).hexdigest()[:8],16)%D]+=1.0\n"
  " n=sum(x*x for x in v)**0.5\n"
  " return [x/n for x in v] if n else [1.0/D**0.5]*D\n"
  "class H(BaseHTTPRequestHandler):\n"
  " def s(self,o,c=200):\n"
  "  b=json.dumps(o).encode();self.send_response(c)\n"
  "  self.send_header('Content-Type','application/json')\n"
  "  self.send_header('Content-Length',str(len(b)));self.end_headers()\n"
  "  self.wfile.write(b)\n"
  " def do_GET(self):\n"
  "  self.s({'data':[{'id':M}]}) if '/v1/models' in self.path else self.s({'ok':1})\n"
  " def do_POST(self):\n"
  "  n=int(self.headers.get('Content-Length',0))\n"
  "  b=json.loads(self.rfile.read(n) or b'{}');i=b.get('input',[])\n"
  "  i=[i] if isinstance(i,str) else i\n"
  "  if L:\n"
  "   with open(L,'a') as f: f.write(json.dumps(len(i))+'\\n')\n"
  "  self.s({'data':[{'index':k,'embedding':vec(t)} for k,t in enumerate(i)]})\n"
  " def log_message(self,*a):pass\n"
  "ThreadingHTTPServer(('127.0.0.1',int(os.environ['P'])),H).serve_forever()\n";

static int estub_answers(int port) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  llm_client c = { .http = NULL, .base_url = url, .interactive = 1 };
  const char *t[1] = { "ping" };
  float *v = NULL; int d = 0; llm_status st;
  int ok = (llm_embed(&c, t, 1, &v, &d, 500, &st) == 0);
  free(v);
  return ok;
}

static void estub_stop(void) {
  if (estub_pid > 0) { kill(estub_pid, SIGKILL); waitpid(estub_pid, NULL, 0); }
  estub_pid = 0;
  unsetenv("JO_EMBED_URL");
  unsetenv("JO_EMBED_QUERY_URL");
}

/* POSTs received since start (or estub_log_reset), probes included. */
static int estub_posts(void) {
  FILE *f = fopen(estub_log, "r");
  if (!f) return 0;
  int n = 0, c;
  while ((c = fgetc(f)) != EOF) if (c == '\n') n++;
  fclose(f);
  return n;
}
static void estub_log_reset(void) { FILE *f = fopen(estub_log, "w"); if (f) fclose(f); }

static void estub_use(void) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", estub_port);
  setenv("JO_EMBED_URL", url, 1);
}

/* Own the port or fail (test_service_vec.c's port_busy() says why). */
static int estub_start(int port, int dim, const char *model) {
  estub_stop();
  for (int tries = 0; tries < 40 && estub_answers(port); tries++) port++;
  if (estub_answers(port)) return -1;
  char path[256];
  snprintf(path, sizeof path, "/tmp/jo_estub_%d.py", (int)getpid());
  snprintf(estub_log, sizeof estub_log, "/tmp/jo_estub_%d.log", (int)getpid());
  estub_log_reset();
  FILE *f = fopen(path, "w");
  if (!f) return -1;
  fputs(ESTUB_PY, f);
  fclose(f);
  fflush(stdout);                 /* or the child repeats buffered lines */
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ds[16], ps[16];
    snprintf(ds, sizeof ds, "%d", dim);
    snprintf(ps, sizeof ps, "%d", port);
    setenv("D", ds, 1); setenv("P", ps, 1); setenv("M", model, 1);
    setenv("LOG", estub_log, 1);
    /* Die with the test, and never hold its stdout: a test that aborts on an
     * assert would otherwise leave the stub alive, owning the port and the
     * pipe a `| tail` is waiting on. */
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (!freopen("/dev/null", "w", stdout)) { /* stub noise is harmless */ }
    if (!freopen("/dev/null", "w", stderr)) { /* stub noise is harmless */ }
    execlp("python3", "python3", path, (char *)NULL);
    _exit(127);
  }
  estub_pid = p; estub_port = port;
  for (int i = 0; i < 100; i++) {
    if (estub_answers(port)) { estub_log_reset(); return 0; }
    usleep(100000);
  }
  return -1;
}

#endif
