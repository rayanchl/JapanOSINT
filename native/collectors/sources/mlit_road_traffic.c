/* collectors/transport/sources/mlit_road_traffic.c
 * Port of server/src/collectors/mlitRoadTraffic.js — probe-only (no public
 * nationwide JSON API). fetchHead the MLIT road-information portal → ONE
 * honest intel pointer (uid = mlit-road-traffic|portal). OTHER portal-status
 * family (lib/probe.h). No fabricated traffic points. */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PORTAL_URL "https://www.mlit.go.jp/road/road_e/index.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PORTAL_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['transport','road','traffic','mlit', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("transport"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("road"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("traffic"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("mlit"));
  cJSON_AddItemToArray(tags,
    cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "国土交通省 道路局");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";              /* → uid mlit-road-traffic|portal */
  it.title        = "MLIT road traffic / regulation information";
  it.summary      = "National road-regulation and traffic-census information "
                    "distributed via MLIT regional road portals (no public "
                    "nationwide JSON API)";
  it.link         = PORTAL_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[mlit-road-traffic] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def mlit_road_traffic_def = {
  .id = "mlit-road-traffic", .collector = "transport",
  .name = "MLIT Road Traffic Census", .name_ja = "国交省 道路交通センサス",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(mlit_road_traffic_def)
