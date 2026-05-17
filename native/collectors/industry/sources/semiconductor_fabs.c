/* collectors/industry/sources/semiconductor_fabs.c — port of
 * server/src/collectors/semiconductorFabs.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_FABS offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
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
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "FAB_LIVE_%05d", i + 1);
  (void)id;
  cJSON_AddStringToObject(p, "fab_id", fid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  char nbuf[64];
  if (!name) {
    cJSON *idn = cJSON_GetObjectItem(el, "id");
    snprintf(nbuf, sizeof nbuf, "Fab %lld",
             idn && cJSON_IsNumber(idn) ? (long long)idn->valuedouble : 0);
    name = nbuf;
  }
  cJSON_AddStringToObject(p, "name", name);
  const char *company = ov_tag(el, "operator");
  if (!company) company = ov_tag(el, "brand");
  cJSON_AddStringToObject(p, "company", company ? company : "unknown");
  const char *tech = ov_tag(el, "industrial");
  cJSON_AddStringToObject(p, "tech", tech ? tech : "semiconductor");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "semicon_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"industrial\"=\"semiconductor\"](area.jp);"
    "way[\"industrial\"=\"semiconductor\"](area.jp);"
    "node[\"industrial\"=\"electronics\"](area.jp);"
    "way[\"industrial\"=\"electronics\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def semiconductor_fabs_def = {
  .id = "semiconductor-fabs", .collector = "industry",
  .name = "Semiconductor Fabs", .name_ja = "半導体工場",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(semiconductor_fabs_def)
