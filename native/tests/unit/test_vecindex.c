/* tests/unit/test_vecindex.c — lib/vecindex, the one vector index under
 * intel_vec_v2 and service_vec, against tools/embed_stub_server.py on
 * loopback.
 *
 * WHAT IS BEING PROTECTED. Each property below failed silently somewhere in
 * this tree before the extraction, and the point of one library is that a new
 * index (entity_vec, fewshot_vec, anomaly_vec, image_vec) inherits all of
 * them by construction:
 *
 *   1. a new index is ~50 lines: a spec + vecindex_sync() (example_* below IS
 *      such an index, written the way a pod would write it);
 *   2. an unchanged text is not re-sent; a changed one is; a metadata-only
 *      change is rewritten from the stored vector, never re-embedded;
 *   3. a filter runs INSIDE the KNN, a partition IN answers exactly k (sqlite-
 *      vec answers k PER IN-value — measured), and a filter naming a column
 *      the index lacks is an error, never an unfiltered answer;
 *   4. a different model is refused, not mixed;
 *   5. RRF fusion ranks an item both arms like above one only one arm likes;
 *   6. the post-batch hook sees exactly what committed, with metadata;
 *   7. /api/status lists every registered index, compact.
 *
 * Includes vecindex.c directly to reach its statics, so run.sh links every
 * object EXCEPT obj/lib/vecindex.o and obj/main.o. */

#include "../../lib/vecindex.c"
#include "../../core/statusapi.h"

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t g_stub = 0;
static int   g_port = 0;

static int answers(int port) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  llm_client c = { .http = NULL, .base_url = url, .interactive = 1 };
  return vecindex_probe_dim(&c, 500, NULL) > 0;
}

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
}

/* Own the port or fail — see test_service_vec.c's port_busy() for why. */
static int stub_start(int port, int dim) {
  stub_stop();
  for (int tries = 0; tries < 40 && answers(port); tries++) port++;
  if (answers(port)) return -1;
  fflush(stdout);          /* the child's freopen would flush our buffer too */
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ps[16], ds[16];
    snprintf(ps, sizeof ps, "%d", port);
    snprintf(ds, sizeof ds, "%d", dim);
    if (!freopen("/dev/null", "w", stdout)) { /* stub noise is harmless */ }
    if (!freopen("/dev/null", "w", stderr)) { /* stub noise is harmless */ }
    execlp("python3", "python3", "tools/embed_stub_server.py", ps, ds, (char *)NULL);
    _exit(127);
  }
  g_stub = p; g_port = port;
  for (int i = 0; i < 100; i++) { if (answers(port)) return 0; usleep(100000); }
  return -1;
}

static db_handle g_db;

static long sql_long(const char *q) {
  sqlite3_stmt *s;
  long v = -1;
  if (sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) v = (long)sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  return v;
}

/* ===== 1: an example index, written the way a new pod would write it ===== */

static const vecindex_col EX_COLS[] = {
  { "tenant_id", VI_TEXT, 1 },          /* partition key */
  { "kind",      VI_TEXT, 0 },
  { "ts",        VI_INT,  0 },
};
static const vecindex_spec example_spec = {
  .name = "example", .table = "example_vec", .key_col = "id",
  .meta_table = "example_vec_meta", .done_table = "example_vec_done",
  .fail_table = "example_vec_failed", .cols = EX_COLS, .ncols = 3,
  .rebuild_hint = " (rebuild: DROP TABLE example_vec*)" };
VECINDEX_REGISTER(example_spec)

typedef struct { const char *id, *text, *tenant, *kind; long long ts; } ex_row;

/* The whole pod body: build items, sync. */
static int example_sync(const ex_row *rows, int n, vecindex_stats *st) {
  vecindex_item *it = calloc((size_t)n, sizeof *it);
  vecindex_val *vals = calloc((size_t)n * 3, sizeof *vals);
  assert(it && vals);
  for (int i = 0; i < n; i++) {
    vals[i * 3 + 0] = VI_S(rows[i].tenant);
    vals[i * 3 + 1] = VI_S(rows[i].kind);
    vals[i * 3 + 2] = VI_I(rows[i].ts);
    it[i].key = rows[i].id;
    it[i].text = rows[i].text;
    it[i].vals = &vals[i * 3];
  }
  char model[256];
  assert(vecindex_detect_model(NULL, vecindex_base_url(), model, sizeof model) == 0);
  llm_client llm = { .http = NULL, .base_url = vecindex_base_url(), .interactive = 0 };
  int rc = vecindex_sync(&g_db, &example_spec, &llm, model, it, n, 8,
                         "2026-10-11T00:00:00.000Z", "2026-10-11T00:00:00.000Z", st);
  free(it); free(vals);
  return rc;
}

