/* collectors/statistics/sources/estat_population.c — port of
 * server/src/collectors/estatPopulation.js. The JS primary path is the e-Stat
 * getStatsData JSON API, gated on ESTAT_API_KEY; without a key it throws and
 * the collector falls through to an OSM Overpass admin-population proxy
 * (single area.jp query) which is the faithful keyless live path. The curated
 * CITY_GRIDS seed / _meta envelope is intentionally not ported (JS does
 * `features = []` when nothing live). Each feature is a 1km mesh Polygon. */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

#define STEP 0.009

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  const char *ps = ov_tag(el, "population");
  long pop = ps ? strtol(ps, NULL, 10) : 0;
  if (pop < 0) pop = 0;
  double h = STEP / 2.0;

  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Polygon");
  cJSON *ring = cJSON_CreateArray();
  double pts[5][2] = {
    { lon - h, lat - h }, { lon + h, lat - h }, { lon + h, lat + h },
    { lon - h, lat + h }, { lon - h, lat - h } };
  for (int k = 0; k < 5; k++) {
    cJSON *pr = cJSON_CreateArray();
    cJSON_AddItemToArray(pr, cJSON_CreateNumber(pts[k][0]));
    cJSON_AddItemToArray(pr, cJSON_CreateNumber(pts[k][1]));
    cJSON_AddItemToArray(ring, pr);
  }
  cJSON *coords = cJSON_CreateArray();
  cJSON_AddItemToArray(coords, ring);
  cJSON_AddItemToObject(g, "coordinates", coords);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char mid[64];
  snprintf(mid, sizeof mid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "mesh_id", mid);
  const char *ca = ov_tag(el, "name:en");
  if (!ca) ca = ov_tag(el, "name");
  char cbuf[64];
  if (!ca) { snprintf(cbuf, sizeof cbuf, "Place %d", i + 1); ca = cbuf; }
  cJSON_AddStringToObject(p, "city_area", ca);
  const char *place = ov_tag(el, "place");
  cJSON_AddStringToObject(p, "place", place ? place : "admin");
  cJSON_AddNumberToObject(p, "population", (double)pop);
  cJSON_AddNumberToObject(p, "density_per_sqkm", (double)pop);
  const char *yr = ov_tag(el, "population:date");
  if (yr) cJSON_AddStringToObject(p, "year", yr);
  else cJSON_AddItemToObject(p, "year", cJSON_CreateNull());
  const char *wd = ov_tag(el, "wikidata");
  if (wd) cJSON_AddStringToObject(p, "wikidata", wd);
  else cJSON_AddItemToObject(p, "wikidata", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"place\"~\"city|town\"][\"population\"](area.jp);"
    "relation[\"admin_level\"~\"7|8\"][\"population\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def estat_population_def = {
  .id = "estat-population", .collector = "statistics",
  .name = "e-Stat Population Mesh", .name_ja = "e-Stat 人口メッシュ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(estat_population_def)
