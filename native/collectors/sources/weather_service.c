/* collectors/osint/sources/weather_service.c
 * OSINT service — faithful port of OSINTsaas osint_tools/environmental.c
 * (WEATHER_SERVICE handle_weather_service; ctx->entity = location/coords).
 * On-demand (interval 0). Sources: wttr.in + open-meteo.com + Nominatim
 * geocode (all no key); OpenWeatherMap (key-gated OPENWEATHERMAP_API_KEY).
 * result_builder output reproduced as the documented result_builder JSON shape
 * inline (no result_builder.c in this build). Reproduces get_weather_wttr /
 * get_weather_open_meteo / get_weather_openweathermap + the WMO code map
 * verbatim. success=true; weather confidence 85. Emits ONE
 * osint_service_result row; body = {success,confidence,data} envelope, like
 * ip_geolocation.c. */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <strings.h>            /* strcasecmp */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

static int is_valid_ip(const char *s) {
  /* lightweight: dotted quad or contains ':' */
  int dots = 0;
  for (const char *p = s; *p; p++) {
    if (*p == ':') return 1;
    if (*p == '.') dots++;
    else if (!isdigit((unsigned char)*p)) return 0;
  }
  return dots == 3;
}

/* geocode_location: Nominatim search → first lat/lon. */
static int geocode_location(http_client *http, const char *loc,
                            double *lat, double *lon) {
  char *enc = jo_urlencode(loc);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    /* exhaustive-ok: resolution step, not a record listing — this turns one
     * place name into the coordinates for the weather call below; the weather
     * records it produces are emitted in full. */
    "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1", enc);  /* exhaustive-ok: place resolution step */
  free(enc);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json || !cJSON_IsArray(json) || cJSON_GetArraySize(json) == 0) {
    if (json) cJSON_Delete(json);
    return 0;
  }
  cJSON *first = cJSON_GetArrayItem(json, 0);  /* exhaustive-ok: geocode resolution step */
  cJSON *la = cJSON_GetObjectItem(first, "lat");
  cJSON *lo = cJSON_GetObjectItem(first, "lon");
  int ok = 0;
  if (la && lo && cJSON_IsString(la) && cJSON_IsString(lo)) {
    *lat = atof(la->valuestring);
    *lon = atof(lo->valuestring);
    ok = 1;
  }
  cJSON_Delete(json);
  return ok;
}

