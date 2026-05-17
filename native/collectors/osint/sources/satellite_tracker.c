/* collectors/osint/sources/satellite_tracker.c
 * OSINT service — faithful port of OSINTsaas osint_tools/environmental.c
 * (SATELLITE_TRACKER handle_satellite_tracker; entity ignored; N2YO @NYC).
 * On-demand (interval 0). N2YO satellites (key-gated N2YO_API_KEY — without
 * it OSINTsaas emits the N2YO not-found item + note, reproduced faithfully).
 * result_builder output reproduced as the documented result_builder JSON shape
 * inline (no result_builder.c in this build). Reproduces get_satellites_above
 * verbatim. success=true; satellite confidence 80. Emits ONE
 * osint_service_result row; body = {success,confidence,data} envelope, like
 * ip_geolocation.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static cJSON *get_satellites_above(http_client *http, double lat, double lon,
                                   const char *key) {
  cJSON *results = cJSON_CreateArray();
  if (!key || !*key) return results;
  char url[512];
  snprintf(url, sizeof url,
    "https://api.n2yo.com/rest/v1/satellite/above/%f/%f/0/90/18&apiKey=%s",
    lat, lon, key);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return results; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (json) {
    cJSON *ab = cJSON_GetObjectItem(json, "above");
    if (ab && cJSON_IsArray(ab)) {
      int c = cJSON_GetArraySize(ab);
      for (int i = 0; i < c && i < 50; i++) {
        cJSON *s = cJSON_GetArrayItem(ab, i);
        if (!s) continue;
        cJSON *item = cJSON_CreateObject();
        cJSON *id = cJSON_GetObjectItem(s, "satid");
        cJSON *nm = cJSON_GetObjectItem(s, "satname");
        cJSON *de = cJSON_GetObjectItem(s, "intDesignator");
        cJSON *sla = cJSON_GetObjectItem(s, "satlat");
        cJSON *slo = cJSON_GetObjectItem(s, "satlng");
        cJSON *sal = cJSON_GetObjectItem(s, "satalt");
        if (id) cJSON_AddNumberToObject(item, "norad_id", id->valueint);
        if (nm && nm->valuestring) cJSON_AddStringToObject(item, "name", nm->valuestring);
        if (de && de->valuestring) cJSON_AddStringToObject(item, "intl_designator", de->valuestring);
        if (sla) cJSON_AddNumberToObject(item, "latitude", sla->valuedouble);
        if (slo) cJSON_AddNumberToObject(item, "longitude", slo->valuedouble);
        if (sal) cJSON_AddNumberToObject(item, "altitude_km", sal->valuedouble);
        cJSON_AddItemToArray(results, item);
      }
    }
    cJSON_Delete(json);
  }
  return results;
}

/* result_builder reproduction. */
static void rb_add(cJSON *results, const char *source, int found,
                   int confidence, cJSON *data /*owned or NULL*/,
                   const char *url, int *total, int *fc) {
  cJSON *it = cJSON_CreateObject();
  cJSON_AddStringToObject(it, "source", source);
  cJSON_AddBoolToObject(it, "found", found);
  if (data) cJSON_AddItemToObject(it, "data", data);
  else cJSON_AddNullToObject(it, "data");
  cJSON_AddNumberToObject(it, "confidence", confidence);
  cJSON_AddStringToObject(it, "detection_method", "direct");
  if (url) cJSON_AddStringToObject(it, "url", url);
  cJSON_AddItemToArray(results, it);
  (*total)++;
  if (found) (*fc)++;
}

static int emit_one(intel_sink *sink, const char *svc, const char *entity,
                    const char *qtype, int confidence, cJSON *results,
                    int total, int fc) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", entity ? entity : "");
  cJSON_AddStringToObject(root, "query_type", qtype);
  cJSON_AddItemToObject(root, "results", results);
  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "total_sources", total);
  cJSON_AddNumberToObject(summary, "found_count", fc);
  cJSON *bd = cJSON_CreateObject();
  cJSON_AddNumberToObject(bd, "direct", total);
  cJSON_AddNumberToObject(bd, "llama", 0);
  cJSON_AddItemToObject(summary, "detection_breakdown", bd);
  cJSON_AddItemToObject(root, "summary", summary);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);          /* OSINTsaas: always true */
  cJSON_AddNumberToObject(env, "confidence", confidence);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", entity ? entity : "");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "%s:%s", svc, entity ? entity : qtype);
  char title[360];
  snprintf(title, sizeof title, "%s — %s", svc, entity ? entity : qtype);
  char tags[96];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = "environmental result";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run_satellite(const source_ctx *ctx, intel_sink *sink) {
  cJSON *results = cJSON_CreateArray();
  int total = 0, fc = 0;
  const char *key = getenv("N2YO_API_KEY");
  if (key && *key) {
    double lat = 40.7128, lon = -74.0060;   /* NYC, == upstream */
    cJSON *sats = get_satellites_above(ctx->http, lat, lon, key);
    int count = cJSON_GetArraySize(sats);
    if (count > 0) {
      cJSON *d = cJSON_CreateObject();
      cJSON_AddItemToObject(d, "satellites_overhead", sats);
      cJSON_AddNumberToObject(d, "count", count);
      cJSON_AddNumberToObject(d, "observer_lat", lat);
      cJSON_AddNumberToObject(d, "observer_lon", lon);
      rb_add(results, "N2YO", 1, 90, d,
             "https://www.n2yo.com/", &total, &fc);
    } else {
      rb_add(results, "N2YO", 0, 0, NULL, NULL, &total, &fc);
      cJSON_Delete(sats);
    }
  } else {
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "note",
      "Set N2YO_API_KEY for satellite tracking data");
    rb_add(results, "N2YO", 0, 0, d, NULL, &total, &fc);
  }
  return emit_one(sink, "SATELLITE_TRACKER",
                  ctx->entity ? ctx->entity : "overhead",
                  "satellite", 80, results, total, fc);
}

static const source_def satellite_tracker_def = {
  .id = "SATELLITE_TRACKER", .collector = "osint",
  .name = "Satellite Tracker", .name_ja = "衛星追跡",
  .update_interval_sec = 0, .run = run_satellite,
};
REGISTER_SOURCE(satellite_tracker_def)
