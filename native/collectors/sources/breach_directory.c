/* collectors/cyber/sources/breach_directory.c
 * Port of server/src/collectors/breachDirectoryLookup.js — fetchHead probe
 * (key-gated) → ONE intel item (uid breach-directory|portal). OTHER
 * portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://breachdirectory.org/"
#define KEY_ENV   "RAPIDAPI_KEY"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *kv = getenv(KEY_ENV);
  int has_key = kv && *kv;                     /* !!process.env[KEY_ENV] */
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['breach','rapidapi', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("breach"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("rapidapi"));
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
  it.remote_key   = "portal";                 /* → uid breach-directory|portal */
  it.title        = "BreachDirectory.org lookup";
  it.summary      = has_key ? "Configured (RapidAPI)"
                            : "Set " KEY_ENV " (RapidAPI) to enable lookups";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[breach-directory] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def breach_directory_def = {
  .id = "breach-directory", .collector = "cyber",
  .name = "BreachDirectory.org (RapidAPI)", .name_ja = "BreachDirectory (RapidAPI)",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(breach_directory_def)
