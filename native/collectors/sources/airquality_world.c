/* collectors/sources/airquality_world.c
 * OSINT services — global air-quality observations. On-demand / scheduled pull
 * of REAL measurements from the major open air-quality networks:
 *
 *   OPENAQ_GLOBAL   OpenAQ aggregator (api.openaq.org). v3 now requires an
 *                   X-API-Key (OPENAQ_KEY) — gated. When no key is present we
 *                   fall back to the keyless v2 measurements endpoint IF it is
 *                   still reachable; otherwise honest-empty.
 *   WAQI_GLOBAL     World Air Quality Index (api.waqi.info) — gated on the
 *                   AQICN_TOKEN feed token.
 *   OPENMETEO_AQ    Open-Meteo air-quality API (air-quality-api.open-meteo.com)
 *                   — keyless. Coordinate-based; emits current pollutant values.
 *
 * ctx->entity is used as a place/city pivot for OpenAQ/WAQI (city query) and,
 * when it contains "lat,lon", as coordinates for Open-Meteo. When entity is
 * NULL (scheduled feed run) Open-Meteo defaults to Tokyo (35.68,139.69), and
 * WAQI/OpenAQ default to a broad recent-measurements pull. Every value emitted
 * is parsed from the live JSON — nothing is synthesized. Missing key → gated
 * (log + return 0); fetch failure / no data → honest empty (return 0).
 *
 * One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Parse "lat,lon" (optionally with spaces) out of s. Returns 1 on success. */
static int aq_parse_latlon(const char *s, double *lat, double *lon) {
  if (!s) return 0;
  char buf[128];
  snprintf(buf, sizeof buf, "%.127s", s);
  char *comma = strchr(buf, ',');
  if (!comma) return 0;
  *comma = 0;
  char *a = buf; while (*a == ' ') a++;
  char *b = comma + 1; while (*b == ' ') b++;
  char *ea = NULL, *eb = NULL;
  double la = strtod(a, &ea), lo = strtod(b, &eb);
  if (ea == a || eb == b) return 0;
  if (la < -90 || la > 90 || lo < -180 || lo > 180) return 0;
  *lat = la; *lon = lo;
  return 1;
}

/* ---- OpenAQ (v3 gated, v2 keyless fallback) ---------------------------- *
 * v3 /locations returns { results: [ { id, name, coordinates:{latitude,
 * longitude}, country:{code}, sensors:[{parameter:{name,units}}], ... } ] }.
 * v2 /measurements returns { results: [ { parameter, value, unit, date:{utc},
 * coordinates:{latitude,longitude}, location, city, country } ] }. We only emit
 * parsed rows. */