/* ===== 6: a hook that records what it is handed ========================== */

static int g_hook_calls = 0, g_hook_rows = 0, g_hook_dim = 0;
static char g_hook_first[64], g_hook_tenant[64];
static void hook(db_handle *db, const vecindex_batch *b, void *user) {
  (void)db;
  assert(user == &g_hook_calls);
  g_hook_calls++;
  g_hook_rows += b->n;
  g_hook_dim = b->dim;
  snprintf(g_hook_first, sizeof g_hook_first, "%s", b->keys[0]);
  snprintf(g_hook_tenant, sizeof g_hook_tenant, "%s", b->vals[0].s);
  /* The batch committed before the hook ran: its vectors are readable. */
  float *v = NULL; int d = 0;
  assert(vecindex_load(db, b->spec, b->keys[0], &v, &d) == 0 && d == b->dim);
  assert(vec_cosine(v, b->vecs, d) > 0.9999);
  free(v);
}

static ex_row ROWS[] = {
  { "r1", "harbour inspection report for the port of Kobe", "legacy", "news",  1000 },
  { "r2", "harbour inspection report for the port of Osaka", "legacy", "news", 2000 },
  { "r3", "harbour inspection report for the port of Yokohama", "t1", "filing", 3000 },
  { "r4", "harbour inspection report for the port of Nagoya", "t2", "news", 4000 },
  { "r5", "earthquake magnitude bulletin from the meteorological agency", "legacy", "alert", 5000 },
};
#define NROWS ((int)(sizeof ROWS / sizeof ROWS[0]))

static void test_sync_incremental(void) {
  assert(vecindex_hook_register(&example_spec, hook, &g_hook_calls) == 0);
  vecindex_stats st = {0};
  assert(example_sync(ROWS, NROWS, &st) == 0);
  assert(st.sent == NROWS && st.embedded == NROWS && st.unchanged == 0);
  assert(sql_long("SELECT count(*) FROM example_vec_done") == NROWS);
  assert(vecindex_meta_long(&g_db, &example_spec, "dim") == 64);
  assert(g_hook_rows == NROWS && g_hook_dim == 64 && !strcmp(g_hook_first, "r1"));
  assert(!strcmp(g_hook_tenant, "legacy"));

  /* 2: nothing changed — nothing sent, nothing re-delivered to the hook. */
  vecindex_stats st2 = {0};
  assert(example_sync(ROWS, NROWS, &st2) == 0);
  assert(st2.sent == 0 && st2.unchanged == NROWS && st2.rewritten == 0);
  assert(g_hook_rows == NROWS);

  /* 2: one text changed — exactly that one sent. */
  ex_row edited[NROWS];
  memcpy(edited, ROWS, sizeof edited);
  edited[1].text = "harbour inspection report for the port of Osaka (amended)";
  vecindex_stats st3 = {0};
  assert(example_sync(edited, NROWS, &st3) == 0);
  assert(st3.sent == 1 && st3.embedded == 1 && st3.unchanged == NROWS - 1);

  /* 2: only metadata moved — rewritten from the stored vector, NOT sent. */
  edited[0].ts = 999999;
  vecindex_stats st4 = {0};
  assert(example_sync(edited, NROWS, &st4) == 0);
  assert(st4.sent == 0 && st4.rewritten == 1);
  assert(sql_long("SELECT ts FROM example_vec WHERE id='r1'") == 999999);
  /* and a partition-key move is a rewrite too (vec0 cannot UPDATE one) */
  edited[0].tenant = "t9";
  vecindex_stats st5 = {0};
  assert(example_sync(edited, NROWS, &st5) == 0);
  assert(st5.sent == 0 && st5.rewritten == 1);
  float *v = NULL; int d = 0;
  assert(vecindex_load(&g_db, &example_spec, "r1", &v, &d) == 0 && d == 64);
  free(v);
  assert(vecindex_load(&g_db, &example_spec, "nope", &v, &d) == 1 && !v);
  /* restore r1 for the query cases */
  vecindex_stats st6 = {0};
  assert(example_sync(ROWS, NROWS, &st6) == 0 && st6.sent == 1 && st6.rewritten == 1);
  printf("  example index: synced, unchanged not re-sent, edit re-sent alone, "
         "metadata/partition move rewritten without an embed, hook saw the "
         "committed batch: ok\n");
}

/* ===== 3: filters inside the KNN ========================================= */

static int knn(const char *qtext, int k, const vecindex_filter *f, int nf,
               vecindex_hit **h, char *err) {
  llm_client llm = { .http = NULL, .base_url = vecindex_base_url(), .interactive = 1 };
  const char *t[1] = { qtext };
  float *qv = NULL; int dim = 0; llm_status st;
  assert(llm_embed(&llm, t, 1, &qv, &dim, 5000, &st) == 0);
  int n = vecindex_knn(&g_db, &example_spec, qv, dim, k, f, nf, h, err, 256);
  free(qv);
  return n;
}

