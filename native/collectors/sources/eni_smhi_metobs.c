/* SMHI Swedish meteorological observation network (metobs open data).
 * Endpoint (keyless), one call per parameter:
 *   https://opendata-download-metobs.smhi.se/api/version/latest/parameter/{p}
 *     /station-set/all/period/latest-hour/data.json
 *   p = 1 air temperature, 4 wind speed, 9 air pressure, 21 wind gust
 * Emits one row per (station, parameter) with a reading: value plus the UNIT
 * that the response states ONCE at the top in parameter.unit ("celsius",
 * "meter per second", "hectopascal") — the unit is NOT repeated per reading,
 * so it is read from the header and attached to every row from that call.
 *
 * NULL TRAP: stations whose telemetry is late have value: null — skipped.
 * TIMESTAMP TRAP: station[].from/to are the epoch-ms span of the station's
 * RECORD, not the reading time; the observation time comes from
 * station[].value[].date. `to` is never used as an observation timestamp.
 * STRING TRAP: value[].value is a STRING ("9.5").
 * GEO (R2): station[].latitude/longitude from the response.
 * Licence: CC BY 4.0 (SMHI open data). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "smhi-metobs"

static const struct { const char *id, *slug; } PARAMS[] = {
  { "1",  "air_temperature" },
  { "4",  "wind_speed"      },
  { "9",  "air_pressure"    },
  { "21", "wind_gust"       },
  { NULL, NULL }
};

static int collect_param(const source_ctx *ctx, intel_sink *sink,
                         const char *pid, const char *slug, int *fetched) {
  char url[256];
  snprintf(url, sizeof url,
    "https://opendata-download-metobs.smhi.se/api/version/latest/parameter/%s"
    "/station-set/all/period/latest-hour/data.json", pid);
  cJSON *doc = feed_get_json(ctx->http, url, 40000);
  if (!doc) return 0;
  *fetched = 1;

  /* unit is stated ONCE, at the top */
  cJSON *par = cJSON_GetObjectItem(doc, "parameter");
  const char *unit = par ? jo_sv(par, "unit") : NULL;
  const char *pname = par ? jo_sv(par, "name") : NULL;
  if (!unit) { cJSON_Delete(doc); return 0; }

  cJSON *stations = cJSON_GetObjectItem(doc, "station");
  if (!cJSON_IsArray(stations)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *st;
  cJSON_ArrayForEach(st, stations) {
    const char *name = jo_sv(st, "name");
    cJSON *la = cJSON_GetObjectItem(st, "latitude");
    cJSON *lo = cJSON_GetObjectItem(st, "longitude");
    cJSON *vals = cJSON_GetObjectItem(st, "value");
    if (!name || !cJSON_IsArray(vals) || cJSON_GetArraySize(vals) == 0) continue;
    cJSON *v0 = cJSON_GetArrayItem(vals, cJSON_GetArraySize(vals) - 1);
    const char *vs = jo_sv(v0, "value");
    if (!vs) continue;                    /* late telemetry -> null -> skip */
    char *e = NULL;
    double v = strtod(vs, &e);
    if (e == vs) continue;

    int has_geo = 0; double lat = 0, lon = 0;
    if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
      lat = la->valuedouble; lon = lo->valuedouble;
      if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) has_geo = 1;
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_name", name);
    cJSON *key = cJSON_GetObjectItem(st, "key");
    const char *skey = cJSON_IsString(key) ? key->valuestring : NULL;
    if (skey) cJSON_AddStringToObject(p, "station_key", skey);
    cJSON_AddStringToObject(p, "parameter", slug);
    if (pname) cJSON_AddStringToObject(p, "parameter_name", pname);
    cJSON_AddNumberToObject(p, "value", v);
    cJSON_AddStringToObject(p, "unit", unit);
    /* observation time comes from value[].date (epoch ms), NOT station.to */
    cJSON *dt = cJSON_GetObjectItem(v0, "date");
    if (cJSON_IsNumber(dt))
      cJSON_AddNumberToObject(p, "observed_at_epoch_ms", dt->valuedouble);
    const char *q = jo_sv(v0, "quality");
    if (q) cJSON_AddStringToObject(p, "quality", q);
    cJSON_AddStringToObject(p, "country", "SE");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char rk[160], title[288];
    snprintf(rk, sizeof rk, "%s|%s", skey ? skey : name, slug);
    snprintf(title, sizeof title, "%s: %s = %.2f %s", name, slug, v, unit);

    intel_item row = {0};
    row.remote_key      = rk;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = url;
    row.record_type     = "weather-station";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"weather\",\"sweden\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, fetched = 0;
  for (int i = 0; PARAMS[i].id; i++)
    total += collect_param(ctx, sink, PARAMS[i].id, PARAMS[i].slug, &fetched);
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", total);
  return 0;
}

static const source_def eni_smhi_metobs_def = {
  .id = SRC, .collector = "environment",
  .name = "SMHI Swedish meteorological observation network",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://opendata-download-metobs.smhi.se/api/version/latest/parameter/1/station-set/all/period/latest-hour/data.json",
  .description = "Latest-hour readings from every station in the Swedish national weather observation network (air temperature, wind speed, pressure, gust) with station coordinates and the API's own unit strings.",
  .license = "CC BY 4.0 (SMHI open data).",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_smhi_metobs_def)
