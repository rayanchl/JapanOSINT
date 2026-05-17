/* collectors/transport/sources/nexco_roadwork.c
 * Port of server/src/collectors/nexcoRoadwork.js — per-operator fetchHead
 * reachability probe → ONE intel item per NEXCO operator
 * (uid = nexco-roadwork|<op> == intelUid(SOURCE_ID, op)).
 * OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

static const struct { const char *op; const char *url; } OPS[] = {
  { "NEXCO-East",    "https://www.e-nexco.co.jp/traffic/" },
  { "NEXCO-Central", "https://www.c-nexco.co.jp/road_info/" },
  { "NEXCO-West",    "https://www.w-nexco.co.jp/traffic/" },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32]; probe_iso_now(now, sizeof now);
  int ok = 0;
  for (size_t i = 0; i < sizeof OPS / sizeof OPS[0]; i++) {
    const char *op = OPS[i].op, *url = OPS[i].url;
    int live = probe_head(ctx->http, url);

    /* tags: ['highway','roadwork','nexco', live?'reachable':'unreachable'] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("highway"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("roadwork"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("nexco"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { operator, reachable } — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "operator", op);
    cJSON_AddBoolToObject(p, "reachable", live);
    char *pj = cJSON_PrintUnformatted(p);

    char title[128];
    snprintf(title, sizeof title, "%s roadwork / planned closures", op);

    intel_item it = {0};
    it.remote_key   = op;                       /* → uid nexco-roadwork|<op> */
    it.title        = title;
    it.summary      = "Construction + lane-closure outlook by section";
    it.link         = url;
    it.lang         = "ja";
    it.published_at = now;
    it.tags_json    = tj;
    it.properties_json = pj;
    int rc = sink->emit(sink, &it);

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
    if (rc < 0) ok = -1;
    fprintf(stderr, "[nexco-roadwork] %s reachable=%d\n", op, live);
  }
  return ok;
}

static const source_def nexco_roadwork_def = {
  .id = "nexco-roadwork", .collector = "transport",
  .name = "NEXCO roadwork / closures", .name_ja = "NEXCO 工事・閉鎖",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(nexco_roadwork_def)
