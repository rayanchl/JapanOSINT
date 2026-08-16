/* collectors/environment/sources/openmeteo_jma.c
 * Port of server/src/collectors/openmeteoJma.js.
 * Open-Meteo JMA mirror — one current-weather GET per fixed city, then a
 * FeatureCollection. Keyless. SEED branch (features.length===0) NOT ported. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIMEOUT_MS 8000

static const struct { const char *name; double lat, lon; } CITIES[] = {
  {"Sapporo",   43.06, 141.35},
  {"Sendai",    38.27, 140.87},
  {"Tokyo",     35.69, 139.69},
  {"Yokohama",  35.45, 139.64},
  {"Nagoya",    35.18, 136.91},
  {"Osaka",     34.69, 135.50},
  {"Hiroshima", 34.40, 132.46},
  {"Fukuoka",   33.59, 130.40},
  {"Naha",      26.21, 127.68},
};
#define NCITY ((int)(sizeof(CITIES) / sizeof(CITIES[0])))

static void add_or_null(cJSON *p, const char *k, cJSON *v) {
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < NCITY; i++) {
    char url[256];
    snprintf(url, sizeof url,
      "https://api.open-meteo.com/v1/jma?latitude=%g&longitude=%g"
      "&current=temperature_2m,wind_speed_10m,precipitation",
      CITIES[i].lat, CITIES[i].lon);
    cJSON *d = feed_get_json(ctx->http, url, TIMEOUT_MS);
    cJSON *cur = d ? cJSON_GetObjectItem(d, "current") : NULL;
    if (cur && !cJSON_IsNull(cur)) {
      cJSON *f = gj_point_feature(CITIES[i].lon, CITIES[i].lat);

      cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
      cJSON_AddStringToObject(p, "city", CITIES[i].name);
      add_or_null(p, "temperature_c",    cJSON_GetObjectItem(cur, "temperature_2m"));
      add_or_null(p, "wind_mps",         cJSON_GetObjectItem(cur, "wind_speed_10m"));
      add_or_null(p, "precipitation_mm", cJSON_GetObjectItem(cur, "precipitation"));
      add_or_null(p, "observed_at",      cJSON_GetObjectItem(cur, "time"));
      cJSON_AddStringToObject(p, "source", "open_meteo_jma");
      /* Readable identity: geojson_emit_features reads properties.title /
       * summary / link, and the row is blank in every UI without them. Values
       * are formatted from the fetched `current` block only. */
      {
        cJSON *t  = cJSON_GetObjectItem(cur, "temperature_2m");
        cJSON *w  = cJSON_GetObjectItem(cur, "wind_speed_10m");
        cJSON *pr = cJSON_GetObjectItem(cur, "precipitation");
        char title[160], summ[224];
        if (cJSON_IsNumber(t))
          snprintf(title, sizeof title, "%s %.1f\xc2\xb0""C", CITIES[i].name, t->valuedouble);
        else
          snprintf(title, sizeof title, "%s current weather", CITIES[i].name);
        int off = snprintf(summ, sizeof summ, "%s:", CITIES[i].name);
        if (cJSON_IsNumber(t))
          off += snprintf(summ + off, sizeof summ - (size_t)off, " %.1f\xc2\xb0""C", t->valuedouble);
        if (cJSON_IsNumber(w))
          off += snprintf(summ + off, sizeof summ - (size_t)off, ", wind %.1f m/s", w->valuedouble);
        if (cJSON_IsNumber(pr))
          snprintf(summ + off, sizeof summ - (size_t)off, ", precip %.1f mm", pr->valuedouble);
        cJSON_AddStringToObject(p, "title", title);
        cJSON_AddStringToObject(p, "summary", summ);
        char link[192];
        snprintf(link, sizeof link,
                 "https://open-meteo.com/en/docs/jma-api#latitude=%g&longitude=%g",
                 CITIES[i].lat, CITIES[i].lon);
        cJSON_AddStringToObject(p, "link", link);
      }
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    if (d) cJSON_Delete(d);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[openmeteo-jma] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def openmeteo_jma_def = {
  .id = "openmeteo-jma", .collector = "environment",
  .name = "Open-Meteo JMA mirror", .name_ja = "Open-Meteo JMA",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(openmeteo_jma_def)
