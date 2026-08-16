/* collectors/sources/mar_helcom_oil_spills.c
 * HELCOM — Baltic Sea detected oil and substance spills: aerial-surveillance
 * detections of illegal discharges.
 *
 * Endpoint: https://maps.helcom.fi/arcgis/rest/services/MADS/Shipping/
 *   MapServer/323/query?where=1%3D1&outFields=*&f=geojson&outSR=4326
 * Emits (all fetched): HELCOM_ID, Country, Year, Spill_ID, FlightType,
 *   Length__km / Width__km_ / Area__km2_, EstimVol_m3, Vol_Category,
 *   Spill_cat, Type_Substance, Polluter, Remarks, wind speed/direction and the
 *   observed slick position from features[].geometry.coordinates.
 * Keyless. Licence: HELCOM Map and Data Service, aerial surveillance reporting
 *   by contracting parties. Attribution to HELCOM required; not to be
 *   presented as an official HELCOM position.
 *
 * *** parse_notes trap — "Same ArcGIS pattern as the accident layer: force
 * f=geojson&outSR=4326." Both are pinned in the URL; the ArcGIS default would
 * return Web Mercator metres. ***
 * parse_notes — "Coordinate is the observed slick position from the
 * surveillance flight, not a port or a vessel's AIS position." record_type
 * says "oil-spill-detection".
 * parse_notes trap — "'Remarks' often carries the suspected ship name — treat
 * as an allegation, not a confirmed attribution." Remarks is emitted verbatim
 * under a suspected_* label with an explicit caveat, never as a vessel id.
 * parse_notes — "Date/Time_UTC are epoch milliseconds."
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

#define HELCOM_URL "https://maps.helcom.fi/arcgis/rest/services/MADS/Shipping/" \
                   "MapServer/323/query?where=1%3D1&outFields=*" \
                   "&f=geojson&outSR=4326"

static const char *hsv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!cJSON_IsString(v) || !v->valuestring) return NULL;
  const char *s = v->valuestring;
  while (*s == ' ' || *s == '\t') s++;
  return *s ? s : NULL;
}
static inline void add_s(cJSON *dst, const cJSON *src, const char *k) {
  const char *s = hsv(src, k);
  if (s) cJSON_AddStringToObject(dst, k, s);
}
static const char *iso_ms(double ms, char *buf, size_t n) {
  if (!(ms > 0)) return NULL;
  time_t t = (time_t)(ms / 1000.0);
  struct tm g;
  if (!gmtime_r(&t, &g)) return NULL;
  if (strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &g) == 0) return NULL;
  return buf;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, HELCOM_URL, 120000);
  if (!doc) {
    fprintf(stderr, "[helcom-oil-spills] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const cJSON *hid = cJSON_GetObjectItem(pr, "HELCOM_ID");
    if (!cJSON_IsNumber(hid)) continue;
    char key[48];
    snprintf(key, sizeof key, "%lld", (long long)hid->valuedouble);

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    char when[40]; const char *whenp = NULL;
    const cJSON *dt = cJSON_GetObjectItem(pr, "Date");
    if (cJSON_IsNumber(dt)) whenp = iso_ms(dt->valuedouble, when, sizeof when);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "HELCOM_ID", key);
    add_s(p, pr, "Country");
    add_s(p, pr, "Spill_ID");
    add_s(p, pr, "FlightType");
    add_s(p, pr, "Spill_cat");
    add_s(p, pr, "Vol_Category");
    add_s(p, pr, "Type_Substance");
    add_s(p, pr, "Polluter");
    add_s(p, pr, "Wind_speed");
    jo_copy_num(p, pr, "Year");
    jo_copy_num(p, pr, "Length__km");
    jo_copy_num(p, pr, "Width__km_");
    jo_copy_num(p, pr, "Area__km2_");
    jo_copy_num(p, pr, "EstimVol_m3");
    jo_copy_num(p, pr, "Wind_direc");
    const char *rem = hsv(pr, "Remarks");
    if (rem) {
      cJSON_AddStringToObject(p, "suspected_vessel_remarks", rem);
      cJSON_AddStringToObject(p, "attribution_caveat",
        "Remarks may name a suspected polluting vessel — this is an allegation from the surveillance report, not a confirmed attribution");
    }
    if (whenp) cJSON_AddStringToObject(p, "date", whenp);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "observed-slick-position");
      cJSON_AddStringToObject(p, "geo_note",
        "position of the observed slick from a surveillance flight — not a port and not an AIS vessel position");
    }
    cJSON_AddStringToObject(p, "source_service", "HELCOM Map and Data Service");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *cc = hsv(pr, "Country");
    const char *pol = hsv(pr, "Polluter");
    const cJSON *area = cJSON_GetObjectItem(pr, "Area__km2_");
    char title[240], summary[240];
    snprintf(title, sizeof title, "Baltic spill detection %s%s%s",
             hsv(pr, "Spill_ID") ? hsv(pr, "Spill_ID") : key,
             cc ? " — " : "", cc ? cc : "");
    if (cJSON_IsNumber(area))
      snprintf(summary, sizeof summary, "%.2f km2 slick%s%s",
               area->valuedouble, pol ? " · polluter " : "", pol ? pol : "");
    else
      snprintf(summary, sizeof summary, "aerial surveillance detection%s%s",
               pol ? " · polluter " : "", pol ? pol : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://maps.helcom.fi/website/mapservice/";
    it.lang            = "en";
    it.published_at    = whenp;
    it.record_type     = "oil-spill-detection";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"pollution\",\"baltic\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[helcom-oil-spills] emitted %d\n", n);
  return 0;
}

static const source_def mar_helcom_oil_spills_def = {
  .id = "helcom-oil-spills", .collector = "maritime",
  .name = "HELCOM — Baltic Sea detected oil and substance spills",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset", .url = HELCOM_URL,
  .description = "Aerial-surveillance detections of illegal discharges in the Baltic: slick position, length/width/area, estimated volume, substance category and — where identified — the suspected polluting vessel.",
  .license = "HELCOM Map and Data Service — attribution to HELCOM required; not an official HELCOM position",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_helcom_oil_spills_def)
