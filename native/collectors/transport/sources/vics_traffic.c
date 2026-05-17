/* collectors/transport/sources/vics_traffic.c
 * Port of server/src/collectors/vicsTraffic.js — probe-only (VICS data is
 * licensed FM-multiplex/ITS-spot, no free public JSON feed). fetchHead the
 * VICS Center portal → ONE honest intel pointer (uid = vics-traffic|portal).
 * OTHER portal-status family (lib/probe.h). No fabricated traffic data. */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PORTAL_URL "https://www.vics.or.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PORTAL_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['transport','road','traffic','vics', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("transport"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("road"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("traffic"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("vics"));
  cJSON_AddItemToArray(tags,
    cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable, public_feed } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "VICSセンター");
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "public_feed", 0);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid vics-traffic|portal */
  it.title        = "VICS road traffic information center";
  it.summary      = "VICS congestion/travel-time data is licensed (FM "
                    "multiplex / ITS spot) with no free public JSON feed; "
                    "portal status only";
  it.link         = PORTAL_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[vics-traffic] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def vics_traffic_def = {
  .id = "vics-traffic", .collector = "transport",
  .name = "VICS Traffic Info", .name_ja = "VICS 道路交通情報",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(vics_traffic_def)
