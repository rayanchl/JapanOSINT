/* collectors/infrastructure/sources/bridge_tunnel_infra.c — port of
 * server/src/collectors/bridgeTunnelInfra.js. Primary live path is
 * tryOSMBridgesTunnels() (fetchOverpass, single area.jp query). The curated
 * BRIDGE_TUNNEL_FACILITIES seed / _meta envelope is intentionally not ported
 * (seedFeatures is [] and JS does `if (!live) features = []`). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "structure_id", sid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Structure %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *nja = ov_tag(el, "name");
  if (nja) cJSON_AddStringToObject(p, "name_ja", nja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *mm = ov_tag(el, "man_made");
  const char *bridge = ov_tag(el, "bridge");
  const char *tunnel = ov_tag(el, "tunnel");
  cJSON_AddStringToObject(p, "facility_type",
    ((mm && strcmp(mm, "bridge") == 0) || bridge) ? "bridge" : "tunnel");
  cJSON_AddStringToObject(p, "structure_type",
    bridge ? bridge : tunnel ? tunnel : "unknown");
  const char *len = ov_tag(el, "length");
  double lv = len ? strtod(len, NULL) : 0;
  if (lv) cJSON_AddNumberToObject(p, "length_m", lv);
  else cJSON_AddItemToObject(p, "length_m", cJSON_CreateNull());
  const char *spn = ov_tag(el, "span");
  double sv = spn ? strtod(spn, NULL) : 0;
  if (sv) cJSON_AddNumberToObject(p, "span_m", sv);
  else cJSON_AddItemToObject(p, "span_m", cJSON_CreateNull());
  const char *dep = ov_tag(el, "depth");
  double dv = dep ? strtod(dep, NULL) : 0;
  if (dv) cJSON_AddNumberToObject(p, "depth_m", dv);
  else cJSON_AddItemToObject(p, "depth_m", cJSON_CreateNull());
  const char *yo = ov_tag(el, "start_date");
  if (!yo) yo = ov_tag(el, "opening_date");
  if (yo) cJSON_AddStringToObject(p, "year_opened", yo);
  else cJSON_AddItemToObject(p, "year_opened", cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");
  cJSON_AddItemToObject(p, "inspection_grade", cJSON_CreateNull());
  cJSON_AddItemToObject(p, "prefecture", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"bridge\"](area.jp);"
    "way[\"bridge\"=\"yes\"][\"name\"](area.jp);"
    "node[\"tunnel\"=\"yes\"][\"name\"](area.jp);"
    "way[\"tunnel\"=\"yes\"][\"name\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def bridge_tunnel_infra_def = {
  .id = "bridge-tunnel-infra", .collector = "infrastructure",
  .name = "Bridge & Tunnel Infrastructure", .name_ja = "橋梁・トンネルインフラ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(bridge_tunnel_infra_def)
