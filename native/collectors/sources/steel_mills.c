/* collectors/industry/sources/steel_mills.c — port of
 * server/src/collectors/steelMills.js. fetchOverpass (single area.jp query,
 * tryOverpass). SEED_STEEL offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char mid[64];
  snprintf(mid, sizeof mid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "mill_id", mid);
  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Steel Mill");
  const char *company = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "company", company ? company : "unknown");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"industrial\"=\"steel\"](area.jp);"
    "way[\"landuse\"=\"industrial\"][\"product\"=\"steel\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def steel_mills_def = {
  .id = "steel-mills", .collector = "industry", .name = "Steel Mills",
  .name_ja = "製鉄所", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(steel_mills_def)
