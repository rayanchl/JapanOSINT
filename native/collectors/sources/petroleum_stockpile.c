/* collectors/industry/sources/petroleum_stockpile.c — port of
 * server/src/collectors/petroleumStockpile.js. Only the live upstream
 * tryOverpass() rows are emitted; the curated NATIONAL_BASES /
 * COMMERCIAL_DEPOTS seed arrays and the JOGMEC HTML scrape enrichment are
 * intentionally not ported (rule 7 — curated/seed data). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  char rid[32];
  snprintf(rid, sizeof rid, "STOCK_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "reserve_id", rid);
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char dn[48];
    snprintf(dn, sizeof dn, "Oil storage %lld", oid);
    cJSON_AddStringToObject(p, "name", dn);
  }
  const char *ind = ov_tag(el, "industrial");
  cJSON_AddStringToObject(p, "kind",
    (ind && strcmp(ind, "oil") == 0) ? "commercial" : "storage_tank");
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "petroleum_stockpile_osm");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"storage_tank\"][\"content\"=\"oil\"](area.jp);"
    "way[\"man_made\"=\"storage_tank\"][\"content\"=\"oil\"](area.jp);"
    "node[\"industrial\"=\"oil\"](area.jp);"
    "way[\"industrial\"=\"oil\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def petroleum_stockpile_def = {
  .id = "petroleum-stockpile", .collector = "industry",
  .name = "Petroleum Stockpile", .name_ja = "国家石油備蓄基地",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(petroleum_stockpile_def)
