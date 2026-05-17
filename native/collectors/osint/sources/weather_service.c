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
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

static char *url_encode(const char *s) {
  static const char *unres = "-_.~";
  size_t n = strlen(s);
  char *out = malloc(n * 3 + 1);
  if (!out) return NULL;
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(unres, c))
      out[w++] = (char)c;
    else { sprintf(out + w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
  return out;
}

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
  char *enc = url_encode(loc);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1", enc);
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
  cJSON *first = cJSON_GetArrayItem(json, 0);
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
  char *enc = url_encode(loc);
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
      cJSON *c = cJSON_GetArrayItem(cur, 0);
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
        cJSON *dv = cJSON_GetObjectItem(cJSON_GetArrayItem(da, 0), "value");
        if (dv && cJSON_IsString(dv))
          cJSON_AddStringToObject(result, "description", dv->valuestring);
      }
    }
    cJSON *near = cJSON_GetObjectItem(json, "nearest_area");
    if (near && cJSON_IsArray(near) && cJSON_GetArraySize(near) > 0) {
      cJSON *area = cJSON_GetArrayItem(near, 0);
      cJSON *an = cJSON_GetObjectItem(area, "areaName");
      cJSON *co = cJSON_GetObjectItem(area, "country");
      cJSON *la = cJSON_GetObjectItem(area, "latitude");
      cJSON *lo = cJSON_GetObjectItem(area, "longitude");
      if (an && cJSON_IsArray(an) && cJSON_GetArraySize(an) > 0) {
        cJSON *nm = cJSON_GetObjectItem(cJSON_GetArrayItem(an, 0), "value");
        if (nm && cJSON_IsString(nm))
          cJSON_AddStringToObject(result, "location", nm->valuestring);
      }
      if (co && cJSON_IsArray(co) && cJSON_GetArraySize(co) > 0) {
        cJSON *nm = cJSON_GetObjectItem(cJSON_GetArrayItem(co, 0), "value");
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

static cJSON *get_weather_open_meteo(http_client *http, double lat, double lon) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "open_meteo");
  char url[512];
  snprintf(url, sizeof url,
    "https://api.open-meteo.com/v1/forecast?latitude=%f&longitude=%f&current_weather=true&hourly=temperature_2m,relative_humidity_2m,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min&timezone=auto",
    lat, lon);
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
    cJSON *cur = cJSON_GetObjectItem(json, "current_weather");
    if (cur) {
      cJSON *t = cJSON_GetObjectItem(cur, "temperature");
      cJSON *w = cJSON_GetObjectItem(cur, "windspeed");
      cJSON *wd = cJSON_GetObjectItem(cur, "winddirection");
      cJSON *code = cJSON_GetObjectItem(cur, "weathercode");
      cJSON *tm = cJSON_GetObjectItem(cur, "time");
      if (t) cJSON_AddNumberToObject(result, "temperature_c", t->valuedouble);
      if (w) cJSON_AddNumberToObject(result, "wind_speed_kmh", w->valuedouble);
      if (wd) cJSON_AddNumberToObject(result, "wind_direction", wd->valuedouble);
      if (code) cJSON_AddNumberToObject(result, "weather_code", code->valueint);
      if (tm && tm->valuestring) cJSON_AddStringToObject(result, "time", tm->valuestring);
      int wc = code ? code->valueint : 0;
      const char *desc = "Unknown";
      if (wc == 0) desc = "Clear sky";
      else if (wc == 1) desc = "Mainly clear";
      else if (wc == 2) desc = "Partly cloudy";
      else if (wc == 3) desc = "Overcast";
      else if (wc == 45 || wc == 48) desc = "Fog";
      else if (wc >= 51 && wc <= 55) desc = "Drizzle";
      else if (wc >= 56 && wc <= 57) desc = "Freezing drizzle";
      else if (wc >= 61 && wc <= 65) desc = "Rain";
      else if (wc >= 66 && wc <= 67) desc = "Freezing rain";
      else if (wc >= 71 && wc <= 77) desc = "Snow";
      else if (wc >= 80 && wc <= 82) desc = "Rain showers";
      else if (wc >= 85 && wc <= 86) desc = "Snow showers";
      else if (wc >= 95 && wc <= 99) desc = "Thunderstorm";
      cJSON_AddStringToObject(result, "description", desc);
    }
    cJSON *daily = cJSON_GetObjectItem(json, "daily");
    if (daily) {
      cJSON *tx = cJSON_GetObjectItem(daily, "temperature_2m_max");
      cJSON *tn = cJSON_GetObjectItem(daily, "temperature_2m_min");
      if (tx && cJSON_IsArray(tx) && cJSON_GetArraySize(tx) > 0)
        cJSON_AddNumberToObject(result, "today_max_c",
          cJSON_GetArrayItem(tx, 0)->valuedouble);
      if (tn && cJSON_IsArray(tn) && cJSON_GetArraySize(tn) > 0)
        cJSON_AddNumberToObject(result, "today_min_c",
          cJSON_GetArrayItem(tn, 0)->valuedouble);
    }
    cJSON *tz = cJSON_GetObjectItem(json, "timezone");
    if (tz && cJSON_IsString(tz))
      cJSON_AddStringToObject(result, "timezone", tz->valuestring);
    cJSON_AddNumberToObject(result, "latitude", lat);
    cJSON_AddNumberToObject(result, "longitude", lon);
    cJSON_Delete(json);
  }
  return result;
}

