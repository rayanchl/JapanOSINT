/* collectors/social/sources/sumo_tournaments.c
 * Port of server/src/collectors/sumoTournaments.js — fetchHead reachability
 * probe → ONE intel item (uid = sumo-tournaments|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.sumo.or.jp/EnHonbashoMain/torikumi/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['sumo','sport','jsa', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("sumo"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("sport"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jsa"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Japan Sumo Association");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";
  it.title        = "JSA — sumo tournament (\xe6\x9c\xac\xe5\xa0\xb4\xe6\x89\x80) data";
  it.summary      = "6 honbasho per year, per-day bout matchups + results + injury reports";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[sumo-tournaments] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def sumo_tournaments_def = {
  .id = "sumo-tournaments", .collector = "social",
  .name = "JSA sumo (\xe6\x9c\xac\xe5\xa0\xb4\xe6\x89\x80) data", .name_ja = "\xe6\x97\xa5\xe6\x9c\xac\xe7\x9b\xb8\xe6\x92\xb2\xe5\x8d\x94\xe4\xbc\x9a \xe6\x9c\xac\xe5\xa0\xb4\xe6\x89\x80",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(sumo_tournaments_def)
