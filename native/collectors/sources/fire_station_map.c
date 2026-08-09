/* collectors/safety/sources/fire_station_map.c
 * Port of server/src/collectors/fireStationMap.js (fetchOverpassTiled).
 * Curated SEED_FIRE_STATIONS offline fallback intentionally NOT ported —
 * correctness-neutral (JS does `if (!live) features = []` anyway). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"amenity\"=\"fire_station\"](%s);"
    "way[\"amenity\"=\"fire_station\"](%s);",
    bbox, bbox);
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "FIRE_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  cJSON_AddStringToObject(p, "name", name ? name : "Fire Station");
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *ph = ov_tag(el, "phone");
  cJSON_AddItemToObject(p, "phone",
                        ph ? cJSON_CreateString(ph) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def fire_station_map_def = {
  .id = "fire-station-map", .collector = "safety", .name = "Fire Stations",
  .name_ja = "消防署マップ", 
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(fire_station_map_def)
