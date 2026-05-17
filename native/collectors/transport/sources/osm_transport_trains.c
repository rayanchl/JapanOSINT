/* collectors/transport/sources/osm_transport_trains.c — port of
 * server/src/collectors/osmTransportTrains.js
 * (createOsmTransportCollector, geometry:'point' → fetchOverpassTiled).
 * Always-on nationwide mainline train stations. No SEED / _meta envelope. */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include "../../../lib/linecolor.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  (void)ud;
  snprintf(o, n,
    "node[\"railway\"=\"station\"](%s);"
    "node[\"railway\"=\"halt\"](%s);"
    "way[\"railway\"=\"station\"](%s);"
    "node[\"public_transport\"=\"station\"][\"train\"=\"yes\"](%s);",
    bbox, bbox, bbox, bbox);
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
  const char *st = ov_tag(el, "station");
  cJSON_AddStringToObject(p, "type", st ? st : "railway");
  const char *cl = ov_tag(el, "railway");
  cJSON_AddItemToObject(p, "classification", cl ? cJSON_CreateString(cl) : cJSON_CreateNull());
  const char *uic = ov_tag(el, "uic_ref");
  cJSON_AddItemToObject(p, "uic_ref", uic ? cJSON_CreateString(uic) : cJSON_CreateNull());
  const char *wd = ov_tag(el, "wikidata");
  cJSON_AddItemToObject(p, "wikidata", wd ? cJSON_CreateString(wd) : cJSON_CreateNull());
  const char *wc = ov_tag(el, "wheelchair");
  cJSON_AddItemToObject(p, "wheelchair", wc ? cJSON_CreateString(wc) : cJSON_CreateNull());
  char lc[8];
  cJSON_AddItemToObject(p, "line_color",
    line_color(cJSON_GetObjectItem(el, "tags"), lc) ? cJSON_CreateString(lc)
                                                    : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_transport_trains");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 120000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def osm_transport_trains_def = {
  .id = "osm-transport-trains", .collector = "transport",
  .name = "OSM Transport Trains", .name_ja = "OSM Trains",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(osm_transport_trains_def)
