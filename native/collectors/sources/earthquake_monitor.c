/* collectors/osint/sources/earthquake_monitor.c
 * OSINT service — recent earthquakes from USGS GeoJSON (no key). On-demand
 * (interval 0); entity ignored (global feed).
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per earthquake (keyed by
 * the USGS event id), not a single summary blob. Each row carries real geo
 * (lat/lon) + event time so individual quakes surface as distinct search/map
 * items. If USGS returns nothing, emits nothing (honest empty — no seeded
 * rows). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Emit one intel row for a single USGS feature. Returns 1 if emitted. */
static int emit_quake(intel_sink *sink, cJSON *ft) {
  if (!ft) return 0;
  cJSON *props = cJSON_GetObjectItem(ft, "properties");
  cJSON *geo   = cJSON_GetObjectItem(ft, "geometry");
  if (!props) return 0;

  cJSON *id_j  = cJSON_GetObjectItem(ft, "id");
  cJSON *mag   = cJSON_GetObjectItem(props, "mag");
  cJSON *place = cJSON_GetObjectItem(props, "place");
  cJSON *tm    = cJSON_GetObjectItem(props, "time");
  cJSON *u     = cJSON_GetObjectItem(props, "url");
  cJSON *ts    = cJSON_GetObjectItem(props, "tsunami");
  cJSON *al    = cJSON_GetObjectItem(props, "alert");

  /* Per-record body: the actual quake measurements. */
  cJSON *data = cJSON_CreateObject();
  if (mag) cJSON_AddNumberToObject(data, "magnitude", mag->valuedouble);
  if (place && place->valuestring)
    cJSON_AddStringToObject(data, "location", place->valuestring);

  char iso[40] = {0};
  if (tm) {
    time_t t = (time_t)(tm->valuedouble / 1000);
    strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    cJSON_AddStringToObject(data, "time", iso);
  }
  if (u && u->valuestring)  cJSON_AddStringToObject(data, "details_url", u->valuestring);
  if (ts) cJSON_AddBoolToObject(data, "tsunami_warning", ts->valueint > 0);
  if (al && al->valuestring) cJSON_AddStringToObject(data, "alert_level", al->valuestring);

  int has_geo = 0; double lat = 0, lon = 0, depth = 0;
  if (geo) {
    cJSON *co = cJSON_GetObjectItem(geo, "coordinates");
    if (co && cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 3) {
      lon   = cJSON_GetArrayItem(co, 0)->valuedouble;
      lat   = cJSON_GetArrayItem(co, 1)->valuedouble;
      depth = cJSON_GetArrayItem(co, 2)->valuedouble;
      has_geo = 1;
      cJSON_AddNumberToObject(data, "longitude", lon);
      cJSON_AddNumberToObject(data, "latitude", lat);
      cJSON_AddNumberToObject(data, "depth_km", depth);
    }
  }
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props_out = cJSON_CreateObject();
  cJSON_AddStringToObject(props_out, "service", "EARTHQUAKE_MONITOR");
  if (mag) cJSON_AddNumberToObject(props_out, "magnitude", mag->valuedouble);
  if (al && al->valuestring) cJSON_AddStringToObject(props_out, "alert_level", al->valuestring);
  cJSON_AddBoolToObject(props_out, "success", 1);
  char *pj = cJSON_PrintUnformatted(props_out);

  /* Stable per-quake key = USGS event id (globally unique, dedup-safe). */
  char rk[160];
  const char *eid = (id_j && id_j->valuestring) ? id_j->valuestring : NULL;
  if (eid) snprintf(rk, sizeof rk, "quake:%s", eid);
  else     snprintf(rk, sizeof rk, "quake:%s:%s", iso, place && place->valuestring ? place->valuestring : "?");

  char title[320];
  double m = mag ? mag->valuedouble : 0;
  snprintf(title, sizeof title, "M%.1f — %s", m,
           place && place->valuestring ? place->valuestring : "earthquake");

  intel_item it = {0};
  it.remote_key   = rk;
  it.title        = title;
  it.body         = bj;
  it.summary      = place && place->valuestring ? place->valuestring : "earthquake";
  it.published_at = iso[0] ? iso : NULL;
  it.has_geo      = has_geo;
  it.lat          = lat;
  it.lon          = lon;
  it.record_type  = "osint_service_result";
  it.properties_json = pj;
  it.tags_json    = "[\"osint-search\",\"EARTHQUAKE_MONITOR\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props_out);
  return rc >= 0 ? 1 : 0;
}

static int run_earthquake(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  char url[512];
  snprintf(url, sizeof url,
    "https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&limit=%d&orderby=time",
    20);
  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) return 0;

  int emitted = 0;
  cJSON *feats = cJSON_GetObjectItem(json, "features");
  if (feats && cJSON_IsArray(feats)) {
    int c = cJSON_GetArraySize(feats);
    for (int i = 0; i < c; i++) emitted += emit_quake(sink, cJSON_GetArrayItem(feats, i));
  }
  cJSON_Delete(json);
  return emitted > 0 ? 0 : 0;   /* honest empty is not an error */
}

static const source_def earthquake_monitor_def = {
  .id = "EARTHQUAKE_MONITOR", .collector = "osint",
  .name = "Earthquake Monitor", .name_ja = "地震モニター",
  .update_interval_sec = 0, .run = run_earthquake,
  .category = "environment", .type = "api",
  .url = "internal://osint/earthquake-monitor",
  .description = "Recent earthquakes near a location (USGS/JMA feeds).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(earthquake_monitor_def)
