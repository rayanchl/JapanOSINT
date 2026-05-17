/* collectors/social/sources/vrchat_active_jp.c
 * Port of server/src/collectors/vrchatActiveJp.js — fetchHead reachability
 * probe → ONE intel item (uid = vrchat-active-jp|portal). Key-gated by
 * VRC_AUTH. OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://vrchat.com/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int has_key = getenv("VRC_AUTH") && *getenv("VRC_AUTH");
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['vrchat','social','metaverse', live?'reachable':'unreachable', hasKey?'key-present':'key-missing'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("vrchat"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("social"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("metaverse"));
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
  it.title        = "VRChat active JP-tagged worlds";
  it.summary      = has_key ? "Configured" : "Set VRC_AUTH (session cookie) to enable enumeration";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[vrchat-active-jp] probe reachable=%d has_key=%d\n", live, has_key);
  return rc >= 0 ? 0 : -1;
}

static const source_def vrchat_active_jp_def = {
  .id = "vrchat-active-jp", .collector = "social",
  .name = "VRChat — active JP worlds", .name_ja = "VRChat アクティブ JPワールド",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(vrchat_active_jp_def)
