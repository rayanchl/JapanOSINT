/* collectors/transport/sources/jcg_navarea.c
 * Port of server/src/collectors/jcgNavarea.js — fetchHead reachability
 * probe → ONE intel item (uid jcg-navarea|navarea-xi-portal). OTHER
 * portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www6.kaiho.mlit.go.jp/JAPANNAVAREA/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['maritime','msi','navarea-xi', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("maritime"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("msi"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("navarea-xi"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { coordinator, role, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "coordinator", "Japan Coast Guard HQ");
  cJSON_AddStringToObject(p, "role", "NAVAREA XI coordinator");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "navarea-xi-portal";      /* → uid jcg-navarea|navarea-xi-portal */
  it.title        = "JCG NAVAREA XI navigation warnings";
  it.summary      = "Japan Coast Guard maritime safety information for the West Pacific NAVAREA XI region";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jcg-navarea] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jcg_navarea_def = {
  .id = "jcg-navarea", .collector = "transport",
  .name = "JCG NAVAREA XI warnings", .name_ja = "海上保安庁 航行警報",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(jcg_navarea_def)
