/* tests/unit/test_arcgis_dir.c — lib/arcgis_dir.c against a local stub ArcGIS
 * services directory (python3 http.server on 127.0.0.1).  Pins that a folder
 * list of bare strings is not mistaken for records (the services are), the
 * composed `id` / `service_url`, that upstream-supplied id/url are never
 * overwritten, that a folder whose listing fails is DISCLOSED as a truncation
 * notice while the other folders are still emitted, and the -1 contract (root
 * unreachable, or everything failed and nothing emitted).  Reached through the
 * header. */
#include "../../lib/arcgis_dir.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

static const char *STUB_PY =
  "import json,os\n"
  "from http.server import BaseHTTPRequestHandler,HTTPServer\n"
  "def send(h,o,c=200):\n"
  " b=json.dumps(o).encode();h.send_response(c);h.send_header('Content-Type','application/json')\n"
  " h.send_header('Content-Length',str(len(b)));h.end_headers();h.wfile.write(b)\n"
  "class H(BaseHTTPRequestHandler):\n"
  " def do_GET(self):\n"
  "  p=self.path.split('?')[0].strip('/').split('/')\n"
  "  mode=p[0];rest=p[3:]\n"
  "  if mode=='dead': self.send_response(503);self.end_headers();return\n"
  "  if not rest:\n"
  "   if mode=='ok': return send(self,{'folders':['Hydro','Hazards','Broken'],'services':[{'name':'Basemap','type':'MapServer'}]})\n"
  "   if mode=='allfail': return send(self,{'folders':['A','B'],'services':[]})\n"
  "   if mode=='foldersonly': return send(self,{'folders':['Hydro']})\n"
  "   if mode=='html': self.send_response(200);self.end_headers();self.wfile.write(b'<html>maintenance</html>');return\n"
  "  f=rest[0]\n"
  "  if f=='Broken' or mode=='allfail': self.send_response(500);self.end_headers();return\n"
  "  if f=='Hydro': return send(self,{'services':[{'name':'Hydro/Rivers','type':'MapServer'},{'name':'Hydro/Rivers','type':'FeatureServer'},{'name':'Hydro/NoType'}]})\n"
  "  if f=='Hazards': return send(self,{'services':[{'id':'upstream-id','name':'Hazards/Quake','type':'MapServer','service_url':'https://elsewhere/x','extra':{'k':1}}]})\n"
  "  send(self,{'services':[]})\n"
  " def log_message(self,*a):pass\n"
  "HTTPServer(('127.0.0.1',int(os.environ['P'])),H).serve_forever()\n";

static pid_t g_stub;
static int stub_start(int port) {
  char path[64];
  snprintf(path, sizeof path, "/tmp/jo_arcgis_stub_%d.py", (int)getpid());
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
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d/ok/rest/services?f=json", port);
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
  if (g_stub > 0) { kill(g_stub, SIGTERM); waitpid(g_stub, NULL, 0); }
}

/* A capturing sink. */
typedef struct { cJSON *items; } cap;
static int cap_emit(intel_sink *s, const intel_item *it) {
  cap *c = s->ctx;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "key", it->remote_key ? it->remote_key : "");
  cJSON_AddStringToObject(o, "type", it->record_type ? it->record_type : "");
  cJSON_AddStringToObject(o, "title", it->title ? it->title : "");
  cJSON_AddStringToObject(o, "props", it->properties_json ? it->properties_json : "");
  cJSON_AddItemToArray(c->items, o);
  return 1;
}
static intel_sink mk(cap *c) {
  c->items = cJSON_CreateArray();
  intel_sink s;
  s.ctx = c;
  s.emit = cap_emit;
  return s;
}
static int count_type(cap *c, const char *t) {
  int n = 0;
  cJSON *o;
  cJSON_ArrayForEach(o, c->items) if (!strcmp(cJSON_GetObjectItem(o, "type")->valuestring, t)) n++;
  return n;
}
static cJSON *props_named(cap *c, const char *name) {
  cJSON *o;
  cJSON_ArrayForEach(o, c->items) {
    cJSON *p = cJSON_Parse(cJSON_GetObjectItem(o, "props")->valuestring);
    cJSON *n = p ? cJSON_GetObjectItem(p, "name") : NULL;
    if (cJSON_IsString(n) && !strcmp(n->valuestring, name)) return p;
    cJSON_Delete(p);
  }
  return NULL;
}
static cJSON *props_of(cap *c, const char *key_substr) {
  cJSON *o;
  cJSON_ArrayForEach(o, c->items)
    if (strstr(cJSON_GetObjectItem(o, "key")->valuestring, key_substr))
      return cJSON_Parse(cJSON_GetObjectItem(o, "props")->valuestring);
  return NULL;
}

