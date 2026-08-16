/* Sensor.Community global citizen air-quality sensor network.
 * Endpoint: https://data.sensor.community/static/v2/data.json          (keyless)
 * Last-5-minute readings from tens of thousands of PM sensors worldwide.
 * Emits one row per sensor that returned a particulate reading:
 *   pm10 / pm2_5 / pm1 (UNIT: ug/m3 — value_type P1 = PM10, P2 = PM2.5,
 *   P0 = PM1), the sensor type, the reading timestamp and coordinates.
 *
 * OUTLIER NOTE: cheap sensors produce wild readings (a P1 of 1117 ug/m3 is a
 * faulty/duty-cycle sensor, not smog). Values above 1000 ug/m3 are emitted
 * with "suspect_outlier": true rather than presented as fact or silently
 * dropped.
 * GEO PRECISION (R2): location.exact_location = 0 means the publisher has
 * deliberately rounded the coordinates to ~1 km; those rows carry
 * "geo_precision": "rounded-1km".
 * STRING TRAP: sensordatavalues[].value is a STRING.
 * Row cap: JO_SENSORCOMMUNITY_MAX (default 3000) bounds a multi-MB payload.
 * Licence: ODbL 1.0 (attribution + share-alike). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "sensor-community-air"

/* location fields come back as strings on this feed; accept either */
static int numish(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring[0]) {
    char *e = NULL; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int cap = 3000;
  const char *capenv = getenv("JO_SENSORCOMMUNITY_MAX");
  if (capenv && *capenv) { int c = atoi(capenv); if (c > 0) cap = c; }

  cJSON *doc = feed_get_json(ctx->http,
                             "https://data.sensor.community/static/v2/data.json",
                             60000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] unexpected shape\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (n >= cap) break;
    cJSON *loc = cJSON_GetObjectItem(r, "location");
    cJSON *sen = cJSON_GetObjectItem(r, "sensor");
    cJSON *vals = cJSON_GetObjectItem(r, "sensordatavalues");
    if (!loc || !cJSON_IsArray(vals)) continue;

    double lat, lon;
    if (!numish(loc, "latitude", &lat) || !numish(loc, "longitude", &lon)) continue;
    if (lat == 0 && lon == 0) continue;
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) continue;

    cJSON *p = cJSON_CreateObject();
    int have = 0, suspect = 0;
    double pm10 = 0; int have_pm10 = 0;
    cJSON *v;
    cJSON_ArrayForEach(v, vals) {
      const char *vt = jo_sv(v, "value_type");
      const char *vs = jo_sv(v, "value");
      if (!vt || !vs) continue;
      char *e = NULL; double d = strtod(vs, &e);
      if (e == vs) continue;
      const char *field = NULL, *unit = NULL;
      if      (strcmp(vt, "P1") == 0) { field = "pm10";        unit = "ug/m3"; }
      else if (strcmp(vt, "P2") == 0) { field = "pm2_5";       unit = "ug/m3"; }
      else if (strcmp(vt, "P0") == 0) { field = "pm1";         unit = "ug/m3"; }
      else if (strcmp(vt, "temperature") == 0) { field = "temperature"; unit = "degC"; }
      else if (strcmp(vt, "humidity") == 0)    { field = "humidity";    unit = "%"; }
      else if (strcmp(vt, "pressure") == 0)    { field = "pressure";    unit = "Pa"; }
      else continue;
      cJSON_AddNumberToObject(p, field, d);
      if (!have) cJSON_AddStringToObject(p, "pm_unit", "ug/m3");
      (void)unit;
      if (strcmp(vt, "P1") == 0) { pm10 = d; have_pm10 = 1; }
      /* flag, do not present as fact: >1000 ug/m3 is a faulty sensor */
      if ((field[0] == 'p' && field[1] == 'm') && d > 1000.0) suspect = 1;
      have = 1;
    }
    if (!have) { cJSON_Delete(p); continue; }   /* no measurement -> no row */

    const char *when = jo_sv(r, "timestamp");
    if (when) cJSON_AddStringToObject(p, "observed_at", when);
    const char *country = jo_sv(loc, "country");
    if (country) cJSON_AddStringToObject(p, "country", country);
    if (sen) {
      cJSON *stype = cJSON_GetObjectItem(sen, "sensor_type");
      const char *sn = stype ? jo_sv(stype, "name") : NULL;
      if (sn) cJSON_AddStringToObject(p, "sensor_type", sn);
    }
    double exact = 1;
    if (numish(loc, "exact_location", &exact) && exact == 0)
      cJSON_AddStringToObject(p, "geo_precision", "rounded-1km");
    if (suspect) cJSON_AddBoolToObject(p, "suspect_outlier", 1);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    long long sid = 0;
    cJSON *sidj = sen ? cJSON_GetObjectItem(sen, "id") : NULL;
    if (cJSON_IsNumber(sidj)) sid = (long long)sidj->valuedouble;
    char key[64], title[224];
    snprintf(key, sizeof key, "sensor:%lld", sid);
    if (have_pm10)
      snprintf(title, sizeof title, "Sensor.Community %lld: PM10 %.1f ug/m3%s",
               sid, pm10, suspect ? " (suspect)" : "");
    else
      snprintf(title, sizeof title, "Sensor.Community sensor %lld reading", sid);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = "https://sensor.community/";
    row.record_type     = "air-quality-sensor";
    row.has_geo         = 1;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"air-quality\",\"citizen-science\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_sensor_community_air_def = {
  .id = SRC, .collector = "environment",
  .name = "Sensor.Community global citizen air-quality sensors",
  .update_interval_sec = 300, .run = run,
  .category = "environment", .type = "api",
  .url = "https://data.sensor.community/static/v2/data.json",
  .description = "Last-5-minute PM2.5/PM10 readings (ug/m3) from the global Sensor.Community network with sensor coordinates.",
  .license = "ODbL 1.0 — Sensor.Community open data, attribution + share-alike.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_sensor_community_air_def)
