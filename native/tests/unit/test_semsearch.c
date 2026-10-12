/* tests/unit/test_semsearch.c — the SEARCH surfaces over the intel vector
 * index (core/semsearchapi.c and the endpoints built on it), against the
 * trigram-hash stub embedding server (tools/embed_stub_server.py), with the
 * index built by the real backfill pod.
 *
 * WHAT IS BEING PROTECTED:
 *   S1. /api/intel/semantic filters (source list, record_type, since, until)
 *       run INSIDE the KNN — a narrow filter is never starved by the k nearest
 *       of everything — and bbox, a post-filter, COUNTS what it dropped. An
 *       unreadable filter is a 400, never a search without it.
 *
 * Includes embed_pod.c directly to reach run() (the pod tick), so run.sh
 * links every object EXCEPT obj/core/embed_pod.o and obj/main.o. */

#include "../../core/embed_pod.c"
#include "../../core/semsearchapi.h"
#include "../../core/intel.h"
#include "../../core/nearapi.h"
#include "../../core/reportapi.h"

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t g_stub = 0;
static int   g_port = 0;
static db_handle g_db;

/* ---- the stub ------------------------------------------------------------ */

static int answers(int port) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  llm_client c = { .http = NULL, .base_url = url, .interactive = 1 };
  const char *t[1] = { "ping" };
  float *v = NULL; int d = 0; llm_status st;
  int ok = (llm_embed(&c, t, 1, &v, &d, 500, &st) == 0);
  free(v);
  return ok;
}

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
}

/* Own the port or fail: see test_service_vec.c's port_busy() for why. */
static int stub_start(int port) {
  for (int tries = 0; tries < 40 && answers(port); tries++) port++;
  if (answers(port)) return -1;
  if (access("tools/embed_stub_server.py", R_OK) != 0) return -1;
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    if (!freopen("/dev/null", "w", stdout)) { /* stub noise is harmless */ }
    if (!freopen("/dev/null", "w", stderr)) { /* stub noise is harmless */ }
    execlp("python3", "python3", "tools/embed_stub_server.py", ps, "64", (char *)NULL);
    _exit(127);
  }
  g_stub = p; g_port = port;
  for (int i = 0; i < 100; i++) { if (answers(port)) return 0; usleep(100000); }
  return -1;
}

/* ---- database helpers ---------------------------------------------------- */

static void sql(const char *q) {
  char *e = NULL;
  if (sqlite3_exec(g_db.h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "SQL failed: %s\n  %s\n", e ? e : "?", q);
    sqlite3_free(e);
    assert(0);
  }
}

static long sql_long(const char *q) {
  sqlite3_stmt *s;
  long v = -1;
  if (sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) v = (long)sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  return v;
}

/* One intel_items row. pub NULL = fetched_at is the date; lat/lon NaN = no
 * geometry. */
typedef struct {
  const char *uid, *src, *title, *rtype, *pub, *tenant, *lang;
  double lat, lon;
} row;

static void put(const row *r) {
  char now[40];
  iso_ms(now, sizeof now, wall_ms() - 30000);
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(g_db.h,
    "INSERT INTO intel_items(uid,source_id,title,summary,published_at,fetched_at,"
    "record_type,tenant_id,language,lat,lon) "
    "VALUES(?1,?2,?3,?3,?4,?5,?6,?7,?8,?9,?10)", -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, r->uid, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, r->src ? r->src : "src-a", -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 3, r->title, -1, SQLITE_STATIC);
  if (r->pub) sqlite3_bind_text(s, 4, r->pub, -1, SQLITE_STATIC);
  else        sqlite3_bind_null(s, 4);
  sqlite3_bind_text(s, 5, now, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 6, r->rtype ? r->rtype : "news", -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 7, r->tenant ? r->tenant : "legacy", -1, SQLITE_STATIC);
  if (r->lang) sqlite3_bind_text(s, 8, r->lang, -1, SQLITE_STATIC);
  else         sqlite3_bind_null(s, 8);
  if (r->lat == r->lat) sqlite3_bind_double(s, 9, r->lat); else sqlite3_bind_null(s, 9);
  if (r->lon == r->lon) sqlite3_bind_double(s, 10, r->lon); else sqlite3_bind_null(s, 10);
  assert(sqlite3_step(s) == SQLITE_DONE);
  sqlite3_finalize(s);
  intel_fts_remirror(&g_db, r->uid);       /* the BM25 arm reads the FTS mirror */
}

