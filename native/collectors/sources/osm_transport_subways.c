/* collectors/transport/sources/osm_transport_subways.c — port of
 * server/src/collectors/osmTransportSubways.js
 * (createOsmTransportCollector, geometry:'point' → fetchOverpassTiled).
 * Always-on subway / metro / monorail / tram / light_rail stops.
 * No SEED / _meta envelope. */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include "../../lib/linecolor.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  (void)ud;
  snprintf(o, n,
    "node[\"station\"=\"subway\"](%s);"
    "node[\"station\"=\"light_rail\"](%s);"
    "node[\"station\"=\"monorail\"](%s);"
    "node[\"railway\"=\"tram_stop\"](%s);"
    "node[\"public_transport\"=\"station\"][\"subway\"=\"yes\"](%s);"
    "node[\"public_transport\"=\"station\"][\"monorail\"=\"yes\"](%s);"
    "node[\"public_transport\"=\"station\"][\"tram\"=\"yes\"](%s);",
    bbox, bbox, bbox, bbox, bbox, bbox, bbox);
}

/* properties built in EXACT JS key order. */
static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *idv = cJSON_GetObjectItem(el, "id");
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           idv && cJSON_IsNumber(idv) ? (long long)idv->valuedouble : 0);
  cJSON_AddStringToObject(p, "station_id", sid);

  const char *nm = ov_tag(el, "name:en"); if (!nm) nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", nm ? nm : "Station");
  const char *nj = ov_tag(el, "name"); if (!nj) nj = ov_tag(el, "name:ja");
  cJSON_AddItemToObject(p, "name_ja", nj ? cJSON_CreateString(nj) : cJSON_CreateNull());
  const char *ln = ov_tag(el, "line"); if (!ln) ln = ov_tag(el, "network");
  cJSON_AddItemToObject(p, "line", ln ? cJSON_CreateString(ln) : cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator", op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *st = ov_tag(el, "station"); if (!st) st = ov_tag(el, "railway");
  cJSON_AddStringToObject(p, "type", st ? st : "subway");
  const char *nw = ov_tag(el, "network");
  cJSON_AddItemToObject(p, "network", nw ? cJSON_CreateString(nw) : cJSON_CreateNull());
  const char *wd = ov_tag(el, "wikidata");
  cJSON_AddItemToObject(p, "wikidata", wd ? cJSON_CreateString(wd) : cJSON_CreateNull());
  const char *wc = ov_tag(el, "wheelchair");
  cJSON_AddItemToObject(p, "wheelchair", wc ? cJSON_CreateString(wc) : cJSON_CreateNull());
  char lc[8];
  cJSON_AddItemToObject(p, "line_color",
    line_color(cJSON_GetObjectItem(el, "tags"), lc) ? cJSON_CreateString(lc)
                                                    : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_transport_subways");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 120000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def osm_transport_subways_def = {
  .id = "osm-transport-subways", .collector = "transport",
  .name = "OSM Transport Subways", .name_ja = "OSM Subways",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(osm_transport_subways_def)
