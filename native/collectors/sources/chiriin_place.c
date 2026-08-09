/* collectors/geospatial/sources/chiriin_place.c
 * Port of server/src/collectors/chiriinPlace.js — GSI key-free
 * AddressSearch JSON over a bounded set of representative geographic
 * feature terms (mountains, capes, lakes, peninsulas, plateaus...).
 * Real GSI data only; dedupe by title|lon|lat (5dp). Honest empty when
 * nothing was fetched (rule 8 — no fabrication). Property key order
 * (title, dataType, source) mirrors the JS mapFn for featureUid parity. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE "https://msearch.gsi.go.jp/address-search/AddressSearch?q="

/* QUERIES = 岳 山 岬 湖 半島 高原 平野 盆地 峠 島 (encodeURIComponent). */
static const char *QUERIES[] = {
  "%E5%B2%B3", "%E5%B1%B1", "%E5%B2%AC", "%E6%B9%96",
  "%E5%8D%8A%E5%B3%B6", "%E9%AB%98%E5%8E%9F", "%E5%B9%B3%E9%87%8E",
  "%E7%9B%86%E5%9C%B0", "%E5%B3%A0", "%E5%B3%B6",
};
#define NQ ((int)(sizeof QUERIES / sizeof QUERIES[0]))

/* dedupe key store: "title|lon5|lat5" */
#define MAX_SEEN 8192
static int seen_has_add(char keys[][96], int *cnt, const char *k) {
  for (int i = 0; i < *cnt; i++) if (strcmp(keys[i], k) == 0) return 1;
  if (*cnt < MAX_SEEN) { snprintf(keys[*cnt], 96, "%s", k); (*cnt)++; }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  static char seen[MAX_SEEN][96];
  int seen_cnt = 0;
  int any_fetched = 0;

  for (int qi = 0; qi < NQ; qi++) {
    char url[256];
    snprintf(url, sizeof url, "%s%s", BASE, QUERIES[qi]);
    cJSON *data = feed_get_json(ctx->http, url, 8000);
    if (!data) continue;
    any_fetched = 1;
    if (!cJSON_IsArray(data)) { cJSON_Delete(data); continue; }

    cJSON *h;
    cJSON_ArrayForEach(h, data) {
      cJSON *geom = cJSON_GetObjectItem(h, "geometry");
      cJSON *co = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
      if (!cJSON_IsArray(co) || cJSON_GetArraySize(co) < 2) continue;
      cJSON *c0 = cJSON_GetArrayItem(co, 0);
      cJSON *c1 = cJSON_GetArrayItem(co, 1);
      if (!c0 || !cJSON_IsNumber(c0) || !c1 || !cJSON_IsNumber(c1)) continue;
      double lon = c0->valuedouble, lat = c1->valuedouble;

      cJSON *props = cJSON_GetObjectItem(h, "properties");
      cJSON *tv = props ? cJSON_GetObjectItem(props, "title") : NULL;
      cJSON *dv = props ? cJSON_GetObjectItem(props, "dataType") : NULL;
      const char *title = (tv && cJSON_IsString(tv)) ? tv->valuestring : NULL;

      char key[96];
      snprintf(key, sizeof key, "%s|%.5f|%.5f",
               title ? title : "null", lon, lat);
      if (seen_has_add(seen, &seen_cnt, key)) continue;

      cJSON *f = gj_point_feature(lon, lat);

      cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
      if (title) cJSON_AddStringToObject(p, "title", title);
      else       cJSON_AddNullToObject(p, "title");
      if (dv && cJSON_IsString(dv)) cJSON_AddStringToObject(p, "dataType", dv->valuestring);
      else                          cJSON_AddNullToObject(p, "dataType");
      cJSON_AddStringToObject(p, "source", "gsi_chiriin");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    cJSON_Delete(data);
  }

  if (!any_fetched) { cJSON_Delete(features); return -1; }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[chiriin-place] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def chiriin_place_def = {
  .id = "chiriin-place", .collector = "geospatial",
  .name = "GSI Place Names", .name_ja = "地理院 地名情報",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(chiriin_place_def)