static void reset_all(void) {
  sql("DELETE FROM intel_items_fts");
  sql("DELETE FROM intel_items");
  sql("DROP TABLE IF EXISTS intel_vec");
  sql("DROP TABLE IF EXISTS intel_vec_v2");
  sql("DROP TABLE IF EXISTS intel_vec_done");
  sql("DROP TABLE IF EXISTS intel_vec_failed");
  sql("DROP TABLE IF EXISTS intel_vec_meta");
}

/* One pod tick: embeds every eligible row of a small corpus. */
static void build_index(void) {
  http_client *hc = http_client_new();
  volatile int cancel = 0;
  source_ctx ctx = { .source_id = EMBED_SID, .http = hc, .db = &g_db,
                     .cancel = &cancel };
  assert(run(&ctx, NULL) == 0);
  http_client_free(hc);
}

static cJSON *jget(cJSON *o, const char *path) {
  char buf[128];
  snprintf(buf, sizeof buf, "%s", path);
  for (char *p = strtok(buf, "."); p && o; p = strtok(NULL, "."))
    o = cJSON_GetObjectItem(o, p);
  return o;
}
static double jnum(cJSON *o, const char *path) {
  cJSON *v = jget(o, path);
  assert(v && cJSON_IsNumber(v));
  return v->valuedouble;
}
static int has_uid(cJSON *root, const char *uid) {
  cJSON *it;
  cJSON_ArrayForEach(it, cJSON_GetObjectItem(root, "data")) {
    cJSON *u = cJSON_GetObjectItem(it, "uid");
    if (u && cJSON_IsString(u) && !strcmp(u->valuestring, uid)) return 1;
  }
  return 0;
}

#define NOGEO NAN, NAN

/* ---- S1: filters ---------------------------------------------------------- */

static cJSON *sem(const char *tenant, const char *q, int k,
                  const semsearch_filters *f, int want) {
  int st = 0;
  char *b = semsearchapi_query_ex(&g_db, tenant, q, "vector", 50, k, 0, f, &st);
  if (st != want) fprintf(stderr, "status %d (want %d): %s\n", st, want, b ? b : "");
  assert(st == want && b);
  cJSON *j = cJSON_Parse(b);
  free(b);
  assert(j);
  return j;
}