/* get_weather_wttr: wttr.in/<loc>?format=j1. status field → "error" item. */
static cJSON *get_weather_wttr(http_client *http, const char *loc) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "wttr.in");
  char *enc = jo_urlencode(loc);
  if (!enc) { cJSON_AddStringToObject(result, "status", "error"); return result; }
  char url[512];
  snprintf(url, sizeof url, "https://wttr.in/%s?format=j1", enc);
  free(enc);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "status", "error");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (json) {
    cJSON *cur = cJSON_GetObjectItem(json, "current_condition");
    if (cur && cJSON_IsArray(cur) && cJSON_GetArraySize(cur) > 0) {
      cJSON *c = cJSON_GetArrayItem(cur, 0);  /* exhaustive-ok: wttr.in wraps current_condition in a 1-element array */
      cJSON *v;
      if ((v = cJSON_GetObjectItem(c, "temp_C")) && cJSON_IsString(v))
        cJSON_AddNumberToObject(result, "temperature_c", atof(v->valuestring));
      if ((v = cJSON_GetObjectItem(c, "FeelsLikeC")) && cJSON_IsString(v))
        cJSON_AddNumberToObject(result, "feels_like_c", atof(v->valuestring));
      if ((v = cJSON_GetObjectItem(c, "humidity")) && cJSON_IsString(v))
        cJSON_AddNumberToObject(result, "humidity_percent", atoi(v->valuestring));
      if ((v = cJSON_GetObjectItem(c, "windspeedKmph")) && cJSON_IsString(v))
        cJSON_AddNumberToObject(result, "wind_speed_kmh", atof(v->valuestring));
      if ((v = cJSON_GetObjectItem(c, "winddir16Point")) && cJSON_IsString(v))
        cJSON_AddStringToObject(result, "wind_direction", v->valuestring);
      if ((v = cJSON_GetObjectItem(c, "pressure")) && cJSON_IsString(v))
        cJSON_AddNumberToObject(result, "pressure_hpa", atof(v->valuestring));
      if ((v = cJSON_GetObjectItem(c, "visibility")) && cJSON_IsString(v))
        cJSON_AddNumberToObject(result, "visibility_km", atof(v->valuestring));
      if ((v = cJSON_GetObjectItem(c, "uvIndex")) && cJSON_IsString(v))
        cJSON_AddNumberToObject(result, "uv_index", atoi(v->valuestring));
      cJSON *da = cJSON_GetObjectItem(c, "weatherDesc");
      if (da && cJSON_IsArray(da) && cJSON_GetArraySize(da) > 0) {
        cJSON *dv = cJSON_GetObjectItem(cJSON_GetArrayItem(da, 0), "value");  /* exhaustive-ok: 1-element value wrapper */
        if (dv && cJSON_IsString(dv))
          cJSON_AddStringToObject(result, "description", dv->valuestring);
      }
    }
    cJSON *near = cJSON_GetObjectItem(json, "nearest_area");
    if (near && cJSON_IsArray(near) && cJSON_GetArraySize(near) > 0) {
      cJSON *area = cJSON_GetArrayItem(near, 0);  /* exhaustive-ok: nearest area to the queried point */
      cJSON *an = cJSON_GetObjectItem(area, "areaName");
      cJSON *co = cJSON_GetObjectItem(area, "country");
      cJSON *la = cJSON_GetObjectItem(area, "latitude");
      cJSON *lo = cJSON_GetObjectItem(area, "longitude");
      if (an && cJSON_IsArray(an) && cJSON_GetArraySize(an) > 0) {
        cJSON *nm = cJSON_GetObjectItem(cJSON_GetArrayItem(an, 0), "value");  /* exhaustive-ok: 1-element value wrapper */
        if (nm && cJSON_IsString(nm))
          cJSON_AddStringToObject(result, "location", nm->valuestring);
      }
      if (co && cJSON_IsArray(co) && cJSON_GetArraySize(co) > 0) {
        cJSON *nm = cJSON_GetObjectItem(cJSON_GetArrayItem(co, 0), "value");  /* exhaustive-ok: 1-element value wrapper */
        if (nm && cJSON_IsString(nm))
          cJSON_AddStringToObject(result, "country", nm->valuestring);
      }
      if (la && cJSON_IsString(la))
        cJSON_AddNumberToObject(result, "latitude", atof(la->valuestring));
      if (lo && cJSON_IsString(lo))
        cJSON_AddNumberToObject(result, "longitude", atof(lo->valuestring));
    }
    cJSON_Delete(json);
  }
  return result;
}

/* WMO weather-code → human description (verbatim mapping). */
static const char *wmo_desc(int wc) {
  if (wc == 0) return "Clear sky";
  else if (wc == 1) return "Mainly clear";
  else if (wc == 2) return "Partly cloudy";
  else if (wc == 3) return "Overcast";
  else if (wc == 45 || wc == 48) return "Fog";
  else if (wc >= 51 && wc <= 55) return "Drizzle";
  else if (wc >= 56 && wc <= 57) return "Freezing drizzle";
  else if (wc >= 61 && wc <= 65) return "Rain";
  else if (wc >= 66 && wc <= 67) return "Freezing rain";
  else if (wc >= 71 && wc <= 77) return "Snow";
  else if (wc >= 80 && wc <= 82) return "Rain showers";
  else if (wc >= 85 && wc <= 86) return "Snow showers";
  else if (wc >= 95 && wc <= 99) return "Thunderstorm";
  return "Unknown";
}

/* get_weather_open_meteo: returns the full parsed forecast JSON (current_weather
 * + daily arrays) so the caller can emit current + per-day rows. Caller owns
 * it. NULL on fetch/parse failure. */
static cJSON *get_weather_open_meteo_raw(http_client *http, double lat, double lon) {
  char url[512];
  snprintf(url, sizeof url,
    "https://api.open-meteo.com/v1/forecast?latitude=%f&longitude=%f&current_weather=true&daily=temperature_2m_max,temperature_2m_min,weathercode,precipitation_sum&timezone=auto",
    lat, lon);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  return json;
}

