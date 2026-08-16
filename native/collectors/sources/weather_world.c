/* collectors/sources/weather_world.c
 * OSINT services — global weather. On-demand entity pivot (ctx->entity) against
 * three REAL, keyless public weather APIs. One run() dispatches on
 * ctx->source_id and emits one intel_item per parsed record, or honest empty.
 *
 *   OPENMETEO_GLOBAL  Open-Meteo forecast. The forecast endpoint needs lat/lon,
 *                     so we first geocode the entity via the Open-Meteo
 *                     geocoding-api (geocoding-api.open-meteo.com/v1/search),
 *                     then fetch api.open-meteo.com/v1/forecast for the resolved
 *                     coordinates and emit one item per daily forecast row.
 *   NWS_US            US National Weather Service. Geocode the entity (Open-Meteo
 *                     geocoding, filtered to US), then api.weather.gov/points/
 *                     <lat>,<lon> → the forecast URL → one item per period.
 *   AVIATION_METAR    Aviation Weather Center METAR/TAF. entity = an ICAO
 *                     station id (e.g. RJTT, KJFK); fetch
 *                     aviationweather.gov/api/data/metar?ids=<entity>&format=json
 *                     (keyless) and emit one item per returned observation.
 *
 * Every field emitted is a real, parsed value from the live API. No entity,
 * fetch failure, unresolvable location, or empty result → honest empty
 * (return 0). Nothing is synthesized. All LIVE + keyless. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Geocode an entity via the Open-Meteo geocoding API. On success writes lat/lon
 * and (optionally) a resolved place label into name[namesz]. Returns 1 on hit,
 * 0 otherwise. country_filter (e.g. "US") restricts to a country_code when set. */
static int ww_geocode(const source_ctx *ctx, const char *entity, const char *tag,
                      const char *country_filter,
                      double *lat, double *lon, char *name, size_t namesz) {
  char *enc = jo_urlencode(entity);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=10&language=en&format=json",
    enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (weather)", NULL };
  char *body = jo_get(ctx, url, hdrs, tag);
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int found = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r;
    cJSON_ArrayForEach(r, results) {
      if (country_filter) {
        const char *cc = jo_sv(r, "country_code");
        if (!cc || strcasecmp(cc, country_filter) != 0) continue;
      }
      double la, lo;
      if (jo_num(r, "latitude", &la) && jo_num(r, "longitude", &lo)) {
        *lat = la; *lon = lo;
        const char *nm = jo_sv(r, "name");
        const char *ctry = jo_sv(r, "country");
        if (nm && name && namesz) {
          if (ctry) snprintf(name, namesz, "%s, %s", nm, ctry);
          else      snprintf(name, namesz, "%s", nm);
        }
        found = 1;
        break;
      }
    }
  }
  cJSON_Delete(root);
  return found;
}

