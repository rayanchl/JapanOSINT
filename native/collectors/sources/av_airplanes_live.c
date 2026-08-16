/* collectors/sources/av_airplanes_live.c
 * airplanes.live unfiltered ADS-B/MLAT aggregation — three net-new endpoints.
 *
 *   airplanes-live-mil   GET https://api.airplanes.live/v2/mil
 *                        every aircraft currently flagged MILITARY, worldwide.
 *   airplanes-live-ladd  GET https://api.airplanes.live/v2/ladd
 *                        aircraft whose owners hold an FAA LADD suppression but
 *                        which are still broadcasting ADS-B in the clear.
 *   AIRCRAFT_NEARBY      GET /v2/point/{lat}/{lon}/{radius_nm}  (entity pivot,
 *                        entity = "lat,lon" or "lat,lon,radius_nm", radius
 *                        clamped to the documented 250 NM maximum).
 *
 * Emits per aircraft, all straight from the response: hex (ICAO24), callsign
 * (`flight`, trimmed), registration (`r`), ICAO type (`t`), type description
 * (`desc`), owner/operator (`ownOp`), year of manufacture, barometric and
 * geometric altitude, ground speed, indicated airspeed, Mach, track, squawk,
 * emergency state, dbFlags and seen_pos age. Keyless.
 *
 * *** LICENCE / TERMS — READ BEFORE OPERATING COMMERCIALLY ***
 * The airplanes.live REST API is keyless but its terms state NON-COMMERCIAL
 * USE ONLY, no SLA, and a hard rate limit of ONE REQUEST PER SECOND. If this
 * platform is operated commercially those terms are NOT satisfied and these
 * three sources must be dropped or explicit permission obtained from
 * airplanes.live. The 1 req/s limit is satisfied here by construction: each
 * run() below issues exactly ONE HTTP request to api.airplanes.live and
 * contains no request loop.
 *
 * R2 / geometry: `lat`/`lon` are present ONLY when a position was actually
 * decoded. Aircraft heard Mode-S-only carry `rr_lat`/`rr_lon` — a coarse
 * RECEIVER-derived guess, not a fix — and/or a stale `lastPosition`. Neither
 * is ever promoted to has_geo here; an aircraft with no fix is emitted with
 * has_geo=0. For the point pivot the queried coordinate is the ANCHOR of the
 * query and is never copied onto an aircraft row.
 * R3: `{"ac":[],"total":0}` is an honest empty — a bbox with nothing in it
 * right now returns 0, not -1.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define AL_LICENSE \
  "airplanes.live keyless REST API. Terms state NON-COMMERCIAL USE and a hard " \
  "rate limit of 1 request/second; no SLA. Not licensed for commercial use."

/* `alt_baro` is a number in feet OR the string "ground". Both are real. */
static void al_alt(cJSON *dst, const cJSON *ac, const char *key,
                   const char *outkey) {
  const cJSON *v = cJSON_GetObjectItem(ac, key);
  if (!v) return;
  if (cJSON_IsNumber(v)) cJSON_AddNumberToObject(dst, outkey, v->valuedouble);
  else if (cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    cJSON_AddStringToObject(dst, outkey, v->valuestring);
}

/* One aircraft object → a GeoJSON Feature appended to `feats`. */
static void al_feature(cJSON *feats, const cJSON *ac, const char *channel) {
  const char *hex = jo_sv(ac, "hex");
  char fbuf[32];
  const char *flight = av_trim(jo_sv(ac, "flight"), fbuf, sizeof fbuf);
  const char *reg  = jo_sv(ac, "r");
  const char *type = jo_sv(ac, "t");
  const char *desc = jo_sv(ac, "desc");
  if (!hex && !flight && !reg) return;             /* nothing real → no row */

  /* Geometry ONLY from a decoded fix. rr_lat/rr_lon and lastPosition are
   * deliberately NOT consulted (R2). */
  double lat, lon;
  cJSON *geom = NULL;
  if (av_dbl(ac, "lat", &lat) && av_dbl(ac, "lon", &lon) &&
      lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
    geom = cJSON_CreateObject();
    cJSON_AddStringToObject(geom, "type", "Point");
    cJSON *c = cJSON_CreateArray();
    cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(geom, "coordinates", c);
  }

  char title[256];
  snprintf(title, sizeof title, "%s%s%s%s%s",
           flight ? flight : (reg ? reg : hex),
           reg && flight ? " · " : "", reg && flight ? reg : "",
           type ? " · " : "", type ? type : "");

  cJSON *p = cJSON_CreateObject();
  if (hex) cJSON_AddStringToObject(p, "uid", hex);   /* ICAO24 = natural id */
  cJSON_AddStringToObject(p, "title", title);
  if (hex)    cJSON_AddStringToObject(p, "hex", hex);
  if (flight) cJSON_AddStringToObject(p, "callsign", flight);
  if (reg)    cJSON_AddStringToObject(p, "registration", reg);
  if (type)   cJSON_AddStringToObject(p, "icao_type", type);
  if (desc)   cJSON_AddStringToObject(p, "description", desc);
  av_copy(p, ac, "ownOp");
  av_copy(p, ac, "year");
  av_copy(p, ac, "dbFlags");
  al_alt(p, ac, "alt_baro", "alt_baro_ft");
  al_alt(p, ac, "alt_geom", "alt_geom_ft");
  av_copy(p, ac, "gs");
  av_copy(p, ac, "ias");
  av_copy(p, ac, "mach");
  av_copy(p, ac, "track");
  av_copy(p, ac, "squawk");
  av_copy(p, ac, "emergency");
  av_copy(p, ac, "category");
  av_copy(p, ac, "seen_pos");
  if (!geom) cJSON_AddStringToObject(p, "position", "no ADS-B fix decoded");
  cJSON_AddStringToObject(p, "record_type", "aircraft-position");
  cJSON_AddStringToObject(p, "channel", channel);
  cJSON_AddStringToObject(p, "source", "airplanes.live");
  if (hex) {
    char link[160];
    snprintf(link, sizeof link, "https://globe.airplanes.live/?icao=%s", hex);
    cJSON_AddStringToObject(p, "link", link);
  }
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("adsb"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(channel));
  cJSON_AddItemToObject(p, "tags", tags);

  cJSON_AddItemToArray(feats, av_feature(geom, p));
}

/* Fetch one airplanes.live v2 URL and emit its `ac[]`. Exactly one request. */
static int al_fetch(const source_ctx *ctx, intel_sink *sink, const char *url,
                    const char *channel, const char *tag) {
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, url, hdrs, tag);
  if (!body) { fprintf(stderr, "[%s] fetch failed\n", tag); return -1; }
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) { fprintf(stderr, "[%s] unparseable JSON\n", tag); return -1; }

  cJSON *ac = cJSON_GetObjectItem(doc, "ac");
  cJSON *feats = cJSON_CreateArray();
  if (cJSON_IsArray(ac)) {
    cJSON *a;
    cJSON_ArrayForEach(a, ac) if (cJSON_IsObject(a)) al_feature(feats, a, channel);
  }
  int n = geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", tag, n);
  return 0;                       /* fetched fine; empty sky is honest (R3) */
}

