/* collectors/government/sources/government_buildings.c — port of
 * server/src/collectors/governmentBuildings.js (fetchOverpass single
 * area.jp). SEED_GOV_BUILDINGS offline fallback not ported (rule 8). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char bb[52];
  snprintf(bb, sizeof bb, "OSM_GOV_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "building_id", bb);

  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  cJSON_AddStringToObject(p, "name", nm ? nm : "Government Office");

  const char *gov = ov_tag(el, "government");
  cJSON_AddStringToObject(p, "kind", gov ? gov : "government");

  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"office\"=\"government\"](area.jp);"
    "way[\"office\"=\"government\"](area.jp);",
    180, 200000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def government_buildings_def = {
  .id = "government-buildings", .collector = "government",
  .name = "Government Buildings", .name_ja = "官公庁",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(government_buildings_def)
