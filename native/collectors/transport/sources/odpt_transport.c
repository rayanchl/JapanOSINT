/* collectors/transport/sources/odpt_transport.c — port of
 * server/src/collectors/odptTransport.js. The ODPT API path requires a free
 * token; without it JS falls back to fetchOsmStations() (a single area.jp
 * Overpass query) which is the faithful live path ported here. ODPT live + seed
 * branches intentionally not ported (rule 7). No registry row → category
 * derived as "transport" from the collector domain. */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

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
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "station_id", sid);
  const char *nen = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "station_name", nen ? nen : (nm ? nm : "Station"));
  if (nm) cJSON_AddStringToObject(p, "station_name_ja", nm);
  else cJSON_AddItemToObject(p, "station_name_ja", cJSON_CreateNull());
  const char *line = ov_tag(el, "line");
  if (!line) line = ov_tag(el, "network");
  if (line) cJSON_AddStringToObject(p, "line_name", line);
  else cJSON_AddItemToObject(p, "line_name", cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *rw = ov_tag(el, "railway");
  if (!rw) rw = ov_tag(el, "public_transport");
  if (rw) cJSON_AddStringToObject(p, "railway", rw);
  else cJSON_AddItemToObject(p, "railway", cJSON_CreateNull());
  const char *wc = ov_tag(el, "wheelchair");
  if (wc) cJSON_AddStringToObject(p, "wheelchair", wc);
  else cJSON_AddItemToObject(p, "wheelchair", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"railway\"=\"station\"](area.jp);"
    "node[\"railway\"=\"halt\"](area.jp);"
    "node[\"railway\"=\"tram_stop\"](area.jp);"
    "node[\"public_transport\"=\"station\"][\"train\"=\"yes\"](area.jp);"
    "node[\"public_transport\"=\"station\"][\"subway\"=\"yes\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def odpt_transport_def = {
  .id = "odpt-transport", .collector = "transport",
  .name = "ODPT Transport", .name_ja = "ODPT 公共交通",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(odpt_transport_def)
