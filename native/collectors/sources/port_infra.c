/* collectors/transport/sources/port_infra.c — port of
 * server/src/collectors/portInfra.js. Primary live path = tryOSMPorts()
 * Overpass query; the curated PORT_FACILITIES seed (only used when OSM is
 * down) is intentionally not ported (rule 7). No registry row for
 * "port-infra" (only unified-port-infra exists, category "transport");
 * category derived as "transport". */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
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
  char pid[64];
  snprintf(pid, sizeof pid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "port_id", pid);
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char dn[48];
    snprintf(dn, sizeof dn, "Port facility %d", i + 1);
    cJSON_AddStringToObject(p, "name", dn);
  }
  const char *nja = ov_tag(el, "name:ja");
  if (!nja) nja = ov_tag(el, "name");
  if (nja) cJSON_AddStringToObject(p, "name_ja", nja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *hb = ov_tag(el, "harbour");
  cJSON_AddStringToObject(p, "port_class", hb ? hb : "unknown");
  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");
  const char *pv = ov_tag(el, "addr:province");
  if (!pv) pv = ov_tag(el, "addr:state");
  if (pv) cJSON_AddStringToObject(p, "prefecture", pv);
  else cJSON_AddItemToObject(p, "prefecture", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"harbour\"](area.jp);"
    "way[\"harbour\"](area.jp);"
    "node[\"industrial\"=\"port\"](area.jp);"
    "way[\"landuse\"=\"port\"](area.jp);"
    "node[\"seamark:type\"=\"harbour\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def port_infra_def = {
  .id = "port-infra", .collector = "transport",
  .name = "Port Infrastructure", .name_ja = "港湾インフラ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(port_infra_def)
