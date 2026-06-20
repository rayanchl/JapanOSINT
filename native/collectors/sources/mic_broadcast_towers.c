/* collectors/infrastructure/sources/mic_broadcast_towers.c
 * Port of server/src/collectors/micBroadcastTowers.js — fetchHead reachability
 * probe → ONE intel item (uid = mic-broadcast-towers|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.tele.soumu.go.jp/giga-search/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['broadcast','radio','soumu', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("broadcast"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("radio"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("soumu"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "総務省 電波利用ホームページ");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid mic-broadcast-towers|portal */
  it.title        = "MIC radio station registry (giga-search)";
  it.summary      = "Every licensed AM/FM/TV/community-FM transmitter + amateur repeater";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[mic-broadcast-towers] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def mic_broadcast_towers_def = {
  .id = "mic-broadcast-towers", .collector = "infrastructure",
  .name = "MIC broadcast tower registry", .name_ja = "総務省 放送局検索",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(mic_broadcast_towers_def)