/* ---- OPENMETEO_GLOBAL --------------------------------------------------- */
static int ww_openmeteo(const source_ctx *ctx, intel_sink *sink, const char *q) {
  double lat = 0, lon = 0;
  char place[256] = {0};
  if (!ww_geocode(ctx, q, "openmeteo", NULL, &lat, &lon, place, sizeof place)) {
    fprintf(stderr, "[OPENMETEO_GLOBAL] no geocode for '%s'\n", q);
    return 0;
  }
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,"
    "wind_speed_10m_max,weather_code&timezone=auto&forecast_days=7",
    lat, lon);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (weather)", NULL };
  char *body = jo_get(ctx, url, hdrs, "openmeteo");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *daily = cJSON_GetObjectItem(root, "daily");
  cJSON *time  = daily ? cJSON_GetObjectItem(daily, "time") : NULL;
  cJSON *tmax  = daily ? cJSON_GetObjectItem(daily, "temperature_2m_max") : NULL;
  cJSON *tmin  = daily ? cJSON_GetObjectItem(daily, "temperature_2m_min") : NULL;
  cJSON *psum  = daily ? cJSON_GetObjectItem(daily, "precipitation_sum") : NULL;
  cJSON *wmax  = daily ? cJSON_GetObjectItem(daily, "wind_speed_10m_max") : NULL;

  int emitted = 0;
  if (cJSON_IsArray(time)) {
    int n = cJSON_GetArraySize(time);
    for (int i = 0; i < n; i++) {
      cJSON *dv = cJSON_GetArrayItem(time, i);
      const char *day = (dv && cJSON_IsString(dv)) ? dv->valuestring : NULL;
      if (!day) continue;
      double hi = 0, lo = 0, pr = 0, wd = 0;
      int hhi = tmax && (dv = cJSON_GetArrayItem(tmax, i)) && cJSON_IsNumber(dv) ? (hi = dv->valuedouble, 1) : 0;
      int hlo = tmin && (dv = cJSON_GetArrayItem(tmin, i)) && cJSON_IsNumber(dv) ? (lo = dv->valuedouble, 1) : 0;
      int hpr = psum && (dv = cJSON_GetArrayItem(psum, i)) && cJSON_IsNumber(dv) ? (pr = dv->valuedouble, 1) : 0;
      int hwd = wmax && (dv = cJSON_GetArrayItem(wmax, i)) && cJSON_IsNumber(dv) ? (wd = dv->valuedouble, 1) : 0;

      char title[320];
      snprintf(title, sizeof title, "%s %s: %s%.1f°C / %s%.1f°C",
               place[0] ? place : q, day,
               hhi ? "" : "?", hi, hlo ? "" : "?", lo);

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "location", place[0] ? place : q);
      cJSON_AddNumberToObject(data, "latitude", lat);
      cJSON_AddNumberToObject(data, "longitude", lon);
      cJSON_AddStringToObject(data, "date", day);
      if (hhi) cJSON_AddNumberToObject(data, "temp_max_c", hi);
      if (hlo) cJSON_AddNumberToObject(data, "temp_min_c", lo);
      if (hpr) cJSON_AddNumberToObject(data, "precipitation_mm", pr);
      if (hwd) cJSON_AddNumberToObject(data, "wind_speed_max_kmh", wd);
      cJSON_AddStringToObject(data, "source", "Open-Meteo");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "OPENMETEO_GLOBAL");
      cJSON_AddStringToObject(props, "source", "Open-Meteo");
      cJSON_AddStringToObject(props, "date", day);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      char key[384];
      snprintf(key, sizeof key, "OPENMETEO|%.3f,%.3f|%s", lat, lon, day);

      intel_item it = {0};
      it.remote_key      = key;
      it.title           = title;
      it.body            = bj;
      it.summary         = place[0] ? place : q;
      it.lang            = "en";
      it.published_at    = day;
      it.has_geo         = 1;
      it.lat             = lat;
      it.lon             = lon;
      it.record_type     = "weather-forecast-daily";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"weather\",\"OPENMETEO_GLOBAL\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[OPENMETEO_GLOBAL] emitted %d\n", emitted);
  return emitted;
}

