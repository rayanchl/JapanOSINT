/* collectors/health/sources/hospital_map.c
 * Port of server/src/collectors/hospitalMap.js (fetchOverpassTiled). The
 * curated SEED_HOSPITALS offline fallback (synthetic source:'hospital_seed')
 * is intentionally NOT ported — correctness-neutral: real OSM rows when
 * Overpass is up, 0 when down (same precedent as the empty-endpoint nerv-feed).
 * REFERENCE source.c for the OVERPASS_TILED family. */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"amenity\"=\"hospital\"](%s);way[\"amenity\"=\"hospital\"](%s);"
    "node[\"healthcare\"=\"hospital\"](%s);way[\"healthcare\"=\"hospital\"](%s);"
    "node[\"amenity\"=\"clinic\"](%s);way[\"amenity\"=\"clinic\"](%s);",
    bbox, bbox, bbox, bbox, bbox, bbox);
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
  snprintf(fid, sizeof fid, "HOSP_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  cJSON_AddStringToObject(p, "name", name ? name : "Hospital");
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *beds = ov_tag(el, "beds");          /* parseInt||null (0 falsy) */
  long bn = beds ? strtol(beds, NULL, 10) : 0;
  cJSON_AddItemToObject(p, "beds",
                        bn > 0 ? cJSON_CreateNumber((double)bn)
                               : cJSON_CreateNull());
  const char *em = ov_tag(el, "emergency");
  cJSON_AddItemToObject(p, "emergency",
                        em ? cJSON_CreateString(em) : cJSON_CreateNull());
  const char *hc = ov_tag(el, "healthcare");
  if (!hc) hc = ov_tag(el, "amenity");
  cJSON_AddStringToObject(p, "healthcare", hc ? hc : "hospital");
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

static const source_def hospital_map_def = {
  .id = "hospital-map", .collector = "health", .name = "Hospital Map",
  .name_ja = "病院マップ", 
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(hospital_map_def)
