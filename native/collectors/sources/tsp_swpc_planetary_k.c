/* collectors/sources/tsp_swpc_planetary_k.c
 * NOAA SWPC estimated planetary K index, 1-minute cadence — the single number
 * that predicts HF radio blackouts, GNSS degradation and satellite drag spikes.
 * Endpoint: https://services.swpc.noaa.gov/json/planetary_k_index_1m.json
 *   (keyless)
 * Emits per sample: time_tag (UTC), kp_index (integer bin), estimated_kp
 *   (float) and kp (the coded string, e.g. "1P" / "0Z").
 * Geo: none — a planetary index has no location (R2).
 *
 * parse_notes honoured:
 *  - "Flat JSON array, oldest-first; take the tail for 'now'": we emit only the
 *    last TAIL_SAMPLES entries, i.e. the most recent ~hour, on every 15-minute
 *    run. STATED BOUND: the file carries ~7 days of 1-minute samples and the
 *    history does not change, so re-emitting all 10k of them every run would
 *    be churn, not data.
 *  - "time_tag has no timezone suffix but is UTC": a 'Z' is appended for the
 *    published_at timestamp and the raw value is kept in properties.
 * Licence: NOAA SWPC — US Government public domain, keyless.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KP_URL "https://services.swpc.noaa.gov/json/planetary_k_index_1m.json"
#define TAIL_SAMPLES 60

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, KP_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[swpc-planetary-k] fetch failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[swpc-planetary-k] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }
  int total = cJSON_GetArraySize(doc);
  int start = total > TAIL_SAMPLES ? total - TAIL_SAMPLES : 0;

  int n = 0;
  for (int i = start; i < total; i++) {
    cJSON *s = cJSON_GetArrayItem(doc, i);
    if (!cJSON_IsObject(s)) continue;
    const char *tt = jo_sv(s, "time_tag");
    if (!tt) continue;
    double kpi = 0, ekp = 0;
    int has_kpi = jo_num(s, "kp_index", &kpi);
    int has_ekp = jo_num(s, "estimated_kp", &ekp);
    const char *kpc = jo_sv(s, "kp");
    if (!has_kpi && !has_ekp && !kpc) continue;   /* nothing measured -> no row */

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "time_tag_utc", tt);
    if (has_kpi) cJSON_AddNumberToObject(pr, "kp_index", kpi);
    if (has_ekp) cJSON_AddNumberToObject(pr, "estimated_kp", ekp);
    if (kpc)     cJSON_AddStringToObject(pr, "kp_code", kpc);
    cJSON_AddStringToObject(pr, "source", "NOAA SWPC planetary_k_index_1m");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[160], summary[160], ts[48];
    snprintf(ts, sizeof ts, "%sZ", tt);       /* time_tag is UTC, unsuffixed */
    if (has_ekp) snprintf(title, sizeof title, "Estimated planetary Kp %.2f at %s", ekp, tt);
    else         snprintf(title, sizeof title, "Planetary K index %s at %s",
                          kpc ? kpc : "", tt);
    snprintf(summary, sizeof summary, "%s%.0f%s%s",
             has_kpi ? "Kp bin " : "", has_kpi ? kpi : 0.0,
             kpc ? " · code " : "", kpc ? kpc : "");

    intel_item it = {0};
    it.remote_key      = tt;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.swpc.noaa.gov/products/planetary-k-index";
    it.lang            = "en";
    it.published_at    = ts;
    it.record_type     = "geomagnetic-index";
    it.properties_json = pj;
    it.tags_json       = "[\"space-weather\",\"geomagnetic\",\"noaa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[swpc-planetary-k] emitted %d of %d samples (tail)\n", n, total);
  return 0;
}

static const source_def tsp_swpc_planetary_k_def = {
  .id = "swpc-planetary-k", .collector = "satellite",
  .name = "NOAA SWPC estimated planetary K index (1-minute)",
  .update_interval_sec = 900, .run = run,
  .category = "satellite", .type = "api", .url = KP_URL,
  .description = "Near-real-time geomagnetic activity at one-minute cadence — the index that predicts HF radio blackouts, GNSS positioning degradation and satellite drag spikes.",
  .license = "NOAA SWPC — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_swpc_planetary_k_def)
