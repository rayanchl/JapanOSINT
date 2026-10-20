/* test_alert_semantic.c — semantic ("Describe it in words") alert rules:
 * core/alert_sem.c (the rule vector) and alert_eval.c's post-batch hook
 * (eval_semantic), end to end through the real embedding pod and the
 * bag-of-words stub (embed_stub.h).
 *
 * WHAT IS BEING PROTECTED. mode:"llm" rules saved, listed and NEVER fired:
 *   1. inert: without JO_EMBED_URL nothing is embedded, nothing is sent, and
 *      the rule says in-band that it cannot fire and why;
 *   2. a rule fires on an item whose vector is within its threshold, after
 *      the pod embeds it — and the ingest path still never fires it;
 *   3. every other predicate term still holds (source_ids narrows);
 *   4. tenancy is the live path's: a 'legacy' row meets every tenant's rules,
 *      another tenant's private row does not;
 *   5. novelty_min gates out a near-duplicate of an earlier item;
 *   6. a re-embed fires nothing twice;
 *   7. a vector made from older text is not used after nl_query changes. */
#include "../../core/alert_eval.c"
#include "core/alert_sem.h"
#include "core/httpclient.h"
#include "../../source.h"
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
static long count(const char *q) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}
static void item(const char *uid, const char *title, const char *src,
                 const char *ago, const char *tenant) {
  char q[1024];
  snprintf(q, sizeof q,
    "INSERT INTO intel_items(uid,source_id,title,published_at,fetched_at,"
    "record_type,tenant_id) VALUES('%s','%s','%s',"
    "strftime('%%Y-%%m-%%dT%%H:%%M:%%SZ','now','%s'),"
    "strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now'),'news','%s')",
    uid, src, title, ago, tenant);
  sql(q);
}
static void rule(const char *id, const char *tenant, const char *pred) {
  char q[1024];
  snprintf(q, sizeof q, "INSERT INTO alert_rules(id,tenant_id,name,predicate_json,"
           "channels_json) VALUES('%s','%s','%s','%s','[]')", id, tenant, id, pred);
  sql(q);
  g_checked_set = 0;                   /* next acquire re-reads the generation */
}
static void pod_tick(void) {
  const source_def *d = registry_get("embedding-backfill");
  assert(d && d->run);
  http_client *hc = http_client_new();
  volatile int cancel = 0;
  source_ctx ctx = { .source_id = d->id, .http = hc, .db = &g_db, .cancel = &cancel };
  assert(d->run(&ctx, NULL) == 0);
  http_client_free(hc);
  g_checked_set = 0;
}
static long fired(const char *rule_id, const char *uid) {
  char q[256];
  snprintf(q, sizeof q, "SELECT count(*) FROM alert_events WHERE rule_id='%s'"
           " AND item_uid='%s'", rule_id, uid);
  return count(q);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&g_db, NULL, NULL) == 0);
  unsetenv("JO_EMBED_URL");
  sql("INSERT INTO tenants(id,slug,name) VALUES ('t1','t1','T1'),('t2','t2','T2')");

  /* 1. inert */
  rule("r-sem", "t1", "{\"mode\":\"llm\",\"nl_query\":\"earthquake tsunami kobe\","
                      "\"threshold\":0.7}");
  assert(alert_sem_embed_rule(&g_db, "r-sem") == -1);
  assert(alert_sem_embed_missing(&g_db) == 0);
  {
    cJSON *st = alert_sem_status(&g_db, "r-sem",
      "{\"mode\":\"llm\",\"nl_query\":\"earthquake tsunami kobe\",\"threshold\":0.7}");
    assert(st && cJSON_IsFalse(cJSON_GetObjectItem(st, "evaluable")));
    assert(strstr(cJSON_GetObjectItem(st, "reason")->valuestring, "JO_EMBED_URL"));
    assert(cJSON_GetObjectItem(st, "evaluates"));
    cJSON_Delete(st);
  }
  assert(!alert_sem_status(&g_db, "x", "{\"q\":\"kobe\"}"));
  {
    int st = 0;
    char *js = alert_eval_preview(&g_db, "t1",
      "{\"mode\":\"llm\",\"nl_query\":\"earthquake tsunami kobe\"}", NULL, &st);
    assert(st == 200 && strstr(js, "\"skipped\":\"semantic_index_unavailable\""));
    assert(strstr(js, "\"reason\""));
    free(js);
  }
  printf("  inert: nothing embedded, the rule states it cannot fire and why: ok\n");

  if (estub_start(18480, 64, "stub-bow") != 0) {
    printf("  SKIP: embedding stub did not start (python3?)\n");
    return 0;
  }
  estub_use();
  /* the intel index first, so the rule vector can be checked against its model */
  item("i|seed", "weather report", "unit-src", "-3 hours", "legacy");
  pod_tick();
  assert(alert_sem_embed_missing(&g_db) == 1);
  rule("r-src", "t1", "{\"mode\":\"llm\",\"nl_query\":\"earthquake tsunami kobe\","
                      "\"threshold\":0.7,\"source_ids\":[\"other-src\"]}");
  rule("r-nov", "t1", "{\"mode\":\"llm\",\"nl_query\":\"volcano eruption sakurajima\","
                      "\"threshold\":0.7,\"novelty_min\":0.5}");
  assert(alert_sem_embed_missing(&g_db) == 2);
  {
    cJSON *st = alert_sem_status(&g_db, "r-sem",
      "{\"mode\":\"llm\",\"nl_query\":\"earthquake tsunami kobe\",\"threshold\":0.7}");
    assert(cJSON_IsTrue(cJSON_GetObjectItem(st, "evaluable")));
    cJSON_Delete(st);
  }

  /* 2 + 3 + 4 */
  item("i|hit", "earthquake tsunami kobe port", "unit-src", "-1 hours", "legacy");
  item("i|miss", "sony quarterly earnings", "unit-src", "-1 hours", "legacy");
  item("i|t2", "earthquake tsunami kobe port", "unit-src", "-1 hours", "t2");
  /* the ingest path never fires a semantic rule */
  g_checked_set = 0;
  alert_eval_on_item(&g_db, "legacy", "i|hit");
  assert(fired("r-sem", "i|hit") == 0);
  pod_tick();
  assert(fired("r-sem", "i|hit") == 1);
  assert(fired("r-sem", "i|miss") == 0);
  assert(fired("r-src", "i|hit") == 0);         /* source_ids still holds */
  assert(fired("r-sem", "i|t2") == 0);          /* another tenant's row   */
  assert(count("SELECT count(*) FROM alert_events WHERE rule_id='r-sem'"
               " AND tenant_id='t1'") == 1);
  printf("  fires after embedding on cos >= threshold; ingest path does not; "
         "source_ids narrows; another tenant's row is invisible: ok\n");

  /* 5. novelty gate */
  item("i|v1", "volcano eruption sakurajima ash", "unit-src", "-2 hours", "legacy");
  pod_tick();
  item("i|v2", "volcano eruption sakurajima ash", "unit-src", "-1 hours", "legacy");
  pod_tick();
  assert(fired("r-nov", "i|v1") == 1);
  assert(fired("r-nov", "i|v2") == 0);
  printf("  novelty_min: first report fires, its near-duplicate does not: ok\n");

  /* 6. re-embed fires nothing twice */
  sql("UPDATE intel_items SET title='earthquake tsunami kobe port update' WHERE uid='i|hit'");
  pod_tick();
  assert(fired("r-sem", "i|hit") == 1);
  printf("  re-embedded item: one event, not two: ok\n");

  /* 7. stale vector: nl_query edited, vector not yet re-made */
  sql("UPDATE alert_rules SET predicate_json='{\"mode\":\"llm\",\"nl_query\":"
      "\"stock market crash\",\"threshold\":0.7}', updated_at=datetime('now','+1 minute')"
      " WHERE id='r-sem'");
  g_checked_set = 0;
  {
    float *v = NULL; int d = 0; char m[64];
    assert(alert_sem_load(&g_db, "r-sem", "stock market crash", &v, &d, m, sizeof m) == 0);
    assert(!v);
  }
  item("i|hit2", "earthquake tsunami kobe port", "unit-src", "-1 hours", "legacy");
  pod_tick();
  assert(fired("r-sem", "i|hit2") == 0);
  assert(alert_sem_embed_missing(&g_db) == 1);
  printf("  edited description: the old vector is never used; the sweep re-embeds: ok\n");

  /* A2b: preview of a semantic predicate is a KNN, with the live tenancy and
   * the other terms, and states its window and threshold. */
  {
    int st = 0;
    char *js = alert_eval_preview(&g_db, "t1",
      "{\"mode\":\"llm\",\"nl_query\":\"earthquake tsunami kobe\",\"threshold\":0.7}",
      NULL, &st);
    assert(st == 200 && js);
    cJSON *o = cJSON_Parse(js);
    cJSON *m = cJSON_GetObjectItem(o, "meta");
    assert(cJSON_IsNull(cJSON_GetObjectItem(m, "skipped")));
    assert(cJSON_GetObjectItem(cJSON_GetObjectItem(m, "semantic"), "threshold"));
    int n = cJSON_GetObjectItem(m, "match_count")->valueint;
    assert(n == 2);                                 /* i|hit, i|hit2 (legacy) */
    cJSON *r;
    cJSON_ArrayForEach(r, cJSON_GetObjectItem(o, "data")) {
      assert(strcmp(cJSON_GetObjectItem(r, "uid")->valuestring, "i|t2") != 0);
      assert(cJSON_GetObjectItem(r, "cosine")->valuedouble >= 0.7);
    }
    cJSON_Delete(o); free(js);
    js = alert_eval_preview(&g_db, "t1",
      "{\"mode\":\"llm\",\"nl_query\":\"earthquake tsunami kobe\",\"threshold\":0.7,"
      "\"source_ids\":[\"other-src\"]}", NULL, &st);
    assert(strstr(js, "\"match_count\":0"));
    free(js);
    printf("  preview: KNN over the index, legacy + own tenant, other terms "
           "applied, window and threshold stated: ok\n");
  }

  estub_stop();
  db_close(&g_db);
  printf("test_alert_semantic: ok\n");
  return 0;
}
