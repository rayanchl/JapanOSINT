/* collectors/infrastructure/sources/mlit_dam.c
 * Port of server/src/collectors/mlitDam.js (fetchOverpass — single area.jp
 * query). MLIT CGI has no public JSON feed; OSM Overpass waterway=dam is
 * the real, policy-clean source. Honest empty on failure (RULE 8).
 * REFERENCE embassies.c (lib/overpass.h). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

/* JS: el.tags?.name || el.tags?.['name:ja'] || el.tags?.['name:en'] || null */
static void add_str_or_null(cJSON *p, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(p, k, v);
  else   cJSON_AddItemToObject(p, k, cJSON_CreateNull());
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *type = cJSON_GetObjectItem(el, "type");
  cJSON *id   = cJSON_GetObjectItem(el, "id");
  char osm[64];
  snprintf(osm, sizeof osm, "%s/%lld",
           (type && cJSON_IsString(type)) ? type->valuestring : "",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "osm_id", osm);

  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:ja");
  if (!name) name = ov_tag(el, "name:en");
  add_str_or_null(p, "name", name);
  add_str_or_null(p, "name_en", ov_tag(el, "name:en"));
  add_str_or_null(p, "operator", ov_tag(el, "operator"));

  const char *h = ov_tag(el, "height");
  if (h) {
    char *end; double hv = strtod(h, &end);
    if (end != h) cJSON_AddNumberToObject(p, "height_m", hv);
    else cJSON_AddItemToObject(p, "height_m", cJSON_CreateNull());
  } else {
    cJSON_AddItemToObject(p, "height_m", cJSON_CreateNull());
  }
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass_dam");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"waterway\"=\"dam\"](area.jp);"
    "relation[\"waterway\"=\"dam\"](area.jp);"
    "node[\"waterway\"=\"dam\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_dam_def = {
  .id = "mlit-dam", .collector = "infrastructure",
  .name = "MLIT Dam Data", .name_ja = "国交省 ダム情報",
  .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(mlit_dam_def)
