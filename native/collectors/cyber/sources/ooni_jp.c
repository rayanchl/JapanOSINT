/* collectors/cyber/sources/ooni_jp.c
 * Port of server/src/collectors/ooniJp.js (createThreatIntelCollector).
 * Keyless: OONI measurements probe_cc=JP, recent 200, TOKYO points. */
#include "../../../source.h"
#include "../../../lib/threatintel.h"
#include "../../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
#define OONI_URL "https://api.ooni.io/api/v1/measurements?probe_cc=JP&limit=200&order_by=measurement_start_time&order=desc"

static void put(cJSON *p, const char *k, cJSON *r, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(r, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *json = feed_get_json_h(ctx->http, OONI_URL, hdrs, 20000);
  if (!json) return NULL;

  cJSON *results = cJSON_GetObjectItem(json, "results");
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(results)) {
    cJSON *m;
    cJSON_ArrayForEach(m, results) {
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      put(pr, "measurement_uid", m, "measurement_uid");
      put(pr, "report_id", m, "report_id");
      put(pr, "test_name", m, "test_name");
      put(pr, "input", m, "input");
      put(pr, "probe_cc", m, "probe_cc");
      put(pr, "probe_asn", m, "probe_asn");
      put(pr, "anomaly", m, "anomaly");
      put(pr, "confirmed", m, "confirmed");
      put(pr, "failure", m, "failure");
      put(pr, "test_start_time", m, "test_start_time");
      put(pr, "url", m, "measurement_url");
      cJSON_AddStringToObject(pr, "source", "ooni");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def ooni_jp_def = {
  .id = "ooni-jp", .collector = "cyber",
  .name = "OONI Explorer (JP)", .name_ja = "OONI Explorer 日本",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(ooni_jp_def)
