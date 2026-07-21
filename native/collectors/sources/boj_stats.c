/* collectors/government/sources/boj_stats.c
 * Port of server/src/collectors/bojStats.js — fetchHead reachability probe →
 * ONE intel item (uid = boj-stats|portal == intelUid(SOURCE_ID,'portal')).
 * REFERENCE source.c for the OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.stat-search.boj.or.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['economy','statistics', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("economy"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { org, datasets[], reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "org", "Bank of Japan");
  cJSON *ds = cJSON_CreateArray();
  cJSON_AddItemToArray(ds, cJSON_CreateString("Monetary aggregates"));
  cJSON_AddItemToArray(ds, cJSON_CreateString("Interest rates"));
  cJSON_AddItemToArray(ds, cJSON_CreateString("Balance of payments"));
  cJSON_AddItemToObject(p, "datasets", ds);
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid boj-stats|portal */
  it.title        = "Bank of Japan statistics portal";
  it.summary      = "Monetary aggregates · rates · BOP indexes";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[boj-stats] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def boj_stats_def = {
  .id = "boj-stats", .collector = "government",
  .name = "Bank of Japan statistics", .name_ja = "日本銀行 統計",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(boj_stats_def)
