/* collectors/government/sources/jftc_mergers.c
 * Port of server/src/collectors/jftcMergers.js — fetchHead reachability
 * probe → ONE intel item (uid jftc-mergers|portal). OTHER portal-status
 * family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.jftc.go.jp/dk/kiseido/todokeide/index.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['antitrust','m&a','jftc', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("antitrust"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("m&a"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jftc"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "公正取引委員会");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid jftc-mergers|portal */
  it.title        = "JFTC merger / concentration filings";
  it.summary      = "Japan Fair Trade Commission — M&A notifications and review outcomes";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jftc-mergers] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jftc_mergers_def = {
  .id = "jftc-mergers", .collector = "government",
  .name = "JFTC merger filings", .name_ja = "公取委 届出",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(jftc_mergers_def)
