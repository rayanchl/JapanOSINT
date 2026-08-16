/* collectors/sources/tsp_jpl_fireballs.c
 * NASA/JPL CNEOS fireball (bolide) detections — US government sensor
 * detections of atmospheric bolide events.
 * Endpoint: https://ssd-api.jpl.nasa.gov/fireball.api?limit=100&sort=-date
 *   (keyless; falls back to plain ?limit=100 if the sort parameter is rejected)
 * Emits: event date/time, total radiated energy (J x1e10), calculated impact
 *   energy (kt), altitude (km), velocity (km/s) and the entry point.
 *
 * parse_notes honoured, verbatim where it matters:
 *  - "Columnar: 'fields' names the columns, 'data' is an array of arrays of
 *    STRINGS in that order — index by the fields array, never by fixed
 *    position": column indices are resolved from fields[] on every run.
 *  - "Coordinates are magnitude + hemisphere letter: lat='46.6',
 *    lat-dir='N', lon='133.1', lon-dir='E' -> +46.6/+133.1; apply the sign
 *    from lat-dir/lon-dir": done below. A row missing lat/lon (many are
 *    energy-only detections) is emitted with NO geometry (R2).
 *  - "Some rows have null vel/alt": those keys are simply omitted.
 * Licence: NASA/JPL SSD/CNEOS public API, no key, US Government data,
 *   courtesy attribution.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FB_URL_SORTED "https://ssd-api.jpl.nasa.gov/fireball.api?limit=100&sort=-date"
#define FB_URL_PLAIN  "https://ssd-api.jpl.nasa.gov/fireball.api?limit=100"

/* index of column `name` in the API's fields[] array, or -1 */
static int col_of(const cJSON *fields, const char *name) {
  int i = 0;
  const cJSON *f;
  cJSON_ArrayForEach(f, fields) {
    if (cJSON_IsString(f) && f->valuestring && strcmp(f->valuestring, name) == 0)
      return i;
    i++;
  }
  return -1;
}
/* cell as a non-empty string, or NULL (every value in this API is a string) */
static const char *cell(const cJSON *row, int idx) {
  if (idx < 0) return NULL;
  const cJSON *c = cJSON_GetArrayItem(row, idx);
  if (!cJSON_IsString(c) || !c->valuestring || !c->valuestring[0]) return NULL;
  return c->valuestring;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, FB_URL_SORTED, 30000);
  if (!doc) doc = feed_get_json(ctx->http, FB_URL_PLAIN, 30000);
  if (!doc) {
    fprintf(stderr, "[jpl-fireballs] fetch failed\n");
    return -1;
  }
  cJSON *fields = cJSON_GetObjectItem(doc, "fields");
  cJSON *data   = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(fields)) {
    fprintf(stderr, "[jpl-fireballs] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }
  if (!cJSON_IsArray(data)) {          /* upstream genuinely had nothing (R3) */
    cJSON_Delete(doc);
    fprintf(stderr, "[jpl-fireballs] emitted 0 (no events returned)\n");
    return 0;
  }

  int i_date = col_of(fields, "date");
  int i_en   = col_of(fields, "energy");
  int i_ie   = col_of(fields, "impact-e");
  int i_lat  = col_of(fields, "lat");
  int i_latd = col_of(fields, "lat-dir");
  int i_lon  = col_of(fields, "lon");
  int i_lond = col_of(fields, "lon-dir");
  int i_alt  = col_of(fields, "alt");
  int i_vel  = col_of(fields, "vel");
  if (i_date < 0) {
    fprintf(stderr, "[jpl-fireballs] no 'date' column; not parsing\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *row;
  cJSON_ArrayForEach(row, data) {
    if (!cJSON_IsArray(row)) continue;
    const char *date = cell(row, i_date);
    if (!date) continue;
    const char *energy = cell(row, i_en);
    const char *impact = cell(row, i_ie);
    const char *alt    = cell(row, i_alt);
    const char *vel    = cell(row, i_vel);

    /* magnitude + hemisphere letter -> signed decimal degrees */
    double lat = 0, lon = 0;
    int has_geo = 0;
    const char *slat = cell(row, i_lat), *slatd = cell(row, i_latd);
    const char *slon = cell(row, i_lon), *slond = cell(row, i_lond);
    if (slat && slon) {
      char *e1 = NULL, *e2 = NULL;
      double a = strtod(slat, &e1), b = strtod(slon, &e2);
      if (e1 && e1 != slat && e2 && e2 != slon) {
        if (slatd && (slatd[0] == 'S' || slatd[0] == 's')) a = -a;
        if (slond && (slond[0] == 'W' || slond[0] == 'w')) b = -b;
        if (a >= -90.0 && a <= 90.0 && b >= -180.0 && b <= 180.0) {
          lat = a; lon = b; has_geo = 1;
        }
      }
    }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "date_utc", date);
    if (energy) cJSON_AddStringToObject(pr, "total_radiated_energy_j_e10", energy);
    if (impact) cJSON_AddStringToObject(pr, "calculated_impact_energy_kt", impact);
    if (alt)    cJSON_AddStringToObject(pr, "altitude_km", alt);
    if (vel)    cJSON_AddStringToObject(pr, "velocity_km_s", vel);
    if (has_geo) {
      cJSON_AddStringToObject(pr, "geo_subject", "atmospheric entry point");
      if (slatd) cJSON_AddStringToObject(pr, "lat_dir", slatd);
      if (slond) cJSON_AddStringToObject(pr, "lon_dir", slond);
    } else {
      cJSON_AddStringToObject(pr, "geo_note",
        "this detection was published without a location");
    }
    cJSON_AddStringToObject(pr, "source", "NASA/JPL CNEOS fireball API");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[224], summary[256];
    if (has_geo)
      snprintf(title, sizeof title, "Fireball %s at %.3f, %.3f", date, lat, lon);
    else
      snprintf(title, sizeof title, "Fireball %s (no location published)", date);
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
             impact ? "impact energy " : "energy n/a",
             impact ? impact : "", impact ? " kt" : "",
             alt ? " · altitude " : "", alt ? alt : "", alt ? " km" : "");

    intel_item it = {0};
    it.remote_key      = date;               /* CNEOS keys events by time */
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://cneos.jpl.nasa.gov/fireballs/";
    it.lang            = "en";
    it.published_at    = date;
    it.record_type     = "fireball-detection";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"fireball\",\"nasa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[jpl-fireballs] emitted %d\n", n);
  return 0;
}

static const source_def tsp_jpl_fireballs_def = {
  .id = "jpl-fireballs", .collector = "satellite",
  .name = "NASA/JPL CNEOS fireball (bolide) detections",
  .update_interval_sec = 43200, .run = run,
  .category = "satellite", .type = "api", .url = FB_URL_SORTED,
  .description = "US government sensor detections of atmospheric bolide events: time, entry point, altitude, velocity and radiated/impact energy.",
  .license = "NASA/JPL SSD/CNEOS public API — US Government data, keyless, courtesy attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_jpl_fireballs_def)
