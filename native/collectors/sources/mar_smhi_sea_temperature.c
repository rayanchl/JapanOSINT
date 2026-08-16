/* collectors/sources/mar_smhi_sea_temperature.c
 * SMHI oceanographic observations — latest-hour sea temperature from the
 * Swedish buoy and coastal-station network.
 *
 * Endpoint: https://opendata-download-ocobs.smhi.se/api/version/latest/
 *           parameter/5/station-set/all/period/latest-hour/data.json
 * Emits (all fetched): station key, name, owner, the parameter name/unit from
 *   the document header, the surface (depth 0) reading with its quality flag
 *   and epoch-ms date, the full depth-tagged value list, and the buoy/station
 *   position from station[].latitude / station[].longitude.
 * Keyless. Licence: SMHI Open Data, CC BY 4.0.
 *
 * parse_notes — "value[] is an array of depth-tagged readings sharing one
 * epoch-ms 'date'; emit the depth=0 reading as the surface value rather than
 * averaging." Done: no averaging anywhere; the deeper readings are carried as
 * a list so nothing fetched is lost.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

#define SMHI_URL "https://opendata-download-ocobs.smhi.se/api/version/latest/" \
                 "parameter/5/station-set/all/period/latest-hour/data.json"

static const char *iso_ms(double ms, char *buf, size_t n) {
  if (!(ms > 0)) return NULL;
  time_t t = (time_t)(ms / 1000.0);
  struct tm g;
  if (!gmtime_r(&t, &g)) return NULL;
  if (strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &g) == 0) return NULL;
  return buf;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SMHI_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[smhi-sea-temperature] fetch/parse failed\n");
    return -1;
  }
  const char *pname = NULL, *punit = NULL;
  cJSON *par = cJSON_GetObjectItem(doc, "parameter");
  if (par) { pname = jo_sv(par, "name"); punit = jo_sv(par, "unit"); }

  cJSON *arr = cJSON_GetObjectItem(doc, "station");
  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    if (!cJSON_IsObject(s)) continue;
    const char *key = jo_sv(s, "key");
    const char *name = jo_sv(s, "name");
    if (!key) continue;

    const cJSON *latj = cJSON_GetObjectItem(s, "latitude");
    const cJSON *lonj = cJSON_GetObjectItem(s, "longitude");
    int geo = cJSON_IsNumber(latj) && cJSON_IsNumber(lonj);
    double lat = geo ? latj->valuedouble : 0;
    double lon = geo ? lonj->valuedouble : 0;

    /* surface reading = the depth "0" entry; never an average of the profile */
    int have_surf = 0; double surf = 0;
    const char *surf_q = NULL;
    char when[40]; const char *whenp = NULL;
    cJSON *readings = cJSON_CreateArray();
    const cJSON *va = cJSON_GetObjectItem(s, "value");
    if (cJSON_IsArray(va)) {
      const cJSON *r;
      cJSON_ArrayForEach(r, va) {
        const cJSON *vv = cJSON_GetObjectItem(r, "value");
        const char *depth = jo_sv(r, "depth");
        const cJSON *dt = cJSON_GetObjectItem(r, "date");
        cJSON *o = cJSON_CreateObject();
        if (cJSON_IsNumber(vv)) cJSON_AddNumberToObject(o, "value", vv->valuedouble);
        if (depth) cJSON_AddStringToObject(o, "depth_m", depth);
        const char *qf = jo_sv(r, "quality");
        if (qf) cJSON_AddStringToObject(o, "quality", qf);
        cJSON_AddItemToArray(readings, o);
        if (!whenp && cJSON_IsNumber(dt))
          whenp = iso_ms(dt->valuedouble, when, sizeof when);
        if (!have_surf && depth && strcmp(depth, "0") == 0 && cJSON_IsNumber(vv)) {
          surf = vv->valuedouble; have_surf = 1; surf_q = qf;
        }
      }
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_key", key);
    if (name) cJSON_AddStringToObject(p, "name", name);
    const char *owner = jo_sv(s, "owner");
    if (owner) cJSON_AddStringToObject(p, "owner", owner);
    if (pname) cJSON_AddStringToObject(p, "parameter", pname);
    if (punit) cJSON_AddStringToObject(p, "unit", punit);
    if (have_surf) {
      cJSON_AddNumberToObject(p, "surface_value", surf);
      if (surf_q) cJSON_AddStringToObject(p, "surface_quality", surf_q);
    }
    if (whenp) cJSON_AddStringToObject(p, "observed_at", whenp);
    cJSON_AddItemToObject(p, "readings", readings);   /* ownership transferred */
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "fixed-station-position");
    }
    cJSON_AddStringToObject(p, "source", "SMHI Open Data (oceanographic)");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[220], summary[220];
    snprintf(title, sizeof title, "%s (%s)", name ? name : key, key);
    if (have_surf)
      snprintf(summary, sizeof summary, "surface %.2f %s%s%s", surf,
               punit ? punit : "", whenp ? " at " : "", whenp ? whenp : "");
    else
      snprintf(summary, sizeof summary, "SMHI oceanographic station");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = SMHI_URL;
    it.lang            = "sv";
    it.published_at    = whenp;
    it.record_type     = "sea-temperature-observation";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"sea-temperature\",\"sweden\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[smhi-sea-temperature] emitted %d\n", n);
  return 0;
}

static const source_def mar_smhi_sea_temperature_def = {
  .id = "smhi-sea-temperature", .collector = "maritime",
  .name = "SMHI oceanographic observations — sea temperature, all stations",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api", .url = SMHI_URL,
  .description = "Latest-hour sea temperature from the Swedish oceanographic buoy and coastal-station network, with the per-depth profile (0 m to 70 m) at moored buoys.",
  .license = "SMHI Open Data, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_smhi_sea_temperature_def)
