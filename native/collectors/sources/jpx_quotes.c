/* collectors/government/sources/jpx_quotes.c
 * Port of server/src/collectors/jpxQuotes.js — fetchHead reachability
 * probe → ONE intel item (uid jpx-quotes|portal). OTHER portal-status
 * family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.jpx.co.jp/markets/statistics-equities/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['equity','jpx','tse', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("equity"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jpx"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tse"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Japan Exchange Group");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid jpx-quotes|portal */
  it.title        = "JPX equity statistics";
  it.summary      = "TSE daily equity stats — top movers, sectors, Nikkei225 components";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jpx-quotes] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jpx_quotes_def = {
  .id = "jpx-quotes", .collector = "government",
  .name = "JPX equity statistics", .name_ja = "JPX 株式統計",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(jpx_quotes_def)
