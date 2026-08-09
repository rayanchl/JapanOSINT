/* collectors/industry/sources/shipyards.c — port of
 * server/src/collectors/shipyards.js. fetchOverpass (single area.jp query,
 * tryOverpass). SEED_SHIPYARDS offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char yid[64];
  snprintf(yid, sizeof yid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "yard_id", yid);
  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Shipyard");
  const char *company = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "company", company ? company : "unknown");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"industrial\"=\"shipyard\"](area.jp);"
    "way[\"landuse\"=\"industrial\"][\"shipyard\"=\"yes\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def shipyards_def = {
  .id = "shipyards", .collector = "industry", .name = "Shipyards",
  .name_ja = "造船所", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(shipyards_def)
