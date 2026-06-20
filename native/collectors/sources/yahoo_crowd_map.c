/* collectors/social/sources/yahoo_crowd_map.c
 * Port of server/src/collectors/yahooCrowdMap.js — fetchHead reachability
 * probe → ONE intel item (uid = yahoo-crowd-map|yahoo-crowd-portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://map.yahoo.co.jp/crowd"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['crowd','mobility','yahoo', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("crowd"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("mobility"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("yahoo"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, mesh_size_m, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Yahoo! Japan");
  cJSON_AddNumberToObject(p, "mesh_size_m", 250);
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "yahoo-crowd-portal";
  it.title        = "Yahoo! Japan Crowd Radar";
  it.summary      = "250 m mesh real-time crowd density derived from Yahoo mobile app footprint";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[yahoo-crowd-map] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def yahoo_crowd_map_def = {
  .id = "yahoo-crowd-map", .collector = "social",
  .name = "Yahoo! Japan Crowd Radar", .name_ja = "Yahoo! 混雑レーダー",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(yahoo_crowd_map_def)
