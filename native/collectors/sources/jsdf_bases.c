/* collectors/defense/sources/jsdf_bases.c — port of
 * server/src/collectors/jsdfBases.js (fetchOverpass single area.jp).
 * SEED_JSDF offline fallback intentionally not ported (rule 8).
 * mapFn skips elements whose operator is set but matches none of
 * /自衛|jsdf|jasdf|jgsdf|jmsdf/ (case-insensitive, plain substrings). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int has_ci(const char *hay, const char *needle) {
  size_t nl = strlen(needle);
  for (const char *h = hay; *h; h++) {
    size_t k = 0;
    while (k < nl && h[k] &&
           tolower((unsigned char)h[k]) == tolower((unsigned char)needle[k]))
      k++;
    if (k == nl) return 1;
  }
  return 0;
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  const char *op = ov_tag(el, "operator"); /* '' coerced -> NULL by ov_tag */
  if (op) {
    /* JS: if (op && !/自衛|jsdf|jasdf|jgsdf|jmsdf/.test(op)) return null; */
    if (!(strstr(op, "自衛") || has_ci(op, "jsdf") || has_ci(op, "jasdf") ||
          has_ci(op, "jgsdf") || has_ci(op, "jmsdf")))
      return NULL;
  }

  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char bb[48];
  snprintf(bb, sizeof bb, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "base_id", bb);

  const char *nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", nm ? nm : "JSDF Base");
  cJSON_AddStringToObject(p, "branch", "JSDF");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"landuse\"=\"military\"](area.jp);"
    "relation[\"landuse\"=\"military\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def jsdf_bases_def = {
  .id = "jsdf-bases", .collector = "defense",
  .name = "JSDF Bases", .name_ja = "自衛隊基地",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(jsdf_bases_def)
