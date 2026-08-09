/* collectors/sources/mar_noaa_coops_water_level.c
 * OSINT service — NOAA_COOPS_WATER_LEVEL. On-demand coordinate pivot
 * (ctx->entity = "lat,lon", or a NOAA station id given directly): returns the
 * latest observed water level for the nearest US CO-OPS gauge — storm surge,
 * port-closure risk and under-keel clearance context for a US port.
 *
 * Endpoints:
 *   https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json
 *     ?type=waterlevels                       (registry, to resolve the coord)
 *   https://tidesandcurrents.noaa.gov/api/datagetter?product=water_level
 *     &application=japanosint&date=latest&datum=MLLW&station=<id>
 *     &time_zone=gmt&units=metric&format=json               (the observation)
 * Emits (all fetched): station id and name, the gauge coordinate from
 *   metadata.lat/metadata.lon, and the latest sample's time t, value v (m,
 *   MLLW), sigma s and quality flag q.
 * Keyless. Licence: NOAA / US Government work, public domain. The
 *   'application' parameter identifies the caller as NOAA asks.
 *
 * parse_notes trap — "Coordinate comes from metadata.lat / metadata.lon
 * (STRINGS, need atof) and is the gauge position, not a vessel." Honoured; the
 * plotted point is the gauge, and record_type says so.
 * parse_notes trap — "Every data[] sample inherits that one gauge coordinate —
 * do not emit one row per sample with geo ... prefer one row per station
 * carrying the latest v." Exactly one row is emitted, carrying the latest
 * sample.
 * parse_notes trap — "When the station has no data the response is
 * {\"error\":{\"message\":...}} with HTTP 200 — check for the error key and
 * treat as honest empty." Checked; returns 0.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define COOPS_REG "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/" \
                  "stations.json?type=waterlevels"

/* A CO-OPS station id is 7 digits. */
static int looks_like_station(const char *s) {
  if (!s) return 0;
  int d = 0;
  for (const char *p = s; *p; p++) {
    if (!isdigit((unsigned char)*p)) return 0;
    d++;
  }
  return d == 7;
}

/* Nearest registry station to (lat,lon), using ONLY the coordinates the
 * registry returned. Writes the id into `out`; 0 when nothing usable. */
