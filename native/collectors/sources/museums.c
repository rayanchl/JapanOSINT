/* collectors/tourism/sources/museums.c — port of
 * server/src/collectors/museums.js (fetchOverpass single area.jp).
 * SEED_MUSEUMS offline fallback intentionally not ported (rule 8).
 * JS `.slice(0,300)` cap is count-only / correctness-neutral; omitted. */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  char mb[48];
  snprintf(mb, sizeof mb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "museum_id", mb);

  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Museum %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *mu = ov_tag(el, "museum");
  cJSON_AddStringToObject(p, "kind", mu ? mu : "general");

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"tourism\"=\"museum\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def museums_def = {
  .id = "museums", .collector = "tourism",
  .name = "Museums", .name_ja = "博物館・美術館",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(museums_def)
