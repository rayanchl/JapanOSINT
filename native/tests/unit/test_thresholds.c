/* test_thresholds.c — the embedding-similarity threshold registry
 * (core/thresholds.c) and its admin route body (thresholds_api_put).
 *
 * WHAT IS BEING PROTECTED.
 *   1. resolution order: a runtime (DB) override beats the env var, which
 *      beats the compiled default — and every key's default IS the constant
 *      its module documents;
 *   2. the env var keeps today's rule: an unparsable or out-of-range value
 *      is ignored, never clamped into force;
 *   3. range: threshold_set and the PUT refuse a value outside [min,max];
 *      a value already in the table outside it is clamped on load;
 *   4. unknown keys: -1 from the setters, 404 from the PUT, NAN from get;
 *   5. the cache: a PUT is visible to the hot-path getters (story_tau,
 *      entity_vec_cos_floor, alert_sem_threshold …) at once, no restart; a
 *      reset hands the key back to the env/default; a row written behind the
 *      cache's back is picked up by threshold_reload;
 *   6. a semantic alert rule's own threshold still beats every layer;
 *   7. every change writes an audit_events row naming key, old and new;
 *   8. the gate the route sits behind (opgate_check) refuses a non-operator.
 */
#include "../../core/thresholds.c"
#include "core/story.h"
#include "core/entity_vec.h"
#include "core/alert_sem.h"
#include "core/operatorgate.h"
#include "core/auth.h"

#include <assert.h>

static db_handle g_db;

