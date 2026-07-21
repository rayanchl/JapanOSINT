/* collectors/cyber/sources/nict_atlas.c
 * Port of server/src/collectors/nictAtlas.js — fetchHead reachability probe →
 * ONE intel item (uid = nict-atlas|nict-atlas-portal ==
 * intelUid(SOURCE_ID,'nict-atlas-portal')). OTHER portal-status family. */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.nicter.jp/atlas/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['cyber','darknet','nict', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("cyber"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("darknet"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("nict"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { sensor_type, operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "sensor_type", "darknet");
  cJSON_AddStringToObject(p, "operator", "NICT");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "nict-atlas-portal";      /* → uid nict-atlas|nict-atlas-portal */
  it.title        = "NICT NICTER Atlas — darknet sensor";
  it.summary      = "NICTER project darknet visualisation portal (Koganei, Tokyo)";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[nict-atlas] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def nict_atlas_def = {
  .id = "nict-atlas", .collector = "cyber",
  .name = "NICT Atlas / NICTER stats", .name_ja = "NICT Atlas / NICTER",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(nict_atlas_def)
