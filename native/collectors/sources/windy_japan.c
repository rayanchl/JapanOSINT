/* collectors/environment/sources/windy_japan.c
 * FEED source — port of server/src/collectors/windyJapan.js.
 * Gated on WINDY_API_KEY: POST api.windy.com point-forecast over a sparse
 * Japan city grid -> FeatureCollection. No key / no data -> honest empty. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define POINT_FORECAST_URL "https://api.windy.com/api/point-forecast/v2"
#define TIMEOUT_MS 15000

struct grid_pt { const char *name; double lat, lon; };
static const struct grid_pt GRID[] = {
  { "Sapporo",   43.06, 141.35 },
  { "Sendai",    38.27, 140.87 },
  { "Tokyo",     35.69, 139.69 },
  { "Nagoya",    35.18, 136.91 },
  { "Osaka",     34.69, 135.50 },
  { "Hiroshima", 34.40, 132.46 },
  { "Fukuoka",   33.59, 130.40 },
  { "Naha",      26.21, 127.68 },
};
#define GRID_N ((int)(sizeof(GRID)/sizeof(GRID[0])))

/* data['key-surface'][0] as a finite number, or not-present. */
static int arr0_num(cJSON *d, const char *key, double *out) {
  cJSON *a = cJSON_GetObjectItem(d, key);
  if (!a || !cJSON_IsArray(a) || cJSON_GetArraySize(a) == 0) return 0;
  cJSON *v = cJSON_GetArrayItem(a, 0);  /* exhaustive-ok: point forecast: index 0 is the current step of the queried cell */
  if (!v || !cJSON_IsNumber(v)) return 0;
  *out = v->valuedouble;
  return 1;
}

/* new Date(ms).toISOString() */
static void ms_to_iso(double ms, char *out, size_t n) {
  time_t t = (time_t)(ms / 1000.0);
  struct tm g; gmtime_r(&t, &g);
  int frac = (int)(fmod(ms, 1000.0));
  char base[32];
  strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &g);
  snprintf(out, n, "%s.%03dZ", base, frac < 0 ? 0 : frac);
}

static cJSON *fetch_point(http_client *http, const char *key,
                          const struct grid_pt *p) {
  cJSON *req = cJSON_CreateObject();
  cJSON_AddNumberToObject(req, "lat", p->lat);
  cJSON_AddNumberToObject(req, "lon", p->lon);
  cJSON_AddStringToObject(req, "model", "gfs");
  cJSON *params = cJSON_CreateArray();
  cJSON_AddItemToArray(params, cJSON_CreateString("temp"));
  cJSON_AddItemToArray(params, cJSON_CreateString("wind"));
  cJSON_AddItemToArray(params, cJSON_CreateString("pressure"));
  cJSON_AddItemToObject(req, "parameters", params);
  cJSON *levels = cJSON_CreateArray();
  cJSON_AddItemToArray(levels, cJSON_CreateString("surface"));
  cJSON_AddItemToObject(req, "levels", levels);
  cJSON_AddStringToObject(req, "key", key);
  char *body = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);

  const char *hdrs[] = { "Content-Type: application/json", NULL };
  cJSON *data = feed_post_json(http, POINT_FORECAST_URL, body, hdrs, TIMEOUT_MS);
  free(body);
  if (!data) return NULL;

  cJSON *err = cJSON_GetObjectItem(data, "error");
  cJSON *ts = cJSON_GetObjectItem(data, "ts");
  if (err || !ts || !cJSON_IsArray(ts) || cJSON_GetArraySize(ts) == 0) {
    cJSON_Delete(data); return NULL;
  }
  cJSON *ts0 = cJSON_GetArrayItem(ts, 0);  /* exhaustive-ok: timestamp of the current step */
  double tsms = (ts0 && cJSON_IsNumber(ts0)) ? ts0->valuedouble : 0;

  double tK, u, v, press;
  int hasT = arr0_num(data, "temp-surface", &tK);
  int hasU = arr0_num(data, "wind_u-surface", &u);
  int hasV = arr0_num(data, "wind_v-surface", &v);
  int hasP = arr0_num(data, "pressure-surface", &press);

  cJSON *f = gj_point_feature(p->lon, p->lat);

  cJSON *pr = cJSON_CreateObject();              /* EXACT JS key order */
  cJSON_AddStringToObject(pr, "city", p->name);
  char iso[40]; ms_to_iso(tsms, iso, sizeof iso);
  cJSON_AddStringToObject(pr, "forecast_time", iso);
  if (hasT) cJSON_AddNumberToObject(pr, "temp_c",
              round((tK - 273.15) * 10) / 10);
  else cJSON_AddNullToObject(pr, "temp_c");
  if (hasU && hasV) {
    double windMs = sqrt(u * u + v * v);
    cJSON_AddNumberToObject(pr, "wind_ms", round(windMs * 10) / 10);
  } else cJSON_AddNullToObject(pr, "wind_ms");
  if (hasP) cJSON_AddNumberToObject(pr, "pressure_pa", press);
  else cJSON_AddNullToObject(pr, "pressure_pa");
  cJSON_AddStringToObject(pr, "model", "gfs");
  cJSON_AddStringToObject(pr, "source", "windy_point_forecast");
  cJSON_AddItemToObject(f, "properties", pr);

  cJSON_Delete(data);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("WINDY_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[windy-japan] gated (no WINDY_API_KEY)\n");
    return 0;
  }
  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < GRID_N; i++) {
    cJSON *f = fetch_point(ctx->http, key, &GRID[i]);
    if (f) cJSON_AddItemToArray(features, f);
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[windy-japan] emitted %d\n", n);
  return 0;              /* audit-09: an empty result set is not a run error */
}

static const source_def windy_japan_def = {
  .id = "windy-japan", .collector = "environment",
  .name = "Windy Japan", .name_ja = "Windy 日本",
  .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(windy_japan_def)
