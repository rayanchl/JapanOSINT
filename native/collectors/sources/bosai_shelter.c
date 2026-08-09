/* collectors/safety/sources/bosai_shelter.c
 * Port of server/src/collectors/bosaiShelter.js (fetchOverpassTiled). The
 * curated SEED_SHELTERS offline fallback is intentionally NOT ported —
 * correctness-neutral (JS does `if (!live) features = []` anyway). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"emergency\"=\"assembly_point\"](%s);"
    "node[\"amenity\"=\"shelter\"](%s);"
    "way[\"amenity\"=\"shelter\"](%s);"
    "node[\"emergency\"=\"evacuation_center\"](%s);"
    "way[\"emergency\"=\"evacuation_center\"](%s);",
    bbox, bbox, bbox, bbox, bbox);
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "SHELTER_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  cJSON_AddStringToObject(p, "name", name ? name : "Evacuation Point");
  const char *st = ov_tag(el, "shelter_type");
  if (!st) st = ov_tag(el, "emergency");
  cJSON_AddStringToObject(p, "shelter_type", st ? st : "assembly_point");
  const char *cap = ov_tag(el, "capacity");           /* parseInt||null */
  long cn = cap ? strtol(cap, NULL, 10) : 0;
  cJSON_AddItemToObject(p, "capacity",
                        cn > 0 ? cJSON_CreateNumber((double)cn)
                               : cJSON_CreateNull());
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

static const source_def bosai_shelter_def = {
  .id = "bosai-shelter", .collector = "safety", .name = "Disaster Shelters",
  .name_ja = "防災避難所", 
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(bosai_shelter_def)
