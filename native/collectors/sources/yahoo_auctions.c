/* collectors/classifieds/sources/yahoo_auctions.c
 * Port of server/src/collectors/yahooAuctionsSellers.js — fetchHead
 * reachability probe → ONE intel item (uid = yahoo-auctions|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://auctions.yahoo.co.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['marketplace','yahoo','auctions', live?'reachable':'unreachable', 'tos-caveat'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("marketplace"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("yahoo"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("auctions"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tos-caveat"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable, tos_caveat } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Yahoo! Japan");
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "tos_caveat", 1);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "Yahoo! Auctions sellers / listings";
  it.summary      = "Top JP marketplace — per-seller history searchable; ToS-limited";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[yahoo-auctions] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def yahoo_auctions_def = {
  .id = "yahoo-auctions", .collector = "classifieds",
  .name = "Yahoo! Auctions sellers", .name_ja = "ヤフオク 出品者",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(yahoo_auctions_def)
