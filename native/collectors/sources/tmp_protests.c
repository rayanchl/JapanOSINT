/* collectors/government/sources/tmp_protests.c
 * Port of server/src/collectors/tmpProtestApplications.js — fetchHead
 * reachability probe → ONE intel item
 * (uid = tmp-protests|tmp-protest-portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.keishicho.metro.tokyo.lg.jp/kotsu/jiko/koutsu_kisei/index.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['protest','police','tokyo','tmp', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("protest"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("police"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tokyo"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tmp"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "警視庁 交通部");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "tmp-protest-portal";
  it.title        = "Tokyo Metropolitan Police — protest / parade applications";
  it.summary      = "道路使用許可・集団行進等 published in advance (predicts riot-police mobilizations)";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[tmp-protests] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def tmp_protests_def = {
  .id = "tmp-protests", .collector = "government",
  .name = "Tokyo Metropolitan Police — protest applications",
  .name_ja = "警視庁 道路使用許可・集団行進",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(tmp_protests_def)
