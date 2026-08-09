/* collectors/osint/sources/satellite_tracker.c
 * OSINT service — SATELLITE_TRACKER. On-demand (interval 0). The entity is the
 * observer: pass "lat,lon" (or set SATELLITE_OBSERVER); with neither the run
 * emits nothing. Key-gated: N2YO_API_KEY.
 *
 * PER-RECORD EMIT (only with N2YO_API_KEY): emit ONE item per satellite above
 * (remote_key="sat:<norad_id>", body=real norad_id/name/intl_designator/
 * latitude/longitude/altitude_km, has_geo + lat/lon). WITHOUT the key: emit
 * NOTHING (no note row). Empty feed: emit nothing. Never fabricate. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Emit one intel row for a single satellite. Returns 1 if emitted. */
static int emit_satellite(intel_sink *sink, cJSON *s,
                          double obs_lat, double obs_lon) {
  cJSON *id  = cJSON_GetObjectItem(s, "satid");
  if (!id) return 0;
  int norad = id->valueint;

  cJSON *nm  = cJSON_GetObjectItem(s, "satname");
  cJSON *de  = cJSON_GetObjectItem(s, "intDesignator");
  cJSON *sla = cJSON_GetObjectItem(s, "satlat");
  cJSON *slo = cJSON_GetObjectItem(s, "satlng");
  cJSON *sal = cJSON_GetObjectItem(s, "satalt");

  cJSON *body = cJSON_CreateObject();
  cJSON_AddNumberToObject(body, "norad_id", norad);
  if (nm && nm->valuestring) cJSON_AddStringToObject(body, "name", nm->valuestring);
  if (de && de->valuestring) cJSON_AddStringToObject(body, "intl_designator", de->valuestring);
  int has_geo = 0; double lat = 0, lon = 0;
  if (sla) { lat = sla->valuedouble; cJSON_AddNumberToObject(body, "latitude", lat); }
  if (slo) { lon = slo->valuedouble; cJSON_AddNumberToObject(body, "longitude", lon); }
  if (sla && slo) has_geo = 1;
  if (sal) cJSON_AddNumberToObject(body, "altitude_km", sal->valuedouble);
  cJSON_AddNumberToObject(body, "observer_lat", obs_lat);
  cJSON_AddNumberToObject(body, "observer_lon", obs_lon);
  char *bj = cJSON_PrintUnformatted(body);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SATELLITE_TRACKER");
  cJSON_AddNumberToObject(props, "norad_id", norad);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[64], title[320];
  snprintf(rk, sizeof rk, "sat:%d", norad);
  snprintf(title, sizeof title, "%s (NORAD %d)",
           (nm && nm->valuestring) ? nm->valuestring : "satellite", norad);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = (nm && nm->valuestring) ? nm->valuestring : "satellite overhead";
  it.has_geo         = has_geo;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SATELLITE_TRACKER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(body); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run_satellite(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("N2YO_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[SATELLITE_TRACKER] gated (no N2YO_API_KEY)\n");
    return 0;                              /* key-gated: emit nothing */
  }

  /* The observer used to be hardcoded to 40.7128,-74.0060 (New York) while
   * ctx->entity — the coordinate the analyst actually pivoted on — was never
   * read. Every row then carried observer_lat/observer_lon for a city nobody
   * asked about, and "above" meant above New York. Read the pivot; fall back
   * to an explicitly configured observer; otherwise emit nothing rather than
   * answer a different question than the one asked. */
  double lat, lon;
  int have_obs = 0;
  if (ctx->entity && *ctx->entity &&
      sscanf(ctx->entity, "%lf , %lf", &lat, &lon) == 2 &&
      lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180)
    have_obs = 1;
  if (!have_obs) {
    const char *o = getenv("SATELLITE_OBSERVER");   /* "lat,lon" */
    if (o && sscanf(o, "%lf , %lf", &lat, &lon) == 2 &&
        lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180)
      have_obs = 1;
  }
  if (!have_obs) {
    fprintf(stderr, "[SATELLITE_TRACKER] no observer: pass the pivot as "
                    "\"lat,lon\" or set SATELLITE_OBSERVER; emitting nothing "
                    "rather than reporting satellites above a made-up point\n");
    return 0;
  }
  char url[512];
  snprintf(url, sizeof url,
    "https://api.n2yo.com/rest/v1/satellite/above/%f/%f/0/90/18&apiKey=%s",
    lat, lon, key);
  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) return 0;

  cJSON *ab = cJSON_GetObjectItem(json, "above");
  if (ab && cJSON_IsArray(ab)) {
    int c = cJSON_GetArraySize(ab);
    for (int i = 0; i < c && i < 50; i++) {
      cJSON *s = cJSON_GetArrayItem(ab, i);
      if (s) emit_satellite(sink, s, lat, lon);
    }
  }
  cJSON_Delete(json);
  return 0;
}

static const source_def satellite_tracker_def = {
  .id = "SATELLITE_TRACKER", .collector = "osint",
  .name = "Satellite Tracker", .name_ja = "衛星追跡",
  .update_interval_sec = 0, .run = run_satellite,
  .category = "transport", .type = "api",
  .url = "internal://osint/satellite-tracker",
  .description = "Track a satellite's position by NORAD id (N2YO).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(satellite_tracker_def)
