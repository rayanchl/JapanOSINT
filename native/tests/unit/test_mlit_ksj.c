/* test_mlit_ksj.c — lib/mlit_ksj.c, the MLIT 国土数値情報 normalizer.
 *
 * Against a local stub (python3 http.server on 127.0.0.1) serving KSJ-shaped
 * GeoJSON:
 *   - field aliases: the first alias that is neither null nor "" wins, and a
 *     field with no alias present is an explicit null, not an invented value;
 *   - geometry: N02 reduces a line to its middle vertex (LineString and
 *     MultiLineString), P02/C02 a polygon to the mean of its vertices with a
 *     malformed vertex skipped rather than dereferenced, N07/P11 keep the
 *     geometry whole; features with no/empty/unsupported geometry are dropped
 *     but still consume their index, so ids stay stable;
 *   - computed keys: name_ja mirrors name, N05 status is abolished iff
 *     closed_year is non-empty, N07 route_type defaults to "bus";
 *   - abolished_only keeps only abolished rows;
 *   - bare-array roots work;
 *   - mirrors: env URL first, a failed or empty URL falls through to the
 *     next, and when NOTHING could be read the run FAILS (rc<0) instead of
 *     reading as an empty source (rule 1); an upstream that answered with an
 *     empty collection is an honest 0;
 *   - an unknown code is refused. */
#include "../../lib/mlit_ksj.c"

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *STUB_PY =
  "import json,os\n"
  "from http.server import BaseHTTPRequestHandler,HTTPServer\n"
  "def f(g,p): return {'type':'Feature','geometry':g,'properties':p}\n"
  "def pt(x,y): return {'type':'Point','coordinates':[x,y]}\n"
  "D={\n"
  " '/n02':{'features':[\n"
  "  f({'type':'LineString','coordinates':[[0,0],[1,1],[2,2]]},{'N02_005':'Tokyo','N02_003':'Chuo','railwayType':'', 'N02_001':'JR'}),\n"
  "  f(pt(5,6),{'stationName':'','name':'','N02_005':'Fallback'}),\n"
  "  f(None,{'N02_005':'NoGeom'}),\n"
  "  f({'type':'MultiLineString','coordinates':[[[7,8],[9,10]],[[1,1],[2,2]]]},{'name':'Multi'}),\n"
  "  f({'type':'LineString','coordinates':[]},{'name':'EmptyLine'}),\n"
  "  f({'type':'Polygon','coordinates':[[[0,0],[1,0],[1,1],[0,0]]]},{'name':'WrongShape'}),\n"
  "  f(pt(11,12),None)]},\n"
  " '/p02':{'features':[\n"
  "  f({'type':'Polygon','coordinates':[[[0,0],[2,0],[2,2],[0,2],[0,0]]]},{'airportName':'Haneda','icao':'RJTT','iata':'HND'}),\n"
  "  f({'type':'MultiPolygon','coordinates':[[[[10,10],[12,10],[12,12],None,[],[10,12]]]]},{'P02_004':'Messy'}),\n"
  "  f({'type':'Polygon','coordinates':[[None,[]]]},{'name':'AllBad'})]},\n"
  " '/n05':{'features':[\n"
  "  f(pt(1,1),{'lineName':'Old','closedYear':1990,'openedYear':1950}),\n"
  "  f(pt(2,2),{'lineName':'Live','closedYear':''}),\n"
  "  f(pt(3,3),{'lineName':'Live2'})]},\n"
  " '/n07':{'features':[\n"
  "  f({'type':'LineString','coordinates':[[0,0],[1,1],[2,2]]},{'routeName':'R1','routeType':'rail'}),\n"
  "  f({'type':'LineString','coordinates':[[3,3],[4,4]]},{'routeName':'R2'})]},\n"
  " '/bare':[f(pt(1,2),{'name':'Bare'})],\n"
  " '/empty':{'features':[]},\n"
  " '/junk':{'foo':1},\n"
  "}\n"
  "class H(BaseHTTPRequestHandler):\n"
  " def do_GET(self):\n"
  "  p=self.path.split('?')[0]\n"
  "  if p=='/dead': self.send_response(503);self.end_headers();return\n"
  "  if p=='/html':\n"
  "   b=b'<html><body>maintenance</body></html>'\n"
  "  elif p in D: b=json.dumps(D[p]).encode()\n"
  "  else: self.send_response(404);self.end_headers();return\n"
  "  self.send_response(200);self.send_header('Content-Type','application/json')\n"
  "  self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)\n"
  " def log_message(self,*a):pass\n"
  "HTTPServer(('127.0.0.1',int(os.environ['P'])),H).serve_forever()\n";

static pid_t g_stub;
static char g_py[64];

