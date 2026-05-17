/* collectors/infrastructure/sources/nhk_relay_towers.c
 * Port of server/src/collectors/nhkRelayTowers.js — fetchHead reachability
 * probe → ONE intel item (uid = nhk-relay-towers|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.nhk.or.jp/corporateinfo/english/operations/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['broadcast','nhk','relay', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("broadcast"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("nhk"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("relay"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "NHK");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid nhk-relay-towers|portal */
  it.title        = "NHK broadcast / relay facility registry";
  it.summary      = "NHK studios + relay-transmitter network";
  it.link         = PROBE_URL;
  it.lang         = "en";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[nhk-relay-towers] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def nhk_relay_towers_def = {
  .id = "nhk-relay-towers", .collector = "infrastructure",
  .name = "NHK broadcast / relay towers", .name_ja = "NHK 放送局・中継局",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(nhk_relay_towers_def)
