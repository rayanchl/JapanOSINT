/* collectors/industry/sources/auto_plants.c — port of
 * server/src/collectors/autoPlants.js (fetchOverpass single area.jp).
 * SEED_AUTO_PLANTS offline fallback intentionally not ported (rule 8). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char pb[48];
  snprintf(pb, sizeof pb, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "plant_id", pb);

  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Auto Plant");

  const char *brand = ov_tag(el, "operator");
  if (!brand) brand = ov_tag(el, "brand");
  cJSON_AddStringToObject(p, "brand", brand ? brand : "unknown");

  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"industrial\"=\"automobile\"](area.jp);"
    "way[\"industrial\"=\"auto_parts\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def auto_plants_def = {
  .id = "auto-plants", .collector = "industry",
  .name = "Automotive Plants", .name_ja = "自動車工場",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(auto_plants_def)