static void test_s1_filters(void) {
  reset_all();
  /* Ten near-identical rows from src-a, and the one src-b / record_type
   * 'report' / dated row that a post-filter on the k=3 nearest would starve. */
  char t[64];
  for (int i = 0; i < 10; i++) {
    char u[16]; snprintf(u, sizeof u, "a%d", i);
    snprintf(t, sizeof t, "harbour crane inspection report %d", i);
    row r = { u, "src-a", t, "news", NULL, "t1", "en", 35.0, 139.0 };
    put(&r);
  }
  row rb = { "b-far", "src-b", "volcanic ash advisory for aviation", "report",
             "2026-01-15T00:00:00Z", "t1", "ja", 43.0, 141.0 };
  put(&rb);
  row ro = { "o-1", "src-b", "harbour crane inspection report", "news", NULL,
             "t-other", "en", NOGEO };
  put(&ro);
  build_index();
  assert(sql_long("SELECT count(*) FROM intel_vec_v2") == 12);

  const char *q = "harbour crane inspection report";
  /* source inside the KNN: k=3 still reaches the far src-b row */
  semsearch_filters f = { .source = "src-b" };
  cJSON *j = sem("t1", q, 3, &f, 200);
  assert(has_uid(j, "b-far") && !has_uid(j, "a0") && !has_uid(j, "o-1"));
  assert(jnum(j, "meta.total") == 1 && jnum(j, "meta.shown") == 1);
  cJSON *ik = jget(j, "meta.filters.in_knn");
  assert(cJSON_GetArraySize(ik) == 1 &&
         !strcmp(cJSON_GetArrayItem(ik, 0)->valuestring, "source"));
  cJSON_Delete(j);

  /* a list: both sources, and the count is what the KNN answered */
  f = (semsearch_filters){ .source = "src-a, src-b" };
  j = sem("t1", q, 20, &f, 200);
  assert(jnum(j, "meta.total") == 11 && has_uid(j, "b-far"));
  assert(cJSON_GetArraySize(jget(j, "meta.filters.source")) == 2);
  cJSON_Delete(j);

  /* record_type and dates, inside the KNN */
  f = (semsearch_filters){ .record_type = "report" };
  j = sem("t1", q, 2, &f, 200);
  assert(jnum(j, "meta.total") == 1 && has_uid(j, "b-far"));
  cJSON_Delete(j);
  f = (semsearch_filters){ .until = "2026-02-01" };
  j = sem("t1", q, 2, &f, 200);
  assert(jnum(j, "meta.total") == 1 && has_uid(j, "b-far"));
  cJSON_Delete(j);
  f = (semsearch_filters){ .since = "2026-02-01" };
  j = sem("t1", q, 50, &f, 200);
  assert(jnum(j, "meta.total") == 10 && !has_uid(j, "b-far"));
  cJSON_Delete(j);

  /* bbox: a post-filter, and every row it removed is counted */
  f = (semsearch_filters){ .bbox = "140,42,142,44" };
  j = sem("t1", q, 50, &f, 200);
  assert(jnum(j, "meta.total") == 1 && has_uid(j, "b-far"));
  assert(jnum(j, "meta.filters.dropped.bbox") == 10);
  assert(jnum(j, "meta.filters.dropped_total") == 10);
  assert(jget(j, "meta.filters.post_note"));
  assert(jget(j, "meta.coverage"));
  cJSON_Delete(j);

  /* unreadable filters are refused, never ignored */
  f = (semsearch_filters){ .bbox = "142,42,140,44" };
  j = sem("t1", q, 50, &f, 400);
  assert(!strcmp(jget(j, "error")->valuestring, "invalid_bbox"));
  cJSON_Delete(j);
  f = (semsearch_filters){ .since = "last tuesday" };
  j = sem("t1", q, 50, &f, 400);
  assert(!strcmp(jget(j, "error")->valuestring, "invalid_since"));
  cJSON_Delete(j);

  /* hybrid: the FTS arm honours the same filters and the tenant */
  int st = 0;
  f = (semsearch_filters){ .source = "src-b" };
  char *b = semsearchapi_query_ex(&g_db, "t1", q, "hybrid", 50, 50, 0, &f, &st);
  assert(st == 200 && b);
  j = cJSON_Parse(b); free(b);
  assert(jnum(j, "meta.total") == 1 && has_uid(j, "b-far") && !has_uid(j, "o-1"));
  cJSON_Delete(j);
  printf("  S1: source/record_type/since/until inside the KNN, bbox dropped "
         "counted, bad filter 400: ok\n");
}

/* ---- S1 over the query string; S4: similar ------------------------------- */

static cJSON *route(semsearch_route_fn fn, const char *tenant, const char *id,
                    const char *qs, int want) {
  int st = 0;
  char *b = fn(&g_db, tenant, id, qs, 0, &st);
  if (st != want) fprintf(stderr, "status %d (want %d): %s\n", st, want, b ? b : "");
  assert(st == want && b);
  cJSON *j = cJSON_Parse(b);
  free(b);
  assert(j);
  return j;
}

static void test_s4_similar(void) {
  /* the S1 corpus is still in place */
  cJSON *j = route(semsearchapi_http, "t1", "",
                   "q=harbour%20crane&mode=vector&source=src-b&k=3", 200);
  assert(jnum(j, "meta.total") == 1 && has_uid(j, "b-far"));
  cJSON_Delete(j);
  char big[1400];
  memset(big, 'x', sizeof big - 1); big[sizeof big - 1] = 0;
  memcpy(big, "q=", 2);
  j = route(semsearchapi_http, "t1", "", big, 414);
  cJSON_Delete(j);

  /* neighbours of a0 are the other a* rows; a0 itself is excluded and
   * o-1 (another tenant's identical text) is not reachable */
  j = route(semsearchapi_similar, "t1", "a0", "k=5", 200);
  assert(!has_uid(j, "a0") && !has_uid(j, "o-1"));
  assert(jnum(j, "meta.shown") == 5 && jnum(j, "meta.excluded_self") == 1);
  assert(jnum(j, "meta.total") == 5);
  cJSON *first = cJSON_GetArrayItem(cJSON_GetObjectItem(j, "data"), 0);
  assert(!strncmp(cJSON_GetObjectItem(first, "uid")->valuestring, "a", 1));
  assert(jget(j, "meta.coverage"));
  cJSON_Delete(j);
  /* with a filter inside the KNN */
  j = route(semsearchapi_similar, "t1", "a0", "k=5&source=src-b", 200);
  assert(jnum(j, "meta.total") == 1 && has_uid(j, "b-far"));
  cJSON_Delete(j);
  /* another tenant's item: 404, same as one that does not exist */
  j = route(semsearchapi_similar, "t1", "o-1", "", 404);
  cJSON_Delete(j);
  j = route(semsearchapi_similar, "t1", "nope", "", 404);
  cJSON_Delete(j);
  /* no embedding server needed: the stored vector is the query */
  char *saved = strdup(getenv("JO_EMBED_URL"));
  unsetenv("JO_EMBED_URL");
  j = route(semsearchapi_similar, "t1", "a1", "k=2", 200);
  assert(jnum(j, "meta.shown") == 2);
  cJSON_Delete(j);
  setenv("JO_EMBED_URL", saved, 1);
  free(saved);
  /* a visible row the pod has not reached: 409 with coverage, not a 404 */
  row rn = { "new-1", "src-a", "freshly fetched harbour notice", "news", NULL,
             "t1", "en", NOGEO };
  put(&rn);
  j = route(semsearchapi_similar, "t1", "new-1", "", 409);
  assert(!strcmp(jget(j, "error")->valuestring, "not_embedded"));
  assert(jget(j, "meta.coverage"));
  cJSON_Delete(j);
  sql("DELETE FROM intel_items WHERE uid='new-1'");
  printf("  S4: similar excludes itself, tenant-scoped, no embed call, "
         "409 not_embedded with coverage; S1 query string + 414: ok\n");
}

