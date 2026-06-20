/* collectors/crime/sources/red_light_zones.c — port of
 * server/src/collectors/redLightZones.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_ZONES offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "../../source.h"
#include "../../lib/overpass.h"
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
  char zid[64];
  snprintf(zid, sizeof zid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "zone_id", zid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Venue %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *category = ov_tag(el, "amenity");
  if (category) cJSON_AddStringToObject(p, "category", category);
  else cJSON_AddItemToObject(p, "category", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"stripclub\"](area.jp);"
    "node[\"amenity\"=\"brothel\"](area.jp);"
    "node[\"amenity\"=\"nightclub\"][\"name\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def red_light_zones_def = {
  .id = "red-light-zones", .collector = "crime",
  .name = "Red Light Districts", .name_ja = "風俗営業地区",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(red_light_zones_def)
