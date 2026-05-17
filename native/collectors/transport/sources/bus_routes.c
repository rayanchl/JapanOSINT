/* collectors/transport/sources/bus_routes.c — port of
 * server/src/collectors/busRoutes.js (fetchOverpass single area.jp).
 * BUS_TERMINALS offline fallback intentionally not ported (rule 8). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  char bb[48];
  snprintf(bb, sizeof bb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "bus_id", bb);

  const char *en = ov_tag(el, "name");
  if (!en) en = ov_tag(el, "name:en");
  if (en) {
    cJSON_AddStringToObject(p, "name", en);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Bus station %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");
  cJSON_AddStringToObject(p, "bus_type", "terminal");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"bus_station\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def bus_routes_def = {
  .id = "bus-routes", .collector = "transport",
  .name = "Bus Terminals", .name_ja = "バスターミナル",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(bus_routes_def)