static cJSON *get_weather_owm(http_client *http, const char *loc,
                              const char *key) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "openweathermap");
  if (!key || !*key) { cJSON_AddStringToObject(result, "status", "no_api_key"); return result; }
  char *enc = url_encode(loc);
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
      cJSON *w = cJSON_GetArrayItem(weather, 0);
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

/* result_builder reproduction. */
static void rb_add(cJSON *results, const char *source, int found,
                   int confidence, cJSON *data /*owned or NULL*/,
                   const char *url, int *total, int *fc) {
  cJSON *it = cJSON_CreateObject();
  cJSON_AddStringToObject(it, "source", source);
  cJSON_AddBoolToObject(it, "found", found);
  if (data) cJSON_AddItemToObject(it, "data", data);
  else cJSON_AddNullToObject(it, "data");
  cJSON_AddNumberToObject(it, "confidence", confidence);
  cJSON_AddStringToObject(it, "detection_method", "direct");
  if (url) cJSON_AddStringToObject(it, "url", url);
  cJSON_AddItemToArray(results, it);
  (*total)++;
  if (found) (*fc)++;
}

static int emit_one(intel_sink *sink, const char *svc, const char *entity,
                    const char *qtype, int confidence, cJSON *results,
                    int total, int fc) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", entity ? entity : "");
  cJSON_AddStringToObject(root, "query_type", qtype);
  cJSON_AddItemToObject(root, "results", results);
  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "total_sources", total);
  cJSON_AddNumberToObject(summary, "found_count", fc);
  cJSON *bd = cJSON_CreateObject();
  cJSON_AddNumberToObject(bd, "direct", total);
  cJSON_AddNumberToObject(bd, "llama", 0);
  cJSON_AddItemToObject(summary, "detection_breakdown", bd);
  cJSON_AddItemToObject(root, "summary", summary);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);          /* OSINTsaas: always true */
  cJSON_AddNumberToObject(env, "confidence", confidence);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", entity ? entity : "");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "%s:%s", svc, entity ? entity : qtype);
  char title[360];
  snprintf(title, sizeof title, "%s — %s", svc, entity ? entity : qtype);
  char tags[96];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = "environmental result";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run_weather(const source_ctx *ctx, intel_sink *sink) {
  const char *loc = ctx->entity;
  if (!loc || !*loc) return -1;
  /* OSINTsaas skips IPs and emails. */
  if (is_valid_ip(loc) || strchr(loc, '@'))
    return emit_one(sink, "WEATHER_SERVICE", loc, "weather", 85,
                    cJSON_CreateArray(), 0, 0);

  const char *owm = getenv("OPENWEATHERMAP_API_KEY");
  int has_owm = owm && *owm;
  cJSON *results = cJSON_CreateArray();
  int total = 0, fc = 0;

  double lat = 0, lon = 0;
  int is_coords = (sscanf(loc, "%lf,%lf", &lat, &lon) == 2 ||
                   sscanf(loc, "%lf, %lf", &lat, &lon) == 2);

  cJSON *wttr = get_weather_wttr(ctx->http, loc);
  if (wttr && !cJSON_GetObjectItem(wttr, "status"))
    rb_add(results, "wttr.in", 1, 90, wttr, NULL, &total, &fc);
  else { rb_add(results, "wttr.in", 0, 0, NULL, NULL, &total, &fc);
         if (wttr) cJSON_Delete(wttr); }

  if (is_coords || geocode_location(ctx->http, loc, &lat, &lon)) {
    cJSON *meteo = get_weather_open_meteo(ctx->http, lat, lon);
    if (meteo && !cJSON_GetObjectItem(meteo, "status"))
      rb_add(results, "Open-Meteo", 1, 90, meteo,
             "https://open-meteo.com/", &total, &fc);
    else { rb_add(results, "Open-Meteo", 0, 0, NULL, NULL, &total, &fc);
           if (meteo) cJSON_Delete(meteo); }
  }

  if (has_owm) {
    cJSON *o = get_weather_owm(ctx->http, loc, owm);
    if (o && !cJSON_GetObjectItem(o, "status"))
      rb_add(results, "OpenWeatherMap", 1, 90, o,
             "https://openweathermap.org/", &total, &fc);
    else { rb_add(results, "OpenWeatherMap", 0, 0, NULL, NULL, &total, &fc);
           if (o) cJSON_Delete(o); }
  }

  return emit_one(sink, "WEATHER_SERVICE", loc, "weather", 85,
                  results, total, fc);
}

static const source_def weather_service_def = {
  .id = "WEATHER_SERVICE", .collector = "osint",
  .name = "Weather Service", .name_ja = "気象サービス",
  .update_interval_sec = 0, .run = run_weather,
};
REGISTER_SOURCE(weather_service_def)
