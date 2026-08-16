/* collectors/transport/sources/bus_routes.c — port of
 * server/src/collectors/busRoutes.js (fetchOverpass single area.jp).
 * BUS_TERMINALS offline fallback intentionally not ported (rule 8). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

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

/* AUDIT NOTE (slice a3): the HTTP timeout used to be 60 000 ms while the query
 * itself asks Overpass for up to [timeout:180] seconds of server time — the
 * client hung up before the server was even allowed to finish. Measured on
 * 2026-07-31: overpass-api.de (endpoint #1 in lib/overpass.c) refuses
 * connections outright, and overpass.kumi.systems (#2) answers a *small*
 * nationwide query in ~78 s. So every single-query Overpass source in this
 * slice timed out 100% of the time. The HTTP budget now exceeds the query
 * budget, which is the only configuration that can ever succeed. The same
 * one-line change was applied to the 11 sibling single-query Overpass sources
 * in this slice. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"bus_station\"](area.jp);",
    180, 200000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def bus_routes_def = {
  .id = "bus-routes", .collector = "transport",
  .name = "Bus Terminals", .name_ja = "バスターミナル",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(bus_routes_def)
