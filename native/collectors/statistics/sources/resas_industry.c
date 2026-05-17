/* collectors/statistics/sources/resas_industry.c — port of
 * server/src/collectors/resasIndustry.js. The RESAS API path requires a key;
 * without it JS falls back to tryOSMIndustrial() (a single area.jp Overpass
 * query), the faithful live path ported here. SEED_INDUSTRY curated list
 * intentionally not ported (rule 7). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
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
  char cid[64];
  snprintf(cid, sizeof cid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "city_id", cid);
  const char *nen = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  if (nen) {
    cJSON_AddStringToObject(p, "name", nen);
  } else if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char dn[48];
    snprintf(dn, sizeof dn, "Industrial zone %d", i + 1);
    cJSON_AddStringToObject(p, "name", dn);
  }
  if (nm) cJSON_AddStringToObject(p, "name_ja", nm);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "primary_sector", "manufacturing");
  const char *ind = ov_tag(el, "industrial");
  cJSON_AddStringToObject(p, "sub_sector", ind ? ind : "mixed");
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"landuse\"=\"industrial\"][\"name\"](area.jp);"
    "node[\"industrial\"][\"name\"](area.jp);"
    "way[\"industrial\"][\"name\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def resas_industry_def = {
  .id = "resas-industry", .collector = "statistics",
  .name = "RESAS Industry", .name_ja = "RESAS 産業構造",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(resas_industry_def)
