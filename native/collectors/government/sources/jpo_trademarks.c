/* collectors/government/sources/jpo_trademarks.c
 * Port of server/src/collectors/jpoTrademarks.js — fetchHead reachability
 * probe → ONE intel item (uid jpo-trademarks|portal). OTHER portal-status
 * family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.j-platpat.inpit.go.jp/p1101"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['trademark','jpo','inpit', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("trademark"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jpo"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("inpit"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "INPIT");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid jpo-trademarks|portal */
  it.title        = "JPO Trademark search (商標検索)";
  it.summary      = "INPIT trademark search — class, applicant, status";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jpo-trademarks] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jpo_trademarks_def = {
  .id = "jpo-trademarks", .collector = "government",
  .name = "JPO trademarks", .name_ja = "JPO 商標検索",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(jpo_trademarks_def)
