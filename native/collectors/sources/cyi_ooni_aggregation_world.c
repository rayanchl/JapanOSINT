/* collectors/sources/cyi_ooni_aggregation_world.c
 * OONI global censorship aggregation — per-country web_connectivity outcomes.
 * Endpoint: https://api.ooni.io/api/v1/aggregation
 *           ?since=<date>&until=<date>&axis_x=probe_cc&test_name=web_connectivity
 * Keyless. Rows are result[] (parse_notes).
 * Emits per country: measurement_count, anomaly_count, confirmed_count,
 * failure_count, ok_count — BOTH anomaly and confirmed are carried because
 * "anomaly_count without confirmed_count is a signal, not proof".
 * No coordinates upstream -> has_geo 0 (R2).
 * Licence: OONI data is CC BY-SA 4.0 / public-domain measurements; attribution
 * to OONI required.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void ymd(time_t t, char *out, size_t n) {
  struct tm tmv;
#if defined(_WIN32)
  if (gmtime_s(&tmv, &t) != 0) { snprintf(out, n, "1970-01-01"); return; }
#else
  if (!gmtime_r(&t, &tmv)) { snprintf(out, n, "1970-01-01"); return; }
#endif
  strftime(out, n, "%Y-%m-%d", &tmv);
}

static double numv(const cJSON *o, const char *k, int *ok) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) { *ok = 1; return v->valuedouble; }
  *ok = 0; return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  char since[16], until[16];
  ymd(now - 7 * 24 * 3600, since, sizeof since);
  ymd(now + 24 * 3600, until, sizeof until);

  char url[384];
  snprintf(url, sizeof url,
           "https://api.ooni.io/api/v1/aggregation?since=%s&until=%s"
           "&axis_x=probe_cc&test_name=web_connectivity", since, until);

  cJSON *doc = feed_get_json(ctx->http, url, 40000);
  if (!doc) { fprintf(stderr, "[ooni-aggregation-world] fetch/parse failed\n"); return -1; }

  const cJSON *rows = cJSON_GetObjectItem(doc, "result");
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const cJSON *cc = cJSON_GetObjectItem(r, "probe_cc");
    if (!cJSON_IsString(cc) || !cc->valuestring || !cc->valuestring[0]) continue;

    int hm = 0, ha = 0, hc = 0, hf = 0, ho = 0;
    double m = numv(r, "measurement_count", &hm);
    double an = numv(r, "anomaly_count", &ha);
    double co = numv(r, "confirmed_count", &hc);
    double fa = numv(r, "failure_count", &hf);
    double okc = numv(r, "ok_count", &ho);
    if (!hm) continue;                        /* no measured value -> no row */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "probe_cc", cc->valuestring);
    cJSON_AddNumberToObject(p, "measurement_count", m);
    if (ha) cJSON_AddNumberToObject(p, "anomaly_count", an);
    if (hc) cJSON_AddNumberToObject(p, "confirmed_count", co);
    if (hf) cJSON_AddNumberToObject(p, "failure_count", fa);
    if (ho) cJSON_AddNumberToObject(p, "ok_count", okc);
    cJSON_AddStringToObject(p, "window_since", since);
    cJSON_AddStringToObject(p, "window_until", until);
    cJSON_AddStringToObject(p, "test_name", "web_connectivity");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[128];
    snprintf(title, sizeof title, "OONI web connectivity — %s", cc->valuestring);
    char summary[256];
    snprintf(summary, sizeof summary,
             "%.0f measurements · %.0f anomalies · %.0f confirmed blocks · %.0f failures",
             m, an, co, fa);
    char key[64];
    snprintf(key, sizeof key, "%s|%s", cc->valuestring, since);
    char link[192];
    snprintf(link, sizeof link,
             "https://explorer.ooni.org/search?probe_cc=%s&test_name=web_connectivity",
             cc->valuestring);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "ooni-country-aggregate";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"censorship\",\"ooni\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[ooni-aggregation-world] emitted %d\n", n);
  return 0;
}

static const source_def cyi_ooni_aggregation_world_def = {
  .id = "ooni-aggregation-world", .collector = "cyber",
  .name = "OONI global censorship aggregation",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.ooni.io/api/v1/aggregation",
  .description = "Country-by-country blocking rates from OONI probes: confirmed blocks, anomalies and successes per country per window.",
  .license = "OONI — CC BY-SA 4.0 / public-domain measurements; attribution required.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_ooni_aggregation_world_def)
