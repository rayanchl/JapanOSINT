/* collectors/infrastructure/sources/cell_towers.c
 * Port of server/src/collectors/cellTowers.js.
 * The primary tryOpenCellID() path needs OPENCELLID_KEY (no faithful keyless
 * live path). The faithful keyless live path the JS actually runs is
 * tryOSMCommTowers() — a fetchOverpassTiled sweep of OSM comm-tower tagging.
 * The curated MIC_STATIONS / RAKUTEN_TOWERS / generateSeedData() synthetic
 * arrays are NOT ported (HARD RULE 7 — offline seed). Properties built in
 * EXACT JS key order (featureUid parity). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void body(const char *b, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"tower:type\"=\"communication\"](%s);"
    "way[\"tower:type\"=\"communication\"](%s);"
    "node[\"man_made\"=\"mast\"][\"tower:type\"~\"communication|cellular\"](%s);"
    "way[\"man_made\"=\"mast\"][\"tower:type\"~\"communication|cellular\"](%s);"
    "node[\"man_made\"=\"communications_tower\"](%s);"
    "way[\"man_made\"=\"communications_tower\"](%s);"
    "node[\"communication:mobile_phone\"=\"yes\"](%s);"
    "way[\"communication:mobile_phone\"=\"yes\"](%s);",
    b, b, b, b, b, b, b, b);
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *etype = cJSON_GetObjectItem(el, "type");
  cJSON *eid = cJSON_GetObjectItem(el, "id");
  const char *ets = (etype && cJSON_IsString(etype)) ? etype->valuestring : "";
  long long eidv = (eid && cJSON_IsNumber(eid)) ? (long long)eid->valuedouble : 0;
  char idbuf[96];
  snprintf(idbuf, sizeof idbuf, "OSM_%s_%lld", ets, eidv);
  cJSON_AddStringToObject(p, "tower_id", idbuf);

  const char *name = ov_tag(el, "name");
  if (name) cJSON_AddStringToObject(p, "name", name);
  else {
    char nb[32]; snprintf(nb, sizeof nb, "Comm tower %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }
  const char *oper = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", oper ? oper : "unknown");
  const char *opshort = ov_tag(el, "operator:short");
  const char *carrier = oper ? oper : (opshort ? opshort : "unknown");
  cJSON_AddStringToObject(p, "carrier", carrier);
  const char *tt = ov_tag(el, "tower:type");
  if (!tt) tt = ov_tag(el, "man_made");
  cJSON_AddStringToObject(p, "tower_type", tt ? tt : "communication");
  const char *h = ov_tag(el, "height");           /* parseFloat||null */
  cJSON_AddItemToObject(p, "height_m",
                        h ? cJSON_CreateNumber(atof(h)) : cJSON_CreateNull());
  const char *mp = ov_tag(el, "communication:mobile_phone");
  cJSON_AddStringToObject(p, "radio",
                          (mp && strcmp(mp, "yes") == 0) ? "mobile"
                                                         : "communication");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def cell_towers_def = {
  .id = "cell-towers", .collector = "infrastructure",
  .name = "Cell Towers", .name_ja = "携帯基地局",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(cell_towers_def)
