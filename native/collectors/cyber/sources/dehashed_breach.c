/* collectors/cyber/sources/dehashed_breach.c
 * Port of server/src/collectors/dehashedBreach.js — fetchHead probe
 * (key-gated on DEHASHED_USER + DEHASHED_KEY) → ONE intel item
 * (uid dehashed-breach|portal). OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://api.dehashed.com/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *u = getenv("DEHASHED_USER");
  const char *k = getenv("DEHASHED_KEY");
  int has_key = (u && *u) && (k && *k);        /* !!(envFor(USER) && envFor(KEY)) */
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['breach','credential','dehashed', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("breach"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("credential"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("dehashed"));
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
  it.remote_key   = "portal";                 /* → uid dehashed-breach|portal */
  it.title        = "DeHashed — credential breach lookup";
  it.summary      = has_key ? "Configured — domain-keyed credential lookups"
                            : "Set DEHASHED_USER + DEHASHED_KEY to enable";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[dehashed-breach] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def dehashed_breach_def = {
  .id = "dehashed-breach", .collector = "cyber",
  .name = "DeHashed (read-only)", .name_ja = "DeHashed 漏洩検索",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(dehashed_breach_def)
