/* collectors/culture/sources/manga_net_cafes.c — port of
 * server/src/collectors/mangaNetCafes.js (fetchOverpass single area.jp).
 * SEED_NET_CAFES offline fallback intentionally not ported (rule 8). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  char cb[48];
  snprintf(cb, sizeof cb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "cafe_id", cb);

  const char *en = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  if (en) {
    cJSON_AddStringToObject(p, "name", en);
  } else if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Net Cafe %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());

  const char *brand = ov_tag(el, "brand");
  cJSON_AddItemToObject(p, "brand",
                        brand ? cJSON_CreateString(brand)
                              : cJSON_CreateNull());

  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());

  const char *oh = ov_tag(el, "opening_hours");
  cJSON_AddStringToObject(p, "opening_hours", oh ? oh : "24/7");

  const char *sh = ov_tag(el, "shower");
  cJSON_AddBoolToObject(p, "shower", sh && strcmp(sh, "yes") == 0);

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"shop\"=\"internet_cafe\"](area.jp);"
    "way[\"shop\"=\"internet_cafe\"](area.jp);"
    "node[\"amenity\"=\"internet_cafe\"](area.jp);"
    "way[\"amenity\"=\"internet_cafe\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def manga_net_cafes_def = {
  .id = "manga-net-cafes", .collector = "culture",
  .name = "Manga / Net Cafes", .name_ja = "漫画喫茶・ネットカフェ",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(manga_net_cafes_def)
