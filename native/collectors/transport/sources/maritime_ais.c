/* collectors/transport/sources/maritime_ais.c — port of
 * server/src/collectors/maritimeAis.js. The JS cascades MarineTraffic API,
 * VesselFinder API (both key-gated), then an OSM Overpass harbour-node proxy
 * (tryOverpassHarbours, single area.jp query) which is the faithful keyless
 * live path ported here. The curated JAPAN_PORTS/shipping-lane seed / _meta
 * envelope is intentionally not ported (JS does `features = []` when nothing
 * live). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
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
  char vid[32];
  snprintf(vid, sizeof vid, "AIS_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "vessel_id", vid);
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  char nbuf[48];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Harbour %lld", oid); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  cJSON_AddStringToObject(p, "vessel_type", "harbour");
  const char *nm = ov_tag(el, "name");
  if (nm) cJSON_AddStringToObject(p, "port", nm);
  else cJSON_AddItemToObject(p, "port", cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass_harbours");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"seamark:type\"=\"harbour\"](area.jp);"
    "node[\"harbour\"=\"yes\"](area.jp);"
    "way[\"harbour\"=\"yes\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def maritime_ais_def = {
  .id = "maritime-ais", .collector = "transport",
  .name = "AIS Ship Tracking", .name_ja = "AIS 船舶追跡",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(maritime_ais_def)
