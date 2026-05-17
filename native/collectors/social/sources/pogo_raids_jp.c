/* collectors/social/sources/pogo_raids_jp.c
 * Port of server/src/collectors/pogoRaidsJp.js — fetchHead reachability probe →
 * ONE intel item (uid = pogo-raids-jp|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://gomap.eu/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['pogo','crowdsource','poi-density', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("pogo"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("crowdsource"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("poi-density"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { reachable, tos_caveat } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "tos_caveat", 1);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid pogo-raids-jp|portal */
  it.title        = "Pokemon GO raid / gym density";
  it.summary      = "Community trackers — proxy for public outdoor POI density";
  it.link         = PROBE_URL;
  it.lang         = "en";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[pogo-raids-jp] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def pogo_raids_jp_def = {
  .id = "pogo-raids-jp", .collector = "social",
  .name = "Pokemon GO raids / gyms", .name_ja = "ポケモンGO レイド / ジム",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(pogo_raids_jp_def)
