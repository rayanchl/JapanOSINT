/* collectors/government/sources/mic_elections.c
 * Port of server/src/collectors/micElections.js — fetchHead reachability probe →
 * ONE intel item (uid = mic-elections|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.soumu.go.jp/senkyo/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['election','soumu','politics', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("election"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("soumu"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("politics"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "総務省 自治行政局");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid mic-elections|portal */
  it.title        = "MIC election results portal";
  it.summary      = "Per-scrutin CSVs — national, prefecture, lower / upper house, local";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[mic-elections] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def mic_elections_def = {
  .id = "mic-elections", .collector = "government",
  .name = "MIC election results", .name_ja = "総務省 選挙結果",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(mic_elections_def)
