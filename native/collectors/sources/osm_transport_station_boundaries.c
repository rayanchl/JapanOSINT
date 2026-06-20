/* collectors/transport/sources/osm_transport_station_boundaries.c — port of
 * server/src/collectors/osmTransportStationBoundaries.js
 * (createOsmTransportCollector, geometry:'way' → fetchOverpassWaysTiled,
 *  factory way defaults {queryTimeout:180,timeoutMs:180_000}).
 * Nationwide OSM station-building Polygons. No source_registry.gen.c row —
 * internal unified-stations layer feed. No SEED / _meta envelope. */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  (void)ud;
  snprintf(o, n,
    "way[\"railway\"=\"station\"][\"building\"](%s);"
    "way[\"railway\"=\"station\"][\"area\"=\"yes\"](%s);"
    "way[\"public_transport\"=\"station\"][\"building\"](%s);"
    "way[\"public_transport\"=\"station\"][\"area\"=\"yes\"](%s);",
    bbox, bbox, bbox, bbox);
}

/* isClosed: Array.isArray(coords) && coords.length>=4 && first==last on
 * both lon & lat. coords is a cJSON array of [lon,lat] pairs. */
static int is_closed(cJSON *coords) {
  if (!coords || !cJSON_IsArray(coords)) return 0;
  int len = cJSON_GetArraySize(coords);
  if (len < 4) return 0;
  cJSON *a = cJSON_GetArrayItem(coords, 0);
  cJSON *b = cJSON_GetArrayItem(coords, len - 1);
  if (!a || !b) return 0;
  cJSON *a0 = cJSON_GetArrayItem(a, 0), *a1 = cJSON_GetArrayItem(a, 1);
  cJSON *b0 = cJSON_GetArrayItem(b, 0), *b1 = cJSON_GetArrayItem(b, 1);
  if (!a0 || !a1 || !b0 || !b1) return 0;
  return a0->valuedouble == b0->valuedouble
      && a1->valuedouble == b1->valuedouble;
}

/* coords toolkit-owned; DUPLICATE into geometry. Polygon = coords wrapped in
 * an outer ring array. Properties in EXACT JS key order. */
static cJSON *map(cJSON *el, int i, cJSON *coords, void *ud) {
  (void)i; (void)ud;
  if (!is_closed(coords)) return NULL;

  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Polygon");
  cJSON *rings = cJSON_CreateArray();
  cJSON_AddItemToArray(rings, cJSON_Duplicate(coords, 1));
  cJSON_AddItemToObject(g, "coordinates", rings);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();
  cJSON *idv = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "OSM_WAY_%lld",
           idv && cJSON_IsNumber(idv) ? (long long)idv->valuedouble : 0);
  cJSON_AddStringToObject(p, "footprint_id", fid);

  const char *nm = ov_tag(el, "name:en"); if (!nm) nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", nm ? nm : "Station");
  const char *nj = ov_tag(el, "name"); if (!nj) nj = ov_tag(el, "name:ja");
  cJSON_AddItemToObject(p, "name_ja", nj ? cJSON_CreateString(nj) : cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator", op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *rw = ov_tag(el, "railway");
  cJSON_AddItemToObject(p, "railway", rw ? cJSON_CreateString(rw) : cJSON_CreateNull());
  const char *bd = ov_tag(el, "building");
  cJSON_AddItemToObject(p, "building", bd ? cJSON_CreateString(bd) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_station_boundary");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_ways_collect(ctx, sink, body, 180, 180000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def osm_transport_station_boundaries_def = {
  .id = "osm-transport-station-boundaries", .collector = "transport",
  .name = "OSM Station Boundaries", .name_ja = "OSM 駅構内ポリゴン",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(osm_transport_station_boundaries_def)
