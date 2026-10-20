/* tests/unit/test_suggest.c — /api/search/suggest's retrieval arm
 * (core/suggest.c) and the exact LLM response cache (core/llmcache.c).
 *
 * WHAT IS BEING PROTECTED.
 *   1. INERT: no JO_EMBED_URL → suggest_build() is NULL and the endpoint body
 *      is the old one byte for byte.
 *   2. Retrieval: the workspace's history and saved searches and the nearest
 *      services are suggested, each naming its source; another workspace's
 *      history never is; nothing below the similarity floor is.
 *   3. The LLM runs only with expand=1, after the retrieved suggestions.
 *   4. The cut at 9 is stated (meta.candidates / shown / truncated).
 *   5. The response cache: an identical request inside the TTL does not reach
 *      the generation server again, although every prompt carries a fresh
 *      random fence id; with no TTL set, nothing is cached.
 *
 * The embedding stub doubles as the generation server (POST /completion
 * answers $ESTUB_COMPLETION). Includes suggest.c directly, so run.sh links
 * every object EXCEPT obj/core/suggest.o and obj/main.o. */

#include "../../core/suggest.c"
#include "../../core/searchapi.h"
#include "../../core/llmcache.h"
#include "embed_stub.h"

#include <assert.h>

static db_handle g_db;

static void sql(const char *q) {
  char *e = NULL;
  if (sqlite3_exec(g_db.h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "SQL failed: %s\n  %s\n", e ? e : "?", q);
    sqlite3_free(e);
    assert(0);
  }
}

static int run_fx(const source_ctx *c, intel_sink *s) { (void)c; (void)s; return 0; }
static const source_def t_fx = {
  .id = "TEST_ACME_SHIPPING_REGISTRY", .collector = "osint",
  .name = "Acme shipping registry", .description = "acme shipping registry",
  .update_interval_sec = 0, .run = run_fx, .category = "test" };
REGISTER_SOURCE(t_fx)

static int has(cJSON *o, const char *text, const char *source) {
  cJSON *it;
  cJSON_ArrayForEach(it, cJSON_GetObjectItem(o, "items"))
    if (!strcmp(cJSON_GetObjectItem(it, "text")->valuestring, text) &&
        (!source || !strcmp(cJSON_GetObjectItem(it, "source")->valuestring, source)))
      return 1;
  return 0;
}

