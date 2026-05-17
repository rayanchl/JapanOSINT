/* collectors/statistics/sources/resas_tourism.c — port of
 * server/src/collectors/resasTourism.js. The RESAS API path requires a key;
 * without it JS falls back to tryOSMTourism() (a single area.jp Overpass
 * query), the faithful live path ported here. SEED_TOURIST_SITES curated
 * list intentionally not ported (rule 7). Note: `wikidata` has no JS `||`
 * fallback (=== JS undefined when the tag is absent → key omitted from
 * JSON.stringify), so it is only emitted when present (featureUid parity). */
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
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "site_id", sid);
  const char *nen = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  if (nen) {
    cJSON_AddStringToObject(p, "name", nen);
  } else if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char dn[48];
    snprintf(dn, sizeof dn, "Site %d", i + 1);
    cJSON_AddStringToObject(p, "name", dn);
  }
  if (nm) cJSON_AddStringToObject(p, "name_ja", nm);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *tr = ov_tag(el, "tourism");
  if (!tr) tr = ov_tag(el, "historic");
  cJSON_AddStringToObject(p, "category", tr ? tr : "attraction");
  const char *wd = ov_tag(el, "wikidata");
  if (wd) cJSON_AddStringToObject(p, "wikidata", wd);
  const char *wp = ov_tag(el, "wikipedia");
  if (wp) cJSON_AddStringToObject(p, "wikipedia", wp);
  else cJSON_AddItemToObject(p, "wikipedia", cJSON_CreateNull());
  const char *ws = ov_tag(el, "website");
  if (ws) cJSON_AddStringToObject(p, "website", ws);
  else cJSON_AddItemToObject(p, "website", cJSON_CreateNull());
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
    "node[\"tourism\"~\"attraction|viewpoint|theme_park|museum|zoo|aquarium\"]"
    "[\"wikidata\"](area.jp);"
    "node[\"historic\"~\"castle|monument|ruins\"][\"wikidata\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def resas_tourism_def = {
  .id = "resas-tourism", .collector = "statistics",
  .name = "RESAS Tourism", .name_ja = "RESAS 観光統計",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(resas_tourism_def)