static long count_sql(const char *q) {
  sqlite3_stmt *s; long v = -1;
  assert(sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

static int near(double a, double b) { return fabs(a - b) < 1e-9; }

static void put(const char *key, const char *body, int want_status) {
  int st = 0;
  char *b = thresholds_api_put(&g_db, key, body, body ? strlen(body) : 0,
                               "op-user", &st);
  assert(b);
  if (st != want_status) {
    fprintf(stderr, "PUT %s %s -> %d (want %d): %s\n", key, body ? body : "",
            st, want_status, b);
    assert(0);
  }
  free(b);
}

int main(void) {
  int n = 0;
  const threshold_def *defs = threshold_defs(&n);
  for (int i = 0; i < n; i++) unsetenv(defs[i].env);
  unsetenv("PLATFORM_OPERATOR_EMAILS");
  assert(db_open(&g_db, NULL, NULL) == 0);
  sqlite3_exec(g_db.h, "DELETE FROM settings_thresholds", NULL, NULL, NULL);
  threshold_reload(&g_db);

  /* 1. defaults are the modules' constants */
  assert(n >= 10);
  assert(near(threshold_get("entity_vec_cos_floor"), ENTITY_VEC_COS_FLOOR));
  assert(near(threshold_get("story_tau"), STORY_TAU_DEFAULT));
  assert(near(threshold_get("alert_sem_threshold"), ALERT_SEM_THRESHOLD_DEFAULT));
  assert(near(threshold_get("relevance_sim_min"), 0.40));
  assert(near(threshold_get("suggest_min_cos"), 0.50));
  assert(near(threshold_get("resolve_emb_min_cos"), 0.60));
  assert(near(threshold_get("resolve_emb_margin"), 0.10));
  assert(near(threshold_get("camera_dup_cosine_min"), 0.97));
  assert(near(threshold_get("repair_sem_cos_floor"), 0.70));
  assert(near(threshold_get("corpus_vec_max_distance"), 1.41421356));
  assert(near(threshold_get("route_min_cos"), 0.55));
  for (int i = 0; i < n; i++) {
    double v; threshold_src s;
    assert(threshold_resolve(defs[i].key, &v, &s) == 0);
    assert(s == THR_SRC_DEFAULT && near(v, defs[i].dflt));
    assert(defs[i].dflt >= defs[i].min && defs[i].dflt <= defs[i].max);
  }
  printf("  defaults are the documented constants: ok\n");

  /* 2. env beats default; an out-of-range or garbage env is ignored */
  setenv("JO_STORY_TAU", "0.9", 1);
  threshold_src src;
  double v;
  threshold_resolve("story_tau", &v, &src);
  assert(src == THR_SRC_ENV && near(v, 0.9) && near(story_tau(), 0.9));
  setenv("JO_STORY_TAU", "1.5", 1);
  threshold_resolve("story_tau", &v, &src);
  assert(src == THR_SRC_DEFAULT && near(v, STORY_TAU_DEFAULT));
  setenv("JO_STORY_TAU", "abc", 1);
  assert(near(story_tau(), STORY_TAU_DEFAULT));
  setenv("JO_STORY_TAU", "0.9", 1);
  printf("  env > default, bad env ignored: ok\n");

  /* 3+5. DB beats env; the PUT is visible to the hot path immediately */
  put("story_tau", "{\"value\":0.77}", 200);
  threshold_resolve("story_tau", &v, &src);
  assert(src == THR_SRC_DB && near(v, 0.77) && near(story_tau(), 0.77));
  put("entity_vec_cos_floor", "{\"value\":0.66}", 200);
  assert(near(entity_vec_cos_floor(), 0.66));
  put("alert_sem_threshold", "{\"value\":0.7}", 200);
  cJSON *pred = cJSON_Parse("{\"mode\":\"llm\",\"nl_query\":\"x\"}");
  assert(near(alert_sem_threshold(pred), 0.7));
  cJSON_Delete(pred);
  /* 6. a rule's own threshold still wins over every layer */
  pred = cJSON_Parse("{\"mode\":\"llm\",\"nl_query\":\"x\",\"threshold\":0.91}");
  assert(near(alert_sem_threshold(pred), 0.91));
  cJSON_Delete(pred);
  cJSON *j = threshold_json("story_tau");
  assert(!strcmp(cJSON_GetObjectItem(j, "source")->valuestring, "db"));
  assert(near(cJSON_GetObjectItem(j, "env_value")->valuedouble, 0.9));
  assert(!strcmp(cJSON_GetObjectItem(j, "updated_by")->valuestring, "op-user"));
  cJSON_Delete(j);
  printf("  db > env, PUT visible to story/entity/alert getters at once: ok\n");

  /* reset hands it back to env, then to the default */
  put("story_tau", "{\"reset\":true}", 200);
  threshold_resolve("story_tau", &v, &src);
  assert(src == THR_SRC_ENV && near(v, 0.9));
  unsetenv("JO_STORY_TAU");
  assert(near(story_tau(), STORY_TAU_DEFAULT));
  assert(count_sql("SELECT count(*) FROM settings_thresholds WHERE key='story_tau'") == 0);
  printf("  reset: env, then default, row gone: ok\n");

  /* 3. range */
  assert(threshold_set(&g_db, "story_tau", 1.01, "x") == -2);
  assert(threshold_set(&g_db, "story_tau", NAN, "x") == -2);
  put("story_tau", "{\"value\":0}", 400);
  put("corpus_vec_max_distance", "{\"value\":2.5}", 400);
  put("corpus_vec_max_distance", "{\"value\":1.3}", 200);
  assert(near(threshold_get("corpus_vec_max_distance"), 1.3));
  put("resolve_emb_margin", "{\"value\":0}", 200);          /* min is 0 */
  assert(near(threshold_get("resolve_emb_margin"), 0.0));
  /* a row outside the range written behind the API's back is clamped */
  sqlite3_exec(g_db.h, "INSERT OR REPLACE INTO settings_thresholds(key,value) "
               "VALUES('suggest_min_cos', 7)", NULL, NULL, NULL);
  assert(near(threshold_get("suggest_min_cos"), 0.50));     /* not reloaded */
  threshold_reload(&g_db);
  assert(near(threshold_get("suggest_min_cos"), 0.99));     /* clamped max */
  printf("  range refused on write, clamped on load, reload picks up rows: ok\n");

  /* bad bodies */
  put("story_tau", NULL, 400);
  put("story_tau", "not json", 400);
  put("story_tau", "{\"value\":\"0.8\"}", 400);
  put("story_tau", "{}", 400);
  printf("  malformed bodies 400: ok\n");

  /* 4. unknown keys */
  assert(threshold_set(&g_db, "nope", 0.5, "x") == -1);
  assert(threshold_reset(&g_db, "nope") == -1);
  assert(threshold_resolve("nope", NULL, NULL) == -1);
  assert(isnan(threshold_get("nope")));
  assert(threshold_json("nope") == NULL);
  put("nope", "{\"value\":0.5}", 404);
  printf("  unknown key: -1 / NAN / 404: ok\n");

  /* list */
  char *list = threshold_list_json();
  cJSON *L = cJSON_Parse(list);
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(L, "data")) == n);
  assert(cJSON_GetObjectItem(cJSON_GetObjectItem(L, "meta"), "count")->valueint == n);
  cJSON_Delete(L); free(list);

  /* 7. audit: every successful PUT, none for refusals */
  long na = count_sql("SELECT count(*) FROM audit_events WHERE action LIKE 'threshold.%'");
  assert(na == 6);   /* story set, entity, alert, story reset, corpus, margin */
  assert(count_sql("SELECT count(*) FROM audit_events WHERE action='threshold.reset' "
                   "AND target='story_tau' AND tenant_id='platform' "
                   "AND user_id='op-user' AND payload_json LIKE '%\"old_value\":0.77%'") == 1);
  printf("  audit rows for every change, none for refusals: ok\n");

  /* 8. the route's gate */
  auth_user anon; memset(&anon, 0, sizeof anon);
  assert(opgate_check(&anon) == -401);
  auth_user u; memset(&u, 0, sizeof u);
  snprintf(u.id, sizeof u.id, "%s", "plain-user");
  unsetenv("PLATFORM_OPERATOR_IDS");
  assert(opgate_check(&u) != 0);
  setenv("PLATFORM_OPERATOR_IDS", "op-user", 1);
  assert(opgate_check(&u) != 0);
  snprintf(u.id, sizeof u.id, "%s", "op-user");
  assert(opgate_check(&u) == 0);
  unsetenv("PLATFORM_OPERATOR_IDS");
  printf("  operator gate refuses non-operators: ok\n");

  db_close(&g_db);
  printf("test_thresholds: ok\n");
  return 0;
}
