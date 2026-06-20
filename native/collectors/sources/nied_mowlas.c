/* collectors/seismic/sources/nied_mowlas.c
 * Port of server/src/collectors/niedMowlas.js — fetchHead reachability probe →
 * ONE intel item (uid = nied-mowlas|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.mowlas.bosai.go.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['seismic','nied','mowlas', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("seismic"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("nied"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("mowlas"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "NIED");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid nied-mowlas|portal */
  it.title        = "NIED MOWLAS unified seismograph portal";
  it.summary      = "K-NET + KiK-net + Hi-net + F-net + S-net + DONET combined";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[nied-mowlas] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def nied_mowlas_def = {
  .id = "nied-mowlas", .collector = "seismic",
  .name = "NIED MOWLAS (unified seismic)", .name_ja = "NIED MOWLAS 統合地震",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(nied_mowlas_def)
