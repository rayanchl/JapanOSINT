/* collectors/infrastructure/sources/transmission_towers.c
 * Port of server/src/collectors/transmissionTowers.js (fetchOverpassTiled).
 * Single nationwide tiled Overpass sweep of power towers/poles/substations. */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"power\"=\"tower\"](%s);"
    "node[\"tower:type\"=\"transmission\"](%s);"
    "node[\"power\"=\"pole\"][\"tower:type\"!=\"lighting\"](%s);"
    "node[\"power\"=\"substation\"](%s);"
    "way[\"power\"=\"substation\"](%s);",
    bbox, bbox, bbox, bbox, bbox);
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "PWR_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "id", fid);
  const char *nen = ov_tag(el, "name:en");
  if (!nen) nen = ov_tag(el, "name");
  cJSON_AddItemToObject(p, "name",
                        nen ? cJSON_CreateString(nen) : cJSON_CreateNull());
  const char *pw = ov_tag(el, "power");
  cJSON_AddItemToObject(p, "power",
                        pw ? cJSON_CreateString(pw) : cJSON_CreateNull());
  const char *tt = ov_tag(el, "tower:type");
  cJSON_AddItemToObject(p, "tower_type",
                        tt ? cJSON_CreateString(tt) : cJSON_CreateNull());
  const char *v = ov_tag(el, "voltage");
  cJSON_AddItemToObject(p, "voltage",
                        v ? cJSON_CreateString(v) : cJSON_CreateNull());
  const char *mat = ov_tag(el, "material");
  cJSON_AddItemToObject(p, "material",
                        mat ? cJSON_CreateString(mat) : cJSON_CreateNull());
  const char *ref = ov_tag(el, "ref");
  cJSON_AddItemToObject(p, "ref",
                        ref ? cJSON_CreateString(ref) : cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def transmission_towers_def = {
  .id = "transmission-towers", .collector = "infrastructure",
  .name = "OSM Transmission Towers", .name_ja = "OSM 送電鉄塔",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(transmission_towers_def)