/* ---- S2: sort=hybrid on the feed ----------------------------------------- */

static void test_s2_items_hybrid(void) {
  /* the S1 corpus: a0..a9 match "harbour crane" in both arms; b-far only by
   * meaning-ish proximity (the vector arm returns it under source=src-b) */
  cJSON *j = route(semsearchapi_items_hybrid, "t1", "",
                   "q=harbour%20crane&sort=hybrid&limit=4", 200);
  cJSON *d = cJSON_GetObjectItem(j, "data");
  assert(cJSON_GetArraySize(d) == 4 && !has_uid(j, "o-1"));
  cJSON *first = cJSON_GetArrayItem(d, 0);
  assert(jnum(first, "semantic.fts_rank") >= 1 && jnum(first, "semantic.vector_rank") >= 1);
  assert(!strcmp(jget(j, "meta.sort")->valuestring, "hybrid"));
  assert(!strcmp(jget(j, "meta.hybrid.vector_arm")->valuestring, "ok"));
  assert(jnum(j, "meta.hybrid.fts_hits") == 10);
  assert(jget(j, "meta.hybrid.coverage") && jget(j, "meta.hybrid.note"));
  long total = (long)jnum(j, "page.total");
  assert(total >= 10);
  cJSON *nc = jget(j, "page.next_cursor");
  assert(nc && cJSON_IsString(nc) && !strcmp(nc->valuestring, "hybrid:4"));
  cJSON_Delete(j);
  /* page 2 continues where page 1 stopped, ranks carry on */
  j = route(semsearchapi_items_hybrid, "t1", "",
            "q=harbour%20crane&sort=hybrid&limit=4&cursor=hybrid:4", 200);
  first = cJSON_GetArrayItem(cJSON_GetObjectItem(j, "data"), 0);
  assert(jnum(first, "semantic.rank") == 5);
  cJSON_Delete(j);

  /* the vector arm reaches what BM25 cannot */
  j = route(semsearchapi_items_hybrid, "t1", "",
            "q=harbour%20crane&sort=hybrid&source=src-b", 200);
  assert(jnum(j, "page.total") == 1 && has_uid(j, "b-far"));
  first = cJSON_GetArrayItem(cJSON_GetObjectItem(j, "data"), 0);
  assert(cJSON_IsNull(jget(first, "semantic.fts_rank")));
  cJSON_Delete(j);

  /* a feed filter the KNN cannot express is applied and counted */
  j = route(semsearchapi_items_hybrid, "t1", "",
            "q=harbour%20crane&sort=hybrid&lang=ja", 200);
  assert(jnum(j, "page.total") <= 1 && !has_uid(j, "a0"));
  assert(jnum(j, "meta.hybrid.filtered_out") >= 10);
  cJSON_Delete(j);

  /* q is required; a breach source is refused like sort=relevance */
  j = route(semsearchapi_items_hybrid, "t1", "", "sort=hybrid", 400);
  cJSON_Delete(j);

  /* no embedding server: BM25 alone, and the envelope says so */
  char *saved = strdup(getenv("JO_EMBED_URL"));
  unsetenv("JO_EMBED_URL");
  j = route(semsearchapi_items_hybrid, "t1", "", "q=harbour%20crane&sort=hybrid", 200);
  assert(!strcmp(jget(j, "meta.hybrid.vector_arm")->valuestring, "unavailable"));
  assert(!strcmp(jget(j, "meta.hybrid.vector_arm_error.error")->valuestring,
                 "semantic_unavailable"));
  assert(jnum(j, "page.total") == 10 && jnum(j, "meta.hybrid.vector_hits") == 0);
  cJSON_Delete(j);
  setenv("JO_EMBED_URL", saved, 1);
  free(saved);
  printf("  S2: sort=hybrid fuses BM25 + KNN, pages by offset, feed filters "
         "post-checked and counted, BM25-only fallback disclosed: ok\n");
}

