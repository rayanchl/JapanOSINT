/* collectors/sources/mar_dfo_iwls_stations.c
 * DFO Canada — Integrated Water Level System (Canadian Hydrographic Service)
 * station registry: every tide gauge and prediction point on Canadian coasts.
 *
 * Endpoint: https://api-iwls.dfo-mpo.gc.ca/api/v1/stations
 * Emits (all fetched): id, code, officialName, alternativeName, operating
 *   flag, type, the available timeSeries codes (wlo observed / wlp predicted /
 *   wlp-hilo), and the gauge position from the latitude/longitude numerics.
 * Keyless. Licence: Fisheries and Oceans Canada, Open Government Licence –
 *   Canada.
 *
 * parse_notes — "Coordinate is the top-level latitude / longitude numerics
 * (decimal degrees) — the gauge position. operating=false marks
 * decommissioned/temporary stations; keep the flag rather than dropping the
 * row." Both honoured; a station missing either numeric is emitted with
 * has_geo=0 (R2).
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

#define IWLS_URL "https://api-iwls.dfo-mpo.gc.ca/api/v1/stations"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, IWLS_URL, 40000);
  if (!doc) {
    fprintf(stderr, "[dfo-iwls-stations] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "stations");
  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    if (!cJSON_IsObject(s)) continue;
    const char *code = jo_sv(s, "code");
    const char *id   = jo_sv(s, "id");
    const char *name = jo_sv(s, "officialName");
    if (!code && !id) continue;
    const char *key = code ? code : id;

    const cJSON *latj = cJSON_GetObjectItem(s, "latitude");
    const cJSON *lonj = cJSON_GetObjectItem(s, "longitude");
    int geo = cJSON_IsNumber(latj) && cJSON_IsNumber(lonj);
    double lat = geo ? latj->valuedouble : 0;
    double lon = geo ? lonj->valuedouble : 0;
    if (geo && (lat < -90 || lat > 90 || lon < -180 || lon > 180)) geo = 0;

    cJSON *p = cJSON_CreateObject();
    if (code) cJSON_AddStringToObject(p, "code", code);
    if (id)   cJSON_AddStringToObject(p, "station_id", id);
    if (name) cJSON_AddStringToObject(p, "officialName", name);
    const char *alt = jo_sv(s, "alternativeName");
    if (alt) cJSON_AddStringToObject(p, "alternativeName", alt);
    const char *type = jo_sv(s, "type");
    if (type) cJSON_AddStringToObject(p, "type", type);
    const cJSON *op = cJSON_GetObjectItem(s, "operating");
    if (cJSON_IsBool(op)) cJSON_AddBoolToObject(p, "operating", cJSON_IsTrue(op));
    const cJSON *tsa = cJSON_GetObjectItem(s, "timeSeries");
    if (cJSON_IsArray(tsa)) {
      cJSON *codes = cJSON_CreateArray();
      const cJSON *t;
      cJSON_ArrayForEach(t, tsa) {
        const char *tc = jo_sv(t, "code");
        if (tc) cJSON_AddItemToArray(codes, cJSON_CreateString(tc));
      }
      cJSON_AddItemToObject(p, "timeSeries", codes);
    }
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "surveyed-gauge-position");
    }
    cJSON_AddStringToObject(p, "source", "Fisheries and Oceans Canada IWLS");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[220], summary[200], link[220];
    snprintf(title, sizeof title, "%s (%s)", name ? name : key, key);
    snprintf(summary, sizeof summary, "CHS water-level station%s%s%s",
             type ? " · " : "", type ? type : "",
             (cJSON_IsBool(op) && !cJSON_IsTrue(op)) ? " · not operating" : "");
    if (id) snprintf(link, sizeof link, "%s/%s", IWLS_URL, id);
    else    snprintf(link, sizeof link, "%s", IWLS_URL);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.record_type     = "tide-gauge-station";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"tide-gauge\",\"canada\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[dfo-iwls-stations] emitted %d\n", n);
  return 0;
}

static const source_def mar_dfo_iwls_stations_def = {
  .id = "dfo-iwls-stations", .collector = "maritime",
  .name = "DFO Canada — Integrated Water Level System station registry",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api", .url = IWLS_URL,
  .description = "Canadian Hydrographic Service water-level station registry: every tide gauge and prediction point on Canadian coasts with coordinates, operating status and available time series.",
  .license = "Fisheries and Oceans Canada, Open Government Licence – Canada",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_dfo_iwls_stations_def)