/* ---- NWS_US ------------------------------------------------------------- */
static int ww_nws(const source_ctx *ctx, intel_sink *sink, const char *q) {
  double lat = 0, lon = 0;
  char place[256] = {0};
  if (!ww_geocode(ctx, q, "nws", "US", &lat, &lon, place, sizeof place)) {
    fprintf(stderr, "[NWS_US] no US geocode for '%s'\n", q);
    return 0;
  }
  const char *hdrs[] = { "Accept: application/geo+json, application/json",
                         "User-Agent: JapanOSINT/1.0 (weather; osint@example.org)",
                         NULL };
  /* step 1: /points/<lat>,<lon> → gridpoint metadata carrying forecast URL */
  char purl[512];
  snprintf(purl, sizeof purl, "https://api.weather.gov/points/%.4f,%.4f", lat, lon);
  char *pbody = jo_get(ctx, purl, hdrs, "nws");
  if (!pbody) return 0;
  cJSON *proot = cJSON_Parse(pbody);
  free(pbody);
  if (!proot) return 0;
  cJSON *pprops = cJSON_GetObjectItem(proot, "properties");
  const char *furl = pprops ? jo_sv(pprops, "forecast") : NULL;
  char forecast_url[600] = {0};
  if (furl) snprintf(forecast_url, sizeof forecast_url, "%s", furl);
  cJSON_Delete(proot);
  if (!forecast_url[0]) {
    fprintf(stderr, "[NWS_US] no forecast url\n");
    return 0;
  }

  /* step 2: forecast → periods[] */
  char *fbody = jo_get(ctx, forecast_url, hdrs, "nws");
  if (!fbody) return 0;
  cJSON *froot = cJSON_Parse(fbody);
  free(fbody);
  if (!froot) return 0;
  cJSON *fprops  = cJSON_GetObjectItem(froot, "properties");
  cJSON *periods = fprops ? cJSON_GetObjectItem(fprops, "periods") : NULL;

  int emitted = 0;
  if (cJSON_IsArray(periods)) {
    cJSON *p;
    cJSON_ArrayForEach(p, periods) {
      const char *pname = jo_sv(p, "name");
      const char *start = jo_sv(p, "startTime");
      const char *detail = jo_sv(p, "detailedForecast");
      const char *shortf = jo_sv(p, "shortForecast");
      const char *tunit = jo_sv(p, "temperatureUnit");
      const char *wind  = jo_sv(p, "windSpeed");
      double temp = 0; int htemp = jo_num(p, "temperature", &temp);
      if (!pname) continue;

      char title[384];
      snprintf(title, sizeof title, "%s — %s: %s%s",
               place[0] ? place : q, pname,
               shortf ? shortf : "", shortf ? "" : "");

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "location", place[0] ? place : q);
      cJSON_AddStringToObject(data, "period", pname);
      if (htemp) cJSON_AddNumberToObject(data, "temperature", temp);
      if (tunit) cJSON_AddStringToObject(data, "temperature_unit", tunit);
      if (wind)  cJSON_AddStringToObject(data, "wind", wind);
      if (shortf) cJSON_AddStringToObject(data, "short_forecast", shortf);
      if (detail) cJSON_AddStringToObject(data, "detailed_forecast", detail);
      cJSON_AddStringToObject(data, "source", "US NWS");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "NWS_US");
      cJSON_AddStringToObject(props, "source", "US NWS");
      cJSON_AddStringToObject(props, "period", pname);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      char key[384];
      snprintf(key, sizeof key, "NWS|%.3f,%.3f|%s", lat, lon, pname);

      intel_item it = {0};
      it.remote_key      = key;
      it.title           = title;
      it.body            = bj;
      it.summary         = detail ? detail : shortf;
      it.lang            = "en";
      it.published_at    = start;
      it.has_geo         = 1;
      it.lat             = lat;
      it.lon             = lon;
      it.record_type     = "weather-forecast-period";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"weather\",\"NWS_US\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(froot);
  fprintf(stderr, "[NWS_US] emitted %d\n", emitted);
  return emitted;
}

