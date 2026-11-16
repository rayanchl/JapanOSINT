/* collectors/transport/sources/flightradar_jp.c
 * Port of server/src/collectors/flightradarJp.js.
 * FR24 public zone feed for the Japan bbox — KEY-FREE. The reply is an object
 * whose values are per-aircraft arrays (plus full_count/version scalars). On
 * failure: honest empty FeatureCollection — never fabricated aircraft. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* FR24 feed bounds order: lat_max,lat_min,lon_min,lon_max */
/* RE-POINTED 2026-11-16: data-live -> data-cloud. Measured that day, one
 * request each: data-live.flightradar24.com answers this path 302 to
 * www.flightradar24.com/static/legacy/feed.json, which is a 403 Cloudflare
 * "Just a moment..." challenge (the 5,683 bytes this row had been fetching);
 * data-cloud.flightradar24.com answers the SAME path and query HTTP 200
 * application/json, 98,521 bytes, {"full_count":15858,"version":4,<id>:[19
 * cells]...} with 769 aircraft, every one inside the bbox (lat 24.88..45.78,
 * lon 122.00..145.74). It answers the engine's own JO_USER_AGENT (200), and
 * the bounds filter is honoured: the control bounds=1,0,-140,-139 (empty
 * Pacific) answers {"full_count":15835,"version":4} and no aircraft. */
#define FEED_URL \
  "https://data-cloud.flightradar24.com/zones/fcgi/feed.js?bounds=46,24,122,146" \
  "&faa=1&satellite=1&mlat=1&flarm=1&adsb=1&gnd=1&air=1&vehicles=1" \
  "&estimated=1&maxage=14400"

/* +v[i]: JSON number passthrough → finite, else not. */
static int num_at(cJSON *arr, int i, double *out) {
  cJSON *e = cJSON_GetArrayItem(arr, i);
  if (e && cJSON_IsNumber(e) && isfinite(e->valuedouble)) {
    *out = e->valuedouble; return 1;
  }
  if (e && cJSON_IsString(e) && e->valuestring[0]) {
    char *end; double d = strtod(e->valuestring, &end);
    if (end != e->valuestring && isfinite(d)) { *out = d; return 1; }
  }
  return 0;
}

/* v[i] || null  (string, "" treated as falsy → next/null). */
static void str_or(cJSON *p, const char *k, cJSON *arr, int i, int j) {
  cJSON *a = cJSON_GetArrayItem(arr, i);
  if (a && cJSON_IsString(a) && a->valuestring[0]) {
    cJSON_AddStringToObject(p, k, a->valuestring); return;
  }
  if (j >= 0) {
    cJSON *b = cJSON_GetArrayItem(arr, j);
    if (b && cJSON_IsString(b) && b->valuestring[0]) {
      cJSON_AddStringToObject(p, k, b->valuestring); return;
    }
  }
  cJSON_AddItemToObject(p, k, cJSON_CreateNull());
}

static void num_or_null(cJSON *p, const char *k, cJSON *arr, int i) {
  double d;
  if (num_at(arr, i, &d)) cJSON_AddNumberToObject(p, k, d);
  else cJSON_AddItemToObject(p, k, cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Referer: https://www.flightradar24.com/", NULL };
  cJSON *data = feed_get_json_h(ctx->http, FEED_URL, hdrs, 12000);
  /* A failed fetch is a FAILED RUN (2026-11-16). This returned 0 under a note
   * calling a blocked endpoint an "honest empty", so a Cloudflare challenge
   * page read `rc=0 records=0 stored=0 bytes=5683 requests=1` — byte for byte
   * a sky with no aircraft, and nothing in the database said otherwise. A
   * reply that is not 2xx JSON is never an empty sky: the feed's own empty
   * answer is the object {"full_count":N,"version":4}, which parses, emits
   * nothing and still returns 0 below. With -1 the scheduler files the
   * collector-fetch-failure record naming hosts and bytes. */
  if (!data) {
    fprintf(stderr, "[flightradar-jp] feed fetch failed/blocked: %s\n", FEED_URL);
    return -1;
  }

  cJSON *features = cJSON_CreateArray();
  if (data && cJSON_IsObject(data)) {
    cJSON *v;
    cJSON_ArrayForEach(v, data) {
      const char *k = v->string;
      if (!k || strcmp(k, "full_count") == 0 || strcmp(k, "version") == 0)
        continue;
      if (!cJSON_IsArray(v)) continue;
      double lat, lon;
      if (!num_at(v, 1, &lat) || !num_at(v, 2, &lon)) continue;

      cJSON *f = gj_point_feature(lon, lat);

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      cJSON_AddStringToObject(p, "flight_id", k);
      str_or(p, "callsign", v, 16, 13);
      str_or(p, "registration", v, 9, -1);
      str_or(p, "aircraft", v, 8, -1);
      str_or(p, "origin", v, 11, -1);
      str_or(p, "destination", v, 12, -1);
      num_or_null(p, "altitude_ft", v, 4);
      num_or_null(p, "speed_kt", v, 5);
      num_or_null(p, "track", v, 3);
      cJSON_AddStringToObject(p, "source", "fr24_feed");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[flightradar-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def flightradar_jp_def = {
  .id = "flightradar-jp", .collector = "transport",
  .name = "FlightRadar24 Japan", .name_ja = "FlightRadar24 日本",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(flightradar_jp_def)
