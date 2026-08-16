/* collectors/tourism/sources/ski_resorts.c — port of
 * server/src/collectors/skiResorts.js. fetchOverpass (single area.jp query,
 * tryLive). SEED_RESORTS offline fallback intentionally not ported (JS does
 * `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char rid[64];
  snprintf(rid, sizeof rid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "resort_id", rid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Ski resort %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *website = ov_tag(el, "website");
  if (website) cJSON_AddStringToObject(p, "website", website);
  else cJSON_AddItemToObject(p, "website", cJSON_CreateNull());
  const char *wikidata = ov_tag(el, "wikidata");
  if (wikidata) cJSON_AddStringToObject(p, "wikidata", wikidata);
  else cJSON_AddItemToObject(p, "wikidata", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "way[\"landuse\"=\"winter_sports\"][\"name\"](area.jp);"
    "relation[\"landuse\"=\"winter_sports\"][\"name\"](area.jp);"
    "node[\"sport\"=\"skiing\"][\"name\"](area.jp);",
    /* see marine_traffic.c: a nationwide area.jp query needs ~100s on the one
     * reachable Overpass mirror, so a 60s per-endpoint budget always lost. */
    180, 150000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def ski_resorts_def = {
  .id = "ski-resorts", .collector = "tourism", .name = "Ski Resorts",
  .name_ja = "スキー場", 
  .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(ski_resorts_def)
