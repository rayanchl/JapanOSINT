/* collectors/osint/sources/earthquake_monitor.c
 * OSINT service — faithful port of OSINTsaas osint_tools/environmental.c
 * (EARTHQUAKE_MONITOR handle_earthquake_service; entity ignored; global USGS).
 * On-demand (interval 0). No API key; USGS earthquakes (no key).
 * result_builder output reproduced as the documented result_builder JSON shape
 * inline (no result_builder.c in this build). Reproduces get_earthquakes
 * verbatim. success=true; earthquake confidence 95. Emits ONE
 * osint_service_result row; body = {success,confidence,data} envelope, like
 * ip_geolocation.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* get_earthquakes: USGS GeoJSON. */
static cJSON *get_earthquakes(http_client *http, int limit) {
  cJSON *results = cJSON_CreateArray();
  char url[512];
  snprintf(url, sizeof url,
    "https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&limit=%d&orderby=time",
    limit);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return results; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (json) {
    cJSON *feats = cJSON_GetObjectItem(json, "features");
    if (feats && cJSON_IsArray(feats)) {
      int c = cJSON_GetArraySize(feats);
      for (int i = 0; i < c && i < limit; i++) {
        cJSON *ft = cJSON_GetArrayItem(feats, i);
        if (!ft) continue;
        cJSON *props = cJSON_GetObjectItem(ft, "properties");
        cJSON *geo = cJSON_GetObjectItem(ft, "geometry");
        if (props) {
          cJSON *item = cJSON_CreateObject();
          cJSON *mag = cJSON_GetObjectItem(props, "mag");
          cJSON *place = cJSON_GetObjectItem(props, "place");
          cJSON *tm = cJSON_GetObjectItem(props, "time");
          cJSON *u = cJSON_GetObjectItem(props, "url");
          cJSON *ts = cJSON_GetObjectItem(props, "tsunami");
          cJSON *al = cJSON_GetObjectItem(props, "alert");
          if (mag) cJSON_AddNumberToObject(item, "magnitude", mag->valuedouble);
          if (place && place->valuestring)
            cJSON_AddStringToObject(item, "location", place->valuestring);
          if (tm) {
            time_t t = (time_t)(tm->valuedouble / 1000);
            char date[32];
            strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S UTC", gmtime(&t));
            cJSON_AddStringToObject(item, "time", date);
          }
          if (u && u->valuestring)
            cJSON_AddStringToObject(item, "details_url", u->valuestring);
          if (ts) cJSON_AddBoolToObject(item, "tsunami_warning", ts->valueint > 0);
          if (al && al->valuestring)
            cJSON_AddStringToObject(item, "alert_level", al->valuestring);
          if (geo) {
            cJSON *co = cJSON_GetObjectItem(geo, "coordinates");
            if (co && cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 3) {
              cJSON_AddNumberToObject(item, "longitude",
                cJSON_GetArrayItem(co, 0)->valuedouble);
              cJSON_AddNumberToObject(item, "latitude",
                cJSON_GetArrayItem(co, 1)->valuedouble);
              cJSON_AddNumberToObject(item, "depth_km",
                cJSON_GetArrayItem(co, 2)->valuedouble);
            }
          }
          cJSON_AddItemToArray(results, item);
        }
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

static int run_earthquake(const source_ctx *ctx, intel_sink *sink) {
  cJSON *results = cJSON_CreateArray();
  int total = 0, fc = 0;
  cJSON *recent = get_earthquakes(ctx->http, 20);
  int count = cJSON_GetArraySize(recent);
  if (count > 0) {
    cJSON *d = cJSON_CreateObject();
    cJSON_AddItemToObject(d, "recent_earthquakes", recent);
    cJSON_AddNumberToObject(d, "count", count);
    rb_add(results, "USGS", 1, 90, d,
           "https://earthquake.usgs.gov", &total, &fc);
  } else {
    rb_add(results, "USGS", 0, 0, NULL, NULL, &total, &fc);
    cJSON_Delete(recent);
  }
  return emit_one(sink, "EARTHQUAKE_MONITOR",
                  ctx->entity ? ctx->entity : "global",
                  "earthquake", 95, results, total, fc);
}

static const source_def earthquake_monitor_def = {
  .id = "EARTHQUAKE_MONITOR", .collector = "osint",
  .name = "Earthquake Monitor", .name_ja = "地震モニター",
  .update_interval_sec = 0, .run = run_earthquake,
};
REGISTER_SOURCE(earthquake_monitor_def)
