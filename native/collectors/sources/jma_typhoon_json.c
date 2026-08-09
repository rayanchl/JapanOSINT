/* collectors/environment/sources/jma_typhoon_json.c
 * JMA active typhoon index → one Feature per tropical cyclone, with the
 * analysed centre position, intensity and the forecast track points.
 *
 * targetTc.json is only an INDEX — its entries are
 *   {"tropicalCyclone":"TC2615","typhoonNumber":"2613","category":"TY",
 *    "issue":"…"}
 * with no lat/lon anywhere. The port read tc.lat / tc.lon off those entries,
 * found nothing, and emitted 0 features for every active storm — including a
 * category-TY typhoon live at the time of this audit. The positions live one
 * level down, in data/<tropicalCyclone>/specifications.json. SEED branch
 * dropped (rule 8). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDEX_URL "https://www.jma.go.jp/bosai/typhoon/data/targetTc.json"
#define JMA_VIEWER "https://www.jma.go.jp/bosai/map.html#contents=typhoon"

/* {"jp":"…","en":"…"} → the requested language, or the plain string. */
static const char *loc_str(const cJSON *o, const char *k, const char *lang) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return NULL;
  if (cJSON_IsString(v)) return v->valuestring[0] ? v->valuestring : NULL;
  return jo_sv(v, lang);
}

/* the element of the specifications array with this advancedHours value
 * (0 = 実況/Analysis, 12/24/… = forecasts) */
static cJSON *part_at(cJSON *arr, int hours) {
  cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    cJSON *h = cJSON_GetObjectItem(e, "advancedHours");
    if (h && cJSON_IsNumber(h) && (int)h->valuedouble == hours) return e;
  }
  return NULL;
}

/* position.deg = [lat, lon] */
static int part_pos(cJSON *part, double *lat, double *lon) {
  cJSON *p = part ? cJSON_GetObjectItem(part, "position") : NULL;
  cJSON *d = p ? cJSON_GetObjectItem(p, "deg") : NULL;
  cJSON *a = d ? cJSON_GetArrayItem(d, 0) : NULL;
  cJSON *b = d ? cJSON_GetArrayItem(d, 1) : NULL;
  if (!cJSON_IsNumber(a) || !cJSON_IsNumber(b)) return 0;
  *lat = a->valuedouble; *lon = b->valuedouble;
  return 1;
}

/* wind.sustained["m/s"] etc. are strings in this API. */
static const char *wind_str(cJSON *part, const char *which, const char *unit) {
  cJSON *w = part ? cJSON_GetObjectItem(part, "maximumWind") : NULL;
  cJSON *s = w ? cJSON_GetObjectItem(w, which) : NULL;
  return s ? jo_sv(s, unit) : NULL;
}

