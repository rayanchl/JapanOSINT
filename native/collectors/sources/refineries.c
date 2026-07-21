/* collectors/industry/sources/refineries.c — port of
 * server/src/collectors/refineries.js. fetchOverpass (single area.jp query,
 * tryOverpass). SEED_REFINERIES offline fallback intentionally not ported
 * (JS does `if (!live) features = []`). */
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
  char rid[64];
  snprintf(rid, sizeof rid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "refinery_id", rid);
  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Refinery");
  const char *company = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "company", company ? company : "unknown");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"industrial\"=\"oil_refinery\"](area.jp);"
    "way[\"industrial\"=\"refinery\"](area.jp);"
    "way[\"man_made\"=\"petroleum_refinery\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def refineries_def = {
  .id = "refineries", .collector = "industry", .name = "Oil Refineries",
  .name_ja = "製油所", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(refineries_def)
