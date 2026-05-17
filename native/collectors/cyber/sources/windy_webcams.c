/* collectors/cyber/sources/windy_webcams.c
 * Port of server/src/collectors/windyWebcams.js.
 * Windy Webcams API v3 filtered to JP bbox → FeatureCollection.
 * Gated on WINDY_API_KEY (header x-windy-api-key). No seed. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL \
  "https://api.windy.com/webcams/api/v3/webcams" \
  "?limit=250&include=location,urls,images&lang=en"

static int num_of(cJSON *v, double *out) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

static cJSON *sv_or_null(cJSON *o, const char *k) {
  if (!o) return cJSON_CreateNull();
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    return cJSON_CreateString(v->valuestring);
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *k = getenv("WINDY_API_KEY");
  if (!k || !*k) {
    fprintf(stderr, "[windy-webcams] gated (no WINDY_API_KEY)\n");
    return 0;
  }
  char hdr[256];
  snprintf(hdr, sizeof hdr, "x-windy-api-key: %s", k);
  const char *hdrs[] = { hdr, NULL };
  cJSON *data = feed_get_json_h(ctx->http, API_URL, hdrs, 15000);
  cJSON *cams = data ? cJSON_GetObjectItem(data, "webcams") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(cams)) {
    cJSON *w;
    cJSON_ArrayForEach(w, cams) {
      cJSON *loc = cJSON_GetObjectItem(w, "location");
      double lon, lat;
      if (!(loc &&
            num_of(cJSON_GetObjectItem(loc, "longitude"), &lon) &&
            num_of(cJSON_GetObjectItem(loc, "latitude"), &lat)))
        continue;
      if (!(lat >= 24 && lat <= 46 && lon >= 122 && lon <= 146)) continue;

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
      /* camera_uid: `windy-${w.webcamId || w.id}` */
      cJSON *wid = cJSON_GetObjectItem(w, "webcamId");
      if (!(wid && (cJSON_IsString(wid) || cJSON_IsNumber(wid))))
        wid = cJSON_GetObjectItem(w, "id");
      char cu[96];
      if (wid && cJSON_IsNumber(wid))
        snprintf(cu, sizeof cu, "windy-%g", wid->valuedouble);
      else if (wid && cJSON_IsString(wid))
        snprintf(cu, sizeof cu, "windy-%s", wid->valuestring);
      else
        snprintf(cu, sizeof cu, "windy-undefined");
      cJSON_AddStringToObject(p, "camera_uid", cu);
      cJSON_AddItemToObject(p, "title", sv_or_null(w, "title"));
      cJSON_AddItemToObject(p, "city", sv_or_null(loc, "city"));
      /* embed_url: w.urls.detail || w.urls.edit || null */
      cJSON *urls = cJSON_GetObjectItem(w, "urls");
      cJSON *eu = NULL;
      if (urls) {
        cJSON *d = cJSON_GetObjectItem(urls, "detail");
        if (d && cJSON_IsString(d) && d->valuestring[0]) eu = d;
        else {
          cJSON *e = cJSON_GetObjectItem(urls, "edit");
          if (e && cJSON_IsString(e) && e->valuestring[0]) eu = e;
        }
      }
      cJSON_AddItemToObject(p, "embed_url",
        eu ? cJSON_CreateString(eu->valuestring) : cJSON_CreateNull());
      /* image: w.images.current.preview || null */
      cJSON *imgs = cJSON_GetObjectItem(w, "images");
      cJSON *cur = imgs ? cJSON_GetObjectItem(imgs, "current") : NULL;
      cJSON_AddItemToObject(p, "image", sv_or_null(cur, "preview"));
      cJSON_AddItemToObject(p, "status", sv_or_null(w, "status"));
      cJSON_AddStringToObject(p, "source", "windy_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[windy-webcams] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def windy_webcams_def = {
  .id = "windy-webcams", .collector = "cyber",
  .name = "Windy Webcams API (JP)",
  .name_ja = "Windy \xE3\x82\xA6\xE3\x82\xA7\xE3\x83\x96\xE3\x82\xAB\xE3\x83\xA1\xE3\x83\xA9 API",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(windy_webcams_def)
