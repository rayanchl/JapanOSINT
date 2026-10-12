/* test_repair_semantic.c — the repair pod's semantic gate
 * (collectors/pod/collector_repair.c, semantic_gate): a URL swap is accepted
 * only when the new payload resembles what the source has stored before.
 *
 *   1. skipped, and SAID so, with no embedding server;
 *   2. skipped, and said so, when the source has too few stored vectors;
 *   3. a payload about the source's subject passes the floor;
 *   4. a healthy payload of an unrelated dataset fails it — the case the
 *      LLM sanity classifier cannot see (records > 0, plausible JSON). */
#include "../../collectors/pod/collector_repair.c"
#include "core/db.h"
#include "embed_stub.h"

#include <assert.h>

static db_handle g_db;

static void sql(const char *q) {
  char *e = NULL;
  if (sqlite3_exec(g_db.h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", q);
    assert(0);
  }
}

static int gate(const char *src, const char *body, cJSON **g) {
  *g = cJSON_CreateObject();
  return semantic_gate(&g_db, NULL, src, body, *g);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&g_db, NULL, NULL) == 0);
  unsetenv("JO_EMBED_URL");
  setenv("JO_REPAIR_SEM_FLOOR", "0.5", 1);
  const char *cities[] = { "kobe", "osaka", "sendai", "sapporo", "fukuoka", "nagoya" };
  for (int i = 0; i < 6; i++) {
    char q[512];
    snprintf(q, sizeof q, "INSERT INTO intel_items(uid,source_id,title,fetched_at)"
             " VALUES('sem|%d','UNIT_SEM','earthquake bulletin %s',"
             "strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now','-%d minutes'))", i, cities[i], i);
    sql(q);
  }
  sql("INSERT INTO intel_items(uid,source_id,title,fetched_at) VALUES"
      " ('few|1','UNIT_FEW','earthquake bulletin',strftime('%Y-%m-%dT%H:%M:%fZ','now'))");

  /* 1 */
  cJSON *g;
  assert(gate("UNIT_SEM", "[{\"title\":\"earthquake bulletin tokyo\"}]", &g) == -1);
  assert(strstr(cJSON_GetObjectItem(g, "skipped")->valuestring, "JO_EMBED_URL"));
  cJSON_Delete(g);
  printf("  no embedder: skipped and stated: ok\n");

  if (estub_start(18520, 64, "stub-bow") != 0) {
    printf("  SKIP: embedding stub did not start (python3?)\n");
    return 0;
  }
  estub_use();
  {
    const source_def *d = registry_get("embedding-backfill");
    assert(d && d->run);
    http_client *hc = http_client_new();
    volatile int cancel = 0;
    source_ctx ctx = { .source_id = d->id, .http = hc, .db = &g_db, .cancel = &cancel };
    assert(d->run(&ctx, NULL) == 0);
    http_client_free(hc);
  }

  /* 2 */
  assert(gate("UNIT_FEW", "[{\"title\":\"earthquake bulletin\"}]", &g) == -1);
  assert(strstr(cJSON_GetObjectItem(g, "skipped")->valuestring, "stored item vector"));
  cJSON_Delete(g);
  printf("  1 stored vector: skipped and stated: ok\n");

  /* 3 + 4 */
  assert(gate("UNIT_SEM", "[{\"title\":\"earthquake bulletin tokyo\"}]", &g) == 1);
  double ok_cos = cJSON_GetObjectItem(g, "cos")->valuedouble;
  assert(cJSON_GetObjectItem(g, "vectors")->valueint == 6);
  cJSON_Delete(g);
  assert(gate("UNIT_SEM", "[{\"ticker\":\"7203\",\"close\":\"nikkei stock price\"}]", &g) == 0);
  double bad_cos = cJSON_GetObjectItem(g, "cos")->valuedouble;
  cJSON_Delete(g);
  printf("  same subject cos %.3f passes, unrelated dataset cos %.3f fails "
         "(floor 0.5 here): ok\n", ok_cos, bad_cos);

  estub_stop();
  db_close(&g_db);
  printf("test_repair_semantic: ok\n");
  return 0;
}
