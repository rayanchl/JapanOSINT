/* collectors/transport/sources/jcg_msi.c
 * Port of server/src/collectors/jcgMsi.js — fetchHead reachability probe →
 * ONE intel item (uid jcg-msi|jcg-msi-portal). OTHER portal-status family
 * (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www6.kaiho.mlit.go.jp/info/msi.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['maritime','msi','e-anshin','kaiho', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("maritime"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("msi"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("e-anshin"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("kaiho"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Japan Coast Guard HQ");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "jcg-msi-portal";         /* → uid jcg-msi|jcg-msi-portal */
  it.title        = "JCG Maritime Safety Information (e-Anshin)";
  it.summary      = "Japan Coast Guard MSI broadcasts — exercises, missile-debris zones, sub-surface ops, channel closures";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jcg-msi] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jcg_msi_def = {
  .id = "jcg-msi", .collector = "transport",
  .name = "JCG Maritime Safety Info (e-Anshin)", .name_ja = "海上保安庁 海洋安全情報",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(jcg_msi_def)
