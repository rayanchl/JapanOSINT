/* collectors/government/sources/jnto_arrivals.c
 * Port of server/src/collectors/jntoArrivals.js — fetchHead reachability
 * probe → ONE intel item (uid jnto-arrivals|portal). OTHER portal-status
 * family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://statistics.jnto.go.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['tourism','jnto','arrivals', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("tourism"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jnto"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("arrivals"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "日本政府観光局 (JNTO)");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid jnto-arrivals|portal */
  it.title        = "JNTO arrivals statistics";
  it.summary      = "Monthly inbound visitors by nationality / mode of entry / lodging prefecture";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jnto-arrivals] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jnto_arrivals_def = {
  .id = "jnto-arrivals", .collector = "government",
  .name = "JNTO arrivals statistics", .name_ja = "JNTO 訪日外国人",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(jnto_arrivals_def)
