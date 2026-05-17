/* collectors/social/sources/yahoo_chiebukuro.c
 * Port of server/src/collectors/yahooChiebukuro.js — fetchHead reachability
 * probe → ONE intel item (uid = yahoo-chiebukuro|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://chiebukuro.yahoo.co.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['social','qa','yahoo', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("social"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("qa"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("yahoo"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Yahoo! Japan");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "Yahoo! Chiebukuro — JP Q&A";
  it.summary      = "High-volume JP-locale Q&A — frequent mentions of locations + proper names";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[yahoo-chiebukuro] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def yahoo_chiebukuro_def = {
  .id = "yahoo-chiebukuro", .collector = "social",
  .name = "Yahoo! Chiebukuro Q&A", .name_ja = "Yahoo! 知恵袋",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(yahoo_chiebukuro_def)
