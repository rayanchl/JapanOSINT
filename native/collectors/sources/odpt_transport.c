/* collectors/transport/sources/odpt_transport.c — port of
 * server/src/collectors/odptTransport.js. The ODPT API path requires a free
 * token; without it JS falls back to fetchOsmStations() (a single area.jp
 * Overpass query) which is the faithful live path ported here. ODPT live + seed
 * branches intentionally not ported (rule 7). No registry row → category
 * derived as "transport" from the collector domain. */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

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
  /* geojson pickText wants title|name|name_ja|label — "station_name" is none of
   * them, so all 10,178 stations persisted with a NULL title and were
   * unreadable in every UI. Mirror the OSM-derived name (and give the map a
   * back-link to the node itself, which no row carried). */
  cJSON_AddStringToObject(p, "name", nen ? nen : (nm ? nm : "Station"));
  if (nm) cJSON_AddStringToObject(p, "name_ja", nm);
  if (id && cJSON_IsNumber(id)) {
    char lk[96];
    snprintf(lk, sizeof lk, "https://www.openstreetmap.org/node/%lld",
             (long long)id->valuedouble);
    cJSON_AddStringToObject(p, "link", lk);
  }
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
    /* see marine_traffic.c: 60s per endpoint could never finish a nationwide
     * area.jp query on the one reachable Overpass mirror. */
    180, 150000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def odpt_transport_def = {
  .id = "odpt-transport", .collector = "transport",
  .name = "ODPT Transport", .name_ja = "ODPT 公共交通",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(odpt_transport_def)
