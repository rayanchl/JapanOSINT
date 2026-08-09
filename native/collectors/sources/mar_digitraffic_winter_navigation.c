/* collectors/sources/mar_digitraffic_winter_navigation.c
 * Fintraffic Digitraffic — Baltic winter-navigation ports with their ice
 * restrictions and traffic suspensions (FI/SE/RU/DK/EE).
 *
 * Endpoint: https://meri.digitraffic.fi/api/winter-navigation/v2/locations
 * Emits (all fetched): name, type, locodeList, nationality, winterport, the
 *   restriction/suspension entries (startTime, endTime, textCompilation) and
 *   the port point from features[].geometry.coordinates = [lon, lat].
 * Keyless. Licence: Fintraffic Digitraffic, CC BY 4.0.
 *
 * parse_notes — "the port point, which is the subject of the record" →
 * record_type "winter-navigation-port"; this is a place, not a vessel.
 * parse_notes — "restrictions[] is empty outside the ice season; an empty
 * array is an honest 'no restriction', not an error" → still emitted, and the
 * run returns 0 either way.
 * parse_notes — companion endpoints /vessels and /dirways were live but empty
 * on 2026-08-01 (northern summer); deliberately NOT wired as separate
 * always-empty sources.
 * parse_notes trap — Accept-Encoding: gzip is required; the platform HTTP
 * layer already sends it, so no custom header is added.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define WN_URL "https://meri.digitraffic.fi/api/winter-navigation/v2/locations"

static inline void copy_periods(cJSON *dst, const cJSON *src, const char *key) {
  const cJSON *arr = cJSON_GetObjectItem(src, key);
  if (!cJSON_IsArray(arr)) return;
  cJSON *out = cJSON_CreateArray();
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    cJSON *o = cJSON_CreateObject();
    const char *s;
    if ((s = jo_sv(e, "startTime")))       cJSON_AddStringToObject(o, "startTime", s);
    if ((s = jo_sv(e, "endTime")))         cJSON_AddStringToObject(o, "endTime", s);
    if ((s = jo_sv(e, "textCompilation"))) cJSON_AddStringToObject(o, "text", s);
    cJSON_AddItemToArray(out, o);
  }
  cJSON_AddItemToObject(dst, key, out);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WN_URL, 25000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-winter-navigation] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    if (!cJSON_IsObject(f)) continue;
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    if (!props) continue;
    const char *name = jo_sv(props, "name");
    const char *locodes = jo_sv(props, "locodeList");
    if (!name && !locodes) continue;
    const char *key = locodes ? locodes : name;

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    const cJSON *rr = cJSON_GetObjectItem(props, "restrictions");
    const cJSON *ss = cJSON_GetObjectItem(props, "suspensions");
    int nres = cJSON_IsArray(rr) ? cJSON_GetArraySize(rr) : 0;
    int nsus = cJSON_IsArray(ss) ? cJSON_GetArraySize(ss) : 0;

    cJSON *p = cJSON_CreateObject();
    if (name)    cJSON_AddStringToObject(p, "name", name);
    if (locodes) cJSON_AddStringToObject(p, "locodeList", locodes);
    const char *s;
    if ((s = jo_sv(props, "type")))        cJSON_AddStringToObject(p, "type", s);
    if ((s = jo_sv(props, "nationality"))) cJSON_AddStringToObject(p, "nationality", s);
    const cJSON *wp = cJSON_GetObjectItem(props, "winterport");
    if (cJSON_IsBool(wp)) cJSON_AddBoolToObject(p, "winterport", cJSON_IsTrue(wp));
    copy_periods(p, props, "restrictions");
    copy_periods(p, props, "suspensions");
    cJSON_AddNumberToObject(p, "restriction_count", nres);
    cJSON_AddNumberToObject(p, "suspension_count", nsus);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "port-reference-point");
    }
    cJSON_AddStringToObject(p, "source",
      "Fintraffic Digitraffic winter navigation");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[220], summary[200];
    snprintf(title, sizeof title, "Winter navigation: %s%s%s",
             name ? name : key, locodes ? " " : "", locodes ? locodes : "");
    snprintf(summary, sizeof summary,
             "%d restriction(s), %d suspension(s)", nres, nsus);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = WN_URL;
    it.record_type     = "winter-navigation-port";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"ice\",\"port\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-winter-navigation] emitted %d\n", n);
  return 0;
}

static const source_def mar_digitraffic_winter_navigation_def = {
  .id = "digitraffic-winter-navigation", .collector = "maritime",
  .name = "Fintraffic Digitraffic — Baltic winter navigation port restrictions",
  .update_interval_sec = 21600, .run = run,
  .category = "transport", .type = "api", .url = WN_URL,
  .description = "Baltic winter-navigation ports with their ice restrictions and traffic suspensions (dates plus the official restriction text), covering FI/SE/RU/DK/EE ports.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_digitraffic_winter_navigation_def)
