/* collectors/transport/sources/marine_traffic.c — port of
 * server/src/collectors/marineTraffic.js. The JS primary path is the
 * MarineTraffic Exportvessels REST API, gated on MARINETRAFFIC_API_KEY;
 * without a key it returns null and the collector falls through to an OSM
 * Overpass harbour-node proxy (tryOsmPorts, single area.jp query) which is
 * the faithful keyless live path. The curated SEED_PORTS / _meta envelope is
 * intentionally not ported (JS does `features = []` when nothing live). */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char mid[64];
  snprintf(mid, sizeof mid, "MT_OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "id", mid);
  const char *name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Harbour %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "vessel_name", name);
  cJSON_AddStringToObject(p, "vessel_type", "harbour");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass_port");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"seamark:type\"=\"harbour\"](area.jp);"
    "node[\"harbour\"=\"yes\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def marine_traffic_def = {
  .id = "marine-traffic", .collector = "transport",
  .name = "MarineTraffic AIS", .name_ja = "マリントラフィック AIS",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(marine_traffic_def)