static void add_str(cJSON *p, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(p, k, v);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *index = feed_get_json(ctx->http, INDEX_URL, 8000);
  if (!index || !cJSON_IsArray(index)) {
    if (index) cJSON_Delete(index);
    fprintf(stderr, "[jma-typhoon-json] index unavailable\n");
    return -1;                      /* the fetch itself failed */
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *tc;
  cJSON_ArrayForEach(tc, index) {
    const char *code = jo_sv(tc, "tropicalCyclone");
    if (!code) continue;
    char url[256];
    snprintf(url, sizeof url,
             "https://www.jma.go.jp/bosai/typhoon/data/%s/specifications.json",
             code);
    cJSON *spec = feed_get_json(ctx->http, url, 8000);
    if (!spec || !cJSON_IsArray(spec)) {
      if (spec) cJSON_Delete(spec);
      fprintf(stderr, "[jma-typhoon-json] %s specifications unavailable\n", code);
      continue;
    }

    /* title part carries the storm's name and the bulletin issue time */
    cJSON *title_part = NULL, *e;
    cJSON_ArrayForEach(e, spec) {
      cJSON *pt = cJSON_GetObjectItem(e, "part");
      if (pt && cJSON_IsString(pt) && !strcmp(pt->valuestring, "title")) {
        title_part = e; break;
      }
    }
    cJSON *analysis = part_at(spec, 0);
    double lat = 0, lon = 0;
    if (!part_pos(analysis, &lat, &lon)) {
      cJSON_Delete(spec);
      fprintf(stderr, "[jma-typhoon-json] %s no analysed position\n", code);
      continue;
    }

    const char *name_en = title_part ? loc_str(title_part, "name", "en") : NULL;
    const char *name_jp = title_part ? loc_str(title_part, "name", "jp") : NULL;
    const char *number  = title_part ? jo_sv(title_part, "typhoonNumber") : NULL;
    const char *cat_en  = analysis ? loc_str(analysis, "category", "en") : NULL;
    const char *cat_jp  = analysis ? loc_str(analysis, "category", "jp") : NULL;
    const char *press   = analysis ? jo_sv(analysis, "pressure") : NULL;
    const char *wind_ms = wind_str(analysis, "sustained", "m/s");
    const char *wind_kt = wind_str(analysis, "sustained", "kt");
    const char *gust_ms = wind_str(analysis, "gust", "m/s");
    const char *place   = analysis ? jo_sv(analysis, "location") : NULL;
    const char *course  = analysis ? jo_sv(analysis, "course") : NULL;
    const char *inten   = analysis ? jo_sv(analysis, "intensity") : NULL;
    cJSON *spd = analysis ? cJSON_GetObjectItem(analysis, "speed") : NULL;
    const char *spd_kmh = spd ? jo_sv(spd, "km/h") : NULL;
    cJSON *vt = analysis ? cJSON_GetObjectItem(analysis, "validtime") : NULL;
    const char *valid_utc = vt ? jo_sv(vt, "UTC") : NULL;

    cJSON *f = gj_point_feature(lon, lat);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "id", code);      /* NATIVE_ID_KEY → stable uid */
    char title[256];
    if (name_en && number)
      snprintf(title, sizeof title, "%s %s (No. %s)",
               cat_en ? cat_en : "TC", name_en, number);
    else if (name_en)
      snprintf(title, sizeof title, "%s %s", cat_en ? cat_en : "TC", name_en);
    else
      snprintf(title, sizeof title, "Tropical cyclone %s", code);
    cJSON_AddStringToObject(p, "title", title);
    if (name_jp) {
      char tja[256];
      snprintf(tja, sizeof tja, "%s%s%s%s", cat_jp ? cat_jp : "",
               number ? number : "", number ? "号 " : " ", name_jp);
      cJSON_AddStringToObject(p, "name_ja", tja);
    }
    char summ[320];
    snprintf(summ, sizeof summ,
             "%s%s%s hPa central pressure, %s m/s sustained (%s kt)%s%s",
             place ? place : "", place ? " — " : "",
             press ? press : "?", wind_ms ? wind_ms : "?",
             wind_kt ? wind_kt : "?",
             course ? ", moving " : "", course ? course : "");
    cJSON_AddStringToObject(p, "summary", summ);
    add_str(p, "tropical_cyclone", code);
    add_str(p, "typhoon_number", number);
    add_str(p, "name_en", name_en);
    add_str(p, "name_jp", name_jp);
    add_str(p, "category", cat_en);
    add_str(p, "category_ja", cat_jp);
    add_str(p, "intensity_ja", inten);
    add_str(p, "pressure_hpa", press);
    add_str(p, "max_sustained_wind_ms", wind_ms);
    add_str(p, "max_sustained_wind_kt", wind_kt);
    add_str(p, "max_gust_ms", gust_ms);
    add_str(p, "location", place);
    add_str(p, "course", course);
    add_str(p, "speed_kmh", spd_kmh);
    add_str(p, "observed_at", valid_utc);        /* → published_at */
    cJSON *storm = analysis ? cJSON_GetObjectItem(analysis, "stormWarning") : NULL;
    if (storm) cJSON_AddItemToObject(p, "storm_warning",
                                     cJSON_Duplicate(storm, 1));
    cJSON *gale = analysis ? cJSON_GetObjectItem(analysis, "galeWarning") : NULL;
    if (gale) cJSON_AddItemToObject(p, "gale_warning", cJSON_Duplicate(gale, 1));

    /* forecast positions: every part with advancedHours > 0 */
    cJSON *fcs = cJSON_CreateArray();
    cJSON_ArrayForEach(e, spec) {
      cJSON *h = cJSON_GetObjectItem(e, "advancedHours");
      if (!h || !cJSON_IsNumber(h) || h->valuedouble <= 0) continue;
      double flat, flon;
      if (!part_pos(e, &flat, &flon)) continue;
      cJSON *fc = cJSON_CreateObject();
      cJSON_AddNumberToObject(fc, "hours", h->valuedouble);
      cJSON_AddNumberToObject(fc, "lat", flat);
      cJSON_AddNumberToObject(fc, "lon", flon);
      add_str(fc, "pressure_hpa", jo_sv(e, "pressure"));
      add_str(fc, "category", loc_str(e, "category", "en"));
      cJSON *fvt = cJSON_GetObjectItem(e, "validtime");
      if (fvt) add_str(fc, "valid_utc", jo_sv(fvt, "UTC"));
      cJSON *pcr = cJSON_GetObjectItem(e, "probabilityCircleRadius");
      if (pcr) {
        cJSON *km = cJSON_GetObjectItem(pcr, "km");
        if (km && cJSON_IsNumber(km))
          cJSON_AddNumberToObject(fc, "probability_circle_km", km->valuedouble);
      }
      cJSON_AddItemToArray(fcs, fc);
    }
    cJSON_AddItemToObject(p, "forecast", fcs);
    cJSON_AddStringToObject(p, "record_type", "typhoon");
    cJSON_AddStringToObject(p, "link", JMA_VIEWER);
    cJSON_AddStringToObject(p, "source_url", url);
    cJSON_AddStringToObject(p, "source", "jma_typhoon");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    cJSON_Delete(spec);
  }
  cJSON_Delete(index);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-typhoon-json] emitted %d\n", n);
  return 0;      /* no active typhoon is an honest empty, not an error */
}

static const source_def jma_typhoon_json_def = {
  .id = "jma-typhoon-json", .collector = "environment",
  .name = "JMA Typhoon Track JSON", .name_ja = "気象庁 台風経路",
   .update_interval_sec = 900, .run = run,
};
REGISTER_SOURCE(jma_typhoon_json_def)
