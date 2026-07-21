/* collectors/statistics/sources/mhlw_health.c
 * Port of server/src/collectors/mhlwHealth.js — fetchHead reachability probe
 * → ONE intel item (uid = mhlw-health|portal == intelUid(SOURCE_ID,'portal')).
 * MHLW health-statistics portal is Excel/PDF only; no machine API. ZERO data
 * rows is correct — surfaces a reachable/unreachable status row only. */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.mhlw.go.jp/toukei/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['health','statistics', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("health"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { org, datasets[], reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "org", "MHLW");
  cJSON *ds = cJSON_CreateArray();
  cJSON_AddItemToArray(ds, cJSON_CreateString("Vital statistics"));
  cJSON_AddItemToArray(ds, cJSON_CreateString("Patient survey"));
  cJSON_AddItemToArray(ds, cJSON_CreateString("National Health & Nutrition Survey"));
  cJSON_AddItemToObject(p, "datasets", ds);
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid mhlw-health|portal */
  it.title        = "MHLW Health Statistics portal";
  it.summary      = "人口動態統計・患者調査・国民健康・栄養調査ほか";
  it.body         = "Ministry of Health, Labour and Welfare statistics portal. "
                    "Datasets are published as Excel/PDF; no stable machine API to parse.";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.record_type  = "mhlw-health";
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[mhlw-health] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def mhlw_health_def = {
  .id = "mhlw-health", .collector = "statistics",
  .name = "MHLW Health Statistics", .name_ja = "厚労省 保健統計",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(mhlw_health_def)
