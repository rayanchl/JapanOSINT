/* collectors/cyber/sources/snusbase_breach.c
 * Port of server/src/collectors/snusbaseBreach.js — fetchHead reachability
 * probe → ONE intel item (uid = snusbase-breach|portal). Key-gated by
 * SNUSBASE_KEY. OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://snusbase.com/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int has_key = getenv("SNUSBASE_KEY") && *getenv("SNUSBASE_KEY");
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['breach','combolist','snusbase', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("breach"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("combolist"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("snusbase"));
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
  it.remote_key   = "portal";
  it.title        = "Snusbase — combolist lookup";
  it.summary      = has_key ? "Configured" : "Set SNUSBASE_KEY to enable lookups";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[snusbase-breach] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def snusbase_breach_def = {
  .id = "snusbase-breach", .collector = "cyber",
  .name = "Snusbase combolist", .name_ja = "Snusbase コンボリスト",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(snusbase_breach_def)
