/* collectors/telecom/sources/5g_coverage.c — port of
 * server/src/collectors/coverage5g.js (fetchOverpass single area.jp).
 * SEED_5G offline fallback intentionally not ported (rule 8). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  char cell[32];
  snprintf(cell, sizeof cell, "5G_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "cell_id", cell);

  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  if (name) {
    cJSON_AddStringToObject(p, "name", name);
  } else {
    cJSON *id = cJSON_GetObjectItem(el, "id");
    char nb[48];
    snprintf(nb, sizeof nb, "Tower %lld",
             id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");

  const char *mp = ov_tag(el, "communication:mobile_phone");
  cJSON_AddStringToObject(p, "tech",
                          (mp && strcmp(mp, "yes") == 0) ? "5G/LTE"
                                                         : "communication");

  const char *bands = ov_tag(el, "communication:frequencies");
  cJSON_AddItemToObject(p, "bands",
                        bands ? cJSON_CreateString(bands)
                              : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "5g_coverage_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"tower:type\"=\"communication\"]"
    "[\"communication:mobile_phone\"=\"yes\"](area.jp);"
    "node[\"man_made\"=\"tower\"][\"tower:type\"=\"communication\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def coverage5g_def = {
  .id = "5g-coverage", .collector = "telecom", .name = "5G Coverage",
  .name_ja = "5G カバレッジ", 
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(coverage5g_def)
