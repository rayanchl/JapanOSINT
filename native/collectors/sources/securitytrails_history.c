/* collectors/cyber/sources/securitytrails_history.c
 * Port of server/src/collectors/securityTrailsHistory.js — fetchHead
 * reachability probe → ONE intel item (uid = securitytrails-history|portal).
 * Key-gated on SECURITYTRAILS_KEY. OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://securitytrails.com/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int has_key = getenv("SECURITYTRAILS_KEY") && *getenv("SECURITYTRAILS_KEY");
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['dns','history','securitytrails', live?'reachable':'unreachable',
            hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("dns"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("history"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("securitytrails"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(has_key ? "key-present" : "key-missing"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { reachable, requires_key, has_key } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "requires_key", 1);
  cJSON_AddBoolToObject(p, "has_key", has_key);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid securitytrails-history|portal */
  it.title        = "SecurityTrails historical DNS";
  it.summary      = has_key ? "Configured" : "Set SECURITYTRAILS_KEY to enable history lookups";
  it.link         = PROBE_URL;
  it.lang         = "en";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[securitytrails-history] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def securitytrails_history_def = {
  .id = "securitytrails-history", .collector = "cyber",
  .name = "SecurityTrails historical DNS", .name_ja = "SecurityTrails DNS履歴",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(securitytrails_history_def)
