/* collectors/social/sources/twitch_jp_streams.c
 * Port of server/src/collectors/twitchJpStreams.js — fetchHead reachability
 * probe → ONE intel item (uid = twitch-jp-streams|portal). Key-gated by
 * TWITCH_CLIENT_ID && TWITCH_CLIENT_SECRET (both required).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.twitch.tv/directory/all/ja"

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* hasKey = !!(envFor('TWITCH_CLIENT_ID') && envFor('TWITCH_CLIENT_SECRET')) */
  int has_key = (getenv("TWITCH_CLIENT_ID") && *getenv("TWITCH_CLIENT_ID"))
             && (getenv("TWITCH_CLIENT_SECRET") && *getenv("TWITCH_CLIENT_SECRET"));
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['twitch','live-stream','social', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("twitch"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("live-stream"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("social"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(has_key ? "key-present" : "key-missing"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { reachable, requires_key, has_key } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "requires_key", 1);
  cJSON_AddBoolToObject(p, "has_key", has_key);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "Twitch JP live streams (language=ja)";
  it.summary      = has_key ? "Configured" : "Set TWITCH_CLIENT_ID + TWITCH_CLIENT_SECRET to enable streams pull";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[twitch-jp-streams] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def twitch_jp_streams_def = {
  .id = "twitch-jp-streams", .collector = "social",
  .name = "Twitch JP live streams", .name_ja = "Twitch JP ライブ",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(twitch_jp_streams_def)
