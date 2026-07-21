/* collectors/infrastructure/sources/tepco_outage.c
 * Port of server/src/collectors/tepcoOutage.js — fetchHead reachability
 * probe → ONE intel item (uid = tepco-outage|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://teideninfo.tepco.co.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['power','outage','tepco', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("power"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("outage"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tepco"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "東京電力パワーグリッド");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "TEPCO Kanto outage map";
  it.summary      = "Per-municipality power outage feed, customer-count granular";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[tepco-outage] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def tepco_outage_def = {
  .id = "tepco-outage", .collector = "infrastructure",
  .name = "TEPCO Kanto outage map", .name_ja = "TEPCO 停電マップ",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(tepco_outage_def)