int main(void) {
  unsetenv("JO_EMBED_URL"); unsetenv("JO_EMBED_QUERY_URL"); unsetenv("JO_EMBED_MODEL");
  unsetenv("JO_LLM_CACHE_TTL_SEC"); unsetenv("JO_SUGGEST_MIN_COS");
  setenv("NO_PROXY", "127.0.0.1,localhost", 1);
  setenv("LLM_SUGGEST_BASE_URL", "http://127.0.0.1:1", 1);   /* nothing there */
  assert(db_open(&g_db, getenv("JO_DB"), NULL) == 0);
  sql("INSERT INTO search_history(tenant_id,user_id,kind,params_json) VALUES"
      "('t-a','u1','osint','{\"q\":\"acme shipping sanctions\"}'),"
      "('t-a','u2','intel','{\"q\":\"acme shipping vessels\"}'),"
      "('t-a','u1','intel','{\"q\":\"cooking recipes tokyo\"}'),"
      "('t-b','u9','osint','{\"q\":\"acme shipping secret\"}')");
  sql("INSERT INTO saved_searches(id,tenant_id,user_id,name,kind,params_json) VALUES"
      "('s1','t-a','u1','fleet','osint','{\"q\":\"acme shipping fleet\"}')");

  /* 1. inert */
  assert(suggest_build(&g_db, NULL, "t-a", "acme shipping", 0) == NULL);
  char *old = searchapi_suggest("acme shipping");
  char *now = searchapi_suggest_ex(&g_db, "t-a", "acme shipping", 0);
  assert(!strcmp(old, now) && !strcmp(now, "{\"suggestions\":[]}"));
  free(old); free(now);
  printf("  inert: the old body, byte for byte: ok\n");

  setenv("ESTUB_COMPLETION", "[\"acme shipping llm idea one\",\"acme shipping llm idea two\"]", 1);
  if (estub_start(18420, 256, "stub-bow") != 0) {
    printf("test_suggest: stub unavailable, retrieval cases skipped\n");
    db_close(&g_db); estub_stop();
    return 0;
  }
  estub_use();
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", estub_port);
  setenv("LLM_SUGGEST_BASE_URL", url, 1);
  assert(service_vec_build(&g_db) > 0);
  llm_client llm = { .http = NULL, .base_url = url, .interactive = 1 };

  /* 2 + 3: retrieval, no LLM */
  int comp0 = estub_completions();
  char *b = suggest_build(&g_db, &llm, "t-a", "acme shipping", 0);
  assert(b);
  cJSON *o = cJSON_Parse(b);
  assert(has(o, "acme shipping sanctions", "history"));
  assert(has(o, "acme shipping vessels", "history"));
  assert(has(o, "acme shipping fleet", "saved"));
  assert(has(o, "Acme shipping registry", "service"));
  assert(!has(o, "acme shipping secret", NULL));           /* other workspace */
  assert(!has(o, "cooking recipes tokyo", NULL));          /* below the floor */
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(o, "suggestions")) ==
         cJSON_GetArraySize(cJSON_GetObjectItem(o, "items")));
  cJSON *m = cJSON_GetObjectItem(o, "meta");
  assert(!strncmp(cJSON_GetObjectItem(m, "llm")->valuestring, "not_requested", 13));
  assert(estub_completions() == comp0);
  cJSON_Delete(o); free(b);
  printf("  retrieval: history, saved and service suggestions with their "
         "sources, other workspace excluded, no LLM call: ok\n");

  b = suggest_build(&g_db, &llm, "t-a", "acme shipping", 1);
  o = cJSON_Parse(b);
  assert(has(o, "acme shipping llm idea one", "llm"));
  cJSON *items = cJSON_GetObjectItem(o, "items");
  int n = cJSON_GetArraySize(items);
  assert(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(items, n - 1), "source")->valuestring, "llm"));
  assert(estub_completions() == comp0 + 1);
  cJSON_Delete(o); free(b);
  printf("  expand=1: LLM suggestions after the retrieved ones: ok\n");

  /* 5. the cache */
  int c1 = estub_completions();
  free(suggest_build(&g_db, &llm, "t-a", "acme shipping", 1));
  assert(estub_completions() == c1 + 1);                    /* no TTL: no cache */
  setenv("JO_LLM_CACHE_TTL_SEC", "600", 1);
  free(suggest_build(&g_db, &llm, "t-a", "acme shipping", 1));
  free(suggest_build(&g_db, &llm, "t-a", "acme shipping", 1));
  assert(estub_completions() == c1 + 2);                    /* second was a hit */
  free(suggest_build(&g_db, &llm, "t-a", "acme shipping co", 1));
  assert(estub_completions() == c1 + 3);                    /* new prompt: miss */
  unsetenv("JO_LLM_CACHE_TTL_SEC");
  printf("  response cache: identical request served from SQLite, new one "
         "asked: ok\n");

  /* 4. the cut */
  for (int i = 0; i < 12; i++) {
    char q[200];
    snprintf(q, sizeof q, "INSERT INTO search_history(tenant_id,user_id,kind,"
             "params_json) VALUES('t-a','u1','osint','{\"q\":\"acme shipping "
             "report %d\"}')", i);
    sql(q);
  }
  b = suggest_build(&g_db, &llm, "t-a", "acme shipping", 0);
  o = cJSON_Parse(b);
  m = cJSON_GetObjectItem(o, "meta");
  assert(cJSON_GetObjectItem(m, "shown")->valueint == SUGGEST_MAX);
  assert(cJSON_GetObjectItem(m, "candidates")->valueint > SUGGEST_MAX);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(m, "truncated")));
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(o, "suggestions")) == SUGGEST_MAX);
  printf("  cut at %d of %d disclosed: ok\n", SUGGEST_MAX,
         cJSON_GetObjectItem(m, "candidates")->valueint);
  cJSON_Delete(o); free(b);

  db_close(&g_db);
  estub_stop();
  printf("test_suggest: ok\n");
  return 0;
}
