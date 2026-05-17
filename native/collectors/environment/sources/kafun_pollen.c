/* collectors/environment/sources/kafun_pollen.c
 * Port of server/src/collectors/kafunPollen.js — fetchHead reachability probe →
 * ONE intel item (uid = kafun-pollen|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://kafun.env.go.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['pollen','environment','moe', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("pollen"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("environment"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("moe"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "環境省");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid kafun-pollen|portal */
  it.title        = "MOE pollen monitoring (はなこさん)";
  it.summary      = "Per-prefecture pollen concentration — cedar / cypress / grasses";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[kafun-pollen] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def kafun_pollen_def = {
  .id = "kafun-pollen", .collector = "environment",
  .name = "MOE pollen monitoring (はなこさん)", .name_ja = "環境省 花粉観測",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(kafun_pollen_def)
