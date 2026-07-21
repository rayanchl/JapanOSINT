/* collectors/health/sources/aed_map.c
 * Port of server/src/collectors/aedMap.js (fetchOverpassTiled). The curated
 * SEED_AEDS offline fallback (synthetic source:'aed_seed') is intentionally
 * NOT ported — correctness-neutral: real OSM rows when Overpass is up, 0 when
 * down (same precedent as hospital-map). */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"emergency\"=\"defibrillator\"](%s);"
    "node[\"emergency\"=\"aed\"](%s);",
    bbox, bbox);
}

/* mirrors the JS mapFn — properties built in EXACT JS key order. */
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
  snprintf(fid, sizeof fid, "AED_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "AED");
  const char *indoor = ov_tag(el, "indoor");
  cJSON_AddItemToObject(p, "indoor",
                        indoor ? cJSON_CreateString(indoor) : cJSON_CreateNull());
  const char *access = ov_tag(el, "access");
  cJSON_AddStringToObject(p, "access", access ? access : "public");
  const char *oh = ov_tag(el, "opening_hours");
  cJSON_AddItemToObject(p, "opening_hours",
                        oh ? cJSON_CreateString(oh) : cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def aed_map_def = {
  .id = "aed-map", .collector = "health", .name = "AED Map",
  .name_ja = "AEDマップ", 
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(aed_map_def)
