/* collectors/infrastructure/sources/mlit_bridge.c
 * Port of server/src/collectors/mlitBridge.js (fetchOverpass — single
 * area.jp query). MLIT publishes only Excel/PDF inventory; OSM Overpass
 * man_made=bridge is the real queryable source. Honest empty on failure
 * (RULE 8). REFERENCE embassies.c (lib/overpass.h). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

/* JS: el.tags?.x || ... || null */
static void add_str_or_null(cJSON *p, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(p, k, v);
  else   cJSON_AddItemToObject(p, k, cJSON_CreateNull());
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *type = cJSON_GetObjectItem(el, "type");
  cJSON *id   = cJSON_GetObjectItem(el, "id");
  char osm[64];
  snprintf(osm, sizeof osm, "%s/%lld",
           (type && cJSON_IsString(type)) ? type->valuestring : "",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "osm_id", osm);

  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:ja");
  if (!name) name = ov_tag(el, "name:en");
  add_str_or_null(p, "name", name);
  add_str_or_null(p, "name_en", ov_tag(el, "name:en"));
  add_str_or_null(p, "structure", ov_tag(el, "bridge:structure"));
  add_str_or_null(p, "layer", ov_tag(el, "layer"));
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass_bridge");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"man_made\"=\"bridge\"](area.jp);"
    "relation[\"man_made\"=\"bridge\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_bridge_def = {
  .id = "mlit-bridge", .collector = "infrastructure",
  .name = "MLIT Bridge Inspection", .name_ja = "国交省 橋梁点検",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(mlit_bridge_def)
