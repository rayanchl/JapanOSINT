/* collectors/environment/sources/blitzortung_lightning.c
 * Port of server/src/collectors/blitzortungLightning.js — fetchHead
 * reachability probe → ONE intel item (uid blitzortung-lightning|portal ==
 * intelUid(SOURCE_ID,'portal')). OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.blitzortung.org/en/live_lightning_maps.php"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['lightning','weather','citizen-science', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("lightning"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("weather"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("citizen-science"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Blitzortung.org");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid blitzortung-lightning|portal */
  it.title        = "Blitzortung lightning network";
  it.summary      = "Citizen-detected real-time lightning strikes (WSS feed)";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[blitzortung-lightning] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def blitzortung_lightning_def = {
  .id = "blitzortung-lightning", .collector = "environment",
  .name = "Blitzortung lightning network", .name_ja = "Blitzortung 雷検知",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(blitzortung_lightning_def)