static int stub_start(int port) {
  snprintf(g_py, sizeof g_py, "/tmp/jo_mlit_stub_%d.py", (int)getpid());
  FILE *f = fopen(g_py, "w");
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
    execlp("python3", "python3", g_py, (char *)NULL);
    _exit(127);
  }
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d/empty", port);
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
  if (g_py[0]) unlink(g_py);
}

/* ── recording sink ──────────────────────────────────────────────────────── */
#define MAXREC 32
static cJSON *g_props[MAXREC];
static cJSON *g_geom[MAXREC];
static int g_n;
static int rec_emit(intel_sink *s, const intel_item *it) {
  (void)s;
  if (g_n < MAXREC) {
    g_props[g_n] = cJSON_Parse(it->properties_json ? it->properties_json : "{}");
    g_geom[g_n] = it->geometry_geojson ? cJSON_Parse(it->geometry_geojson) : NULL;
    g_n++;
  }
  return 1;
}
static void rec_reset(void) {
  for (int i = 0; i < g_n; i++) { cJSON_Delete(g_props[i]); cJSON_Delete(g_geom[i]); }
  g_n = 0;
}
static const char *sv(int i, const char *k) {
  cJSON *v = cJSON_GetObjectItem(g_props[i], k);
  return cJSON_IsString(v) ? v->valuestring : NULL;
}
static int is_null(int i, const char *k) {
  cJSON *v = cJSON_GetObjectItem(g_props[i], k);
  return v && cJSON_IsNull(v);
}
static double coord(int i, int axis) {
  return cJSON_GetArrayItem(cJSON_GetObjectItem(g_geom[i], "coordinates"), axis)->valuedouble;
}

static int g_port;
static const char *U(const char *path) {
  static char b[4][128]; static int k;
  char *o = b[k++ & 3];
  snprintf(o, 128, "http://127.0.0.1:%d%s", g_port, path);
  return o;
}

