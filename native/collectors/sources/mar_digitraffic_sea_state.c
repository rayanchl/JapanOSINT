/* collectors/sources/mar_digitraffic_sea_state.c
 * Fintraffic Digitraffic — SSE sea-state measurements from instrumented buoys
 * and fixed aids on Finnish fairways.
 *
 * Endpoint: https://meri.digitraffic.fi/api/sse/v1/measurements
 * Emits (all fetched): siteNumber, siteName, siteType, seaState, trend,
 *   windWaveDir, confidence, heelAngle, lightStatus, temperature, lastUpdate,
 *   and the fixed buoy/aid position from features[].geometry.coordinates.
 * Keyless. Licence: Fintraffic Digitraffic, CC BY 4.0.
 *
 * parse_notes — "Some sites carry a stale properties.lastUpdate (one sample
 * was 2022) — use lastUpdate to age the row rather than dropping the
 * coordinate." lastUpdate is carried through as published_at so the analyst
 * sees the age; the surveyed coordinate is kept.
 * parse_notes trap — Accept-Encoding: gzip is required; the platform HTTP
 * layer already sends it, so no custom header is added.
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

#define SSE_URL "https://meri.digitraffic.fi/api/sse/v1/measurements"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SSE_URL, 25000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-sea-state] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    if (!cJSON_IsObject(f)) continue;
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    if (!props) continue;
    const cJSON *sn = cJSON_GetObjectItem(props, "siteNumber");
    if (!cJSON_IsNumber(sn)) sn = cJSON_GetObjectItem(f, "siteNumber");
    if (!cJSON_IsNumber(sn)) continue;
    char site[32];
    snprintf(site, sizeof site, "%lld", (long long)sn->valuedouble);

    const char *name = jo_sv(props, "siteName");
    const char *state = jo_sv(props, "seaState");
    const char *last = jo_sv(props, "lastUpdate");

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "siteNumber", site);
    jo_copy_str(p, props, "siteName");
    jo_copy_str(p, props, "siteType");
    jo_copy_str(p, props, "seaState");
    jo_copy_str(p, props, "trend");
    jo_copy_str(p, props, "confidence");
    jo_copy_str(p, props, "lightStatus");
    jo_copy_str(p, props, "lastUpdate");
    jo_copy_num(p, props, "windWaveDir");
    jo_copy_num(p, props, "heelAngle");
    jo_copy_num(p, props, "temperature");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "fixed-buoy-position");
    }
    cJSON_AddStringToObject(p, "source", "Fintraffic Digitraffic SSE");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[200], summary[220];
    snprintf(title, sizeof title, "Sea state %s", name ? name : site);
    const cJSON *tmp = cJSON_GetObjectItem(props, "temperature");
    if (cJSON_IsNumber(tmp))
      snprintf(summary, sizeof summary, "%s · water %.1f °C",
               state ? state : "(no class)", tmp->valuedouble);
    else
      snprintf(summary, sizeof summary, "%s", state ? state : "(no class)");

    intel_item it = {0};
    it.remote_key      = site;
    it.title           = title;
    it.summary         = summary;
    it.link            = SSE_URL;
    it.published_at    = last;
    it.record_type     = "sea-state-measurement";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"sea-state\",\"buoy\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-sea-state] emitted %d\n", n);
  return 0;
}

static const source_def mar_digitraffic_sea_state_def = {
  .id = "digitraffic-sea-state", .collector = "maritime",
  .name = "Fintraffic Digitraffic — SSE sea-state buoy measurements",
  .update_interval_sec = 1800, .run = run,
  .category = "transport", .type = "api", .url = SSE_URL,
  .description = "Sea-state estimates from instrumented buoys and fixed aids on Finnish fairways: sea state class, wave direction, trend, heel angle and water temperature per site.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_digitraffic_sea_state_def)
