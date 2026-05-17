/* collectors/infrastructure/sources/water_towers.c — port of
 * server/src/collectors/waterTowers.js. fetchOverpass(body, mapFn, 60000,
 * {limit:0, queryTimeout:180}) — single area.jp query. No SEED branch in JS
 * (returns live rows or []). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char wid[64];
  snprintf(wid, sizeof wid, "WATER_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "id", wid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Water infrastructure");
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *kind = ov_tag(el, "man_made");
  if (kind) cJSON_AddStringToObject(p, "kind", kind);
  else cJSON_AddItemToObject(p, "kind", cJSON_CreateNull());
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *height = ov_tag(el, "height");
  if (height) cJSON_AddStringToObject(p, "height", height);
  else cJSON_AddItemToObject(p, "height", cJSON_CreateNull());
  const char *capacity = ov_tag(el, "capacity");
  if (capacity) cJSON_AddStringToObject(p, "capacity", capacity);
  else cJSON_AddItemToObject(p, "capacity", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"water_tower\"](area.jp);"
    "way[\"man_made\"=\"water_tower\"](area.jp);"
    "node[\"man_made\"=\"water_works\"](area.jp);"
    "way[\"man_made\"=\"water_works\"](area.jp);"
    "node[\"man_made\"=\"reservoir_covered\"](area.jp);"
    "way[\"man_made\"=\"reservoir_covered\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def water_towers_def = {
  .id = "water-towers", .collector = "infrastructure",
  .name = "OSM Water Towers", .name_ja = "OSM 給水塔",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(water_towers_def)
