/* collectors/classifieds/sources/kakaku_prices.c
 * Port of server/src/collectors/kakakuPrices.js — fetchHead reachability probe →
 * ONE intel item (uid = kakaku-prices|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://kakaku.com/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['marketplace','price','kakaku', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("marketplace"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("price"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("kakaku"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "価格.com");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid kakaku-prices|portal */
  it.title        = "Kakaku.com price intelligence";
  it.summary      = "JP price tracker — electronics, cars, appliances, ISP plans, real estate";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[kakaku-prices] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def kakaku_prices_def = {
  .id = "kakaku-prices", .collector = "classifieds",
  .name = "Kakaku.com price intel", .name_ja = "価格.com 価格情報",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(kakaku_prices_def)
