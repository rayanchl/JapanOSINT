/* collectors/transport/sources/michi_no_eki.c
 * Port of server/src/collectors/michiNoEki.js — fetchHead reachability probe →
 * ONE intel item (uid = michi-no-eki|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.michi-no-eki.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['poi','tourism','roadside', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("poi"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tourism"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("roadside"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "全国「道の駅」連絡会");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid michi-no-eki|portal */
  it.title        = "Michi-no-Eki — roadside station registry";
  it.summary      = "MLIT-designated 道の駅 — camping-car / secondary-route POIs";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[michi-no-eki] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def michi_no_eki_def = {
  .id = "michi-no-eki", .collector = "transport",
  .name = "Michi-no-Eki directory", .name_ja = "道の駅 ディレクトリ",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(michi_no_eki_def)