static int aq_emit_openaq_v3(intel_sink *sink, const cJSON *loc) {
  const char *name = jo_sv(loc, "name");
  const cJSON *idn = cJSON_GetObjectItem(loc, "id");
  if (!name && !(idn && cJSON_IsNumber(idn))) return 0;

  const cJSON *coord = cJSON_GetObjectItem(loc, "coordinates");
  double lat = 0, lon = 0; int hasgeo = 0;
  if (coord) hasgeo = jo_num(coord, "latitude", &lat) & jo_num(coord, "longitude", &lon);

  const cJSON *country = cJSON_GetObjectItem(loc, "country");
  const char *cc = country ? jo_sv(country, "code") : NULL;

  cJSON *data = cJSON_CreateObject();
  if (name) cJSON_AddStringToObject(data, "location", name);
  if (idn && cJSON_IsNumber(idn)) cJSON_AddNumberToObject(data, "location_id", idn->valuedouble);
  if (cc) cJSON_AddStringToObject(data, "country", cc);
  if (hasgeo) { cJSON_AddNumberToObject(data, "lat", lat); cJSON_AddNumberToObject(data, "lon", lon); }
  const cJSON *sensors = cJSON_GetObjectItem(loc, "sensors");
  if (cJSON_IsArray(sensors)) {
    cJSON *params = cJSON_CreateArray();
    const cJSON *s;
    cJSON_ArrayForEach(s, sensors) {
      const cJSON *par = cJSON_GetObjectItem(s, "parameter");
      const char *pn = par ? jo_sv(par, "name") : NULL;
      if (pn) cJSON_AddItemToArray(params, cJSON_CreateString(pn));
    }
    cJSON_AddItemToObject(data, "parameters", params);
  }
  cJSON_AddStringToObject(data, "source", "OpenAQ");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENAQ_GLOBAL");
  cJSON_AddStringToObject(props, "source", "OpenAQ");
  if (cc) cJSON_AddStringToObject(props, "country", cc);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char key[256];
  if (idn && cJSON_IsNumber(idn)) snprintf(key, sizeof key, "openaq|loc|%.0f", idn->valuedouble);
  else snprintf(key, sizeof key, "openaq|loc|%.200s", name);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = name ? name : "OpenAQ location";
  it.summary         = cc;
  it.body            = bj;
  it.lang            = "en";
  it.has_geo         = hasgeo;
  it.lat = lat; it.lon = lon;
  it.record_type     = "air-quality-station";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"air-quality\",\"OpenAQ\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int aq_emit_openaq_v2(intel_sink *sink, const cJSON *m, int idx) {
  const char *param = jo_sv(m, "parameter");
  double val = 0; int hasval = jo_num(m, "value", &val);
  if (!param || !hasval) return 0;
  const char *unit = jo_sv(m, "unit");
  const char *locn = jo_sv(m, "location");
  const char *city = jo_sv(m, "city");
  const char *ctry = jo_sv(m, "country");
  const cJSON *date = cJSON_GetObjectItem(m, "date");
  const char *utc = date ? jo_sv(date, "utc") : NULL;
  const cJSON *coord = cJSON_GetObjectItem(m, "coordinates");
  double lat = 0, lon = 0; int hasgeo = 0;
  if (coord) hasgeo = jo_num(coord, "latitude", &lat) & jo_num(coord, "longitude", &lon);

  cJSON *data = cJSON_CreateObject();
  if (locn) cJSON_AddStringToObject(data, "location", locn);
  if (city) cJSON_AddStringToObject(data, "city", city);
  if (ctry) cJSON_AddStringToObject(data, "country", ctry);
  cJSON_AddStringToObject(data, "parameter", param);
  cJSON_AddNumberToObject(data, "value", val);
  if (unit) cJSON_AddStringToObject(data, "unit", unit);
  if (utc) cJSON_AddStringToObject(data, "measured_at", utc);
  cJSON_AddStringToObject(data, "source", "OpenAQ");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENAQ_GLOBAL");
  cJSON_AddStringToObject(props, "source", "OpenAQ");
  cJSON_AddStringToObject(props, "parameter", param);
  if (ctry) cJSON_AddStringToObject(props, "country", ctry);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[256];
  snprintf(title, sizeof title, "%s %g%s%s @ %s",
           param, val, unit ? " " : "", unit ? unit : "",
           locn ? locn : (city ? city : "?"));
  char key[320];
  snprintf(key, sizeof key, "openaq|m|%s|%.150s|%s", param, locn ? locn : city ? city : "", utc ? utc : "");
  (void)idx;

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = city ? city : ctry;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = utc;
  it.has_geo         = hasgeo;
  it.lat = lat; it.lon = lon;
  it.record_type     = "air-quality-measurement";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"air-quality\",\"OpenAQ\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_openaq(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  const char *key = jo_env("OPENAQ_KEY");

  if (key) {
    /* v3 locations, key required */
    char url[1024];
    if (q) {
      char *enc = jo_urlencode(q);
      snprintf(url, sizeof url,
        "https://api.openaq.org/v3/locations?limit=50&order_by=id&city=%s", enc ? enc : "");
      free(enc);
    } else {
      snprintf(url, sizeof url, "https://api.openaq.org/v3/locations?limit=50&order_by=id");
    }
    char khdr[256];
    snprintf(khdr, sizeof khdr, "X-API-Key: %s", key);
    const char *hdrs[] = { khdr, "Accept: application/json",
                           "User-Agent: JapanOSINT/1.0 (air-quality)", NULL };
    char *body = jo_get(ctx, url, hdrs, "OPENAQ_GLOBAL");
    if (!body) return 0;
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return 0;
    cJSON *results = cJSON_GetObjectItem(root, "results");
    int emitted = 0;
    if (cJSON_IsArray(results)) {
      cJSON *r; cJSON_ArrayForEach(r, results) emitted += aq_emit_openaq_v3(sink, r);
    }
    cJSON_Delete(root);
    fprintf(stderr, "[OPENAQ_GLOBAL] emitted %d (v3)\n", emitted);
    return 0;
  }

  /* No key → try keyless v2 fallback if still reachable. */
  fprintf(stderr, "[OPENAQ_GLOBAL] no OPENAQ_KEY — trying keyless v2 fallback\n");
  char url[1024];
  if (q) {
    char *enc = jo_urlencode(q);
    snprintf(url, sizeof url,
      "https://api.openaq.org/v2/measurements?limit=50&city=%s&order_by=datetime&sort=desc",
      enc ? enc : "");
    free(enc);
  } else {
    snprintf(url, sizeof url,
      "https://api.openaq.org/v2/measurements?limit=50&order_by=datetime&sort=desc");
  }
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (air-quality)", NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENAQ_GLOBAL");
  if (!body) {
    fprintf(stderr, "[OPENAQ_GLOBAL] gated (no OPENAQ_KEY, v2 unreachable)\n");
    return 0;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0, idx = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r; cJSON_ArrayForEach(r, results) emitted += aq_emit_openaq_v2(sink, r, idx++);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[OPENAQ_GLOBAL] emitted %d (v2)\n", emitted);
  return 0;
}