/* ---- AVIATION_METAR ----------------------------------------------------- */
static int ww_metar(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://aviationweather.gov/api/data/metar?ids=%s&format=json", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (weather)", NULL };
  char *body = jo_get(ctx, url, hdrs, "metar");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  /* API returns a top-level array of observation objects. */
  int emitted = 0;
  if (cJSON_IsArray(root)) {
    cJSON *o;
    cJSON_ArrayForEach(o, root) {
      const char *icao = jo_sv(o, "icaoId");
      const char *raw  = jo_sv(o, "rawOb");
      const char *rt   = jo_sv(o, "reportTime");
      const char *nm   = jo_sv(o, "name");
      double temp = 0, dewp = 0, wspd = 0, wdir = 0, lat = 0, lon = 0;
      int htemp = jo_num(o, "temp", &temp);
      int hdewp = jo_num(o, "dewp", &dewp);
      int hwspd = jo_num(o, "wspd", &wspd);
      int hwdir = jo_num(o, "wdir", &wdir);
      int hgeo  = jo_num(o, "lat", &lat) && jo_num(o, "lon", &lon);
      if (!icao && !raw) continue;

      char title[384];
      snprintf(title, sizeof title, "METAR %s%s%s",
               icao ? icao : q,
               nm ? " — " : "", nm ? nm : "");

      cJSON *data = cJSON_CreateObject();
      if (icao) cJSON_AddStringToObject(data, "icao", icao);
      if (nm)   cJSON_AddStringToObject(data, "station", nm);
      if (raw)  cJSON_AddStringToObject(data, "raw_metar", raw);
      if (rt)   cJSON_AddStringToObject(data, "report_time", rt);
      if (htemp) cJSON_AddNumberToObject(data, "temp_c", temp);
      if (hdewp) cJSON_AddNumberToObject(data, "dewpoint_c", dewp);
      if (hwspd) cJSON_AddNumberToObject(data, "wind_speed_kt", wspd);
      if (hwdir) cJSON_AddNumberToObject(data, "wind_dir_deg", wdir);
      cJSON_AddStringToObject(data, "source", "Aviation Weather Center");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "AVIATION_METAR");
      cJSON_AddStringToObject(props, "source", "Aviation Weather Center");
      if (icao) cJSON_AddStringToObject(props, "icao", icao);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      char key[256];
      snprintf(key, sizeof key, "METAR|%s|%s", icao ? icao : q, rt ? rt : "");

      intel_item it = {0};
      it.remote_key      = key;
      it.title           = title;
      it.body            = bj;
      it.summary         = raw;
      it.lang            = "en";
      it.published_at    = rt;
      if (hgeo) { it.has_geo = 1; it.lat = lat; it.lon = lon; }
      it.record_type     = "weather-metar";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"weather\",\"AVIATION_METAR\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[AVIATION_METAR] emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;
  if (strcmp(ctx->source_id, "OPENMETEO_GLOBAL") == 0) { ww_openmeteo(ctx, sink, q); return 0; }
  if (strcmp(ctx->source_id, "NWS_US") == 0)           { ww_nws(ctx, sink, q);       return 0; }
  if (strcmp(ctx->source_id, "AVIATION_METAR") == 0)   { ww_metar(ctx, sink, q);     return 0; }
  return -1;
}

static const source_def openmeteo_def = {
  .id = "OPENMETEO_GLOBAL", .collector = "osint",
  .name = "Open-Meteo Weather Forecast", .name_ja = "Open-Meteo 気象予報",
  .update_interval_sec = 0, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.open-meteo.com/v1/forecast",
  .description = "Open-Meteo — geocode a place then fetch 7-day daily forecast (keyless, LIVE)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(openmeteo_def)

static const source_def nws_def = {
  .id = "NWS_US", .collector = "osint",
  .name = "US NWS Weather Forecast", .name_ja = "米国 NWS 気象予報",
  .update_interval_sec = 0, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.weather.gov/",
  .description = "US National Weather Service — geocode a US place then fetch period forecast (keyless, LIVE)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(nws_def)

static const source_def metar_def = {
  .id = "AVIATION_METAR", .collector = "osint",
  .name = "Aviation Weather METAR", .name_ja = "航空気象 METAR",
  .update_interval_sec = 0, .run = run,
  .category = "environment", .type = "api",
  .url = "https://aviationweather.gov/api/data/metar",
  .description = "Aviation Weather Center — METAR observation for an ICAO station id (keyless, LIVE)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(metar_def)
