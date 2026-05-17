/* collectors/seismic/sources/bosai_volcano_cam.c
 * INTEL/probe source — port of server/src/collectors/bosaiVolcanoCam.js.
 * JMA volcano-camera imagery is viewer-only (no key-free listing); emit ONE
 * portal intel item + reachability probe (uid = bosai-volcano-cam|portal). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define VIEWER "https://www.data.jma.go.jp/vois/data/tokyo/volcano.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, VIEWER);
  char now[32]; probe_iso_now(now, sizeof now);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("volcano"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("camera"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jma"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("monitoring"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81");
  cJSON_AddBoolToObject(p, "reachable", live);
  cJSON_AddBoolToObject(p, "machine_readable", 0);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key = "portal";
  it.title = "JMA volcano monitoring cameras (\xE7\x81\xAB\xE5\xB1\xB1\xE7\x9B\xA3\xE8\xA6\x96\xE3\x82\xAB\xE3\x83\xA1\xE3\x83\xA9)";
  it.summary = "Live monitoring-camera imagery for active Japanese volcanoes (JMA)";
  it.body = "JMA serves volcano monitoring-camera imagery only via an "
            "interactive viewer; no key-free machine-readable camera "
            "listing/geo-feed is available, so no camera point features are "
            "emitted.";
  it.link = VIEWER;
  it.author = "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81 Japan Meteorological Agency";
  it.lang = "ja";
  it.published_at = now;
  it.tags_json = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[bosai-volcano-cam] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def bosai_volcano_cam_def = {
  .id = "bosai-volcano-cam", .collector = "seismic",
  .name = "NIED Volcano Cameras", .name_ja = "防災科研 火山カメラ",
  .update_interval_sec = 300, .run = run,
};
REGISTER_SOURCE(bosai_volcano_cam_def)
