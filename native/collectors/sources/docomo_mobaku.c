/* collectors/social/sources/docomo_mobaku.c
 * Port of server/src/collectors/docomoMobaku.js — fetchHead reachability
 * probe → ONE intel item (uid docomo-mobaku|mobaku-sample-portal). OTHER
 * portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://mobaku.jp/sample/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['crowd','mobility','docomo','mobaku', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("crowd"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("mobility"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("docomo"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("mobaku"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, cadence, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "NTT DOCOMO Insight Marketing");
  cJSON_AddStringToObject(p, "cadence", "hourly");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "mobaku-sample-portal";   /* → uid docomo-mobaku|mobaku-sample-portal */
  it.title        = "DOCOMO Mobile Spatial Statistics (Mobaku sample)";
  it.summary      = "Hourly mesh population for Tokyo, sampled from DOCOMO Mobile Spatial Statistics (公開サンプル)";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[docomo-mobaku] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def docomo_mobaku_def = {
  .id = "docomo-mobaku", .collector = "social",
  .name = "DOCOMO Mobaku (sample)", .name_ja = "モバイル空間統計 公開サンプル",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(docomo_mobaku_def)
