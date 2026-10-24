/* tests/unit/test_ckan_paging.c — OPENDATA_CKAN reads EVERY match a portal
 * holds for the entity, and says in the data what it did not read.
 *
 * WHAT IS BEING PROTECTED. collectors/sources/opendata_ckan.c asked each of
 * its ~40 portals for `rows=5` and never paged, so a query matching 2,500
 * datasets on one portal stored five and recorded nothing about the other
 * 2,495. Against a local stub CKAN portal (python3 http.server, 127.0.0.1):
 *   1. a portal whose rows_max is 300 (below the 1000 asked for) is walked by
 *      what each page HELD, to its own result.count: 2,500 of 2,500, no notice;
 *   2. a portal that ignores `start` is read once and disclosed, not re-read
 *      until the ceiling;
 *   3. a later page that fails is disclosed with the records used, the
 *      portal's total and the failed status;
 *   4. the page ceiling (JO_PAGE_MAX) is disclosed with the portal's total;
 *   5. a portal whose first page does not answer is reported back to the
 *      caller (it lands in the run's coverage notice, rule 1).
 *
 * Includes opendata_ckan.c to reach the static query_portal(); run.sh links
 * every other object. */

#include "../../collectors/sources/opendata_ckan.c"

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *STUB_PY =
  "import json,os,urllib.parse as u\n"
  "from http.server import BaseHTTPRequestHandler,HTTPServer\n"
  "TOTAL=2500;MAXROWS=300\n"
  "class H(BaseHTTPRequestHandler):\n"
  " def do_GET(self):\n"
  "  p=u.urlparse(self.path);qs=u.parse_qs(p.query)\n"
  "  start=int(qs.get('start',['0'])[0]);rows=min(int(qs.get('rows',['10'])[0]),MAXROWS)\n"
  "  mode=p.path.split('/')[1]\n"
  "  if mode=='dead': self.send_response(503);self.end_headers();return\n"
  "  if mode=='fail' and start>0: self.send_response(500);self.end_headers();return\n"
  "  if mode=='ign': start=0\n"
  "  res=[{'id':'d%d'%i,'name':'ds-%d'%i,'title':'Dataset %d'%i}\n"
  "       for i in range(start,min(start+rows,TOTAL))]\n"
  "  b=json.dumps({'success':True,'result':{'count':TOTAL,'results':res}}).encode()\n"
  "  self.send_response(200);self.send_header('Content-Type','application/json')\n"
  "  self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)\n"
  " def log_message(self,*a):pass\n"
  "HTTPServer(('127.0.0.1',int(os.environ['P'])),H).serve_forever()\n";

static pid_t g_stub;

static int stub_start(int port) {
  char path[64];
  snprintf(path, sizeof path, "/tmp/jo_ckan_stub_%d.py", (int)getpid());
  FILE *f = fopen(path, "w");
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
    execlp("python3", "python3", path, (char *)NULL);
    _exit(127);
  }
  /* Wait until it answers. */
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d/ok/api/3/action/package_search?q=x&rows=1&start=0", port);
  http_client *hc = http_client_new();
  for (int i = 0; i < 100; i++) {
    http_response hr = {0};
    int rc = http_request(hc, "GET", url, NULL, NULL, 0, 1000, 0, &hr);
    long st = hr.status;
    http_response_free(&hr);
    if (rc == 0 && st == 200) { http_client_free(hc); return 0; }
    usleep(100 * 1000);
  }
  http_client_free(hc);
  return -1;
}

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
  char path[64];
  snprintf(path, sizeof path, "/tmp/jo_ckan_stub_%d.py", (int)getpid());
  unlink(path);
}

/* ── capturing sink: counts datasets, keeps the last notice's properties ── */
static long g_datasets, g_notices;
static char g_notice[4096];

static int cap_emit(intel_sink *s, const intel_item *it) {
  (void)s;
  if (it->record_type && !strcmp(it->record_type, "ckan-dataset")) g_datasets++;
  else {
    g_notices++;
    snprintf(g_notice, sizeof g_notice, "%s",
             it->properties_json ? it->properties_json : "");
  }
  return 1;
}

static long notice_num(const char *key) {
  cJSON *p = cJSON_Parse(g_notice);
  const cJSON *v = p ? cJSON_GetObjectItem(p, key) : NULL;
  long n = cJSON_IsNumber(v) ? (long)v->valuedouble : -2;
  cJSON_Delete(p);
  return n;
}

static int walk(source_ctx *ctx, intel_sink *sink, int port, const char *mode,
                int page_max, long *status) {
  char base[96];
  snprintf(base, sizeof base, "http://127.0.0.1:%d/%s", port, mode);
  ckan_portal pt = { base, mode, "ZZ" };
  g_datasets = g_notices = 0;
  g_notice[0] = 0;
  return query_portal(ctx, sink, &pt, "tokyo", "tokyo", page_max, status);
}

int main(void) {
  http_client_global_init();
  int port = 18731;
  if (stub_start(port) != 0) {
    printf("  SKIP: no python3 http.server for the stub portal\n");
    stub_stop();
    return 0;
  }
  source_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.http = http_client_new();
  intel_sink sink = { .ctx = NULL, .emit = cap_emit };
  long st = 0;

  /* 1. every match, stepping by what each 300-row page held */
  int n = walk(&ctx, &sink, port, "ok", 100, &st);
  assert(n == 2500 && g_datasets == 2500 && g_notices == 0);
  printf("  all 2,500 matches read at a 300-row rows_max, no notice: ok\n");

  /* 2. start ignored: one page, disclosed */
  n = walk(&ctx, &sink, port, "ign", 100, &st);
  assert(n == 300 && g_notices == 1);
  assert(notice_num("records_used") == 300 && notice_num("records_available") == 2500);
  assert(strstr(g_notice, "ignored `start`"));
  printf("  a portal ignoring start: read once, disclosed 300 of 2500: ok\n");

  /* 3. later page fails */
  n = walk(&ctx, &sink, port, "fail", 100, &st);
  assert(n == 300 && g_notices == 1);
  assert(notice_num("failed_page_status") == 500 && notice_num("records_available") == 2500);
  printf("  a failed later page: disclosed with its status: ok\n");

  /* 4. the ceiling */
  n = walk(&ctx, &sink, port, "ok", 3, &st);
  assert(n == 900 && g_notices == 1);
  assert(notice_num("records_used") == 900 && notice_num("records_available") == 2500);
  assert(strstr(g_notice, "JO_PAGE_MAX"));
  printf("  page ceiling: 900 of 2500 disclosed, remedy names JO_PAGE_MAX: ok\n");

  /* 5. dead portal */
  n = walk(&ctx, &sink, port, "dead", 100, &st);
  assert(n == -1 && st == 503 && g_datasets == 0);
  printf("  a portal that does not answer is reported with its status: ok\n");

  http_client_free(ctx.http);
  stub_stop();
  printf("test_ckan_paging: OK\n");
  return 0;
}
