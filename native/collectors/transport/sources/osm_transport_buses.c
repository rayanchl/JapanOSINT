/* collectors/transport/sources/osm_transport_buses.c — port of
 * server/src/collectors/osmTransportBuses.js
 * (createOsmTransportCollector, geometry:'point' → fetchOverpassTiled,
 *  overpassOpts {queryTimeout:180,timeoutMs:150_000}).
 * Always-on bus stops + terminals. No SEED / _meta envelope. */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <string.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  (void)ud;
  snprintf(o, n,
    "node[\"highway\"=\"bus_stop\"](%s);"
    "node[\"public_transport\"=\"platform\"][\"bus\"=\"yes\"](%s);"
    "node[\"amenity\"=\"bus_station\"](%s);"
    "way[\"amenity\"=\"bus_station\"](%s);"
    "node[\"public_transport\"=\"station\"][\"bus\"=\"yes\"](%s);",
    bbox, bbox, bbox, bbox, bbox);
}

/* properties built in EXACT JS key order. */
static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  const char *am = ov_tag(el, "amenity");
  const char *pt = ov_tag(el, "public_transport");
  int is_terminal = (am && strcmp(am, "bus_station") == 0)
                 || (pt && strcmp(pt, "station") == 0);

  cJSON *p = cJSON_CreateObject();
  cJSON *idv = cJSON_GetObjectItem(el, "id");
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           idv && cJSON_IsNumber(idv) ? (long long)idv->valuedouble : 0);
  cJSON_AddStringToObject(p, "stop_id", sid);

  const char *nm = ov_tag(el, "name:en"); if (!nm) nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", nm ? nm : "Bus stop");
  const char *nj = ov_tag(el, "name"); if (!nj) nj = ov_tag(el, "name:ja");
  cJSON_AddItemToObject(p, "name_ja", nj ? cJSON_CreateString(nj) : cJSON_CreateNull());
  const char *op = ov_tag(el, "operator"); if (!op) op = ov_tag(el, "network");
  cJSON_AddItemToObject(p, "operator", op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *rt = ov_tag(el, "route_ref");
  cJSON_AddItemToObject(p, "route", rt ? cJSON_CreateString(rt) : cJSON_CreateNull());
  const char *sh = ov_tag(el, "shelter");
  cJSON_AddItemToObject(p, "shelter", sh ? cJSON_CreateString(sh) : cJSON_CreateNull());
  const char *wc = ov_tag(el, "wheelchair");
  cJSON_AddItemToObject(p, "wheelchair", wc ? cJSON_CreateString(wc) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "kind", is_terminal ? "terminal" : "stop");
  cJSON_AddStringToObject(p, "source", "osm_transport_buses");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 150000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def osm_transport_buses_def = {
  .id = "osm-transport-buses", .collector = "transport",
  .name = "OSM Transport Buses", .name_ja = "OSM Buses",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(osm_transport_buses_def)
