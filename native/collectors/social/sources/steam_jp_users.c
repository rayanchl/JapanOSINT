/* collectors/social/sources/steam_jp_users.c
 * Port of server/src/collectors/steamJpUsers.js — fetchHead reachability
 * probe → ONE intel item (uid = steam-jp-users|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://steamcommunity.com/search/users/?country=JP"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['gaming','steam','profile-search', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("gaming"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("steam"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("profile-search"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { reachable } */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "Steam Community — JP profile search";
  it.summary      = "Country=JP profile enumeration via Steam Community search";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[steam-jp-users] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def steam_jp_users_def = {
  .id = "steam-jp-users", .collector = "social",
  .name = "Steam Community JP users", .name_ja = "Steam JPユーザー検索",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(steam_jp_users_def)