static int has(const vecindex_hit *h, int n, const char *key) {
  for (int i = 0; i < n; i++) if (!strcmp(h[i].key, key)) return 1;
  return 0;
}

static void test_knn_filters(void) {
  vecindex_hit *h = NULL;
  char err[256];
  /* unfiltered: everything is a candidate */
  int n = knn("harbour inspection report", 10, NULL, 0, &h, err);
  assert(n == NROWS);
  for (int i = 1; i < n; i++) assert(h[i - 1].distance <= h[i].distance);
  vecindex_hits_free(h, n);

  /* partition IN (two tenants), k=2: EXACTLY 2 — sqlite-vec alone would
   * answer 2 per tenant. */
  vecindex_val tv[2] = { VI_S("legacy"), VI_S("t1") };
  vecindex_filter pf = { "tenant_id", "IN", tv, 2 };
  n = knn("harbour inspection report", 2, &pf, 1, &h, err);
  assert(n == 2);
  for (int i = 0; i < n; i++) assert(strcmp(h[i].key, "r4"));      /* t2 never */
  vecindex_hits_free(h, n);
  n = knn("harbour inspection report", 10, &pf, 1, &h, err);
  assert(n == 4 && !has(h, n, "r4"));
  vecindex_hits_free(h, n);

  /* partition + metadata IN + metadata range, all inside the KNN */
  vecindex_val kinds[2] = { VI_S("news"), VI_S("alert") };
  vecindex_val since = VI_I(1500);
  vecindex_filter mf[3] = { { "tenant_id", "=", tv, 1 },
                            { "kind", "IN", kinds, 2 },
                            { "ts", ">=", &since, 1 } };
  n = knn("harbour inspection report", 10, mf, 3, &h, err);
  assert(n == 2 && has(h, n, "r2") && has(h, n, "r5"));
  vecindex_hits_free(h, n);

  /* a filter the index cannot apply is an ERROR, not an unfiltered answer */
  vecindex_filter bad = { "no_such_col", "=", tv, 1 };
  assert(knn("harbour", 5, &bad, 1, &h, err) < 0 && strstr(err, "no_such_col"));
  vecindex_filter badop = { "kind", "LIKE", kinds, 1 };
  assert(knn("harbour", 5, &badop, 1, &h, err) < 0);
  vecindex_filter badp = { "tenant_id", ">", tv, 1 };
  assert(knn("harbour", 5, &badp, 1, &h, err) < 0 && strstr(err, "partition"));
  printf("  knn: partition IN answers exactly k across tenants, metadata IN + "
         "range inside the KNN, an unknown column/operator is an error: ok\n");
}

/* ===== 5: fusion ========================================================= */

static void test_fusion(void) {
  vecindex_fused f = {0};
  vecindex_hit vh[3] = { { "a", 0.1 }, { "b", 0.2 }, { "c", 0.3 } };
  assert(vecindex_fused_add_hits(&f, vh, 3) == 3);
  /* the FTS arm likes c first and d second */
  assert(vecindex_fused_add(&f, "c", 0, 1, 0));
  assert(vecindex_fused_add(&f, "d", 0, 2, 0));
  /* a second credit from the same arm is ignored (first rank wins) */
  assert(vecindex_fused_add(&f, "a", 3, 0, 9.9));
  vecindex_fused_sort(&f);
  assert(f.n == 4);
  assert(!strcmp(f.v[0].key, "c"));                 /* both arms: 1/63 + 1/61 */
  assert(!strcmp(f.v[1].key, "a") && f.v[1].distance == 0.1);
  assert(!strcmp(f.v[2].key, "b") && !strcmp(f.v[3].key, "d"));
  /* equal score: vector rank decides, deterministically */
  vecindex_fused g = {0};
  vecindex_fused_add(&g, "y", 0, 1, 0);
  vecindex_fused_add(&g, "x", 1, 0, 0.5);
  vecindex_fused_sort(&g);
  assert(!strcmp(g.v[0].key, "x"));
  vecindex_fused_free(&f); vecindex_fused_free(&g);
  float a[3] = { 1, 0, 0 }, b[3] = { 0, 1, 0 }, z[3] = { 0, 0, 0 };
  assert(vec_cosine(a, a, 3) > 0.9999 && vec_cosine(a, b, 3) == 0 && vec_cosine(a, z, 3) == 0);
  printf("  rrf: two-arm item first, one credit per arm, deterministic ties; "
         "vec_cosine: ok\n");
}

/* ===== 4: identity ======================================================= */

