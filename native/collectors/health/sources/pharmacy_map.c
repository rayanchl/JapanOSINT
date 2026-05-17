/* collectors/health/sources/pharmacy_map.c
 * Port of server/src/collectors/pharmacyMap.js (fetchOverpassTiled). Curated
 * SEED_PHARMACIES offline fallback intentionally NOT ported —
 * correctness-neutral (JS does `if (!live) features = []` anyway). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"amenity\"=\"pharmacy\"](%s);"
    "way[\"amenity\"=\"pharmacy\"](%s);"
    "node[\"shop\"=\"chemist\"](%s);",
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
  snprintf(fid, sizeof fid, "PHARM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "brand");
  cJSON_AddStringToObject(p, "name", name ? name : "Pharmacy");
  const char *brand = ov_tag(el, "brand");
  cJSON_AddItemToObject(p, "brand",
                        brand ? cJSON_CreateString(brand) : cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *oh = ov_tag(el, "opening_hours");
  cJSON_AddItemToObject(p, "opening_hours",
                        oh ? cJSON_CreateString(oh) : cJSON_CreateNull());
  const char *ph = ov_tag(el, "phone");
  cJSON_AddItemToObject(p, "phone",
                        ph ? cJSON_CreateString(ph) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def pharmacy_map_def = {
  .id = "pharmacy-map", .collector = "health", .name = "Pharmacy Map",
  .name_ja = "薬局マップ", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(pharmacy_map_def)
