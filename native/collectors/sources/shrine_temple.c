/* collectors/culture/sources/shrine_temple.c — port of
 * server/src/collectors/shrineTemple.js. fetchOverpass(body, mapFn, 180_000)
 * — single area.jp query, timeoutMs=180000, queryTimeout default 180.
 * SEED_SITES offline fallback intentionally not ported (JS uses it only when
 * upstream is empty; we emit live rows or 0 — correctness-neutral). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char pid[64];
  snprintf(pid, sizeof pid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "place_id", pid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Place %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *religion = ov_tag(el, "religion");
  cJSON_AddStringToObject(p, "religion", religion ? religion : "unknown");
  const char *denom = ov_tag(el, "denomination");
  if (denom) cJSON_AddStringToObject(p, "denomination", denom);
  else cJSON_AddItemToObject(p, "denomination", cJSON_CreateNull());
  const char *wikidata = ov_tag(el, "wikidata");
  if (wikidata) cJSON_AddStringToObject(p, "wikidata", wikidata);
  else cJSON_AddItemToObject(p, "wikidata", cJSON_CreateNull());
  const char *wikipedia = ov_tag(el, "wikipedia");
  if (wikipedia) cJSON_AddStringToObject(p, "wikipedia", wikipedia);
  else cJSON_AddItemToObject(p, "wikipedia", cJSON_CreateNull());
  const char *website = ov_tag(el, "website");
  if (website) cJSON_AddStringToObject(p, "website", website);
  else cJSON_AddItemToObject(p, "website", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"place_of_worship\"][\"religion\"~\"shinto|buddhist\"][\"name\"](area.jp);"
    "way[\"amenity\"=\"place_of_worship\"][\"religion\"~\"shinto|buddhist\"][\"name\"](area.jp);",
    180, 180000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def shrine_temple_def = {
  .id = "shrine-temple", .collector = "culture",
  .name = "Shrines & Temples", .name_ja = "神社・寺院",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(shrine_temple_def)
