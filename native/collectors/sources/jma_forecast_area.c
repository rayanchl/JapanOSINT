/* collectors/environment/sources/jma_forecast_area.c
 * Port of server/src/collectors/jmaForecastArea.js.
 * Per-area JMA regional forecast JSON; one Point Feature per area that has a
 * weather string. The seed fallback (rule 7) is dropped — live rows only.
 *
 * 2026-09-03 audit: JMA's per-office response bundles several named
 * sub-regions under one office code (e.g. office 016000 currently carries
 * three: 016010 Ishikari, 016020 Sorachi, 016030 Shiribeshi, each with its
 * own weathers[]/winds[]). The previous version read areas[0] only and
 * silently discarded the sibling sub-regions on every run, and separately
 * read weathers[0]/winds[0] only, dropping the fetched "tomorrow" forecast
 * too. Both are now emitted: one feature per sub-region (house rule 2), each
 * carrying both today's and tomorrow's forecast. AREAS[] below still tracks
 * one hand-picked lat/lon per OFFICE, not per sub-region — there is no
 * coordinate to place a sub-region's own pin without extending AREAS[] with
 * upstream sub-region coordinates this collector does not have, so every
 * sub-region under an office shares that office's point. That approximation
 * is disclosed in-band via `geo_precision`/`office_code`, not silently
 * presented as a precise per-sub-region location (house rule 1). */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
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

/* One JMA sub-area element's weathers[idx]/winds[idx]. idx 0 = today, 1 =
 * tomorrow — both are read by run() below; nothing here fixes idx at 0. */
static const char *series_str(cJSON *area_elem, const char *seriesKey, int idx) {
  cJSON *arr = area_elem ? cJSON_GetObjectItem(area_elem, seriesKey) : NULL;
  cJSON *v = (arr && cJSON_IsArray(arr)) ? cJSON_GetArrayItem(arr, idx) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

static const char *obj_str(cJSON *o, const char *key) {
  cJSON *v = o ? cJSON_GetObjectItem(o, key) : NULL;
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
    cJSON *first = (cJSON_IsArray(arr)) ? cJSON_GetArrayItem(arr, 0) : NULL;  /* exhaustive-ok: forecast office block, not a sub-area list */
    cJSON *ts = first ? cJSON_GetObjectItem(first, "timeSeries") : NULL;
    cJSON *ts0 = (ts && cJSON_IsArray(ts)) ? cJSON_GetArrayItem(ts, 0) : NULL;  /* exhaustive-ok: timeSeries[0] is the only block carrying weathers/winds — the other blocks hold unrelated variables (pop/temperature, week outlook) */
    cJSON *as = ts0 ? cJSON_GetObjectItem(ts0, "areas") : NULL;
    cJSON *rd = first ? cJSON_GetObjectItem(first, "reportDatetime") : NULL;
    const char *report_at =
      (rd && cJSON_IsString(rd) && rd->valuestring && rd->valuestring[0])
        ? rd->valuestring : NULL;

    cJSON *ae;
    cJSON_ArrayForEach(ae, as) {                    /* every sub-area under this office, not just [0] */
      const char *weather = series_str(ae, "weathers", 0);
      if (!weather) continue;                        /* if (r?.weather) */
      const char *wind = series_str(ae, "winds", 0);
      /* weathers[]/winds[] normally carry today AND tomorrow. */
      const char *weather_tomorrow = series_str(ae, "weathers", 1);
      const char *wind_tomorrow    = series_str(ae, "winds", 1);

      cJSON *aobj = cJSON_GetObjectItem(ae, "area");
      const char *sub_code = obj_str(aobj, "code");
      const char *sub_name = obj_str(aobj, "name");

      cJSON *f = gj_point_feature(a->lon, a->lat);

      cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
      /* Stable per-sub-area identity: the JMA sub-region code, falling back
       * to the office code for a malformed sub-area element (should not
       * happen, but a missing id must never silently collide two different
       * sub-areas onto one row). */
      {
        char sid[48];
        snprintf(sid, sizeof sid, "jma-area-%s", sub_code ? sub_code : a->code);
        cJSON_AddStringToObject(p, "id", sid);
      }
      cJSON_AddStringToObject(p, "office_code", a->code);
      cJSON_AddStringToObject(p, "office_name", a->name);
      cJSON_AddStringToObject(p, "area_code", sub_code ? sub_code : a->code);
      cJSON_AddStringToObject(p, "area_name", sub_name ? sub_name : a->name);
      /* The pin is the OFFICE's point, not this sub-area's own location —
       * AREAS[] has no per-sub-area coordinate. Disclosed, not fabricated. */
      cJSON_AddStringToObject(p, "geo_precision", "office-level (sub-area has no own coordinate)");
      cJSON_AddStringToObject(p, "weather", weather);
      cJSON_AddItemToObject(p, "wind",
        wind ? cJSON_CreateString(wind) : cJSON_CreateNull());
      if (weather_tomorrow)
        cJSON_AddStringToObject(p, "weather_tomorrow", weather_tomorrow);
      if (wind_tomorrow)
        cJSON_AddStringToObject(p, "wind_tomorrow", wind_tomorrow);
      cJSON_AddItemToObject(p, "report_at",
        report_at ? cJSON_CreateString(report_at) : cJSON_CreateNull());
      /* geojson T_PUB is published_at|observed_at|time|timestamp — "report_at"
       * matched none, so the timeline column stayed empty. */
      if (report_at) cJSON_AddStringToObject(p, "published_at", report_at);
      cJSON_AddStringToObject(p, "source", "jma_forecast");
      /* geojson pickText looks for title|name|name_ja|label; "area_name" is
       * none of them, so every forecast row had a NULL title. Compose it from
       * the fetched area name + the fetched weather string. */
      {
        char t[512];
        snprintf(t, sizeof t, "%s \xE2\x80\x94 %s",
                 sub_name ? sub_name : a->name, weather);
        cJSON_AddStringToObject(p, "title", t);
      }
      {
        char lk[160];
        snprintf(lk, sizeof lk,
          "https://www.jma.go.jp/bosai/forecast/#area_type=offices&area_code=%s",
          a->code);
        cJSON_AddStringToObject(p, "link", lk);
      }
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