static void test_identity(void) {
  char why[600];
  assert(vecindex_check_identity(&g_db, &example_spec, "stub-trigram-hash-64d", 64,
                                 why, sizeof why) == VI_ID_OK);
  assert(vecindex_check_identity(&g_db, &example_spec, "stub-trigram-hash-64d", 32,
                                 why, sizeof why) == VI_ID_DIM_MISMATCH);
  assert(strstr(why, "32-d") && strstr(why, "rebuild"));
  /* a different model behind the same URL: refused, nothing sent */
  setenv("JO_EMBED_MODEL", "some-other-model", 1);
  vecindex_stats st = {0};
  ex_row extra[1] = { { "r6", "new row under the wrong model", "legacy", "news", 1 } };
  assert(example_sync(extra, 1, &st) == -2 && st.sent == 0);
  unsetenv("JO_EMBED_MODEL");
  assert(vecindex_refused(&g_db, &example_spec, why, sizeof why) &&
         strstr(why, "some-other-model"));
  /* the right model again: the refusal is cleared, the row goes in */
  vecindex_stats st2 = {0};
  assert(example_sync(extra, 1, &st2) == 0 && st2.sent == 1);
  assert(!vecindex_refused(&g_db, &example_spec, why, sizeof why));
  printf("  identity: dim and model mismatch refused with the rebuild hint, "
         "cleared when the server matches again: ok\n");
}

/* ===== 7: coverage + /api/status ========================================= */

static void test_coverage(void) {
  vecindex_meta_set_long(&g_db, &example_spec, "eligible_count", 10);
  cJSON *c = vecindex_coverage_json(&g_db, &example_spec);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(c, "index_present")));
  assert(cJSON_GetObjectItem(c, "embedded_count")->valuedouble == 6);
  assert(cJSON_GetObjectItem(c, "backlog")->valuedouble == 4);
  assert(cJSON_GetObjectItem(c, "dim")->valuedouble == 64);
  cJSON_Delete(c);

  const vecindex_spec *const *all = NULL;
  int n = vecindex_all(&all);
  int saw_intel = 0, saw_service = 0, saw_example = 0;
  for (int i = 0; i < n; i++) {
    saw_intel   |= !strcmp(all[i]->name, "intel");
    saw_service |= !strcmp(all[i]->name, "service");
    saw_example |= !strcmp(all[i]->name, "example");
  }
  assert(saw_intel && saw_service && saw_example);

  char *js = statusapi_build_view(&g_db, 0, 0, 0, 1);   /* summary only */
  assert(js);
  cJSON *o = cJSON_Parse(js);
  cJSON *vi = cJSON_GetObjectItem(o, "vectorIndexes");
  assert(vi && cJSON_GetObjectItem(vi, "intel") && cJSON_GetObjectItem(vi, "service"));
  cJSON *ex = cJSON_GetObjectItem(vi, "example");
  assert(ex && cJSON_GetObjectItem(ex, "embedded")->valuedouble == 6);
  assert(cJSON_GetObjectItem(ex, "eligible")->valuedouble == 10);
  assert(cJSON_GetObjectItem(ex, "backlog")->valuedouble == 4);
  assert(cJSON_GetObjectItem(ex, "failed")->valuedouble == 0);
  assert(cJSON_IsNull(cJSON_GetObjectItem(ex, "refused")));
  /* an index nobody has built reports zeros and nulls, not an estimate */
  cJSON *svc = cJSON_GetObjectItem(vi, "service");
  assert(cJSON_IsNull(cJSON_GetObjectItem(svc, "eligible")) &&
         cJSON_IsNull(cJSON_GetObjectItem(svc, "backlog")));
  char *vis = cJSON_PrintUnformatted(vi);
  printf("  coverage: one shape, backlog only when eligible was measured; "
         "/api/status lists %d indexes compact (%zu bytes for all): ok\n",
         cJSON_GetArraySize(vi), vis ? strlen(vis) : 0);
  free(vis);
  cJSON_Delete(o);
  free(js);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  if (db_open(&g_db, dbp, NULL) != 0) {
    fprintf(stderr, "test_vecindex: cannot open db\n");
    return 2;
  }
  unsetenv("JO_EMBED_MODEL");
  unsetenv("JO_EMBED_QUERY_URL");
  test_fusion();
  if (stub_start(18240, 64) != 0) {
    printf("test_vecindex: stub unavailable, server-backed cases skipped\n");
    stub_stop();
    db_close(&g_db);
    return 0;
  }
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", g_port);
  setenv("JO_EMBED_URL", url, 1);
  test_sync_incremental();
  test_knn_filters();
  test_identity();
  test_coverage();
  stub_stop();
  db_close(&g_db);
  printf("test_vecindex: ok\n");
  return 0;
}