static int nearest_station(const source_ctx *ctx, double lat, double lon,
                           char *out, size_t outn) {
  char *body = jo_get(ctx, COOPS_REG, NULL, "NOAA_COOPS_WATER_LEVEL");
  if (!body) return 0;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;
  cJSON *arr = cJSON_GetObjectItem(doc, "stations");
  double best = 1e18;
  int found = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    const char *id = jo_sv(s, "id");
    const cJSON *la = cJSON_GetObjectItem(s, "lat");
    const cJSON *ln = cJSON_GetObjectItem(s, "lng");   /* 'lng', not 'lon' */
    if (!id || !cJSON_IsNumber(la) || !cJSON_IsNumber(ln)) continue;
    double dy = la->valuedouble - lat;
    double dx = (ln->valuedouble - lon) * cos(lat * 3.14159265358979 / 180.0);
    double d2 = dy * dy + dx * dx;
    if (d2 < best) {
      best = d2;
      snprintf(out, outn, "%s", id);
      found = 1;
    }
  }
  cJSON_Delete(doc);
  return found;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;

  char station[32] = {0};
  double qlat = 0, qlon = 0;
  if (looks_like_station(q)) {
    snprintf(station, sizeof station, "%s", q);
  } else if (jo_parse_coord(q, &qlat, &qlon)) {
    if (!nearest_station(ctx, qlat, qlon, station, sizeof station)) return 0;
  } else {
    return 0;                          /* wrong shape -> honest empty (R3) */
  }

  char url[400];
  snprintf(url, sizeof url,
           "https://tidesandcurrents.noaa.gov/api/datagetter?product=water_level"
           "&application=japanosint&date=latest&datum=MLLW&station=%s"
           "&time_zone=gmt&units=metric&format=json", station);

  char *body = jo_get(ctx, url, NULL, "NOAA_COOPS_WATER_LEVEL");
  if (!body) return 0;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;
  if (cJSON_GetObjectItem(doc, "error")) {   /* HTTP 200 + error = no data */
    cJSON_Delete(doc);
    return 0;
  }

  cJSON *meta = cJSON_GetObjectItem(doc, "metadata");
  cJSON *data = cJSON_GetObjectItem(doc, "data");
  cJSON *last = NULL;
  if (cJSON_IsArray(data)) {
    int nd = cJSON_GetArraySize(data);
    if (nd > 0) last = cJSON_GetArrayItem(data, nd - 1);
  }
  if (!meta || !last) { cJSON_Delete(doc); return 0; }

  const char *sid  = jo_sv(meta, "id");
  const char *name = jo_sv(meta, "name");
  const char *slat = jo_sv(meta, "lat");     /* STRINGS upstream */
  const char *slon = jo_sv(meta, "lon");
  const char *t = jo_sv(last, "t");
  const char *v = jo_sv(last, "v");
  if (!sid || !t || !v) { cJSON_Delete(doc); return 0; }

  int geo = 0; double lat = 0, lon = 0;
  if (slat && slon) {
    lat = atof(slat); lon = atof(slon);
    if (lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) geo = 1;
  }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "NOAA_COOPS_WATER_LEVEL");
  cJSON_AddStringToObject(p, "station_id", sid);
  if (name) cJSON_AddStringToObject(p, "station_name", name);
  cJSON_AddStringToObject(p, "observed_at_gmt", t);
  cJSON_AddStringToObject(p, "water_level_m_mllw", v);
  const char *s_sigma = jo_sv(last, "s");
  const char *s_q = jo_sv(last, "q");
  if (s_sigma) cJSON_AddStringToObject(p, "sigma", s_sigma);
  if (s_q)     cJSON_AddStringToObject(p, "quality", s_q);
  if (geo) {
    cJSON_AddNumberToObject(p, "lat", lat);
    cJSON_AddNumberToObject(p, "lon", lon);
    cJSON_AddStringToObject(p, "geo_precision", "surveyed-gauge-position");
    cJSON_AddStringToObject(p, "geo_note",
      "position of the tide gauge, not of any vessel");
  }
  cJSON_AddStringToObject(p, "source", "NOAA CO-OPS");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char title[200], summary[220], link[200];
  snprintf(title, sizeof title, "Water level %s (%s)", name ? name : sid, sid);
  snprintf(summary, sizeof summary, "%s m MLLW at %s GMT", v, t);
  snprintf(link, sizeof link,
           "https://tidesandcurrents.noaa.gov/stationhome.html?id=%s", sid);

  intel_item it = {0};
  it.remote_key      = sid;
  it.title           = title;
  it.summary         = summary;
  it.link            = link;
  it.lang            = "en";
  it.record_type     = "water-level-observation";
  it.has_geo         = geo;
  it.lat             = lat;
  it.lon             = lon;
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"osint-search\",\"maritime\",\"tide-gauge\"]";
  sink->emit(sink, &it);
  free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[NOAA_COOPS_WATER_LEVEL] emitted 1 (%s)\n", sid);
  return 0;
}

static const source_def mar_noaa_coops_water_level_def = {
  .id = "NOAA_COOPS_WATER_LEVEL", .collector = "osint",
  .name = "NOAA CO-OPS observed water level by station",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://tidesandcurrents.noaa.gov/api/datagetter",
  .description = "Latest observed water level for the NOAA gauge nearest a coordinate (or for a station id given directly) — storm surge, port closure risk and under-keel clearance context for a US port.",
  .license = "NOAA / US Government work, public domain",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_noaa_coops_water_level_def)
