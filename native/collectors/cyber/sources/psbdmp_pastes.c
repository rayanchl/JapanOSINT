/* collectors/cyber/sources/psbdmp_pastes.c
 * Port of server/src/collectors/psbdmpPastes.js — fetchHead reachability probe →
 * ONE intel item (uid = psbdmp-pastes|portal == intelUid(SOURCE_ID,'portal')).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://psbdmp.ws/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['paste','leak','psbdmp', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("paste"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("leak"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("psbdmp"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { reachable, requires_key } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "requires_key", 0);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid psbdmp-pastes|portal */
  it.title        = "PSBDMP — pastebin mirror search";
  it.summary      = "Pastebin / mirror search — JP TLD + kanji corp name queries";
  it.link         = PROBE_URL;
  it.lang         = "en";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[psbdmp-pastes] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def psbdmp_pastes_def = {
  .id = "psbdmp-pastes", .collector = "cyber",
  .name = "PSBDMP / Pastebin mirror", .name_ja = "PSBDMP / Pastebin",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(psbdmp_pastes_def)
