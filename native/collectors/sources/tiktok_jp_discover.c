/* collectors/social/sources/tiktok_jp_discover.c
 * Port of server/src/collectors/tiktokJpDiscover.js — fetchHead reachability
 * probe → ONE intel item (uid = tiktok-jp-discover|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.tiktok.com/discover"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['tiktok','social','discover', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("tiktok"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("social"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("discover"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { reachable, tos_caveat } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "tos_caveat", 1);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "TikTok JP discover";
  it.summary      = "JP-locale trending hashtags + sounds + geo-tagged posts";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[tiktok-jp-discover] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def tiktok_jp_discover_def = {
  .id = "tiktok-jp-discover", .collector = "social",
  .name = "TikTok JP discover", .name_ja = "TikTok 日本 ディスカバー",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(tiktok_jp_discover_def)
