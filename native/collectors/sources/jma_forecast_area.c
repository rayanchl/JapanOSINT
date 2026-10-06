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
 * presented as a precise per-sub-region location (house rule 1).
 *
 * 2026-10-03: every other series in the response is carried too — see
 * office_features(). */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* One area code's share of an office response: every series of every
 * timeSeries block (short-term AND weekly) that names this code, plus the
 * weekly tempAverage/precipAverage entries for it. */
typedef struct {
  const char *code, *name;   /* borrowed from the response */
  cJSON *wx;                 /* short-term timeSeries[0] element, if any */
  cJSON *series;             /* owned: array of {forecast, time_defines, …} */
  cJSON *extra;              /* owned: {tempAverage:{…}, precipAverage:{…}} */
} jrec;

static jrec *jrec_get(jrec **v, int *n, int *cap, const char *code,
                      const char *name) {
  for (int i = 0; i < *n; i++)
    if (strcmp((*v)[i].code, code) == 0) return &(*v)[i];
  if (*n == *cap) {
    int nc = *cap ? *cap * 2 : 16;
    jrec *nv = realloc(*v, (size_t)nc * sizeof *nv);
    if (!nv) return NULL;
    *v = nv; *cap = nc;
  }
  jrec *r = &(*v)[(*n)++];
  r->code = code; r->name = name; r->wx = NULL;
  r->series = cJSON_CreateArray();
  r->extra = cJSON_CreateObject();
  return r;
}

/* Every area of every block of one office's response, emitted as features
 * (returns the count emitted).
 *
 * This read the short-term block's timeSeries[0] only — weathers/winds — and
 * discarded the rest of a response it had already fetched and parsed: the
 * same areas' weatherCodes and waves, the 7-slot precipitation probabilities
 * (timeSeries[1]), the city temperatures (timeSeries[2]), and the whole weekly
 * block (weather codes, pops, reliabilities, min/max temperatures with their
 * ranges, and the normals). Each area code now carries every series that
 * names it, with that series' own timeDefines, under `series`. Areas that
 * carry no weather sentence (temperature points such as 44132 東京, weekly-only
 * groupings such as 130100 伊豆諸島) are emitted too instead of being skipped. */
