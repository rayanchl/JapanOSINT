/* collectors/environment/sources/jma_uv.c
 * INTEL/probe source — port of server/src/collectors/jmaUv.js.
 * JMA UV index has no key-free JSON endpoint; emit ONE portal intel item
 * with a reachability probe (uid = jma-uv|portal). Honest — never fabricated. */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PORTAL "https://www.jma.go.jp/jp/uv/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PORTAL);
  char now[32]; probe_iso_now(now, sizeof now);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("uv"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jma"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("health"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81");
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "machine_readable", 0);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key = "portal";
  it.title = "JMA UV index (\xE7\xB4\xAB\xE5\xA4\x96\xE7\xB7\x9A\xE6\x83\x85\xE5\xA0\xB1)";
  it.summary = "Nationwide ultraviolet index forecast and analysis maps (JMA)";
  it.body = "JMA publishes the UV index only as raster maps / an interactive "
            "viewer; no key-free machine-readable JSON endpoint is available, "
            "so no point features are emitted.";
  it.link = PORTAL;
  it.author = "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81 Japan Meteorological Agency";
  it.lang = "ja";
  it.published_at = now;
  it.tags_json = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jma-uv] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jma_uv_def = {
  .id = "jma-uv", .collector = "environment",
  .name = "JMA UV Index", .name_ja = "気象庁 紫外線情報",
  .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(jma_uv_def)
