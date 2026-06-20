/* collectors/government/sources/mext_schools.c
 * Port of server/src/collectors/mextSchools.js — fetchHead reachability probe →
 * ONE intel item (uid = mext-schools|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.mext.go.jp/b_menu/toukei/chousa01/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['education','mext','school-registry', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("education"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("mext"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("school-registry"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "文部科学省");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid mext-schools|portal */
  it.title        = "MEXT school registry (学校基本調査)";
  it.summary      = "Every elementary/junior/HS/college/university — yearly CSV";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[mext-schools] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def mext_schools_def = {
  .id = "mext-schools", .collector = "government",
  .name = "MEXT school registry", .name_ja = "文科省 学校基本調査",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(mext_schools_def)
