/* collectors/cyber/sources/strava_segments_jp.c
 * Port of server/src/collectors/stravaSegmentsJp.js — fetchHead reachability
 * probe → ONE intel item (uid = strava-segments-jp|portal). Key-gated by
 * STRAVA_TOKEN. OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.strava.com/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int has_key = getenv("STRAVA_TOKEN") && *getenv("STRAVA_TOKEN");
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['strava','segment','routine-inference', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("strava"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("segment"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("routine-inference"));
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
  it.title        = "Strava segments — JP-bbox explorer";
  it.summary      = has_key ? "Configured" : "Set STRAVA_TOKEN (OAuth user token) to enable segment exploration";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[strava-segments-jp] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def strava_segments_jp_def = {
  .id = "strava-segments-jp", .collector = "cyber",
  .name = "Strava segments (JP bbox)", .name_ja = "Strava セグメント 日本",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(strava_segments_jp_def)