/* ---- S6: Nearby honours q ------------------------------------------------- */

static void test_s6_near(void) {
  /* a0..a9 sit exactly on the point; this one is 150 m away but is the only
   * row about volcanic ash */
  row rv = { "v-near", "src-a", "volcanic ash advisory for aviation", "news", NULL,
             "t1", "en", 35.001, 139.001 };
  put(&rv);
  build_index();
  const char *qs = "near=35.0,139.0&radius_m=5000&q=volcanic%20ash%20advisory";
  cJSON *j = route(nearapi_items_route, "t1", "35.0,139.0", qs, 200);
  cJSON *d = cJSON_GetObjectItem(j, "data");
  assert(!strcmp(jget(j, "meta.q_mode")->valuestring, "semantic"));
  /* ranked, not filtered: every in-radius row, the ash advisory first */
  assert(jnum(j, "page.total") == 11 && cJSON_GetArraySize(d) == 11);
  cJSON *first = cJSON_GetArrayItem(d, 0);
  assert(!strcmp(cJSON_GetObjectItem(first, "uid")->valuestring, "v-near"));
  assert(jnum(first, "similarity") > jnum(cJSON_GetArrayItem(d, 1), "similarity"));
  assert(jnum(first, "distance_m") > 0);
  assert(jnum(j, "meta.semantic.ranked") == 11 && jget(j, "meta.semantic.coverage"));
  cJSON_Delete(j);

  /* a row not embedded yet is listed after, by distance, similarity null */
  row ru = { "u-near", "src-a", "unembedded harbour row", "news", NULL,
             "t1", "en", 35.0, 139.0 };
  put(&ru);
  j = route(nearapi_items_route, "t1", "35.0,139.0", qs, 200);
  d = cJSON_GetObjectItem(j, "data");
  assert(jnum(j, "page.total") == 12 && jnum(j, "meta.semantic.unembedded") == 1);
  cJSON *last = cJSON_GetArrayItem(d, 11);
  assert(!strcmp(cJSON_GetObjectItem(last, "uid")->valuestring, "u-near"));
  assert(cJSON_IsNull(cJSON_GetObjectItem(last, "similarity")));
  cJSON_Delete(j);
  sql("DELETE FROM intel_items WHERE uid='u-near'");

  /* no embedding server: the FTS filter, disclosed */
  char *saved = strdup(getenv("JO_EMBED_URL"));
  unsetenv("JO_EMBED_URL");
  j = route(nearapi_items_route, "t1", "35.0,139.0", qs, 200);
  assert(!strcmp(jget(j, "meta.q_mode")->valuestring, "fts"));
  assert(jget(j, "meta.semantic_unavailable.error"));
  assert(jnum(j, "page.total") == 1 && has_uid(j, "v-near"));
  assert(!strcmp(jget(j, "meta.order")->valuestring, "distance_m ASC"));
  cJSON_Delete(j);
  setenv("JO_EMBED_URL", saved, 1);
  free(saved);
  printf("  S6: Nearby ranks the radius by similarity to q, unembedded rows "
         "last, FTS fallback disclosed: ok\n");
}

/* ---- S5: case related + report suggestions -------------------------------- */

static char g_rep[65536];
static size_t g_repn = 0;
static int rep_write(void *ctx, const char *buf, size_t n) {
  (void)ctx;
  if (g_repn + n >= sizeof g_rep) n = sizeof g_rep - 1 - g_repn;
  memcpy(g_rep + g_repn, buf, n);
  g_repn += n;
  g_rep[g_repn] = 0;
  return 0;
}

