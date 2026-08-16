/* collectors/government/sources/courts_prisons.c — port of
 * server/src/collectors/courtsPrisons.js (fetchOverpass single area.jp).
 * SEED_FACILITIES offline fallback intentionally not ported (rule 8). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fb[48];
  snprintf(fb, sizeof fb, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fb);

  const char *en = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  const char *name = en ? en : nm;
  cJSON_AddStringToObject(p, "name", name ? name : "Court / Prison");
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());

  const char *am = ov_tag(el, "amenity");
  const char *bld = ov_tag(el, "building");
  int is_prison = (am && strcmp(am, "prison") == 0) ||
                  (bld && strcmp(bld, "prison") == 0);
  cJSON_AddStringToObject(p, "type", is_prison ? "prison" : "courthouse");

  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"courthouse\"](area.jp);"
    "way[\"amenity\"=\"courthouse\"](area.jp);"
    "node[\"amenity\"=\"prison\"](area.jp);"
    "way[\"amenity\"=\"prison\"](area.jp);"
    "node[\"building\"=\"prison\"](area.jp);"
    "way[\"building\"=\"prison\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def courts_prisons_def = {
  .id = "courts-prisons", .collector = "government",
  .name = "Courts & Prisons", .name_ja = "裁判所・刑務所",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(courts_prisons_def)
