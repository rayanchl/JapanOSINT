/* Thailand PCD Air4Thai national air quality network.
 * Endpoint: https://air4thai.pcd.go.th/services/getNewAQI_JSON.php     (keyless)
 * (The publisher advertises the plain http:// URL; it 301-redirects to https,
 * which is what is requested here.)
 * Emits one row per (station, pollutant) with a real reading: the CONCENTRATION
 * from `value` — ug/m3 for PM2.5 / PM10 / O3 / NO2 / SO2 and ppm for CO — with
 * the pollutant's own AQI kept alongside as a separate INDEX field. An index is
 * never presented as a concentration.
 *
 * SENTINEL TRAP, quoted from the source survey: "MISSING-DATA SENTINEL IS -1,
 * NOT NULL: an unavailable pollutant reports {"color_id":"0","aqi":"-1",
 * "value":"-1"}. Emitting -1 as a concentration is a silent data-quality bug -
 * reject any value <= -1."
 * STRING TRAP: lat, long, value and aqi are ALL strings.
 * GEO (R2): lat/long come from the station record itself.
 * Licence: Thai PCD Air4Thai public service, keyless. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "air4thai-stations"

static const struct { const char *key, *unit; } POLL[] = {
  { "PM25", "ug/m3" }, { "PM10", "ug/m3" }, { "O3", "ug/m3" },
  { "NO2",  "ug/m3" }, { "SO2",  "ug/m3" }, { "CO", "ppm"   },
  { NULL, NULL }
};

static int numstr(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring[0]) {
    char *e = NULL; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http,
    "https://air4thai.pcd.go.th/services/getNewAQI_JSON.php", 40000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *stations = cJSON_GetObjectItem(doc, "stations");
  if (!cJSON_IsArray(stations)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no stations[]\n"); return -1; }

  int n = 0;
  cJSON *st;
  cJSON_ArrayForEach(st, stations) {
    const char *sid = jo_sv(st, "stationID");
    const char *name = jo_sv(st, "nameEN");
    const char *area = jo_sv(st, "areaEN");
    if (!sid) continue;
    double lat, lon;
    int has_geo = numstr(st, "lat", &lat) && numstr(st, "long", &lon);
    if (has_geo && (lat < -90 || lat > 90 || lon < -180 || lon > 180 ||
                    (lat == 0 && lon == 0))) has_geo = 0;

    cJSON *last = cJSON_GetObjectItem(st, "AQILast");
    if (!cJSON_IsObject(last)) continue;
    const char *date = jo_sv(last, "date"), *tm = jo_sv(last, "time");

    for (int i = 0; POLL[i].key; i++) {
      cJSON *po = cJSON_GetObjectItem(last, POLL[i].key);
      if (!cJSON_IsObject(po)) continue;
      double v, aqi;
      if (!numstr(po, "value", &v)) continue;
      if (v <= -1.0) continue;            /* -1 sentinel = unavailable (R1) */

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "station_id", sid);
      if (name) cJSON_AddStringToObject(p, "station_name", name);
      if (area) cJSON_AddStringToObject(p, "area", area);
      cJSON_AddStringToObject(p, "pollutant", POLL[i].key);
      cJSON_AddNumberToObject(p, "value", v);
      cJSON_AddStringToObject(p, "unit", POLL[i].unit);
      if (numstr(po, "aqi", &aqi) && aqi > -1.0) {
        cJSON_AddNumberToObject(p, "aqi_index", aqi);
        cJSON_AddStringToObject(p, "aqi_unit", "index (dimensionless)");
      }
      if (date) cJSON_AddStringToObject(p, "date", date);
      if (tm)   cJSON_AddStringToObject(p, "time_local", tm);
      cJSON_AddStringToObject(p, "country", "TH");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      char key[128], title[288];
      snprintf(key, sizeof key, "%s|%s|%s %s", sid, POLL[i].key,
               date ? date : "", tm ? tm : "");
      snprintf(title, sizeof title, "%s (%s): %s = %.2f %s",
               name ? name : sid, sid, POLL[i].key, v, POLL[i].unit);

      intel_item row = {0};
      row.remote_key      = key;
      row.title           = title;
      row.summary         = title;
      row.lang            = "en";
      row.link            = "http://air4thai.pcd.go.th/";
      row.record_type     = "air-quality-measurement";
      row.has_geo         = has_geo;
      row.lat = has_geo ? lat : 0; row.lon = has_geo ? lon : 0;
      row.properties_json = pj;
      row.tags_json       = "[\"environment\",\"air-quality\",\"thailand\"]";
      if (sink->emit(sink, &row) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_air4thai_stations_def = {
  .id = SRC, .collector = "environment",
  .name = "Thailand PCD Air4Thai monitoring network",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://air4thai.pcd.go.th/services/getNewAQI_JSON.php",
  .description = "Thai Pollution Control Department hourly PM2.5, PM10, O3, NO2, SO2 (ug/m3) and CO (ppm) with station coordinates; the -1 no-data sentinel is rejected.",
  .license = "Thai PCD Air4Thai public service, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_air4thai_stations_def)
