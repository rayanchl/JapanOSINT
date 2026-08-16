/* collectors/defense/sources/usfj_bases.c — port of
 * server/src/collectors/usfjBases.js. fetchOverpass (single area.jp query,
 * tryOverpass). SEED_USFJ offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char bid[64];
  snprintf(bid, sizeof bid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "base_id", bid);
  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "US Military Installation");
  cJSON_AddStringToObject(p, "branch", "USFJ");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"landuse\"=\"military\"][\"operator\"~\"US|United States\"](area.jp);"
    "relation[\"landuse\"=\"military\"][\"operator\"~\"US|United States\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def usfj_bases_def = {
  .id = "usfj-bases", .collector = "defense", .name = "USFJ Bases",
  .name_ja = "在日米軍基地", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(usfj_bases_def)
