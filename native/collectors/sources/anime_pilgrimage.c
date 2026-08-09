/* collectors/culture/sources/anime_pilgrimage.c — port of
 * server/src/collectors/animePilgrimage.js (fetchOverpass single area.jp).
 * SEED_PILGRIMAGE offline fallback intentionally not ported (rule 8). */
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
  char sb[48];
  snprintf(sb, sizeof sb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "site_id", sb);

  const char *en = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  if (en) {
    cJSON_AddStringToObject(p, "name", en);
  } else if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Pilgrimage %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());

  const char *subj = ov_tag(el, "subject");
  if (!subj) subj = ov_tag(el, "subject:wikidata");
  cJSON_AddItemToObject(p, "subject",
                        subj ? cJSON_CreateString(subj) : cJSON_CreateNull());

  const char *wd = ov_tag(el, "wikidata");
  cJSON_AddItemToObject(p, "wikidata",
                        wd ? cJSON_CreateString(wd) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"subject:wikidata\"](area.jp);"
    "node[\"tourism\"=\"attraction\"][\"subject\"~\"anime|manga\"](area.jp);",
    180, 200000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def anime_pilgrimage_def = {
  .id = "anime-pilgrimage", .collector = "culture",
  .name = "Anime Pilgrimage Sites", .name_ja = "アニメ聖地巡礼",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(anime_pilgrimage_def)
