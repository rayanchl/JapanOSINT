/* collectors/geospatial/sources/openstreetmap_jp.c
 * Port of server/src/collectors/openstreetmapJp.js — single nationwide
 * Overpass query for civic / transport POIs (townhall, hospital, fire
 * station, police, post office, railway station). Real OSM data only;
 * honest empty on Overpass failure (JS returns empty FeatureCollection).
 * Property key order mirrors the JS mapFn for featureUid hash parity. */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static void add_str_or_null(cJSON *p, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(p, k, v);
  else   cJSON_AddNullToObject(p, k);
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  const cJSON *type = cJSON_GetObjectItem(el, "type");
  const cJSON *id   = cJSON_GetObjectItem(el, "id");
  if (type && cJSON_IsString(type) && id && cJSON_IsNumber(id)) {
    char osm[80];
    snprintf(osm, sizeof osm, "%s/%lld",
             type->valuestring, (long long)id->valuedouble);
    cJSON_AddStringToObject(p, "osm_id", osm);
  } else {
    cJSON_AddNullToObject(p, "osm_id");
  }
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  add_str_or_null(p, "name", name);
  add_str_or_null(p, "name_en", ov_tag(el, "name:en"));
  const char *cat = ov_tag(el, "amenity");
  if (!cat) cat = ov_tag(el, "railway");
  add_str_or_null(p, "category", cat);
  add_str_or_null(p, "operator", ov_tag(el, "operator"));
  const char *addr = ov_tag(el, "addr:full");
  if (!addr) addr = ov_tag(el, "addr:city");
  add_str_or_null(p, "address", addr);
  cJSON_AddStringToObject(p, "source", "openstreetmap");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"townhall\"](area.jp);"
    "node[\"amenity\"=\"hospital\"](area.jp);"
    "node[\"amenity\"=\"fire_station\"](area.jp);"
    "node[\"amenity\"=\"police\"](area.jp);"
    "node[\"amenity\"=\"post_office\"](area.jp);"
    "node[\"railway\"=\"station\"](area.jp);",
    240, 260000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def openstreetmap_jp_def = {
  .id = "openstreetmap-jp", .collector = "geospatial",
  .name = "OpenStreetMap Japan", .name_ja = "OpenStreetMap 日本",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(openstreetmap_jp_def)
