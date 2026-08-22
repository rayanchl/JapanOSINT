/* collectors/crime/sources/red_light_zones.c — port of
 * server/src/collectors/redLightZones.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_ZONES offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char zid[64];
  snprintf(zid, sizeof zid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "zone_id", zid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  /* no-fabrication (house rule 1): OSM carried no name tag for this element.
   * The old code wrote "Venue %d" + the loop index, which is both an invented
   * label and an UNSTABLE one — it feeds geojson's content-hash uid, so the
   * same object was re-keyed whenever Overpass changed element order. An
   * absent name is serialized as null; pick_text() skips nulls, so the row
   * persists with a NULL title rather than a made-up one. */
  if (name) cJSON_AddStringToObject(p, "name", name);
  else cJSON_AddItemToObject(p, "name", cJSON_CreateNull());
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
    180, 200000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def red_light_zones_def = {
  .id = "red-light-zones", .collector = "crime",
  .name = "Red Light Districts", .name_ja = "風俗営業地区",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(red_light_zones_def)
