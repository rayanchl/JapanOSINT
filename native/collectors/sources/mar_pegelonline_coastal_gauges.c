/* collectors/sources/mar_pegelonline_coastal_gauges.c
 * WSV PEGELONLINE — live water level at German federal waterway gauges,
 * filtered to the North Sea (waters=NORDSEE).
 *
 * Endpoint: https://www.pegelonline.wsv.de/webservices/rest-api/v2/
 *           stations.json?includeTimeseries=true&includeCurrentMeasurement=true
 *           &waters=NORDSEE
 * Emits (all fetched): uuid, number, shortname, longname, agency, water name,
 *   and per gauge the "W" timeseries currentMeasurement value + timestamp,
 *   its unit and the gaugeZero datum, plus the gauge position.
 * Keyless. Licence: Wasserstraßen- und Schifffahrtsverwaltung des Bundes
 *   (WSV) / PEGELONLINE — free reuse under Datenlizenz Deutschland –
 *   Namensnennung (attribution required).
 *
 * parse_notes trap — "Current reading is timeseries[].currentMeasurement.value
 * with timeseries[].unit (cm above gauge datum, gaugeZero gives the datum in m
 * above NHN); it is NOT metres and NOT a sea-surface height without applying
 * gaugeZero." The value is emitted as water_level_cm_above_gauge_zero with the
 * unit and the gaugeZero datum alongside; no conversion is invented.
 * parse_notes — coordinate is the station's latitude/longitude numerics
 * (WGS84), the physical gauge; a station without both is emitted with
 * has_geo=0 (R2).
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define PEGEL_URL "https://www.pegelonline.wsv.de/webservices/rest-api/v2/" \
                  "stations.json?includeTimeseries=true" \
                  "&includeCurrentMeasurement=true&waters=NORDSEE"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, PEGEL_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[pegelonline-coastal-gauges] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "stations");
  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    if (!cJSON_IsObject(s)) continue;
    const char *uuid = jo_sv(s, "uuid");
    const char *num  = jo_sv(s, "number");
    const char *sn   = jo_sv(s, "shortname");
    if (!uuid && !num) continue;
    const char *key = uuid ? uuid : num;

    const cJSON *latj = cJSON_GetObjectItem(s, "latitude");
    const cJSON *lonj = cJSON_GetObjectItem(s, "longitude");
    int geo = cJSON_IsNumber(latj) && cJSON_IsNumber(lonj);
    double lat = geo ? latj->valuedouble : 0;
    double lon = geo ? lonj->valuedouble : 0;

    /* pick the water-level ("W") timeseries and its current measurement */
    const char *unit = NULL, *when = NULL;
    int have_val = 0; double value = 0;
    double gauge_zero = 0; int have_gz = 0;
    const char *gz_unit = NULL;
    const cJSON *tsa = cJSON_GetObjectItem(s, "timeseries");
    if (cJSON_IsArray(tsa)) {
      const cJSON *t;
      cJSON_ArrayForEach(t, tsa) {
        const char *tsn = jo_sv(t, "shortname");
        if (!tsn || strcmp(tsn, "W") != 0) continue;
        unit = jo_sv(t, "unit");
        const cJSON *cm = cJSON_GetObjectItem(t, "currentMeasurement");
        if (cm) {
          const cJSON *vv = cJSON_GetObjectItem(cm, "value");
          if (cJSON_IsNumber(vv)) { value = vv->valuedouble; have_val = 1; }
          when = jo_sv(cm, "timestamp");
        }
        const cJSON *gz = cJSON_GetObjectItem(t, "gaugeZero");
        if (gz) {
          const cJSON *gv = cJSON_GetObjectItem(gz, "value");
          if (cJSON_IsNumber(gv)) { gauge_zero = gv->valuedouble; have_gz = 1; }
          gz_unit = jo_sv(gz, "unit");
        }
        break;
      }
    }

    cJSON *p = cJSON_CreateObject();
    if (uuid) cJSON_AddStringToObject(p, "uuid", uuid);
    if (num)  cJSON_AddStringToObject(p, "number", num);
    if (sn)   cJSON_AddStringToObject(p, "shortname", sn);
    const char *v;
    if ((v = jo_sv(s, "longname"))) cJSON_AddStringToObject(p, "longname", v);
    if ((v = jo_sv(s, "agency")))   cJSON_AddStringToObject(p, "agency", v);
    const cJSON *water = cJSON_GetObjectItem(s, "water");
    if (water && (v = jo_sv(water, "shortname")))
      cJSON_AddStringToObject(p, "water", v);
    if (have_val) {
      cJSON_AddNumberToObject(p, "water_level_cm_above_gauge_zero", value);
      if (unit) cJSON_AddStringToObject(p, "unit", unit);
    }
    if (when) cJSON_AddStringToObject(p, "measured_at", when);
    if (have_gz) {
      cJSON_AddNumberToObject(p, "gauge_zero", gauge_zero);
      if (gz_unit) cJSON_AddStringToObject(p, "gauge_zero_unit", gz_unit);
    }
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "surveyed-gauge-position");
    }
    cJSON_AddStringToObject(p, "source", "WSV PEGELONLINE");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[220], summary[220], link[280];
    snprintf(title, sizeof title, "%s%s%s", sn ? sn : key,
             num ? " — " : "", num ? num : "");
    if (have_val)
      snprintf(summary, sizeof summary, "W %.1f %s%s%s", value,
               unit ? unit : "cm", when ? " at " : "", when ? when : "");
    else
      snprintf(summary, sizeof summary, "PEGELONLINE gauge (no current reading)");
    snprintf(link, sizeof link,
             "https://www.pegelonline.wsv.de/webservices/rest-api/v2/stations/%s.json",
             key);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "de";
    it.published_at    = when;
    it.record_type     = "water-level-gauge";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"water-level\",\"germany\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[pegelonline-coastal-gauges] emitted %d\n", n);
  return 0;
}

static const source_def mar_pegelonline_coastal_gauges_def = {
  .id = "pegelonline-coastal-gauges", .collector = "maritime",
  .name = "WSV PEGELONLINE — German North Sea water-level gauges",
  .update_interval_sec = 1800, .run = run,
  .category = "transport", .type = "api", .url = PEGEL_URL,
  .description = "Live water level at German federal waterway gauges on the North Sea, including the gauge datum and the responsible waterways office — port access and surge context for German ports.",
  .license = "WSV / PEGELONLINE — Datenlizenz Deutschland – Namensnennung (attribution required)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_pegelonline_coastal_gauges_def)
