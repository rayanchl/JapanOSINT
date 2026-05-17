/* collectors/cyber/sources/shadowserver_jp.c
 * Port of server/src/collectors/shadowserverJp.js (createThreatIntelCollector).
 * Keyless. Shadowserver public time-series, last point per dataset, TOKYO. */
#include "../../../source.h"
#include "../../../lib/threatintel.h"
#include "../../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
#define SS_BASE "https://dashboard.shadowserver.org/api/"

static const char *const DATASETS[] = {
  "compromised_website", "ics", "sinkhole_http", "scan_rdp", "scan_smb",
  "ssl_freak_vulnerable", "ssl_poodle_vulnerable",
  "open_elasticsearch", "open_redis", "open_mongodb",
};
#define N_DATASETS (int)(sizeof DATASETS / sizeof DATASETS[0])

static void add_tokyo_geom(cJSON *f) {
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);
}

/* a ?? b ?? null, both from `last`. */
static cJSON *coalesce2(cJSON *last, const char *ka, const char *kb) {
  cJSON *v = cJSON_GetObjectItem(last, ka);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  v = cJSON_GetObjectItem(last, kb);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  return cJSON_CreateNull();
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  const char *de = getenv("SHADOWSERVER_DAYS");
  long days = (de && *de) ? strtol(de, NULL, 10) : 14;
  if (days <= 0) days = 14;

  cJSON *features = cJSON_CreateArray();
  for (int d = 0; d < N_DATASETS; d++) {
    const char *dataset = DATASETS[d];
    char url[384];
    snprintf(url, sizeof url,
      "%s?endpoint=stats/time-series&dataset=%s&geo=JP&days=%ld&format=json",
      SS_BASE, dataset, days);
    const char *hdrs[] = { "Accept: application/json", NULL };
    cJSON *json = feed_get_json_h(ctx->http, url, hdrs, 15000);

    /* arr = Array.isArray(json) ? json : (json?.results || []) */
    cJSON *arr = NULL;
    if (cJSON_IsArray(json)) arr = json;
    else if (json) arr = cJSON_GetObjectItem(json, "results");
    int len = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
    cJSON *last = (cJSON_IsArray(arr) && len) ? cJSON_GetArrayItem(arr, len - 1)
                                              : NULL;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    add_tokyo_geom(f);
    cJSON *p = cJSON_CreateObject();
    if (last) {
      cJSON_AddStringToObject(p, "dataset", dataset);
      cJSON_AddItemToObject(p, "date", coalesce2(last, "date", "timestamp"));
      cJSON_AddItemToObject(p, "count", coalesce2(last, "count", "value"));
      cJSON *u = cJSON_GetObjectItem(last, "unique_ips");
      cJSON_AddItemToObject(p, "unique_ips",
        (u && !cJSON_IsNull(u)) ? cJSON_Duplicate(u, 1) : cJSON_CreateNull());
      cJSON_AddNumberToObject(p, "series_length", len);
      cJSON_AddStringToObject(p, "source", "shadowserver_dashboard");
    } else {
      cJSON_AddStringToObject(p, "dataset", dataset);
      /* r.err || 'empty' — HTTP fail surfaces no JS body here → "empty". */
      cJSON_AddStringToObject(p, "err", json ? "empty" : "fetch_failed");
      cJSON_AddStringToObject(p, "source", "shadowserver_dashboard_empty");
    }
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    if (json) cJSON_Delete(json);
  }
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def shadowserver_jp_def = {
  .id = "shadowserver-jp", .collector = "cyber",
  .name = "Shadowserver (JP stats)", .name_ja = "Shadowserver (日本統計)",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(shadowserver_jp_def)
