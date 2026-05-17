/* collectors/cyber/sources/hibp_breach.c
 * Port of server/src/collectors/hibpBreach.js — fetchHead probe (key-gated
 * on HIBP_KEY) → ONE intel item (uid hibp-breach|portal). OTHER
 * portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://haveibeenpwned.com/api/v3/breaches"
#define KEY_ENV   "HIBP_KEY"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *kv = getenv(KEY_ENV);
  int has_key = kv && *kv;                     /* !!process.env[KEY_ENV] */
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['breach','hibp', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("breach"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("hibp"));
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
  it.remote_key   = "portal";                 /* → uid hibp-breach|portal */
  it.title        = "HaveIBeenPwned — domain breach catalogue";
  it.summary      = has_key ? "Configured — pulls breaches affecting *.jp addresses"
                            : "Set " KEY_ENV " to enable lookups";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[hibp-breach] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def hibp_breach_def = {
  .id = "hibp-breach", .collector = "cyber",
  .name = "HaveIBeenPwned (domain)", .name_ja = "HaveIBeenPwned 漏洩",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(hibp_breach_def)
