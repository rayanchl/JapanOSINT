/* collectors/industry/sources/petrochemical.c — port of
 * server/src/collectors/petrochemical.js. fetchOverpass (single area.jp
 * query, tryOverpass). SEED_PETROCHEM offline fallback intentionally not
 * ported (JS does `if (!live) features = []`). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Petrochemical");
  const char *operator_ = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", operator_ ? operator_ : "unknown");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"industrial\"=\"petrochemical\"](area.jp);"
    "way[\"industrial\"=\"chemical\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def petrochemical_def = {
  .id = "petrochemical", .collector = "industry", .name = "Petrochemical",
  .name_ja = "石油化学コンビナート", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(petrochemical_def)
