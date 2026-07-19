/* collectors/sources/geo_world2.c
 * OSINT services — net-new keyless geospatial helpers useful for image/scene
 * geolocation, pivot on ctx->entity = "lat,lon". Real fetch or honest-empty.
 *
 *   SUN_TIMES         api.sunrise-sunset.org/json?lat=&lng=   (shadow analysis)
 *   ELEVATION_LOOKUP  api.open-elevation.com/api/v1/lookup    (terrain elevation) */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *GW_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int parse_latlon(const char *s, double *lat, double *lon) {
  return sscanf(s, "%lf , %lf", lat, lon) == 2 || sscanf(s, "%lf/%lf", lat, lon) == 2;
}

static int run_suntimes(const source_ctx *ctx, intel_sink *sink, double lat, double lon) {
  char url[320];
  snprintf(url, sizeof url,
    "https://api.sunrise-sunset.org/json?lat=%.6f&lng=%.6f&formatted=0", lat, lon);
  const char *hdrs[] = { "Accept: application/json", GW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "SUN_TIMES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *status = jo_sv(root, "status");
  const cJSON *res = cJSON_GetObjectItem(root, "results");
  if (!res || (status && strcmp(status, "OK") != 0)) { cJSON_Delete(root); return 0; }
  const char *sunrise = jo_sv(res, "sunrise");
  const char *sunset = jo_sv(res, "sunset");
  const char *noon = jo_sv(res, "solar_noon");
  const char *daylen = jo_sv(res, "day_length");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddNumberToObject(d, "lat", lat); cJSON_AddNumberToObject(d, "lon", lon);
  if (sunrise) cJSON_AddStringToObject(d, "sunrise_utc", sunrise);
  if (sunset) cJSON_AddStringToObject(d, "sunset_utc", sunset);
  if (noon) cJSON_AddStringToObject(d, "solar_noon_utc", noon);
  if (daylen) cJSON_AddStringToObject(d, "day_length_s", daylen);
  cJSON_AddStringToObject(d, "source", "sunrise-sunset.org");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "SUN_TIMES");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char rk[96], title[160];
  snprintf(rk, sizeof rk, "sun:%.4f,%.4f", lat, lon);
  snprintf(title, sizeof title, "Sun times @ %.4f,%.4f", lat, lon);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.summary = sunrise;
  it.body = bj; it.lang = "en"; it.has_geo = 1; it.lat = lat; it.lon = lon;
  it.record_type = "sun-times";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"SUN_TIMES\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run_elevation(const source_ctx *ctx, intel_sink *sink, double lat, double lon) {
  char url[320];
  snprintf(url, sizeof url,
    "https://api.open-elevation.com/api/v1/lookup?locations=%.6f,%.6f", lat, lon);
  const char *hdrs[] = { "Accept: application/json", GW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "ELEVATION_LOOKUP");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *res = cJSON_GetObjectItem(root, "results");
  const cJSON *r0 = cJSON_IsArray(res) ? cJSON_GetArrayItem(res, 0) : NULL;
  const cJSON *elev = r0 ? cJSON_GetObjectItem(r0, "elevation") : NULL;
  if (!elev || !cJSON_IsNumber(elev)) { cJSON_Delete(root); return 0; }
  cJSON *d = cJSON_CreateObject();
  cJSON_AddNumberToObject(d, "lat", lat); cJSON_AddNumberToObject(d, "lon", lon);
  cJSON_AddNumberToObject(d, "elevation_m", elev->valuedouble);
  cJSON_AddStringToObject(d, "source", "Open-Elevation");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "ELEVATION_LOOKUP");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char rk[96], title[160];
  snprintf(rk, sizeof rk, "elev:%.4f,%.4f", lat, lon);
  snprintf(title, sizeof title, "Elevation %.0f m @ %.4f,%.4f", elev->valuedouble, lat, lon);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.lang = "en";
  it.has_geo = 1; it.lat = lat; it.lon = lon;
  it.record_type = "elevation";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"ELEVATION_LOOKUP\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  double lat = 0, lon = 0;
  if (!parse_latlon(q, &lat, &lon)) {
    fprintf(stderr, "[%s] entity must be 'lat,lon'\n", sid);
    return 0;
  }
  int n = 0;
  if      (!strcmp(sid, "SUN_TIMES"))        n = run_suntimes(ctx, sink, lat, lon);
  else if (!strcmp(sid, "ELEVATION_LOOKUP")) n = run_elevation(ctx, sink, lat, lon);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define GW_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "geospatial", \
    .type = "api", .url = "internal://osint/geo", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

GW_DEF(gw_sun_def, "SUN_TIMES", "Sunrise/Sunset Times",
  "Keyless sun times for lat,lon (sunrise/sunset/solar-noon) for shadow geolocation.");
GW_DEF(gw_elev_def, "ELEVATION_LOOKUP", "Terrain Elevation",
  "Keyless terrain elevation (metres) for a lat,lon via Open-Elevation.");