int main(void) {
  int port = 18377;
  if (stub_start(port) != 0) {
    printf("test_arcgis_dir: stub unavailable, skipped\n");
    stub_stop();
    return 0;
  }
  http_client *hc = http_client_new();
  source_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.http = hc;
  char base[160];

  printf("-- argument gates\n");
  cap c0;
  intel_sink s0 = mk(&c0);
  ok(arcgis_dir_walk(NULL, &s0, "t", "http://x", "r", "en", NULL) == -1, "NULL ctx -> -1");
  ok(arcgis_dir_walk(&ctx, NULL, "t", "http://x", "r", "en", NULL) == -1, "NULL sink -> -1");
  ok(arcgis_dir_walk(&ctx, &s0, NULL, "http://x", "r", "en", NULL) == -1, "NULL source id -> -1");
  ok(arcgis_dir_walk(&ctx, &s0, "t", NULL, "r", "en", NULL) == -1, "NULL base -> -1");
  ok(cJSON_GetArraySize(c0.items) == 0, "nothing emitted by a refused call");
  cJSON_Delete(c0.items);

  printf("-- a directory with a failing folder\n");
  snprintf(base, sizeof base, "http://127.0.0.1:%d/ok/rest/services", port);
  cap c1;
  intel_sink s1 = mk(&c1);
  int rc = arcgis_dir_walk(&ctx, &s1, "unit-arcgis", base, "service", "en", "[\"arcgis\"]");
  ok(rc == 0, "returns 0: records were emitted even though one folder failed");
  ok(count_type(&c1, "service") >= 4, "root service + 3 Hydro + 1 Hazards services emitted (folder names alone are NOT records)");
  cJSON *p = props_of(&c1, "Basemap");
  ok(p && !strcmp(cJSON_GetObjectItem(p, "service_url")->valuestring, "http://127.0.0.1:18377/ok/rest/services/Basemap/MapServer"),
     "root service_url composed as base/name/type");
  ok(p && !strcmp(cJSON_GetObjectItem(p, "id")->valuestring, "Basemap/MapServer"), "id composed as name/type");
  cJSON_Delete(p);
  p = props_of(&c1, "Hydro/Rivers/MapServer");
  ok(p != NULL, "the same name published as MapServer and FeatureServer yields two distinct records");
  cJSON_Delete(p);
  p = props_of(&c1, "Hydro/Rivers/FeatureServer");
  ok(p != NULL, "...and the FeatureServer one is present");
  cJSON_Delete(p);
  p = props_of(&c1, "upstream-id");
  ok(p && !strcmp(cJSON_GetObjectItem(p, "service_url")->valuestring, "https://elsewhere/x") &&
         cJSON_GetObjectItem(cJSON_GetObjectItem(p, "extra"), "k")->valueint == 1,
     "an upstream-supplied id/service_url is kept, and every other upstream field survives");
  cJSON_Delete(p);
  p = props_named(&c1, "Hydro/NoType");
  ok(p != NULL && cJSON_GetObjectItem(p, "service_url") == NULL,
     "a service with no type is still EMITTED (nothing dropped) but gets no invented id/service_url");
  cJSON_Delete(p);
  ok(count_type(&c1, "collector-truncation-notice") == 1, "the failed 'Broken' folder is disclosed as one truncation notice");
  cJSON *o;
  int mentions = 0;
  cJSON_ArrayForEach(o, c1.items)
    if (!strcmp(cJSON_GetObjectItem(o, "type")->valuestring, "collector-truncation-notice") &&
        strstr(cJSON_GetObjectItem(o, "props")->valuestring, "1 of 3 folder listing(s) failed")) mentions = 1;
  ok(mentions, "the notice says 1 of 3 folders failed");
  cJSON_Delete(c1.items);

  printf("-- failure contracts\n");
  snprintf(base, sizeof base, "http://127.0.0.1:%d/dead/rest/services", port);
  cap c2;
  intel_sink s2 = mk(&c2);
  ok(arcgis_dir_walk(&ctx, &s2, "unit-arcgis", base, "service", "en", NULL) == -1 && cJSON_GetArraySize(c2.items) == 0,
     "root unreachable -> -1 and nothing emitted (not an empty success)");
  cJSON_Delete(c2.items);
  snprintf(base, sizeof base, "http://127.0.0.1:%d/html/rest/services", port);
  cap c3;
  intel_sink s3 = mk(&c3);
  ok(arcgis_dir_walk(&ctx, &s3, "unit-arcgis", base, "service", "en", NULL) == -1, "a 200 HTML maintenance page is a failed fetch, not an empty directory");
  cJSON_Delete(c3.items);
  snprintf(base, sizeof base, "http://127.0.0.1:%d/allfail/rest/services", port);
  cap c4;
  intel_sink s4 = mk(&c4);
  ok(arcgis_dir_walk(&ctx, &s4, "unit-arcgis", base, "service", "en", NULL) == -1, "every folder failed and nothing emitted -> -1");
  ok(count_type(&c4, "collector-truncation-notice") == 1, "...and the failure is still disclosed in the data");
  cJSON_Delete(c4.items);
  snprintf(base, sizeof base, "http://127.0.0.1:%d/foldersonly/rest/services", port);
  cap c5;
  intel_sink s5 = mk(&c5);
  ok(arcgis_dir_walk(&ctx, &s5, "unit-arcgis", base, "service", "en", NULL) == 0, "folders-only root succeeds");
  ok(count_type(&c5, "service") == 3, "a root with no services of its own: all three of the folder's entries are emitted");
  ok(count_type(&c5, "collector-truncation-notice") == 0, "no notice when nothing failed");
  cJSON_Delete(c5.items);

  http_client_free(hc);
  stub_stop();
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