static int run_mil(const source_ctx *ctx, intel_sink *sink) {
  return al_fetch(ctx, sink, "https://api.airplanes.live/v2/mil",
                  "airplanes-live-mil", "airplanes-live-mil");
}

static int run_ladd(const source_ctx *ctx, intel_sink *sink) {
  return al_fetch(ctx, sink, "https://api.airplanes.live/v2/ladd",
                  "airplanes-live-ladd", "airplanes-live-ladd");
}

/* Coordinate pivot. entity = "lat,lon" or "lat,lon,radius_nm". */
static int run_point(const source_ctx *ctx, intel_sink *sink) {
  double lat = 0, lon = 0, radius = 50;         /* documented default 50 NM */
  if (!av_latlon(ctx->entity, &lat, &lon, &radius)) return 0;  /* wrong shape */
  if (!(radius > 0)) radius = 50;
  if (radius > 250) radius = 250;               /* API maximum is 250 NM */
  char url[192];
  snprintf(url, sizeof url, "https://api.airplanes.live/v2/point/%.6f/%.6f/%d",
           lat, lon, (int)radius);
  return al_fetch(ctx, sink, url, "AIRCRAFT_NEARBY", "AIRCRAFT_NEARBY");
}

static const source_def av_al_mil_def = {
  .id = "airplanes-live-mil", .collector = "transport",
  .name = "airplanes.live Military Aircraft (global)",
  .update_interval_sec = 120, .run = run_mil,
  .category = "transport", .type = "api",
  .url = "https://api.airplanes.live/v2/mil",
  .description = "Every aircraft currently flagged military in the airplanes.live unfiltered ADS-B/MLAT aggregation, worldwide: live position (only when a fix was decoded), altitude, ground speed, squawk, tail number, ICAO type and type description.",
  .license = AL_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_al_mil_def)

static const source_def av_al_ladd_def = {
  .id = "airplanes-live-ladd", .collector = "transport",
  .name = "airplanes.live LADD-blocked Aircraft in Flight",
  .update_interval_sec = 300, .run = run_ladd,
  .category = "transport", .type = "api",
  .url = "https://api.airplanes.live/v2/ladd",
  .description = "Aircraft whose owners requested FAA LADD suppression from commercial trackers but which are still broadcasting ADS-B in the clear — corporate and private jets the mainstream trackers hide, with registered owner/operator and year of manufacture.",
  .license = AL_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_al_ladd_def)

static const source_def av_al_point_def = {
  .id = "AIRCRAFT_NEARBY", .collector = "osint",
  .name = "Aircraft Overhead (radius pivot)",
  .update_interval_sec = 0, .run = run_point,
  .category = "transport", .type = "api",
  .url = "https://api.airplanes.live/v2/point/",
  .description = "Coordinate pivot (entity = \"lat,lon\" or \"lat,lon,radius_nm\", max 250 NM): what is flying over this point right now, with per-aircraft hex, callsign, registration, type, altitude and speed. Answers 'who was overhead at this place'.",
  .license = AL_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_al_point_def)
