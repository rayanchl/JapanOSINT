/* collectors/sources/mar_marine_ie_wave_buoys.c
 * Marine Institute Ireland — Irish weather buoy network (M2-M6) via ERDDAP.
 *
 * Endpoint: https://erddap.marine.ie/erddap/tabledap/IWBNetwork.json
 *           ?station_id,longitude,latitude,time,WaveHeight,SeaTemperature
 *           &time>=now-2days
 * Emits (all fetched): station_id, observation time, WaveHeight (m),
 *   SeaTemperature (C) and the moored buoy position from the latitude /
 *   longitude columns.
 * Keyless. Licence: Marine Institute Ireland ERDDAP, CC BY 4.0 (attribution to
 *   the Marine Institute).
 *
 * parse_notes trap — "ERDDAP tabledap JSON: table.columnNames plus
 * table.rows[][] positional arrays — index by column name, not by fixed
 * offset." col_index() resolves every column by name below.
 * parse_notes trap — "When the time constraint matches nothing ERDDAP returns
 * HTTP 404 with a text body 'Your query produced no matching results
 * (nRows = 0)' — that is an honest empty and must return 0, not -1." The raw
 * status is inspected so 404 maps to 0 and only a real transport/parse failure
 * maps to -1 (R3).
 * parse_notes — "Use time>=now-2days; a bare request returns the 2001 start of
 * record first."
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define IWB_URL "https://erddap.marine.ie/erddap/tabledap/IWBNetwork.json" \
                "?station_id,longitude,latitude,time,WaveHeight,SeaTemperature" \
                "&time%3E=now-2days"

static int col_index(const cJSON *names, const char *want) {
  int i = 0;
  const cJSON *c;
  cJSON_ArrayForEach(c, names) {
    if (cJSON_IsString(c) && c->valuestring &&
        strcmp(c->valuestring, want) == 0) return i;
    i++;
  }
  return -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", IWB_URL, NULL, NULL, 0, 30000, 1, &hr);
  if (rc != 0) {
    http_response_free(&hr);
    fprintf(stderr, "[marine-ie-wave-buoys] transport failure\n");
    return -1;
  }
  if (hr.status == 404) {         /* ERDDAP's "nRows = 0" — honest empty (R3) */
    http_response_free(&hr);
    fprintf(stderr, "[marine-ie-wave-buoys] no matching rows (404)\n");
    return 0;
  }
  if (hr.status != 200 || !hr.body) {
    fprintf(stderr, "[marine-ie-wave-buoys] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  cJSON *doc = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!doc) {
    fprintf(stderr, "[marine-ie-wave-buoys] parse failed\n");
    return -1;
  }

  cJSON *tbl = cJSON_GetObjectItem(doc, "table");
  cJSON *names = tbl ? cJSON_GetObjectItem(tbl, "columnNames") : NULL;
  cJSON *rows = tbl ? cJSON_GetObjectItem(tbl, "rows") : NULL;
  if (!cJSON_IsArray(names) || !cJSON_IsArray(rows)) {
    cJSON_Delete(doc);
    fprintf(stderr, "[marine-ie-wave-buoys] unexpected table shape\n");
    return -1;
  }
  int i_id = col_index(names, "station_id");
  int i_lon = col_index(names, "longitude");
  int i_lat = col_index(names, "latitude");
  int i_t   = col_index(names, "time");
  int i_wh  = col_index(names, "WaveHeight");
  int i_st  = col_index(names, "SeaTemperature");

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    if (!cJSON_IsArray(r)) continue;
    const cJSON *idv = i_id >= 0 ? cJSON_GetArrayItem(r, i_id) : NULL;
    const cJSON *tv  = i_t  >= 0 ? cJSON_GetArrayItem(r, i_t)  : NULL;
    if (!cJSON_IsString(idv) || !cJSON_IsString(tv)) continue;
    const char *sid = idv->valuestring;
    const char *when = tv->valuestring;

    const cJSON *lonv = i_lon >= 0 ? cJSON_GetArrayItem(r, i_lon) : NULL;
    const cJSON *latv = i_lat >= 0 ? cJSON_GetArrayItem(r, i_lat) : NULL;
    int geo = cJSON_IsNumber(lonv) && cJSON_IsNumber(latv);
    double lon = geo ? lonv->valuedouble : 0;
    double lat = geo ? latv->valuedouble : 0;

    const cJSON *wh = i_wh >= 0 ? cJSON_GetArrayItem(r, i_wh) : NULL;
    const cJSON *st = i_st >= 0 ? cJSON_GetArrayItem(r, i_st) : NULL;

    char key[128];
    snprintf(key, sizeof key, "%s|%s", sid, when);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_id", sid);
    cJSON_AddStringToObject(p, "time", when);
    if (cJSON_IsNumber(wh)) cJSON_AddNumberToObject(p, "wave_height_m", wh->valuedouble);
    if (cJSON_IsNumber(st)) cJSON_AddNumberToObject(p, "sea_temperature_c", st->valuedouble);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "mooring-position");
    }
    cJSON_AddStringToObject(p, "source", "Marine Institute Ireland (ERDDAP)");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[160], summary[200];
    snprintf(title, sizeof title, "Irish weather buoy %s", sid);
    if (cJSON_IsNumber(wh) && cJSON_IsNumber(st))
      snprintf(summary, sizeof summary, "Hs %.3f m · SST %.3f °C · %s",
               wh->valuedouble, st->valuedouble, when);
    else
      snprintf(summary, sizeof summary, "%s", when);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://erddap.marine.ie/erddap/tabledap/IWBNetwork.html";
    it.lang            = "en";
    it.published_at    = when;
    it.record_type     = "wave-buoy-observation";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"wave-buoy\",\"ireland\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[marine-ie-wave-buoys] emitted %d\n", n);
  return 0;
}

static const source_def mar_marine_ie_wave_buoys_def = {
  .id = "marine-ie-wave-buoys", .collector = "maritime",
  .name = "Marine Institute Ireland — Irish weather buoy network (ERDDAP)",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api", .url = IWB_URL,
  .description = "Hourly observations from Ireland's offshore weather buoys (M2-M6) in the North Atlantic approaches: significant wave height and sea temperature at the moored buoy position.",
  .license = "Marine Institute Ireland ERDDAP, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_marine_ie_wave_buoys_def)
