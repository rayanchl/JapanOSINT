/* collectors/health/sources/ndb_open.c
 * Port of server/src/collectors/ndbOpen.js — fetchHead reachability probe →
 * ONE intel item (uid = ndb-open|portal == intelUid(SOURCE_ID,'portal')).
 * NDB open data is bulk Excel only; no machine API. ZERO data rows correct. */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/0000177182.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['health','statistics', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("health"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { org, dataset, format, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "org", "MHLW");
  cJSON_AddStringToObject(p, "dataset", "NDB Open Data");
  cJSON_AddStringToObject(p, "format", "xlsx");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid ndb-open|portal */
  it.title        = "NDB Open Data (health-insurance claims)";
  it.summary      = "レセプト情報・特定健診等オープンデータ";
  it.body         = "MHLW National Database open data — aggregated health-insurance "
                    "claims and specific health checkups, published as Excel workbooks per edition.";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.record_type  = "ndb-open";
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[ndb-open] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def ndb_open_def = {
  .id = "ndb-open", .collector = "health",
  .name = "NDB Open Data", .name_ja = "NDB オープンデータ",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(ndb_open_def)