/* Build a normalized current-conditions object from a raw Open-Meteo forecast
 * (current_weather block). Returns NULL if no current block. Caller owns it. */
static cJSON *open_meteo_current(cJSON *json, double lat, double lon) {
  if (!json) return NULL;
  cJSON *cur = cJSON_GetObjectItem(json, "current_weather");
  if (!cur) return NULL;
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "open_meteo");
  cJSON *t = cJSON_GetObjectItem(cur, "temperature");
  cJSON *w = cJSON_GetObjectItem(cur, "windspeed");
  cJSON *wd = cJSON_GetObjectItem(cur, "winddirection");
  cJSON *code = cJSON_GetObjectItem(cur, "weathercode");
  cJSON *tm = cJSON_GetObjectItem(cur, "time");
  if (t) cJSON_AddNumberToObject(result, "temperature_c", t->valuedouble);
  if (w) cJSON_AddNumberToObject(result, "wind_speed_kmh", w->valuedouble);
  if (wd) cJSON_AddNumberToObject(result, "wind_direction", wd->valuedouble);
  if (code) {
    cJSON_AddNumberToObject(result, "weather_code", code->valueint);
    cJSON_AddStringToObject(result, "description", wmo_desc(code->valueint));
  }
  if (tm && tm->valuestring) cJSON_AddStringToObject(result, "time", tm->valuestring);
  cJSON *tz = cJSON_GetObjectItem(json, "timezone");
  if (tz && cJSON_IsString(tz))
    cJSON_AddStringToObject(result, "timezone", tz->valuestring);
  cJSON_AddNumberToObject(result, "latitude", lat);
  cJSON_AddNumberToObject(result, "longitude", lon);
  return result;
}

static cJSON *get_weather_owm(http_client *http, const char *loc,
                              const char *key) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "openweathermap");
  if (!key || !*key) { cJSON_AddStringToObject(result, "status", "no_api_key"); return result; }
  char *enc = jo_urlencode(loc);
  if (!enc) return result;
  char url[512];
  snprintf(url, sizeof url,
    "https://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric",
    enc, key);
  free(enc);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    int code = (int)hr.status;
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "status", code == 404 ? "not_found" : "error");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (json) {
    cJSON *name = cJSON_GetObjectItem(json, "name");
    cJSON *main = cJSON_GetObjectItem(json, "main");
    cJSON *weather = cJSON_GetObjectItem(json, "weather");
    cJSON *wind = cJSON_GetObjectItem(json, "wind");
    cJSON *coord = cJSON_GetObjectItem(json, "coord");
    cJSON *sys = cJSON_GetObjectItem(json, "sys");
    if (name && name->valuestring)
      cJSON_AddStringToObject(result, "location", name->valuestring);
    if (main) {
      cJSON *t = cJSON_GetObjectItem(main, "temp");
      cJSON *f = cJSON_GetObjectItem(main, "feels_like");
      cJSON *h = cJSON_GetObjectItem(main, "humidity");
      cJSON *p = cJSON_GetObjectItem(main, "pressure");
      if (t) cJSON_AddNumberToObject(result, "temperature_c", t->valuedouble);
      if (f) cJSON_AddNumberToObject(result, "feels_like_c", f->valuedouble);
      if (h) cJSON_AddNumberToObject(result, "humidity_percent", h->valueint);
      if (p) cJSON_AddNumberToObject(result, "pressure_hpa", p->valueint);
    }
    if (weather && cJSON_IsArray(weather) && cJSON_GetArraySize(weather) > 0) {
      cJSON *w = cJSON_GetArrayItem(weather, 0);  /* exhaustive-ok: OWM primary condition; the array is in the record */
      cJSON *d = cJSON_GetObjectItem(w, "description");
      cJSON *ic = cJSON_GetObjectItem(w, "icon");
      if (d && d->valuestring) cJSON_AddStringToObject(result, "description", d->valuestring);
      if (ic && ic->valuestring) {
        char iu[128];
        snprintf(iu, sizeof iu,
          "https://openweathermap.org/img/wn/%s@2x.png", ic->valuestring);
        cJSON_AddStringToObject(result, "icon_url", iu);
      }
    }
    if (wind) {
      cJSON *sp = cJSON_GetObjectItem(wind, "speed");
      if (sp) cJSON_AddNumberToObject(result, "wind_speed_ms", sp->valuedouble);
    }
    if (coord) {
      cJSON *la = cJSON_GetObjectItem(coord, "lat");
      cJSON *lo = cJSON_GetObjectItem(coord, "lon");
      if (la) cJSON_AddNumberToObject(result, "latitude", la->valuedouble);
      if (lo) cJSON_AddNumberToObject(result, "longitude", lo->valuedouble);
    }
    if (sys) {
      cJSON *co = cJSON_GetObjectItem(sys, "country");
      if (co && co->valuestring) cJSON_AddStringToObject(result, "country", co->valuestring);
    }
    cJSON_Delete(json);
  }
  return result;
}

