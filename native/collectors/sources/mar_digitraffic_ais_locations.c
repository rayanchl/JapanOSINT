/* collectors/sources/mar_digitraffic_ais_locations.c
 * Fintraffic Digitraffic — live AIS vessel positions (Finnish/Baltic AIS
 * coverage area). THE ONLY keyless AIS position feed in this batch: no other
 * source here carries a vessel position, and a port point is never one.
 *
 * Endpoint: https://meri.digitraffic.fi/api/ais/v1/locations
 *           ?radius=50&latitude=60.15&longitude=24.95
 * Emits (all fetched): mmsi, sog, cog, heading, navStat, rot, posAcc, raim,
 *   timestampExternal (epoch ms) rendered as position_time ISO-8601, and the
 *   AIS-broadcast position from features[].geometry.coordinates = [lon, lat].
 * Keyless.
 * Licence: Fintraffic Digitraffic, CC BY 4.0.
 *
 * parse_notes trap — "Digitraffic returns HTTP 406 on any request lacking
 * Accept-Encoding: gzip. The platform's HTTP layer already sends
 * Accept-Encoding, so this works as-is — do not add a custom header for it."
 * core/httpclient.c sets CURLOPT_ACCEPT_ENCODING(""); no header is added here.
 *
 * parse_notes trap — "properties.timestamp is the AIS second-of-minute field
 * (0-59), NOT a time": only timestampExternal is used for published_at.
 *
 * parse_notes — "If a vessel has no valid position it simply does not appear
 * as a feature ... Omit rows with missing geometry": features without numeric
 * coordinates are skipped, never backfilled (R2).
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

#define AIS_URL "https://meri.digitraffic.fi/api/ais/v1/locations" \
                "?radius=50&latitude=60.15&longitude=24.95"

static inline void add_bool(cJSON *dst, const cJSON *src, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(src, k);
  if (cJSON_IsBool(v)) cJSON_AddBoolToObject(dst, k, cJSON_IsTrue(v));
}
static inline const char *iso_ms(double ms, char *buf, size_t n) {
  if (!(ms > 0)) return NULL;
  time_t t = (time_t)(ms / 1000.0);
  struct tm g;
  if (!gmtime_r(&t, &g)) return NULL;
  if (strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &g) == 0) return NULL;
  return buf;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, AIS_URL, 25000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-ais-locations] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    if (!cJSON_IsObject(f)) continue;
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    cJSON *mm = cJSON_GetObjectItem(f, "mmsi");
    if (!cJSON_IsNumber(mm) && props) mm = cJSON_GetObjectItem(props, "mmsi");
    if (!cJSON_IsNumber(mm)) continue;

    cJSON *geom = cJSON_GetObjectItem(f, "geometry");
    cJSON *co = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
    cJSON *lonj = cJSON_GetArrayItem(co, 0), *latj = cJSON_GetArrayItem(co, 1);
    if (!cJSON_IsNumber(lonj) || !cJSON_IsNumber(latj)) continue; /* no position */

    char mmsi[32];
    snprintf(mmsi, sizeof mmsi, "%lld", (long long)mm->valuedouble);

    char ptime[40];
    const char *pt = NULL;
    if (props) {
      const cJSON *te = cJSON_GetObjectItem(props, "timestampExternal");
      if (cJSON_IsNumber(te)) pt = iso_ms(te->valuedouble, ptime, sizeof ptime);
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "mmsi", mmsi);
    if (props) {
      jo_copy_num(p, props, "sog");
      jo_copy_num(p, props, "cog");
      jo_copy_num(p, props, "heading");
      jo_copy_num(p, props, "navStat");
      jo_copy_num(p, props, "rot");
      jo_copy_num(p, props, "timestampExternal");
      add_bool(p, props, "posAcc");
      add_bool(p, props, "raim");
    }
    if (pt) cJSON_AddStringToObject(p, "position_time", pt);
    cJSON_AddNumberToObject(p, "lat", latj->valuedouble);
    cJSON_AddNumberToObject(p, "lon", lonj->valuedouble);
    cJSON_AddStringToObject(p, "geo_precision", "ais-report");
    cJSON_AddStringToObject(p, "source", "Fintraffic Digitraffic AIS");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[96], summary[160], link[160];
    snprintf(title, sizeof title, "AIS position MMSI %s", mmsi);
    const cJSON *sog = props ? cJSON_GetObjectItem(props, "sog") : NULL;
    if (cJSON_IsNumber(sog))
      snprintf(summary, sizeof summary, "%.5f, %.5f · SOG %.1f kn",
               latj->valuedouble, lonj->valuedouble, sog->valuedouble);
    else
      snprintf(summary, sizeof summary, "%.5f, %.5f",
               latj->valuedouble, lonj->valuedouble);
    snprintf(link, sizeof link,
             "https://meri.digitraffic.fi/api/ais/v1/vessels/%s", mmsi);

    intel_item it = {0};
    it.remote_key      = mmsi;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.published_at    = pt;
    it.record_type     = "ais-vessel-position";
    it.has_geo         = 1;
    it.lat             = latj->valuedouble;
    it.lon             = lonj->valuedouble;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"ais\",\"vessel\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-ais-locations] emitted %d\n", n);
  return 0;                                     /* fetched fine (R3) */
}

static const source_def mar_digitraffic_ais_locations_def = {
  .id = "digitraffic-ais-locations", .collector = "maritime",
  .name = "Fintraffic Digitraffic — live AIS vessel positions (Baltic)",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api", .url = AIS_URL,
  .description = "Live AIS class-A/B position reports for the Finnish/Baltic AIS coverage area: MMSI, speed/course/heading, navigational status and the AIS receive timestamp, one row per vessel with its broadcast position.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_digitraffic_ais_locations_def)