/* ---- WAQI (api.waqi.info, gated AQICN_TOKEN) --------------------------- *
 * /feed/<city>/?token=... or /feed/geo:<lat>;<lon>/?token=... returns
 * { status:"ok", data:{ aqi, idx, city:{name,geo:[lat,lon],url}, dominentpol,
 *   time:{iso}, iaqi:{ pm25:{v}, pm10:{v}, ... } } }. */
static int run_waqi(const source_ctx *ctx, intel_sink *sink) {
  const char *token = jo_env("AQICN_TOKEN");
  if (!token) { fprintf(stderr, "[WAQI_GLOBAL] gated (no AQICN_TOKEN)\n"); return 0; }

  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  double lat, lon;
  char station[512];
  if (q && aq_parse_latlon(q, &lat, &lon))
    snprintf(station, sizeof station, "geo:%.6f;%.6f", lat, lon);
  else if (q) {
    char *enc = jo_urlencode(q);
    snprintf(station, sizeof station, "%s", enc ? enc : "here");
    free(enc);
  } else {
    snprintf(station, sizeof station, "here");  /* WAQI geolocates by IP */
  }

  char url[1024];
  snprintf(url, sizeof url, "https://api.waqi.info/feed/%s/?token=%s", station, token);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (air-quality)", NULL };
  char *body = jo_get(ctx, url, hdrs, "WAQI_GLOBAL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const char *status = jo_sv(root, "status");
  cJSON *d = cJSON_GetObjectItem(root, "data");
  if (!status || strcmp(status, "ok") != 0 || !cJSON_IsObject(d)) {
    fprintf(stderr, "[WAQI_GLOBAL] status not ok\n");
    cJSON_Delete(root); return 0;
  }

  double aqi = 0; int hasaqi = jo_num(d, "aqi", &aqi);
  const cJSON *cityo = cJSON_GetObjectItem(d, "city");
  const char *cname = cityo ? jo_sv(cityo, "name") : NULL;
  const char *curl  = cityo ? jo_sv(cityo, "url") : NULL;
  double clat = 0, clon = 0; int hasgeo = 0;
  const cJSON *geo = cityo ? cJSON_GetObjectItem(cityo, "geo") : NULL;
  if (cJSON_IsArray(geo) && cJSON_GetArraySize(geo) >= 2) {
    const cJSON *g0 = cJSON_GetArrayItem(geo, 0), *g1 = cJSON_GetArrayItem(geo, 1);
    if (cJSON_IsNumber(g0) && cJSON_IsNumber(g1)) {
      clat = g0->valuedouble; clon = g1->valuedouble; hasgeo = 1;
    }
  }
  const char *domin = jo_sv(d, "dominentpol");
  const cJSON *timeo = cJSON_GetObjectItem(d, "time");
  const char *iso = timeo ? jo_sv(timeo, "iso") : NULL;
  const cJSON *idxo = cJSON_GetObjectItem(d, "idx");

  cJSON *data = cJSON_CreateObject();
  if (cname) cJSON_AddStringToObject(data, "station", cname);
  if (hasaqi) cJSON_AddNumberToObject(data, "aqi", aqi);
  if (domin) cJSON_AddStringToObject(data, "dominant_pollutant", domin);
  const cJSON *iaqi = cJSON_GetObjectItem(d, "iaqi");
  if (cJSON_IsObject(iaqi)) {
    cJSON *pols = cJSON_CreateObject();
    const cJSON *p;
    cJSON_ArrayForEach(p, iaqi) {
      double v; if (p->string && jo_num(p, "v", &v)) cJSON_AddNumberToObject(pols, p->string, v);
    }
    cJSON_AddItemToObject(data, "pollutants", pols);
  }
  if (iso) cJSON_AddStringToObject(data, "measured_at", iso);
  if (hasgeo) { cJSON_AddNumberToObject(data, "lat", clat); cJSON_AddNumberToObject(data, "lon", clon); }
  cJSON_AddStringToObject(data, "source", "WAQI/AQICN");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WAQI_GLOBAL");
  cJSON_AddStringToObject(props, "source", "WAQI/AQICN");
  if (hasaqi) cJSON_AddNumberToObject(props, "aqi", aqi);
  if (domin) cJSON_AddStringToObject(props, "dominant_pollutant", domin);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[320];
  if (hasaqi) snprintf(title, sizeof title, "AQI %g — %s", aqi, cname ? cname : "station");
  else snprintf(title, sizeof title, "%s", cname ? cname : "WAQI station");
  char key[256];
  if (idxo && cJSON_IsNumber(idxo)) snprintf(key, sizeof key, "waqi|%.0f", idxo->valuedouble);
  else snprintf(key, sizeof key, "waqi|%.200s", cname ? cname : station);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = domin;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = iso;
  it.link            = curl;
  it.has_geo         = hasgeo;
  it.lat = clat; it.lon = clon;
  it.record_type     = "air-quality-index";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"air-quality\",\"WAQI\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[WAQI_GLOBAL] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

/* ---- Open-Meteo air-quality (keyless, coordinate-based) ---------------- *
 * air-quality-api.open-meteo.com/v1/air-quality?latitude=..&longitude=..&
 * current=pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone,
 * european_aqi,us_aqi returns { latitude, longitude, current:{ time, pm10,
 * pm2_5, ..., european_aqi, us_aqi }, current_units:{...} }. */
static int run_openmeteo(const source_ctx *ctx, intel_sink *sink) {
  /* This used to default to 35.6785,139.6823 (Tokyo) for ANY pivot that was not
   * already a coordinate — so a query for, say, Osaka or a bare place name came
   * back as a Tokyo measurement, emitted with has_geo=1 and the row titled by
   * the returned coordinate. The service is on-demand (interval 0), so the
   * pivot is always supplied; if it is not a coordinate we cannot answer, and
   * saying so is better than answering about somewhere else. */
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  double lat = 0, lon = 0;
  if (!q || !aq_parse_latlon(q, &lat, &lon)) {
    fprintf(stderr, "[OPENMETEO_AQ] pivot %s is not a coordinate; the API is "
                    "coordinate-only, so nothing is emitted (no Tokyo "
                    "default)\n", q ? q : "(none)");
    return 0;
  }

  char url[512];
  snprintf(url, sizeof url,
    "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.5f&longitude=%.5f"
    "&current=pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone,"
    "european_aqi,us_aqi", lat, lon);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (air-quality)", NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENMETEO_AQ");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *cur = cJSON_GetObjectItem(root, "current");
  if (!cJSON_IsObject(cur)) { cJSON_Delete(root); fprintf(stderr, "[OPENMETEO_AQ] no current block\n"); return 0; }
  double rlat = lat, rlon = lon;
  jo_num(root, "latitude", &rlat); jo_num(root, "longitude", &rlon);
  const char *ctime = jo_sv(cur, "time");

  static const char *FIELDS[] = { "pm2_5","pm10","carbon_monoxide","nitrogen_dioxide",
                                  "sulphur_dioxide","ozone","european_aqi","us_aqi", NULL };
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "lat", rlat);
  cJSON_AddNumberToObject(data, "lon", rlon);
  if (ctime) cJSON_AddStringToObject(data, "measured_at", ctime);
  int any = 0;
  for (int i = 0; FIELDS[i]; i++) {
    double v; if (jo_num(cur, FIELDS[i], &v)) { cJSON_AddNumberToObject(data, FIELDS[i], v); any = 1; }
  }
  if (!any) { cJSON_Delete(data); cJSON_Delete(root); fprintf(stderr, "[OPENMETEO_AQ] empty current\n"); return 0; }
  cJSON_AddStringToObject(data, "source", "Open-Meteo");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  double eaqi = 0; int haseaqi = jo_num(cur, "european_aqi", &eaqi);
  double usaqi = 0; int hasusaqi = jo_num(cur, "us_aqi", &usaqi);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENMETEO_AQ");
  cJSON_AddStringToObject(props, "source", "Open-Meteo");
  cJSON_AddNumberToObject(props, "lat", rlat);
  cJSON_AddNumberToObject(props, "lon", rlon);
  if (haseaqi) cJSON_AddNumberToObject(props, "european_aqi", eaqi);
  if (hasusaqi) cJSON_AddNumberToObject(props, "us_aqi", usaqi);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[256];
  if (haseaqi) snprintf(title, sizeof title, "Air quality @ %.4f,%.4f — EU AQI %g", rlat, rlon, eaqi);
  else snprintf(title, sizeof title, "Air quality @ %.4f,%.4f", rlat, rlon);
  char key[128];
  snprintf(key, sizeof key, "openmeteo-aq|%.4f,%.4f|%s", rlat, rlon, ctime ? ctime : "");

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = NULL;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = ctime;
  it.has_geo         = 1;
  it.lat = rlat; it.lon = rlon;
  it.record_type     = "air-quality-measurement";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"air-quality\",\"Open-Meteo\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[OPENMETEO_AQ] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->source_id) return -1;
  if (strcmp(ctx->source_id, "OPENAQ_GLOBAL") == 0)  return run_openaq(ctx, sink);
  if (strcmp(ctx->source_id, "WAQI_GLOBAL") == 0)    return run_waqi(ctx, sink);
  if (strcmp(ctx->source_id, "OPENMETEO_AQ") == 0)   return run_openmeteo(ctx, sink);
  return -1;
}

static const source_def openaq_def = {
  .id = "OPENAQ_GLOBAL", .collector = "osint",
  .name = "OpenAQ Air Quality", .name_ja = "OpenAQ 大気質",
  .update_interval_sec = 0, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.openaq.org",
  .description = "OpenAQ global air-quality network — locations/measurements (v3 needs OPENAQ_KEY; keyless v2 fallback)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(openaq_def)

static const source_def waqi_def = {
  .id = "WAQI_GLOBAL", .collector = "osint",
  .name = "WAQI Air Quality Index", .name_ja = "WAQI 大気質指数",
  .update_interval_sec = 0, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.waqi.info",
  .description = "World Air Quality Index (aqicn.org) — station AQI + pollutants (needs AQICN_TOKEN)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(waqi_def)

static const source_def openmeteo_aq_def = {
  .id = "OPENMETEO_AQ", .collector = "osint",
  .name = "Open-Meteo Air Quality", .name_ja = "Open-Meteo 大気質",
  .update_interval_sec = 0, .run = run,
  .category = "environment", .type = "api",
  .url = "https://air-quality-api.open-meteo.com",
  .description = "Open-Meteo air-quality API (keyless) — current pollutant values + EU/US AQI by coordinate",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(openmeteo_aq_def)
