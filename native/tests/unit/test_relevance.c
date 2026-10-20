/* tests/unit/test_relevance.c — the run-time rule-4d check (core/relevance.c)
 * as osint_dispatch() applies it after a pivot's run().
 *
 * Three fixture pivots, registered here:
 *   TEST_FILTER_IGNORED  answers every entity with the same 20 bakery permits
 *                        — what an upstream that ignores its filter does;
 *   TEST_FILTER_HONOURED answers with records naming the entity;
 *   TEST_RELATED         answers with records that never spell the entity but
 *                        are about it (lexical miss, semantic hit).
 * Each record ECHOES the entity in its properties ("query"), exactly as
 * hpengine does — the check must not be fooled by its own request.
 *
 * WHAT IS BEING PROTECTED.
 *   1. INERT: no JO_EMBED_URL → no `relevance` on any result, nothing sent.
 *   2. The filter-ignoring pivot is flagged relevance-suspect, in-band, and
 *      its records are all still there (a disclosure, never a filter).
 *   3. The honest pivot and the semantically-related one are not flagged.
 *
 * Includes osint_dispatch.c directly, so run.sh links every object EXCEPT
 * obj/core/osint_dispatch.o and obj/main.o. */

#include "../../core/osint_dispatch.c"
#include "embed_stub.h"

#include <assert.h>

static int emit_n(const source_ctx *ctx, intel_sink *sink, const char *fmt) {
  for (int i = 0; i < 20; i++) {
    char t[160], k[32], p[256];
    snprintf(t, sizeof t, fmt, ctx->entity, i);
    snprintf(k, sizeof k, "r%d", i);
    snprintf(p, sizeof p, "{\"query\":\"%s\",\"n\":%d}", ctx->entity, i);
    intel_item it = {0};
    it.remote_key = k;
    it.title = t;
    it.summary = t;
    it.properties_json = p;
    it.record_type = "record";
    sink->emit(sink, &it);
  }
  return 0;
}

static int run_ignored(const source_ctx *c, intel_sink *s) {
  return emit_n(c, s, "zoning permit bakery %.0s%d");
}
static int run_honoured(const source_ctx *c, intel_sink *s) {
  return emit_n(c, s, "%s annual filing %d");
}
static int run_related(const source_ctx *c, intel_sink *s) {
  return emit_n(c, s, "shipping %.0s%d");
}

static const source_def t_ignored = {
  .id = "TEST_FILTER_IGNORED", .collector = "osint", .name = "t",
  .update_interval_sec = 0, .run = run_ignored, .category = "test" };
static const source_def t_honoured = {
  .id = "TEST_FILTER_HONOURED", .collector = "osint", .name = "t",
  .update_interval_sec = 0, .run = run_honoured, .category = "test" };
static const source_def t_related = {
  .id = "TEST_RELATED", .collector = "osint", .name = "t",
  .update_interval_sec = 0, .run = run_related, .category = "test" };
REGISTER_SOURCE(t_ignored)
REGISTER_SOURCE(t_honoured)
REGISTER_SOURCE(t_related)

static osint_result go(db_handle *db, intel_sink *ps, const char *svc) {
  osint_result r;
  osint_dispatch(db, NULL, svc, "acme shipping", "company", ps, &r);
  assert(r.success && r.records == 20);
  return r;
}

int main(void) {
  unsetenv("JO_EMBED_URL"); unsetenv("JO_EMBED_QUERY_URL");
  unsetenv("JO_RELEVANCE_SIM_MIN"); unsetenv("JO_RELEVANCE_CHECK");
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);
  intel_sink ps = intel_sink_make(&db, "osint-search", "legacy");

  /* 1. inert */
  osint_result r = go(&db, &ps, "TEST_FILTER_IGNORED");
  assert(r.relevance == NULL);
  osint_result_free(&r);
  printf("  inert: no verdict without an embedding server: ok\n");

  if (estub_start(18360, 256, "stub-bow") != 0) {
    printf("test_relevance: stub unavailable, server cases skipped\n");
    intel_sink_free(&ps); db_close(&db); estub_stop();
    return 0;
  }
  estub_use();

  /* 2. the filter-ignoring pivot */
  r = go(&db, &ps, "TEST_FILTER_IGNORED");
  assert(r.relevance);
  cJSON *v = cJSON_Parse(r.relevance);
  assert(!strcmp(cJSON_GetObjectItem(v, "verdict")->valuestring, "relevance-suspect"));
  assert(cJSON_GetObjectItem(v, "sampled")->valueint == 20);
  assert(cJSON_GetObjectItem(v, "carrying_entity")->valueint == 0);   /* echo ignored */
  assert(cJSON_GetObjectItem(v, "semantically_related")->valueint == 0);
  cJSON_Delete(v);
  cJSON *d = cJSON_Parse(r.data);
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(d, "records")) == 20);  /* all kept */
  cJSON_Delete(d);
  osint_result_free(&r);
  printf("  filter-ignoring pivot flagged relevance-suspect, records kept: ok\n");

  /* 3. honest and related pivots */
  int posts = estub_posts();
  r = go(&db, &ps, "TEST_FILTER_HONOURED");
  assert(r.relevance == NULL);
  assert(estub_posts() == posts);            /* lexical pass: nothing embedded */
  osint_result_free(&r);
  r = go(&db, &ps, "TEST_RELATED");
  assert(r.relevance == NULL);
  assert(estub_posts() > posts);             /* lexical miss: asked the model */
  osint_result_free(&r);
  printf("  honoured filter (lexical) and related records (semantic) cleared: ok\n");

  /* disabled by env */
  setenv("JO_RELEVANCE_CHECK", "0", 1);
  r = go(&db, &ps, "TEST_FILTER_IGNORED");
  assert(r.relevance == NULL);
  osint_result_free(&r);
  unsetenv("JO_RELEVANCE_CHECK");

  intel_sink_free(&ps);
  db_close(&db);
  estub_stop();
  printf("test_relevance: ok\n");
  return 0;
}
