/* collectors/tourism/sources/castles.c — port of
 * server/src/collectors/castles.js (fetchOverpass single area.jp).
 * SEED_CASTLES offline fallback intentionally not ported (rule 8).
 * JS `.slice(0,200)` cap is count-only / correctness-neutral; omitted. */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
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
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  char cb[48];
  snprintf(cb, sizeof cb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "castle_id", cb);

  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Castle %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *era = ov_tag(el, "start_date");
  cJSON_AddStringToObject(p, "era", era ? era : "unknown");
  cJSON_AddStringToObject(p, "cls", "osm");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"historic\"=\"castle\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def castles_def = {
  .id = "castles", .collector = "tourism",
  .name = "Japanese Castles", .name_ja = "日本の城",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(castles_def)
