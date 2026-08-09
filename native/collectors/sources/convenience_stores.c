/* collectors/marketplace/sources/convenience_stores.c
 * Port of server/src/collectors/convenienceStores.js (fetchOverpassTiled).
 * Curated SEED_KONBINI offline fallback intentionally NOT ported —
 * correctness-neutral (JS does `if (!live) features = []` anyway). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"shop\"=\"convenience\"](%s);"
    "way[\"shop\"=\"convenience\"](%s);",
    bbox, bbox);
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "KONBINI_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "brand");
  cJSON_AddStringToObject(p, "name", name ? name : "Konbini");
  const char *brand = ov_tag(el, "brand");
  cJSON_AddItemToObject(p, "brand",
                        brand ? cJSON_CreateString(brand) : cJSON_CreateNull());
  /* AUDIT NOTE (slice a3): an untagged store used to be published as
   * opening_hours "24/7". OSM not carrying the tag is not evidence that the
   * shop never closes — that was an invented fact. Absent tag → null. */
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
  int n = overpass_tiled_collect(ctx, sink, body, 180, 200000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def convenience_stores_def = {
  .id = "convenience-stores", .collector = "marketplace",
  .name = "Convenience Stores", .name_ja = "コンビニ店舗",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(convenience_stores_def)
