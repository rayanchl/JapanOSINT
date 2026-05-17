/* collectors/tourism/sources/unesco_heritage.c
 * Port of server/src/collectors/unescoHeritage.js (fetchText + regex XML).
 * REFERENCE source.c for the HTML_SCRAPE family (feed_get_text + htmlparse).
 * SEED_WHS offline fallback NOT ported (JS does `if(!live) features=[]`).
 * uid = whs_id hash: feed_hash_key(name, lat-str, lon-str) — deterministic
 * (raw XML coord text; not Node-String()-identical, fine post-parity). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/htmlparse.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, "https://whc.unesco.org/en/list/xml/", 15000);
  if (!xml || strlen(xml) < 100) { free(xml); return -1; }

  cJSON *features = cJSON_CreateArray();
  const char *cur = xml;
  const char *inner; int ilen;
  char field[2048];
  while ((cur = html_block(cur, "row", &inner, &ilen)) != NULL) {
    char *row = strndup(inner, (size_t)ilen);
    if (!row) continue;

    char iso[64] = {0};
    if (html_tag(row, "iso_code", field, sizeof field)) {
      for (size_t i = 0; field[i] && i < sizeof iso - 1; i++)
        iso[i] = (char)tolower((unsigned char)field[i]);
    }
    if (!strstr(iso, "jp")) { free(row); continue; }

    char name[1024] = {0};
    if (!html_tag(row, "site", name, sizeof name) || !name[0])
      html_tag(row, "name_en", name, sizeof name);

    char lons[64] = {0}, lats[64] = {0}, cat[128] = {0}, date[64] = {0};
    html_tag(row, "longitude", lons, sizeof lons);
    html_tag(row, "latitude", lats, sizeof lats);
    html_tag(row, "category", cat, sizeof cat);
    html_tag(row, "date_inscribed", date, sizeof date);
    double lon = atof(lons), lat = atof(lats);
    int bad = (!lons[0] || !lats[0] || !isfinite(lon) || !isfinite(lat));
    if (bad || !name[0]) { free(row); continue; }

    char hk[24];
    const char *parts[] = { name, lats, lons };   /* intelHashKey(name,lat,lon) */
    feed_hash_key(hk, parts, 3);
    char whs[64];
    snprintf(whs, sizeof whs, "WHS_LIVE_%s", hk);

    char kind[128];
    size_t k = 0;
    for (; cat[k] && k < sizeof kind - 1; k++) kind[k] = (char)tolower((unsigned char)cat[k]);
    kind[k] = 0;
    if (!kind[0]) snprintf(kind, sizeof kind, "cultural");
    long yr = date[0] ? strtol(date, NULL, 10) : 0;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);
    cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
    cJSON_AddStringToObject(p, "whs_id", whs);
    cJSON_AddStringToObject(p, "name", name);
    cJSON_AddStringToObject(p, "name_en", name);
    cJSON_AddStringToObject(p, "kind", kind);
    cJSON_AddItemToObject(p, "year",
      yr > 0 ? cJSON_CreateNumber((double)yr) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "unesco_whs_live");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    free(row);
  }
  free(xml);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[unesco-heritage] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def unesco_heritage_def = {
  .id = "unesco-heritage", .collector = "tourism",
  .name = "UNESCO World Heritage", .name_ja = "ユネスコ世界遺産",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(unesco_heritage_def)
