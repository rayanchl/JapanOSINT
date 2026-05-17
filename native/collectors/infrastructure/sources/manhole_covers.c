/* collectors/infrastructure/sources/manhole_covers.c
 * Port of server/src/collectors/manholeCovers.js (fetchOverpassTiled).
 * GKP manhole-card program has no open bulk API; OSM man_made=manhole is
 * the real queryable source, fetched via the 12-tile nationwide fanout.
 * Honest empty on failure (RULE 8). REFERENCE embassies.c +
 * lib/overpass.h (overpass_tiled_collect). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

/* JS: (bbox) => `node["man_made"="manhole"](${bbox});` */
static void bodyfn(const char *bbox, char *out, size_t n, void *ud) {
  (void)ud;
  snprintf(out, n, "node[\"man_made\"=\"manhole\"](%s);", bbox);
}

/* JS: el.tags?.x || null */
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
  add_str_or_null(p, "name", ov_tag(el, "name"));
  add_str_or_null(p, "manhole_type", ov_tag(el, "manhole"));
  add_str_or_null(p, "operator", ov_tag(el, "operator"));
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass_manhole");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, bodyfn, 180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def manhole_covers_def = {
  .id = "manhole-covers", .collector = "infrastructure",
  .name = "Manhole Cards", .name_ja = "マンホールカード",
  .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(manhole_covers_def)
