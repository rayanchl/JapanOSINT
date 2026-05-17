/* collectors/health/sources/food_poisoning.c
 * Port of server/src/collectors/foodPoisoning.js — fetchHead reachability
 * probe → ONE intel item (uid = food-poisoning|portal). MHLW food-poisoning
 * stats are Excel/PDF + server-side search form; no JSON API. ZERO rows ok. */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/kenkou_iryou/shokuhin/syokuchu/index.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['health','food-safety', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("health"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("food-safety"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { org, dataset, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "org", "MHLW");
  cJSON_AddStringToObject(p, "dataset", "Food-poisoning statistics (食中毒統計)");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";              /* → uid food-poisoning|portal */
  it.title        = "MHLW food-poisoning statistics";
  it.summary      = "食中毒統計調査・食中毒事件一覧";
  it.body         = "MHLW food-poisoning statistics (食中毒統計). Annual summaries "
                    "and an incident database are published as Excel/PDF / a "
                    "server-side search form; no stable JSON API.";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.record_type  = "food-poisoning";
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[food-poisoning] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def food_poisoning_def = {
  .id = "food-poisoning", .collector = "health",
  .name = "Food Poisoning Reports", .name_ja = "食中毒情報",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(food_poisoning_def)
