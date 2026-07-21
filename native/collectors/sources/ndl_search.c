/* collectors/government/sources/ndl_search.c
 * Port of server/src/collectors/ndlSearch.js — fetchHead reachability probe →
 * ONE intel item (uid = ndl-search|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://iss.ndl.go.jp/api/opensearch?title=%E6%97%A5%E6%9C%AC"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['library','ndl','opensearch', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("library"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("ndl"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("opensearch"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "国立国会図書館");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid ndl-search|portal */
  it.title        = "NDL OpenSearch catalogue";
  it.summary      = "National Diet Library OpenSearch — bibliographic API";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[ndl-search] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def ndl_search_def = {
  .id = "ndl-search", .collector = "government",
  .name = "NDL OpenSearch", .name_ja = "国立国会図書館 サーチ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(ndl_search_def)