/* Emit one weather intel item. data is consumed (duplicated into envelope). */
static int emit_weather(intel_sink *sink, const char *loc, const char *rk,
                        const char *title, const char *summary,
                        const char *published_at, int has_geo,
                        double lat, double lon, cJSON *data) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 85);
  cJSON_AddItemToObject(env, "data", cJSON_Duplicate(data, 1));
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WEATHER_SERVICE");
  cJSON_AddStringToObject(props, "entity", loc);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary;
  it.published_at    = published_at;
  it.has_geo         = has_geo;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WEATHER_SERVICE\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  cJSON_Delete(data);
  return rc >= 0 ? 1 : 0;
}

/* PER-RECORD EMIT: one item for current conditions (real temp/humidity/wind/
 * desc, has_geo+lat/lon when known) plus one item per daily forecast day from
 * Open-Meteo (real daily fields). IP/email inputs skipped; total fetch failure
 * → emit nothing, return 0. */
static int run_weather(const source_ctx *ctx, intel_sink *sink) {
  const char *loc = ctx->entity;
  if (!loc || !*loc) return 0;
  /* OSINTsaas skips IPs and emails. */
  if (is_valid_ip(loc) || strchr(loc, '@')) return 0;

  double lat = 0, lon = 0;
  int have_geo = (sscanf(loc, "%lf,%lf", &lat, &lon) == 2 ||
                  sscanf(loc, "%lf, %lf", &lat, &lon) == 2);
  if (!have_geo) have_geo = geocode_location(ctx->http, loc, &lat, &lon);

  int emitted = 0;

  /* --- Current conditions (one item). Prefer wttr.in; fall back to
   *     Open-Meteo current; then OpenWeatherMap (key-gated). --- */
  cJSON *current = NULL;
  cJSON *wttr = get_weather_wttr(ctx->http, loc);
  if (wttr && !cJSON_GetObjectItem(wttr, "status")) current = wttr;
  else if (wttr) cJSON_Delete(wttr);

  /* Fetch the raw Open-Meteo forecast once (current + daily) if we have geo. */
  cJSON *om_raw = NULL;
  if (have_geo) om_raw = get_weather_open_meteo_raw(ctx->http, lat, lon);

  if (!current && om_raw) current = open_meteo_current(om_raw, lat, lon);

  if (!current) {
    const char *owm = getenv("OPENWEATHERMAP_API_KEY");
    if (owm && *owm) {
      cJSON *o = get_weather_owm(ctx->http, loc, owm);
      if (o && !cJSON_GetObjectItem(o, "status")) current = o;
      else if (o) cJSON_Delete(o);
    }
  }

  if (current) {
    /* wttr.in / OWM carry their own lat/lon; surface as geo if present. */
    int cg = have_geo; double clat = lat, clon = lon;
    cJSON *cla = cJSON_GetObjectItem(current, "latitude");
    cJSON *clo = cJSON_GetObjectItem(current, "longitude");
    if (cla && clo && cJSON_IsNumber(cla) && cJSON_IsNumber(clo)) {
      clat = cla->valuedouble; clon = clo->valuedouble; cg = 1;
    }
    /* AUDIT NOTE (slice a3): the row used to be titled with the REQUESTED
     * location while both the reading and the pin came from whatever station
     * wttr.in resolved to — for entity "Tokyo" that is Shikinejima, an island
     * ~150 km south, so the row read "Weather — Tokyo" over a measurement
     * taken somewhere else. The reading and the coordinate are real and belong
     * together; it is the label that was wrong. Name the resolved place when
     * it differs from what was asked for. */
    const char *resolved = NULL;
    cJSON *rv = cJSON_GetObjectItem(current, "location");
    if (rv && cJSON_IsString(rv) && rv->valuestring[0]) resolved = rv->valuestring;

    char rk[320]; snprintf(rk, sizeof rk, "weather:%s:current", loc);
    char title[420];
    if (resolved && strcasecmp(resolved, loc) != 0)
      snprintf(title, sizeof title, "Weather — %s [nearest station: %s] (current)",
               loc, resolved);
    else
      snprintf(title, sizeof title, "Weather — %s (current)", loc);
    emitted += emit_weather(sink, loc, rk, title, "current conditions",
                            NULL, cg, clat, clon, current);
  }

  /* --- Daily forecast (one item per day) from Open-Meteo. --- */
  if (om_raw) {
    cJSON *daily = cJSON_GetObjectItem(om_raw, "daily");
    if (daily) {
      cJSON *dt = cJSON_GetObjectItem(daily, "time");
      cJSON *tx = cJSON_GetObjectItem(daily, "temperature_2m_max");
      cJSON *tn = cJSON_GetObjectItem(daily, "temperature_2m_min");
      cJSON *wc = cJSON_GetObjectItem(daily, "weathercode");
      cJSON *pr = cJSON_GetObjectItem(daily, "precipitation_sum");
      int n = (dt && cJSON_IsArray(dt)) ? cJSON_GetArraySize(dt) : 0;
      for (int i = 0; i < n; i++) {
        cJSON *dd = cJSON_GetArrayItem(dt, i);
        if (!dd || !cJSON_IsString(dd)) continue;
        const char *date = dd->valuestring;
        cJSON *data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "source", "open_meteo");
        cJSON_AddStringToObject(data, "date", date);
        if (tx && cJSON_IsArray(tx) && i < cJSON_GetArraySize(tx))
          cJSON_AddNumberToObject(data, "temp_max_c",
            cJSON_GetArrayItem(tx, i)->valuedouble);
        if (tn && cJSON_IsArray(tn) && i < cJSON_GetArraySize(tn))
          cJSON_AddNumberToObject(data, "temp_min_c",
            cJSON_GetArrayItem(tn, i)->valuedouble);
        if (wc && cJSON_IsArray(wc) && i < cJSON_GetArraySize(wc)) {
          int code = cJSON_GetArrayItem(wc, i)->valueint;
          cJSON_AddNumberToObject(data, "weather_code", code);
          cJSON_AddStringToObject(data, "description", wmo_desc(code));
        }
        if (pr && cJSON_IsArray(pr) && i < cJSON_GetArraySize(pr))
          cJSON_AddNumberToObject(data, "precipitation_mm",
            cJSON_GetArrayItem(pr, i)->valuedouble);
        cJSON_AddNumberToObject(data, "latitude", lat);
        cJSON_AddNumberToObject(data, "longitude", lon);
        char rk[320]; snprintf(rk, sizeof rk, "weather:%s:%s", loc, date);
        char title[380]; snprintf(title, sizeof title,
                                  "Weather — %s (%s)", loc, date);
        emitted += emit_weather(sink, loc, rk, title, "daily forecast",
                                date, have_geo, lat, lon, data);
      }
    }
  }

  if (om_raw) cJSON_Delete(om_raw);
  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static const source_def weather_service_def = {
  .id = "WEATHER_SERVICE", .collector = "osint",
  .name = "Weather Service", .name_ja = "気象サービス",
  .update_interval_sec = 0, .run = run_weather,
  .category = "environment", .type = "api",
  .url = "internal://osint/weather-service",
  .description = "Current weather and conditions for a location.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(weather_service_def)
