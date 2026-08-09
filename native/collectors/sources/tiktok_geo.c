/* collectors/social/sources/tiktok_geo.c
 * Port of server/src/collectors/tiktokGeo.js (tiktok-geo).
 * TikTok discover/place endpoint, keyed on TIKTOK_MS_TOKEN (msToken
 * querystring). No key-free path -> honest empty when absent.
 * Emits a Point Feature per place via geojson toolkit. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* p?.cardItem?.geo?.lon != null (string or number). */
static int num_of(const cJSON *v, double *out) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    *out = strtod(v->valuestring, NULL); return 1;
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *tok = getenv("TIKTOK_MS_TOKEN");
  if (!tok || !*tok) {
    fprintf(stderr, "[tiktok-geo] gated (no TIKTOK_MS_TOKEN)\n");
    return 0;
  }
  char url[512];
  snprintf(url, sizeof url,
    "https://www.tiktok.com/api/discover/place/?region=JP&msToken=%s", tok);

  cJSON *data = feed_get_json(ctx->http, url, 12000);
  cJSON *body = data ? cJSON_GetObjectItem(data, "body") : NULL;
  cJSON *b0 = (body && cJSON_IsArray(body)) ? cJSON_GetArrayItem(body, 0) : NULL;  /* exhaustive-ok: response envelope */
  cJSON *places = b0 ? cJSON_GetObjectItem(b0, "exploreList") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(places)) {
    int i = 0;
    cJSON *p;
    cJSON_ArrayForEach(p, places) {
      cJSON *card = cJSON_GetObjectItem(p, "cardItem");
      cJSON *geo = card ? cJSON_GetObjectItem(card, "geo") : NULL;
      double lon, lat;
      if (!geo ||
          !num_of(cJSON_GetObjectItem(geo, "lon"), &lon) ||
          !num_of(cJSON_GetObjectItem(geo, "lat"), &lat)) { i++; continue; }

      cJSON *f = gj_point_feature(lon, lat);

      cJSON *pr = cJSON_CreateObject();
      cJSON *cid = cJSON_GetObjectItem(card, "id");
      if (cid && cJSON_IsString(cid) && cid->valuestring[0])
        cJSON_AddStringToObject(pr, "post_id", cid->valuestring);
      else {
        char ib[32]; snprintf(ib, sizeof ib, "tiktok-%d", i);
        cJSON_AddStringToObject(pr, "post_id", ib);
      }
      cJSON *t = cJSON_GetObjectItem(card, "title");
      if (t && cJSON_IsString(t) && t->valuestring[0])
        cJSON_AddStringToObject(pr, "hashtag", t->valuestring);
      else cJSON_AddNullToObject(pr, "hashtag");
      cJSON *st = cJSON_GetObjectItem(card, "subTitle");
      if (st && cJSON_IsString(st) && st->valuestring[0])
        cJSON_AddStringToObject(pr, "city", st->valuestring);
      else cJSON_AddNullToObject(pr, "city");
      cJSON *ln = cJSON_GetObjectItem(card, "link");
      if (ln && cJSON_IsString(ln) && ln->valuestring[0])
        cJSON_AddStringToObject(pr, "url", ln->valuestring);
      else cJSON_AddNullToObject(pr, "url");
      cJSON_AddStringToObject(pr, "source", "tiktok_api");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[tiktok-geo] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def tiktok_geo_def = {
  .id = "tiktok-geo", .collector = "social",
  .name = "TikTok Geotagged", .name_ja = "TikTok ジオタグ",
  .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(tiktok_geo_def)
