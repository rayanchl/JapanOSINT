/* collectors/health/sources/influenza_surveillance.c
 * Port of server/src/collectors/influenzaSurveillance.js — fetchHead probe →
 * ONE intel item (uid = influenza-surveillance|flu-map). NIID/JIHS sentinel
 * surveillance is weekly Excel/CSV only; no JSON API. ZERO data rows correct. */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.niid.go.jp/niid/ja/flu-map.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['health','influenza','surveillance', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("health"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("influenza"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("surveillance"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { org, dataset, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "org", "NIID / JIHS");
  cJSON_AddStringToObject(p, "dataset", "Influenza sentinel surveillance (IDWR)");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "flu-map";        /* → uid influenza-surveillance|flu-map */
  it.title        = "NIID/JIHS influenza sentinel surveillance";
  it.summary      = "インフルエンザ定点当たり報告数 (感染症発生動向調査)";
  it.body         = "NIID/JIHS influenza sentinel surveillance (IDWR). Weekly "
                    "per-prefecture sentinel reports are published as Excel/CSV "
                    "bulletins; no stable JSON API.";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.record_type  = "influenza-surveillance";
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[influenza-surveillance] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def influenza_surveillance_def = {
  .id = "influenza-surveillance", .collector = "health",
  .name = "NIID Influenza Watch", .name_ja = "国立感染研 インフルエンザ",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(influenza_surveillance_def)