static void test_s5_case_related(void) {
  sql("INSERT OR IGNORE INTO tenants(id,slug,name) VALUES ('t1','t1','T1'),"
      "('t-other','t-other','Other')");
  sql("INSERT INTO cases(id,tenant_id,name) VALUES ('case-1','t1','harbour'),"
      "('case-x','t-other','theirs'),('case-u','t1','unembedded')");
  sql("INSERT INTO case_items(case_id,ref_type,ref_id) VALUES "
      "('case-1','intel_item','a0'),('case-1','intel_item','a1'),"
      "('case-1','entity','ent-1'),('case-u','intel_item','never-embedded')");

  cJSON *j = route(semsearchapi_case_related, "t1", "case-1", "k=3", 200);
  assert(!has_uid(j, "a0") && !has_uid(j, "a1") && !has_uid(j, "o-1"));
  assert(jnum(j, "meta.shown") == 3 && jnum(j, "meta.excluded_case_items") == 2);
  assert(jnum(j, "meta.case.pinned_total") == 3 && jnum(j, "meta.case.intel_items") == 2);
  assert(jnum(j, "meta.case.with_vector") == 2 && jnum(j, "meta.case.vectors_capped") == 0);
  cJSON *first = cJSON_GetArrayItem(cJSON_GetObjectItem(j, "data"), 0);
  assert(!strncmp(cJSON_GetObjectItem(first, "uid")->valuestring, "a", 1));
  assert(jget(j, "meta.coverage"));
  cJSON_Delete(j);
  /* another tenant's case is the same 404 as none */
  j = route(semsearchapi_case_related, "t1", "case-x", "", 404);
  cJSON_Delete(j);
  /* no pinned vector: 409 with the counts */
  j = route(semsearchapi_case_related, "t1", "case-u", "", 409);
  assert(!strcmp(jget(j, "error")->valuestring, "not_embedded"));
  assert(jnum(j, "meta.case.intel_items") == 1 && jnum(j, "meta.case.with_vector") == 0);
  cJSON_Delete(j);

  /* the report's opt-in section renders the same list, no generated text */
  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "t1");
  int st = 0; long bytes = 0;
  g_repn = 0; g_rep[0] = 0;
  assert(report_run(&g_db, &t, "case-1", "{\"sections\":[\"suggestions\"]}",
                    rep_write, NULL, &bytes, &st) == 0 && st == 200);
  assert(strstr(g_rep, "Evidence suggestions") && strstr(g_rep, "S-1"));
  assert(strstr(g_rep, "of 2 of this case's 2 pinned intel item"));
  assert(strstr(g_rep, "harbour crane inspection report"));
  /* and the default report does not carry it */
  g_repn = 0; g_rep[0] = 0;
  assert(report_run(&g_db, &t, "case-1", NULL, rep_write, NULL, &bytes, &st) == 0);
  assert(!strstr(g_rep, "Evidence suggestions"));
  g_repn = 0; g_rep[0] = 0;
  assert(report_run(&g_db, &t, "case-u", "{\"sections\":[\"suggestions\"]}",
                    rep_write, NULL, &bytes, &st) == 0);
  assert(strstr(g_rep, "No suggestions could be built") && strstr(g_rep, "not_embedded"));
  printf("  S5: case related averages pinned vectors, excludes pins, states "
         "counts; report suggestions opt-in: ok\n");
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  if (db_open(&g_db, dbp, NULL) != 0) {
    fprintf(stderr, "test_semsearch: cannot open db\n");
    return 2;
  }
  unsetenv("JO_EMBED_MODEL");
  unsetenv("JO_EMBED_RECORD_TYPES");
  unsetenv("JO_EMBED_QUERY_URL");
  setenv("JO_EMBED_SINCE_DAYS", "3650", 1);      /* fixtures carry 2026-01 dates */
  setenv("JO_EMBED_MODEL_CHECK_TTL_MS", "0", 1);
  if (stub_start(18290) != 0) {
    printf("test_semsearch: stub unavailable, server-backed cases skipped\n");
    stub_stop();
    db_close(&g_db);
    return 0;
  }
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", g_port);
  setenv("JO_EMBED_URL", url, 1);

  test_s1_filters();
  test_s4_similar();
  test_s2_items_hybrid();
  test_s6_near();
  test_s5_case_related();

  stub_stop();
  db_close(&g_db);
  printf("test_semsearch: ok\n");
  return 0;
}