static int office_features(const struct area *a, cJSON *arr, intel_sink *sink,
                           const char *source_id) {
  cJSON *features = cJSON_CreateArray();
  jrec *recs = NULL;
  int nr = 0, cr = 0;
  const char *report_at = NULL;
  int bi = 0;
  cJSON *blk;
  cJSON_ArrayForEach(blk, arr) {
    const char *kind = bi == 0 ? "short-term" : bi == 1 ? "weekly" : "other";
    const char *rd = obj_str(blk, "reportDatetime");
    if (bi == 0) report_at = rd;
    int ti = 0;
    cJSON *t;
    cJSON_ArrayForEach(t, cJSON_GetObjectItem(blk, "timeSeries")) {
      cJSON *td = cJSON_GetObjectItem(t, "timeDefines");
      cJSON *ae;
      cJSON_ArrayForEach(ae, cJSON_GetObjectItem(t, "areas")) {
        cJSON *ao = cJSON_GetObjectItem(ae, "area");
        const char *code = obj_str(ao, "code");
        if (!code) continue;
        jrec *r = jrec_get(&recs, &nr, &cr, code, obj_str(ao, "name"));
        if (!r) continue;
        if (!r->name) r->name = obj_str(ao, "name");
        if (bi == 0 && ti == 0) r->wx = ae;
        cJSON *sobj = cJSON_CreateObject();
        cJSON_AddStringToObject(sobj, "forecast", kind);
        if (rd) cJSON_AddStringToObject(sobj, "report_at", rd);
        if (td) cJSON_AddItemToObject(sobj, "time_defines", cJSON_Duplicate(td, 1));
        cJSON *m;
        cJSON_ArrayForEach(m, ae) {
          if (!m->string || strcmp(m->string, "area") == 0) continue;
          cJSON_AddItemToObject(sobj, m->string, cJSON_Duplicate(m, 1));
        }
        cJSON_AddItemToArray(r->series, sobj);
      }
      ti++;
    }
    /* tempAverage / precipAverage and any other {areas:[…]} member */
    cJSON *mem;
    cJSON_ArrayForEach(mem, blk) {
      if (!mem->string || strcmp(mem->string, "timeSeries") == 0) continue;
      cJSON *areas = cJSON_IsObject(mem) ? cJSON_GetObjectItem(mem, "areas") : NULL;
      if (!cJSON_IsArray(areas)) continue;
      cJSON *ae;
      cJSON_ArrayForEach(ae, areas) {
        cJSON *ao = cJSON_GetObjectItem(ae, "area");
        const char *code = obj_str(ao, "code");
        if (!code) continue;
        jrec *r = jrec_get(&recs, &nr, &cr, code, obj_str(ao, "name"));
        if (!r) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON *m;
        cJSON_ArrayForEach(m, ae) {
          if (!m->string || strcmp(m->string, "area") == 0) continue;
          cJSON_AddItemToObject(o, m->string, cJSON_Duplicate(m, 1));
        }
        cJSON_AddItemToObject(r->extra, mem->string, o);
      }
    }
    bi++;
  }

  for (int i = 0; i < nr; i++) {
    jrec *r = &recs[i];
    const char *weather = series_str(r->wx, "weathers", 0);
    const char *wind = series_str(r->wx, "winds", 0);
    /* weathers[]/winds[] normally carry today AND tomorrow. */
    const char *weather_tomorrow = series_str(r->wx, "weathers", 1);
    const char *wind_tomorrow    = series_str(r->wx, "winds", 1);
    const char *sub_name = r->name;

    cJSON *f = gj_point_feature(a->lon, a->lat);
    cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
    {
      char sid[48];
      snprintf(sid, sizeof sid, "jma-area-%s", r->code);
      cJSON_AddStringToObject(p, "id", sid);
    }
    cJSON_AddStringToObject(p, "office_code", a->code);
    cJSON_AddStringToObject(p, "office_name", a->name);
    cJSON_AddStringToObject(p, "area_code", r->code);
    cJSON_AddStringToObject(p, "area_name", sub_name ? sub_name : a->name);
    /* The pin is the OFFICE's point, not this area's own location — AREAS[]
     * has no per-area coordinate. Disclosed, not fabricated. */
    cJSON_AddStringToObject(p, "geo_precision", "office-level (sub-area has no own coordinate)");
    if (weather) {
      cJSON_AddStringToObject(p, "weather", weather);
      cJSON_AddItemToObject(p, "wind",
        wind ? cJSON_CreateString(wind) : cJSON_CreateNull());
    }
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
    cJSON_AddItemToObject(p, "series", r->series);          /* transferred */
    cJSON *m = r->extra->child;
    while (m) {                                             /* averages */
      cJSON *next = m->next;
      cJSON *d = cJSON_DetachItemViaPointer(r->extra, m);
      if (!cJSON_GetObjectItem(p, d->string)) cJSON_AddItemToObject(p, d->string, d);
      else cJSON_Delete(d);
      m = next;
    }
    cJSON_Delete(r->extra);
    /* geojson pickText looks for title|name|name_ja|label; compose it from
     * the fetched area name + the fetched weather string (or, for an area
     * with no weather sentence, say which forecast it carries). */
    {
      char t[512];
      snprintf(t, sizeof t, "%s \xE2\x80\x94 %s", sub_name ? sub_name : a->name,
               weather ? weather : "\xE5\xA4\xA9\xE6\xB0\x97\xE4\xBA\x88\xE5\xA0\xB1"
                                   "\xE7\xB3\xBB\xE5\x88\x97");   /* 天気予報系列 */
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
  free(recs);
  int n = geojson_emit_features(sink, source_id, features);
  cJSON_Delete(features);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0;
  for (size_t i = 0; i < sizeof(AREAS) / sizeof(AREAS[0]); i++) {
    const struct area *a = &AREAS[i];
    char url[128];
    snprintf(url, sizeof url,
      "https://www.jma.go.jp/bosai/forecast/data/forecast/%s.json", a->code);
    cJSON *arr = feed_get_json(ctx->http, url, 8000);
    if (!arr) continue;                             /* !res.ok → null */
    if (cJSON_IsArray(arr)) {
      int k = office_features(a, arr, sink, ctx->source_id);
      if (k > 0) n += k;
    }
    cJSON_Delete(arr);
  }
  fprintf(stderr, "[jma-forecast-area] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jma_forecast_area_def = {
  .id = "jma-forecast-area", .collector = "environment",
  .name = "JMA Regional Forecast JSON",
  .name_ja = "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81 \xE5\x9C\xB0\xE5\x9F\x9F\xE5\x88\xA5\xE5\xA4\xA9\xE6\xB0\x97\xE4\xBA\x88\xE5\xA0\xB1",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(jma_forecast_area_def)
