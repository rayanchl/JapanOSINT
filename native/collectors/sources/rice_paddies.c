/* collectors/agriculture/sources/rice_paddies.c — port of
 * server/src/collectors/ricePaddies.js. fetchOverpass (single area.jp query,
 * tryLive). SEED_PADDIES offline fallback intentionally not ported (JS does
 * `if (!live) features = []`). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char pid[64];
  snprintf(pid, sizeof pid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "paddy_id", pid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Rice paddy %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"landuse\"=\"farmland\"][\"crop\"=\"rice\"](area.jp);"
    "relation[\"landuse\"=\"farmland\"][\"crop\"=\"rice\"](area.jp);"
    "way[\"crop\"=\"rice\"][\"name\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def rice_paddies_def = {
  .id = "rice-paddies", .collector = "agriculture", .name = "Rice Paddies",
  .name_ja = "稲作産地", 
  .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(rice_paddies_def)
