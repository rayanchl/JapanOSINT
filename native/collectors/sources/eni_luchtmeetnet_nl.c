/* RIVM Luchtmeetnet — Dutch national air quality measurements.
 * Endpoints (keyless):
 *   https://api.luchtmeetnet.nl/open_api/measurements?page=1
 *     &order_by=timestamp_measured&order_direction=desc
 *   https://api.luchtmeetnet.nl/open_api/stations/<station_number>
 * Emits one row per measurement returned: value with its UNIT
 *   (ug/m3 for PM10/PM25/NO2/NO/O3/SO2/BC; mg/m3 for CO), the pollutant
 *   formula, the station number and the measurement window.
 *
 * GEO (R2): coordinates are NOT in the measurements response. They are
 * resolved per station from /open_api/stations/{number}
 * (data.geometry.coordinates = [lon, lat]); when that lookup fails the row is
 * emitted WITHOUT geometry — never with a guessed location.
 * Station lookups are bounded (JO_LUCHTMEETNET_STATIONS, default 40).
 * Licence: RIVM Luchtmeetnet open API, keyless, open government data. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "luchtmeetnet-nl"
#define MAXST 128

typedef struct { char id[24]; double lat, lon; int ok; } st_t;

/* ug/m3 for everything the network reports except CO, which is mg/m3. */
static const char *unit_for(const char *formula) {
  if (formula && strcmp(formula, "CO") == 0) return "mg/m3";
  return "ug/m3";
}

static st_t *lookup(const source_ctx *ctx, st_t *cache, int *ncache,
                    int budget, const char *num) {
  for (int i = 0; i < *ncache; i++)
    if (strcmp(cache[i].id, num) == 0) return &cache[i];
  if (*ncache >= MAXST || *ncache >= budget) return NULL;
  st_t *e = &cache[(*ncache)++];
  snprintf(e->id, sizeof e->id, "%s", num);
  e->ok = 0;
  char url[192];
  snprintf(url, sizeof url, "https://api.luchtmeetnet.nl/open_api/stations/%s", num);
  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc) return e;
  cJSON *d = cJSON_GetObjectItem(doc, "data");
  cJSON *g = d ? cJSON_GetObjectItem(d, "geometry") : NULL;
  cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
  if (cJSON_IsArray(c) && cJSON_GetArraySize(c) >= 2) {
    cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
    if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
      e->lon = x->valuedouble; e->lat = y->valuedouble;   /* [lon, lat] */
      if (e->lat >= -90 && e->lat <= 90 && e->lon >= -180 && e->lon <= 180 &&
          (e->lat != 0 || e->lon != 0)) e->ok = 1;
    }
  }
  cJSON_Delete(doc);
  return e;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int budget = 40;
  const char *env = getenv("JO_LUCHTMEETNET_STATIONS");
  if (env && *env) { int b = atoi(env); if (b > 0 && b <= MAXST) budget = b; }

  cJSON *doc = feed_get_json(ctx->http,
    "https://api.luchtmeetnet.nl/open_api/measurements?page=1"
    "&order_by=timestamp_measured&order_direction=desc", 30000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *data = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(data)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no data[]\n"); return -1; }

  st_t cache[MAXST]; int ncache = 0;
  int n = 0;
  cJSON *m;
  cJSON_ArrayForEach(m, data) {
    const char *num = jo_sv(m, "station_number");
    const char *formula = jo_sv(m, "formula");
    cJSON *val = cJSON_GetObjectItem(m, "value");
    const char *when = jo_sv(m, "timestamp_measured");
    if (!num || !formula || !cJSON_IsNumber(val)) continue;

    st_t *st = lookup(ctx, cache, &ncache, budget, num);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_number", num);
    cJSON_AddStringToObject(p, "pollutant", formula);
    cJSON_AddNumberToObject(p, "value", val->valuedouble);
    cJSON_AddStringToObject(p, "unit", unit_for(formula));
    if (when) cJSON_AddStringToObject(p, "timestamp_measured", when);
    const char *ws = jo_sv(m, "timestamp_measured_start");
    const char *we = jo_sv(m, "timestamp_measured_end");
    if (ws) cJSON_AddStringToObject(p, "window_start", ws);
    if (we) cJSON_AddStringToObject(p, "window_end", we);
    cJSON_AddStringToObject(p, "country", "NL");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[96], title[224];
    snprintf(key, sizeof key, "%s|%s|%s", num, formula, when ? when : "");
    snprintf(title, sizeof title, "%s %s = %.2f %s", num, formula,
             val->valuedouble, unit_for(formula));

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = "https://www.luchtmeetnet.nl/";
    row.record_type     = "air-quality-measurement";
    if (st && st->ok) { row.has_geo = 1; row.lat = st->lat; row.lon = st->lon; }
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"air-quality\",\"netherlands\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_luchtmeetnet_nl_def = {
  .id = SRC, .collector = "environment",
  .name = "RIVM Luchtmeetnet Netherlands air quality",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.luchtmeetnet.nl/open_api/measurements",
  .description = "Hourly regulatory air quality measurements (PM10, PM2.5, NO2, NO, O3, SO2, black carbon) from the Dutch national network, with station coordinates resolved from the stations endpoint.",
  .license = "RIVM Luchtmeetnet open API, keyless, open government data.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_luchtmeetnet_nl_def)
