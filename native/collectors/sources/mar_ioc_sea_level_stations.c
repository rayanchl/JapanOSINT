/* collectors/sources/mar_ioc_sea_level_stations.c
 * IOC/UNESCO Sea Level Monitoring Facility — the global GLOSS/IOC tide-gauge
 * network with each station's latest reading.
 *
 * Endpoint: http://www.ioc-sealevelmonitoring.org/service.php
 *           ?query=stationlist&showall=all&format=json
 * Emits (all fetched): Code, Location, country, sensor id/type, units,
 *   lastvalue, lasttime, observations count, status, and the gauge position
 *   from the Lat / Lon numeric fields.
 * Keyless. Licence: IOC/UNESCO Sea Level Station Monitoring Facility — free
 *   access, attribution requested; data are for scientific/operational use and
 *   are NOT quality-controlled in real time.
 *
 * parse_notes — "Real coordinate is the Lat / Lon numeric fields — the gauge's
 * fixed position. 'lastvalue' + 'lasttime' are the live reading; a station
 * that has stopped reporting keeps its old lasttime rather than nulling the
 * position, so age the row on lasttime." lasttime becomes published_at so the
 * staleness is visible; the position is kept as published.
 * parse_notes — "Endpoint is plain HTTP (no TLS on the service.php path)."
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define IOC_URL "http://www.ioc-sealevelmonitoring.org/service.php" \
                "?query=stationlist&showall=all&format=json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, IOC_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[ioc-sea-level-stations] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "stations");
  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    if (!cJSON_IsObject(s)) continue;
    const char *code = jo_sv(s, "Code");
    if (!code) code = jo_sv(s, "code");
    if (!code) continue;
    const char *place = jo_sv(s, "Location");

    const cJSON *latj = cJSON_GetObjectItem(s, "Lat");
    const cJSON *lonj = cJSON_GetObjectItem(s, "Lon");
    int geo = cJSON_IsNumber(latj) && cJSON_IsNumber(lonj);
    double lat = geo ? latj->valuedouble : 0;
    double lon = geo ? lonj->valuedouble : 0;
    if (geo && (lat < -90 || lat > 90 || lon < -180 || lon > 180)) geo = 0;

    /* the sensor id/key varies in case between rows of this feed */
    const char *sensor = jo_sv(s, "sensor");
    const char *last   = jo_sv(s, "lasttime");
    if (!last) last = jo_sv(s, "last");
    const char *units  = jo_sv(s, "units");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "code", code);
    jo_copy_str(p, s, "Location");
    jo_copy_str(p, s, "country");
    jo_copy_str(p, s, "type");
    if (sensor) cJSON_AddStringToObject(p, "sensor", sensor);
    if (units)  cJSON_AddStringToObject(p, "units", units);
    jo_copy_str(p, s, "lasttime");
    jo_copy_str(p, s, "first");
    jo_copy_str(p, s, "last");
    jo_copy_str(p, s, "statday");
    jo_copy_num(p, s, "lastvalue");
    jo_copy_num(p, s, "observations");
    jo_copy_num(p, s, "status");
    jo_copy_num(p, s, "sensorid");
    jo_copy_num(p, s, "GlossID");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "surveyed-gauge-position");
    }
    cJSON_AddStringToObject(p, "source",
      "IOC/UNESCO Sea Level Station Monitoring Facility");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[200], summary[220], link[200];
    const char *cc = jo_sv(s, "country");
    snprintf(title, sizeof title, "%s (%s)", place ? place : code, code);
    const cJSON *lv = cJSON_GetObjectItem(s, "lastvalue");
    if (cJSON_IsNumber(lv))
      snprintf(summary, sizeof summary, "last %.3f %s%s%s%s%s",
               lv->valuedouble, units ? units : "",
               last ? " at " : "", last ? last : "",
               cc ? " · " : "", cc ? cc : "");
    else
      snprintf(summary, sizeof summary, "IOC tide gauge%s%s",
               cc ? " · " : "", cc ? cc : "");
    snprintf(link, sizeof link,
             "http://www.ioc-sealevelmonitoring.org/station.php?code=%s", code);

    intel_item it = {0};
    it.remote_key      = code;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.published_at    = last;
    it.record_type     = "tide-gauge-station";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"tide-gauge\",\"gloss\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[ioc-sea-level-stations] emitted %d\n", n);
  return 0;
}

static const source_def mar_ioc_sea_level_stations_def = {
  .id = "ioc-sea-level-stations", .collector = "maritime",
  .name = "IOC/UNESCO Sea Level Monitoring Facility — global tide gauge network",
  .update_interval_sec = 1800, .run = run,
  .category = "transport", .type = "api", .url = IOC_URL,
  .description = "The global GLOSS/IOC tide-gauge network with each station's latest sea-level reading, sensor type and time of last update — coastal flooding and tsunami-network health in one call.",
  .license = "IOC/UNESCO Sea Level Station Monitoring Facility — free access, attribution requested; data not quality-controlled in real time",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_ioc_sea_level_stations_def)
