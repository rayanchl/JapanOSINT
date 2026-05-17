/* collectors/infrastructure/sources/xrain_radar.c
 * Port of server/src/collectors/xrainRadar.js — fetchHead reachability
 * probe → ONE intel item (uid = xrain-radar|portal == intelUid(SOURCE_ID,
 * 'portal')). No public JSON nowcast endpoint; 0 data rows is correct
 * (RULE 8). REFERENCE boj_stats.c (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PORTAL_URL "https://www.river.go.jp/x/krademapcommon/xrain/index.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PORTAL_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['weather','radar','rainfall','mlit', live?'reachable':...] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("weather"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("radar"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("rainfall"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("mlit"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "国土交通省");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";              /* → uid xrain-radar|portal */
  it.title        = "MLIT XRAIN X-band rainfall radar";
  it.summary      = "High-resolution (250 m / 1 min) MP-radar rainfall product, tile-served via the MLIT disaster-information portal";
  it.link         = PORTAL_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[xrain-radar] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def xrain_radar_def = {
  .id = "xrain-radar", .collector = "infrastructure",
  .name = "XRAIN Rain Radar", .name_ja = "XRAIN 高解像度降雨レーダー",
  .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(xrain_radar_def)