int main(void) {
  http_client_global_init();
  g_port = 18747;
  if (stub_start(g_port) != 0) {
    printf("  SKIP: no python3 http.server for the stub\n");
    stub_stop();
    return 0;
  }
  source_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.http = http_client_new();
  ctx.source_id = "mlit-test";
  intel_sink sink = { .ctx = NULL, .emit = rec_emit };
  unsetenv("MLIT_TEST_URL");

  /* ── N02: aliases, midpoint, dropped geometry keeps its index ────────── */
  const char *m[] = { U("/n02") };
  int n = mlit_ksj_collect(&ctx, &sink, "N02", NULL, m, 1, 0);
  assert(n == 4 && g_n == 4 && "Tokyo, Fallback, Multi, and the null-properties point");
  assert(!strcmp(sv(0, "station_id"), "MLIT_N02_0") && !strcmp(sv(0, "name"), "Tokyo"));
  assert(!strcmp(sv(0, "line"), "Chuo") && !strcmp(sv(0, "classification"), "JR") &&
         "railwayType is \"\" so the next alias N02_001 answers");
  assert(is_null(0, "operator") && is_null(0, "institution_type") &&
         "no alias present: an explicit null, nothing invented");
  assert(!strcmp(sv(0, "name_ja"), "Tokyo") && !strcmp(sv(0, "source"), "mlit_n02"));
  assert(coord(0, 0) == 1 && coord(0, 1) == 1 && "middle vertex of the line");
  assert(!strcmp(sv(1, "name"), "Fallback") && !strcmp(sv(1, "station_id"), "MLIT_N02_1"));
  assert(!strcmp(sv(2, "station_id"), "MLIT_N02_3") && !strcmp(sv(2, "name"), "Multi") &&
         "the null-geometry feature at index 2 still consumed its index");
  assert(coord(2, 0) == 9 && coord(2, 1) == 10 && "MultiLineString: first line, floor(len/2)");
  assert(!strcmp(sv(3, "station_id"), "MLIT_N02_6") && is_null(3, "name") &&
         is_null(3, "name_ja") && "a feature with NULL properties is still a station");
  rec_reset();
  printf("  N02: aliases, null for absent, midpoint, stable indexes: ok\n");

  /* ── P02: polygon centroid, malformed vertices skipped ───────────────── */
  m[0] = U("/p02");
  n = mlit_ksj_collect(&ctx, &sink, "P02", NULL, m, 1, 0);
  assert(n == 2 && "the all-malformed polygon is dropped, not crashed on");
  assert(!strcmp(sv(0, "airport_id"), "MLIT_P02_0") && !strcmp(sv(0, "icao"), "RJTT") &&
         !strcmp(sv(0, "iata"), "HND") && !strcmp(sv(0, "name"), "Haneda"));
  assert(coord(0, 0) == 0.8 && coord(0, 1) == 0.8 && "mean of the five ring vertices");
  assert(!strcmp(sv(1, "name"), "Messy") && coord(1, 0) == 11 && coord(1, 1) == 11 &&
         "bad vertices are left out of the mean");
  rec_reset();
  printf("  P02: centroid, malformed vertices skipped, all-bad dropped: ok\n");

  /* ── N05: status; abolished_only ─────────────────────────────────────── */
  m[0] = U("/n05");
  n = mlit_ksj_collect(&ctx, &sink, "N05", NULL, m, 1, 0);
  assert(n == 3 && !strcmp(sv(0, "status"), "abolished") &&
         !strcmp(sv(1, "status"), "active") && !strcmp(sv(2, "status"), "active"));
  assert(!strcmp(sv(0, "segment_id"), "MLIT_N05_0"));
  rec_reset();
  n = mlit_ksj_collect(&ctx, &sink, "N05", NULL, m, 1, 1);
  assert(n == 1 && !strcmp(sv(0, "line"), "Old") && "abolished_only keeps only abolished");
  rec_reset();
  printf("  N05: status from closed_year; abolished_only: ok\n");

  /* ── N07: geometry preserved, route_type default ─────────────────────── */
  m[0] = U("/n07");
  n = mlit_ksj_collect(&ctx, &sink, "N07", NULL, m, 1, 0);
  assert(n == 2 && !strcmp(sv(0, "route_type"), "rail") && !strcmp(sv(1, "route_type"), "bus"));
  assert(!strcmp(cJSON_GetObjectItem(g_geom[0], "type")->valuestring, "LineString") &&
         cJSON_GetArraySize(cJSON_GetObjectItem(g_geom[0], "coordinates")) == 3 &&
         "N07 keeps the whole line");
  rec_reset();
  m[0] = U("/bare");
  n = mlit_ksj_collect(&ctx, &sink, "P11", NULL, m, 1, 0);
  assert(n == 1 && !strcmp(sv(0, "stop_id"), "MLIT_P11_0") && !strcmp(sv(0, "name_ja"), "Bare") &&
         "a bare-array root is read");
  rec_reset();
  printf("  N07/P11: geometry preserved, route_type default, bare array: ok\n");

  /* ── mirrors, env, and failure disclosure ────────────────────────────── */
  const char *mirrors[] = { U("/dead"), U("/n07") };
  n = mlit_ksj_collect(&ctx, &sink, "N07", NULL, mirrors, 2, 0);
  assert(n == 2 && "a dead mirror falls through to the next");
  rec_reset();
  setenv("MLIT_TEST_URL", U("/n07"), 1);
  const char *dead2[] = { U("/dead") };
  n = mlit_ksj_collect(&ctx, &sink, "N07", "MLIT_TEST_URL", dead2, 1, 0);
  assert(n == 2 && "the env URL is tried first");
  rec_reset();
  setenv("MLIT_TEST_URL", U("/empty"), 1);
  const char *good[] = { U("/n07") };
  n = mlit_ksj_collect(&ctx, &sink, "N07", "MLIT_TEST_URL", good, 1, 0);
  assert(n == 2 && "an empty env answer falls through to a mirror that has data");
  rec_reset();
  setenv("MLIT_TEST_URL", "", 1);
  n = mlit_ksj_collect(&ctx, &sink, "N07", "MLIT_TEST_URL", good, 1, 0);
  assert(n == 2 && "an empty env value is unset");
  rec_reset();
  unsetenv("MLIT_TEST_URL");

  const char *all_dead[] = { U("/dead"), U("/html"), U("/junk"), U("/missing") };
  n = mlit_ksj_collect(&ctx, &sink, "N07", NULL, all_dead, 4, 0);
  assert(n == -1 && g_n == 0 &&
         "503, a maintenance page, a non-feature JSON and a 404 are failed fetches: rc<0, not an empty source");
  n = mlit_ksj_collect(&ctx, &sink, "N07", "MLIT_TEST_URL", NULL, 0, 0);
  assert(n == -1 && "nothing to fetch at all is a failure too");
  const char *only_empty[] = { U("/empty") };
  n = mlit_ksj_collect(&ctx, &sink, "N07", NULL, only_empty, 1, 0);
  assert(n == 0 && g_n == 0 && "an upstream that answered with an empty collection is an honest 0");
  n = mlit_ksj_collect(&ctx, &sink, "N05", NULL, (const char *[]){ U("/n05") }, 1, 1);
  assert(n == 1);
  rec_reset();
  n = mlit_ksj_collect(&ctx, &sink, "N07", NULL, (const char *[]){ U("/empty"), U("/dead") }, 2, 0);
  assert(n == 0 && "empty then dead: something was read, so it is not a fetch failure");
  assert(mlit_ksj_collect(&ctx, &sink, "ZZ9", NULL, good, 1, 0) == -1 && "unknown code");
  printf("  mirrors, env precedence, failed fetch = rc<0, empty = 0: ok\n");

  http_client_free(ctx.http);
  stub_stop();
  printf("test_mlit_ksj: all passed\n");
  return 0;
}
