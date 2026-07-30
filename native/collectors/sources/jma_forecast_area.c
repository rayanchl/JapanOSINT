/* collectors/environment/sources/jma_forecast_area.c
 * Port of server/src/collectors/jmaForecastArea.js.
 * Per-area JMA regional forecast JSON; one Point Feature per area that has a
 * weather string. The seed fallback (rule 7) is dropped — live rows only. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

struct area { const char *code, *name; double lat, lon; };
static const struct area AREAS[] = {
  { "016000", "\xE5\x8C\x97\xE6\xB5\xB7\xE9\x81\x93\xE3\x83\xBB\xE7\x9F\xB3\xE7\x8B\xA9", 43.06, 141.35 },
  { "040000", "\xE5\xAE\xAE\xE5\x9F\x8E\xE7\x9C\x8C", 38.27, 140.87 },
  { "130000", "\xE6\x9D\xB1\xE4\xBA\xAC\xE9\x83\xBD", 35.69, 139.69 },
  { "140000", "\xE7\xA5\x9E\xE5\xA5\x88\xE5\xB7\x9D\xE7\x9C\x8C", 35.45, 139.64 },
  { "230000", "\xE6\x84\x9B\xE7\x9F\xA5\xE7\x9C\x8C", 35.18, 136.91 },
  { "270000", "\xE5\xA4\xA7\xE9\x98\xAA\xE5\xBA\x9C", 34.69, 135.50 },
  { "280000", "\xE5\x85\xB5\xE5\xBA\xAB\xE7\x9C\x8C", 34.69, 135.18 },
  { "340000", "\xE5\xBA\x83\xE5\xB3\xB6\xE7\x9C\x8C", 34.40, 132.46 },
  { "400000", "\xE7\xA6\x8F\xE5\xB2\xA1\xE7\x9C\x8C", 33.59, 130.40 },
  { "471000", "\xE6\xB2\x96\xE7\xB8\x84\xE3\x83\xBB\xE6\x9C\xAC\xE5\xB3\xB6", 26.21, 127.68 },
};

/* arr[0].timeSeries[0].areas[0].weathers[0] / winds[0] / reportDatetime */
static const char *first_str(cJSON *first, const char *seriesKey) {
  if (!first) return NULL;
  cJSON *ts = cJSON_GetObjectItem(first, "timeSeries");
  cJSON *ts0 = (ts && cJSON_IsArray(ts)) ? cJSON_GetArrayItem(ts, 0) : NULL;  /* exhaustive-ok: current-period series */
  cJSON *as = ts0 ? cJSON_GetObjectItem(ts0, "areas") : NULL;
  cJSON *a0 = (as && cJSON_IsArray(as)) ? cJSON_GetArrayItem(as, 0) : NULL;  /* exhaustive-ok: this collector emits one row per area code */
  cJSON *arr = a0 ? cJSON_GetObjectItem(a0, seriesKey) : NULL;
  cJSON *v = (arr && cJSON_IsArray(arr)) ? cJSON_GetArrayItem(arr, 0) : NULL;  /* exhaustive-ok: current-period value */
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  for (size_t i = 0; i < sizeof(AREAS) / sizeof(AREAS[0]); i++) {
    const struct area *a = &AREAS[i];
    char url[128];
    snprintf(url, sizeof url,
      "https://www.jma.go.jp/bosai/forecast/data/forecast/%s.json", a->code);
    cJSON *arr = feed_get_json(ctx->http, url, 8000);
    if (!arr) continue;                             /* !res.ok → null */
    cJSON *first = (cJSON_IsArray(arr)) ? cJSON_GetArrayItem(arr, 0) : NULL;  /* exhaustive-ok: forecast office block */
    const char *weather = first_str(first, "weathers");
    if (weather) {                                  /* if (r?.weather) */
      const char *wind = first_str(first, "winds");
      cJSON *rd = first ? cJSON_GetObjectItem(first, "reportDatetime") : NULL;
      const char *report_at =
        (rd && cJSON_IsString(rd) && rd->valuestring && rd->valuestring[0])
          ? rd->valuestring : NULL;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(a->lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(a->lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
      cJSON_AddStringToObject(p, "area_code", a->code);
      cJSON_AddStringToObject(p, "area_name", a->name);
      cJSON_AddStringToObject(p, "weather", weather);
      cJSON_AddItemToObject(p, "wind",
        wind ? cJSON_CreateString(wind) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "report_at",
        report_at ? cJSON_CreateString(report_at) : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "source", "jma_forecast");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    cJSON_Delete(arr);
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-forecast-area] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jma_forecast_area_def = {
  .id = "jma-forecast-area", .collector = "environment",
  .name = "JMA Regional Forecast JSON",
  .name_ja = "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81 \xE5\x9C\xB0\xE5\x9F\x9F\xE5\x88\xA5\xE5\xA4\xA9\xE6\xB0\x97\xE4\xBA\x88\xE5\xA0\xB1",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(jma_forecast_area_def)
