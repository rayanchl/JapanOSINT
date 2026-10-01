/* collectors/infrastructure/sources/utility_poles.c
 * Port of server/src/collectors/utilityPoles.js (fetchOverpassTiled).
 * Single nationwide tiled Overpass sweep of man_made=utility_pole. */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"man_made\"=\"utility_pole\"](%s);"
    "node[\"utility\"=\"power\"](%s);"
    "node[\"utility\"=\"telecom\"](%s);",
    bbox, bbox, bbox);
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "UPOLE_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "id", fid);
  const char *nm = ov_tag(el, "name");
  cJSON_AddItemToObject(p, "name",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
  const char *ut = ov_tag(el, "utility");
  cJSON_AddItemToObject(p, "utility",
                        ut ? cJSON_CreateString(ut) : cJSON_CreateNull());
  const char *mat = ov_tag(el, "material");
  cJSON_AddItemToObject(p, "material",
                        mat ? cJSON_CreateString(mat) : cJSON_CreateNull());
  const char *h = ov_tag(el, "height");
  cJSON_AddItemToObject(p, "height",
                        h ? cJSON_CreateString(h) : cJSON_CreateNull());
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

static const source_def utility_poles_def = {
  .id = "utility-poles", .collector = "infrastructure",
  .name = "OSM Utility Poles", .name_ja = "OSM 電柱",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(utility_poles_def)
