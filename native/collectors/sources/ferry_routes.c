/* collectors/transport/sources/ferry_routes.c — port of
 * server/src/collectors/ferryRoutes.js (fetchOverpass single area.jp).
 * FERRY_TERMINALS offline fallback intentionally not ported (rule 8). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  char fb[48];
  snprintf(fb, sizeof fb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "ferry_id", fb);

  const char *nm = ov_tag(el, "name");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[40];
    snprintf(nb, sizeof nb, "Ferry terminal %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");
  cJSON_AddStringToObject(p, "type", "ferry_terminal");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"ferry_terminal\"](area.jp);"
    "way[\"amenity\"=\"ferry_terminal\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def ferry_routes_def = {
  .id = "ferry-routes", .collector = "transport",
  .name = "Ferry Terminals", .name_ja = "フェリーターミナル",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(ferry_routes_def)
