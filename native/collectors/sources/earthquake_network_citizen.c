/* collectors/environment/sources/earthquake_network_citizen.c
 * Port of server/src/collectors/earthquakeNetworkCitizen.js — fetchHead
 * reachability probe → ONE intel item (uid earthquake-network-citizen|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.earthquakenetwork.it/realtime/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['earthquake','citizen-science','eqn', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("earthquake"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("citizen-science"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("eqn"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Earthquake Network");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid earthquake-network-citizen|portal */
  it.title        = "Earthquake Network (citizen smartphones)";
  it.summary      = "Crowdsourced quake detections from global smartphone accelerometers";
  it.link         = PROBE_URL;
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[earthquake-network-citizen] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def earthquake_network_citizen_def = {
  .id = "earthquake-network-citizen", .collector = "environment",
  .name = "EQN citizen smartphones", .name_ja = "EQN 市民スマホ検知",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(earthquake_network_citizen_def)
