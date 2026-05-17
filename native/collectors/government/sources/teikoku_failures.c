/* collectors/government/sources/teikoku_failures.c
 * Port of server/src/collectors/teikokuFailures.js — fetchHead reachability
 * probe → ONE intel item (uid = teikoku-failures|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.tdb.co.jp/tosan/syosai/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['bankruptcy','tdb','corporate', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("bankruptcy"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tdb"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("corporate"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "\xe5\xb8\x9d\xe5\x9b\xbd\xe3\x83\x87\xe3\x83\xbc\xe3\x82\xbf\xe3\x83\x90\xe3\x83\xb3\xe3\x82\xaf");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "Teikoku Databank — corporate failure listings";
  it.summary      = "Monthly TDB bankruptcy / liquidation summary";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[teikoku-failures] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def teikoku_failures_def = {
  .id = "teikoku-failures", .collector = "government",
  .name = "Teikoku Databank failures", .name_ja = "\xe5\xb8\x9d\xe5\x9b\xbd\xe3\x83\x87\xe3\x83\xbc\xe3\x82\xbf\xe3\x83\x90\xe3\x83\xb3\xe3\x82\xaf \xe5\x80\x92\xe7\x94\xa3\xe6\x83\x85\xe5\xa0\xb1",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(teikoku_failures_def)
