/* collectors/transport/sources/osm_transport_ports.c — port of
 * server/src/collectors/osmTransportPorts.js
 * (createOsmTransportCollector, geometry:'point' → fetchOverpassTiled,
 *  overpassOpts {queryTimeout:120,timeoutMs:90_000}).
 * Always-on harbours, ferry terminals, marinas. No SEED / _meta envelope. */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <string.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  (void)ud;
  snprintf(o, n,
    "node[\"seamark:type\"=\"harbour\"](%s);"
    "node[\"harbour\"=\"yes\"](%s);"
    "way[\"harbour\"=\"yes\"](%s);"
    "node[\"amenity\"=\"ferry_terminal\"](%s);"
    "way[\"amenity\"=\"ferry_terminal\"](%s);"
    "node[\"leisure\"=\"marina\"](%s);"
    "way[\"leisure\"=\"marina\"](%s);",
    bbox, bbox, bbox, bbox, bbox, bbox, bbox);
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
  const char *le = ov_tag(el, "leisure");
  const char *kind = (am && strcmp(am, "ferry_terminal") == 0) ? "ferry_terminal"
                   : (le && strcmp(le, "marina") == 0) ? "marina"
                   : "harbour";

  cJSON *p = cJSON_CreateObject();
  cJSON *idv = cJSON_GetObjectItem(el, "id");
  char pid[64];
  snprintf(pid, sizeof pid, "OSM_%lld",
           idv && cJSON_IsNumber(idv) ? (long long)idv->valuedouble : 0);
  cJSON_AddStringToObject(p, "port_id", pid);

  const char *nm = ov_tag(el, "name:en"); if (!nm) nm = ov_tag(el, "name");
  cJSON_AddItemToObject(p, "name", nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
  const char *nj = ov_tag(el, "name"); if (!nj) nj = ov_tag(el, "name:ja");
  cJSON_AddItemToObject(p, "name_ja", nj ? cJSON_CreateString(nj) : cJSON_CreateNull());
  const char *cat = ov_tag(el, "seamark:harbour:category");
  cJSON_AddStringToObject(p, "classification", cat ? cat : kind);
  cJSON_AddStringToObject(p, "kind", kind);
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator", op ? cJSON_CreateString(op) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_transport_ports");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 120, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def osm_transport_ports_def = {
  .id = "osm-transport-ports", .collector = "transport",
  .name = "OSM Transport Ports", .name_ja = "OSM Ports",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(osm_transport_ports_def)
