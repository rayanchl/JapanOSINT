/* collectors/seismic/sources/vnet.c
 * INTEL/probe source — port of server/src/collectors/vnet.js.
 * NIED V-net has no key-free public station/data endpoint; emit ONE portal
 * intel item + reachability probe (uid = vnet|portal). Never fabricated. */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PORTAL "https://www.vnet.bosai.go.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PORTAL);
  char now[32]; probe_iso_now(now, sizeof now);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("volcano"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("nied"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("vnet"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("seismic"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "NIED");
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "machine_readable", 0);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key = "portal";
  it.title = "NIED V-net (\xE5\x9F\xBA\xE7\x9B\xA4\xE7\x9A\x84\xE7\x81\xAB\xE5\xB1\xB1\xE8\xA6\xB3\xE6\xB8\xAC\xE7\xB6\xB2)";
  it.summary = "National Research Institute for Earth Science and Disaster Resilience fundamental volcano observation network";
  it.body = "V-net broadband seismic / tilt / GNSS volcano observation network "
            "operated by NIED. No key-free public station or data endpoint "
            "exists (data access requires NIED registration); portal "
            "reachability only.";
  it.link = PORTAL;
  it.author = "NIED \xE9\x98\xB2\xE7\x81\xBD\xE7\xA7\x91\xE5\xAD\xA6\xE6\x8A\x80\xE8\xA1\x93\xE7\xA0\x94\xE7\xA9\xB6\xE6\x89\x80";
  it.lang = "ja";
  it.published_at = now;
  it.tags_json = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[vnet] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def vnet_def = {
  .id = "vnet", .collector = "seismic",
  .name = "V-net Volcano Observation", .name_ja = "V-net 火山観測網",
  .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(vnet_def)
