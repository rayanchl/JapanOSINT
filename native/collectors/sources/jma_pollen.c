/* collectors/environment/sources/jma_pollen.c
 * INTEL/probe source — port of server/src/collectors/jmaPollen.js.
 * No separate key-free JMA pollen JSON; emit ONE portal intel item for the
 * MOE はなこさん endpoint + reachability probe (uid = jma-pollen|portal). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PORTAL "https://kafun.env.go.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PORTAL);
  char now[32]; probe_iso_now(now, sizeof now);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("pollen"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("environment"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("health"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "\xE7\x92\xB0\xE5\xA2\x83\xE7\x9C\x81");
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "machine_readable", 0);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key = "portal";
  it.title = "Pollen forecast (\xE8\x8A\xB1\xE7\xB2\x89\xE9\xA3\x9B\xE6\x95\xA3\xE4\xBA\x88\xE6\xB8\xAC / \xE3\x81\xAF\xE3\x81\xAA\xE3\x81\x93\xE3\x81\x95\xE3\x82\x93)";
  it.summary = "Per-prefecture pollen concentration \xE2\x80\x94 cedar / cypress / grasses (MOE \xE3\x81\xAF\xE3\x81\xAA\xE3\x81\x93\xE3\x81\x95\xE3\x82\x93)";
  it.body = "Authoritative pollen observation is the MOE \xE3\x81\xAF\xE3\x81\xAA\xE3\x81\x93\xE3\x81\x95\xE3\x82\x93 system; no separate key-free JMA pollen JSON endpoint exists, so no point features are emitted here.";
  it.link = PORTAL;
  it.author = "\xE7\x92\xB0\xE5\xA2\x83\xE7\x9C\x81 Ministry of the Environment";
  it.lang = "ja";
  it.published_at = now;
  it.tags_json = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jma-pollen] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jma_pollen_def = {
  .id = "jma-pollen", .collector = "environment",
  .name = "Pollen Forecast", .name_ja = "環境省花粉情報",
  .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(jma_pollen_def)
