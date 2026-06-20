/* collectors/geospatial/sources/parking_facilities.c
 * Port of server/src/collectors/parkingFacilities.js (fetchOverpassTiled).
 * Single nationwide tiled Overpass sweep of amenity=parking. */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"amenity\"=\"parking\"](%s);"
    "way[\"amenity\"=\"parking\"](%s);"
    "node[\"amenity\"=\"parking_entrance\"](%s);",
    bbox, bbox, bbox);
}

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

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "PARK_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "id", fid);
  const char *nen = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", nen ? nen : (nm ? nm : "Parking"));
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
  const char *pt = ov_tag(el, "parking");
  cJSON_AddItemToObject(p, "parking_type",
                        pt ? cJSON_CreateString(pt) : cJSON_CreateNull());
  const char *acc = ov_tag(el, "access");
  cJSON_AddStringToObject(p, "access", acc ? acc : "public");
  const char *fee = ov_tag(el, "fee");
  cJSON_AddItemToObject(p, "fee",
                        fee ? cJSON_CreateString(fee) : cJSON_CreateNull());
  const char *cap = ov_tag(el, "capacity");           /* Number()||null */
  cJSON *capv = cJSON_CreateNull();
  if (cap) {
    char *end; double d = strtod(cap, &end);
    while (*end == ' ' || *end == '\t') end++;
    if (end != cap && *end == '\0') capv = cJSON_CreateNumber(d);
  }
  cJSON_AddItemToObject(p, "capacity", capv);
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *sup = ov_tag(el, "supervised");
  cJSON_AddItemToObject(p, "supervised",
                        sup ? cJSON_CreateString(sup) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def parking_facilities_def = {
  .id = "parking-facilities", .collector = "geospatial",
  .name = "OSM Parking Facilities", .name_ja = "OSM 駐車場",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(parking_facilities_def)
