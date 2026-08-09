/* collectors/infrastructure/sources/japan_post_offices.c
 * Port of server/src/collectors/japanPostOffices.js (fetchOverpassTiled).
 * Single nationwide tiled Overpass sweep of amenity=post_office. */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"amenity\"=\"post_office\"](%s);"
    "way[\"amenity\"=\"post_office\"](%s);",
    bbox, bbox);
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "PO_OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "id", fid);
  const char *nen = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", nen ? nen : (nm ? nm : "Post Office"));
  const char *nja = ov_tag(el, "name:ja");
  if (!nja) nja = nm;
  cJSON_AddItemToObject(p, "name_ja",
                        nja ? cJSON_CreateString(nja) : cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *pc = ov_tag(el, "addr:postcode");
  cJSON_AddItemToObject(p, "addr_postcode",
                        pc ? cJSON_CreateString(pc) : cJSON_CreateNull());
  const char *city = ov_tag(el, "addr:city");
  cJSON_AddItemToObject(p, "addr_city",
                        city ? cJSON_CreateString(city) : cJSON_CreateNull());
  const char *oh = ov_tag(el, "opening_hours");
  cJSON_AddItemToObject(p, "opening_hours",
                        oh ? cJSON_CreateString(oh) : cJSON_CreateNull());
  const char *ph = ov_tag(el, "phone");
  cJSON_AddItemToObject(p, "phone",
                        ph ? cJSON_CreateString(ph) : cJSON_CreateNull());
  const char *web = ov_tag(el, "website");
  if (!web) web = ov_tag(el, "contact:website");
  cJSON_AddItemToObject(p, "website",
                        web ? cJSON_CreateString(web) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass_post_office");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 200000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def japan_post_offices_def = {
  .id = "japan-post-offices", .collector = "infrastructure",
  .name = "Japan Post Offices", .name_ja = "郵便局",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(japan_post_offices_def)
