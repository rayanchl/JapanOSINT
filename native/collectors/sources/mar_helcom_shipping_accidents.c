/* collectors/sources/mar_helcom_shipping_accidents.c
 * HELCOM — Baltic Sea shipping accident database (every reported accident
 * since 1989: collisions, groundings, machinery failures).
 *
 * Endpoint: https://maps.helcom.fi/arcgis/rest/services/MADS/Shipping/
 *   MapServer/325/query?where=1%3D1&outFields=*&f=geojson&outSR=4326
 * Emits (all fetched): Unique_ID, Country, Year, Date, Acc_Type, Colli_Type,
 *   Ship1_Name / Sh1_Categ / Cause_Sh1 / Pilot_Sh1, Ship2_Name / Sh2_Categ,
 *   Pollution + Pollu_Type + Pollu_m3, IceCondit, Assistance and the reported
 *   accident position from features[].geometry.coordinates.
 * Keyless. Licence: HELCOM Map and Data Service — openly downloadable;
 *   HELCOM's data policy asks for attribution to HELCOM and the reporting
 *   contracting party, and requires that the data NOT be presented as official
 *   positions of HELCOM.
 *
 * *** parse_notes trap — "Request f=geojson&outSR=4326 explicitly — the ArcGIS
 * default is f=json in Web Mercator." Both parameters are pinned in the URL;
 * without outSR=4326 every coordinate would arrive as Web Mercator metres. ***
 * parse_notes trap — "Many string fields are a single space ' ' meaning 'not
 * reported' — trim and treat as missing." hsv() below drops them.
 * parse_notes — "Date fields are epoch MILLISECONDS": converted to ISO.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

#define HELCOM_URL "https://maps.helcom.fi/arcgis/rest/services/MADS/Shipping/" \
                   "MapServer/325/query?where=1%3D1&outFields=*" \
                   "&f=geojson&outSR=4326"

/* HELCOM writes " " for "not reported" — trim and treat as missing. */
static const char *hsv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!cJSON_IsString(v) || !v->valuestring) return NULL;
  const char *s = v->valuestring;
  while (*s == ' ' || *s == '\t') s++;
  if (!*s) return NULL;
  if (strcmp(s, "n.i.") == 0) return NULL;
  return s;
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
    fprintf(stderr, "[helcom-shipping-accidents] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *uid = hsv(pr, "Unique_ID");
    if (!uid) continue;

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
    cJSON_AddStringToObject(p, "Unique_ID", uid);
    add_s(p, pr, "Country");
    add_s(p, pr, "Location");
    add_s(p, pr, "Acc_Type");
    add_s(p, pr, "Colli_Type");
    add_s(p, pr, "Acc_Detail");
    add_s(p, pr, "CauseDetai");
    add_s(p, pr, "Assistance");
    add_s(p, pr, "Damage");
    add_s(p, pr, "HumanEleme");
    add_s(p, pr, "IceCondit");
    add_s(p, pr, "Pollution");
    add_s(p, pr, "Pollu_Type");
    add_s(p, pr, "Cargo_Type");
    add_s(p, pr, "Ship1_Name");
    add_s(p, pr, "Sh1_Categ");
    add_s(p, pr, "Sh1_Type");
    add_s(p, pr, "Cause_Sh1");
    add_s(p, pr, "Pilot_Sh1");
    add_s(p, pr, "Ship2_Name");
    add_s(p, pr, "Sh2_Categ");
    add_s(p, pr, "Cause_Sh2");
    add_s(p, pr, "Pilot_Sh2");
    add_s(p, pr, "Add_Info");
    add_s(p, pr, "source");
    const cJSON *yr = cJSON_GetObjectItem(pr, "Year");
    if (cJSON_IsNumber(yr)) cJSON_AddNumberToObject(p, "Year", yr->valuedouble);
    const cJSON *pv = cJSON_GetObjectItem(pr, "Pollu_m3");
    if (cJSON_IsNumber(pv)) cJSON_AddNumberToObject(p, "Pollu_m3", pv->valuedouble);
    if (whenp) cJSON_AddStringToObject(p, "date", whenp);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "reported-accident-position");
    }
    cJSON_AddStringToObject(p, "source_service", "HELCOM Map and Data Service");
    cJSON_AddStringToObject(p, "attribution_note",
      "HELCOM and the reporting contracting party; not an official HELCOM position");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *atype = hsv(pr, "Acc_Type");
    const char *s1 = hsv(pr, "Ship1_Name");
    const char *s2 = hsv(pr, "Ship2_Name");
    const char *poll = hsv(pr, "Pollution");
    char title[240], summary[240];
    snprintf(title, sizeof title, "%s — %s",
             atype ? atype : "Baltic shipping accident", s1 ? s1 : uid);
    snprintf(summary, sizeof summary, "%s%s%s%s",
             s2 ? "with " : "", s2 ? s2 : "",
             poll ? " · pollution " : "", poll ? poll : "");

    intel_item it = {0};
    it.remote_key      = uid;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = "https://maps.helcom.fi/website/mapservice/";
    it.lang            = "en";
    it.published_at    = whenp;
    it.record_type     = "shipping-accident";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"accident\",\"baltic\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[helcom-shipping-accidents] emitted %d\n", n);
  return 0;
}

static const source_def mar_helcom_shipping_accidents_def = {
  .id = "helcom-shipping-accidents", .collector = "maritime",
  .name = "HELCOM — Baltic Sea shipping accident database",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset", .url = HELCOM_URL,
  .description = "Every reported shipping accident in the Baltic since 1989 — collisions, groundings and machinery failures with both ships named, cause, ice conditions, pilot on board, and whether pollution resulted.",
  .license = "HELCOM Map and Data Service — attribution to HELCOM and the reporting contracting party; not an official HELCOM position",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_helcom_shipping_accidents_def)
