/* collectors/government/sources/tsr_closures.c
 * Port of server/src/collectors/tsrClosures.js — fetchHead reachability
 * probe → ONE intel item (uid = tsr-closures|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.tsr-net.co.jp/news/tsr_release/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['bankruptcy','tsr','corporate', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("bankruptcy"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tsr"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("corporate"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "東京商工リサーチ");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "Tokyo Shoko Research — corporate closures";
  it.summary      = "TSR release feed — closures, liquidations, restructurings";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[tsr-closures] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def tsr_closures_def = {
  .id = "tsr-closures", .collector = "government",
  .name = "Tokyo Shoko Research closures", .name_ja = "東京商工リサーチ 倒産",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(tsr_closures_def)
