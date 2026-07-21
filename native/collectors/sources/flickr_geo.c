/* collectors/social/sources/flickr_geo.c
 * Port of server/src/collectors/flickrGeo.js (flickr-geo).
 * Flickr REST flickr.photos.search, JP bbox, has_geo=1. Keyed on
 * FLICKR_API_KEY (no key-free fallback) -> honest empty when absent.
 * Emits a Point Feature per geotagged photo via geojson toolkit. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JP_BBOX "122.0,24.0,146.0,46.0"

/* p.longitude != null && +p.longitude !== 0 (string or number accepted). */
static int num_nz(const cJSON *v, double *out) {
  if (!v) return 0;
  double d;
  if (cJSON_IsNumber(v)) d = v->valuedouble;
  else if (cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    d = strtod(v->valuestring, NULL);
  else return 0;
  if (d == 0) return 0;
  *out = d;
  return 1;
}

static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *k = getenv("FLICKR_API_KEY");
  if (!k || !*k) {
    fprintf(stderr, "[flickr-geo] gated (no FLICKR_API_KEY)\n");
    return 0;
  }
  char url[700];
  snprintf(url, sizeof url,
    "https://api.flickr.com/services/rest/"
    "?method=flickr.photos.search&format=json&nojsoncallback=1"
    "&api_key=%s&bbox=%s&has_geo=1&content_type=1"
    "&extras=geo,url_s,owner_name,date_taken&per_page=250"
    "&sort=date-posted-desc",
    k, JP_BBOX);

  cJSON *data = feed_get_json(ctx->http, url, 15000);
  cJSON *photos = data ? cJSON_GetObjectItem(data, "photos") : NULL;
  cJSON *arr = photos ? cJSON_GetObjectItem(photos, "photo") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(arr)) {
    cJSON *p;
    cJSON_ArrayForEach(p, arr) {
      double lon, lat;
      if (!num_nz(cJSON_GetObjectItem(p, "longitude"), &lon)) continue;
      if (!num_nz(cJSON_GetObjectItem(p, "latitude"), &lat)) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();
      const char *id = sv(p, "id");
      const char *owner = sv(p, "owner");
      cJSON_AddStringToObject(pr, "photo_id", id ? id : "");
      const char *title = sv(p, "title");
      if (title) cJSON_AddStringToObject(pr, "title", title);
      else cJSON_AddNullToObject(pr, "title");
      const char *own = sv(p, "ownername");
      if (!own) own = owner;
      if (own) cJSON_AddStringToObject(pr, "owner", own);
      else cJSON_AddNullToObject(pr, "owner");
      const char *us = sv(p, "url_s");
      if (us) cJSON_AddStringToObject(pr, "url", us);
      else {
        char ub[256];
        snprintf(ub, sizeof ub, "https://www.flickr.com/photos/%s/%s",
                 owner ? owner : "", id ? id : "");
        cJSON_AddStringToObject(pr, "url", ub);
      }
      const char *dt = sv(p, "datetaken");
      if (dt) cJSON_AddStringToObject(pr, "taken", dt);
      else cJSON_AddNullToObject(pr, "taken");
      cJSON_AddStringToObject(pr, "source", "flickr_api");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[flickr-geo] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def flickr_geo_def = {
  .id = "flickr-geo", .collector = "social",
  .name = "Flickr Geotagged Photos", .name_ja = "Flickr ジオタグ写真",
  .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(flickr_geo_def)
