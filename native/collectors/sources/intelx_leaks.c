/* collectors/cyber/sources/intelx_leaks.c
 * Port of server/src/collectors/intelxLeaks.js — fetchHead probe (key-gated
 * on INTELX_KEY) → ONE intel item (uid intelx-leaks|portal). OTHER
 * portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://2.intelx.io/"
#define KEY_ENV   "INTELX_KEY"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *kv = getenv(KEY_ENV);
  int has_key = kv && *kv;                     /* !!process.env[KEY_ENV] */
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['breach','paste','intelx', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("breach"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("paste"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("intelx"));
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
  it.remote_key   = "portal";                 /* → uid intelx-leaks|portal */
  it.title        = "IntelX — paste/leak search";
  it.summary      = has_key ? "Configured — search across pastes/leaks/darknet for JP TLDs"
                            : "Set " KEY_ENV " to enable searches";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[intelx-leaks] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def intelx_leaks_def = {
  .id = "intelx-leaks", .collector = "cyber",
  .name = "IntelX leaks search", .name_ja = "IntelX リーク検索",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(intelx_leaks_def)
