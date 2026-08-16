/* collectors/sources/av_awc_hazards.c
 * NOAA/NWS Aviation Weather Center — the four airspace-hazard bulletin
 * products, each a keyless JSON endpoint returning a bare top-level ARRAY.
 *
 *   awc-isigmet    https://aviationweather.gov/api/data/isigmet?format=json
 *       International SIGMETs in force worldwide (incl. Japanese/Asian FIRs).
 *       Emits: icaoId, firId, firName, hazard, qualifier, base, top,
 *       validTimeFrom/To, geom, rawSigmet, dir/spd/chng.
 *   awc-airsigmet  https://aviationweather.gov/api/data/airsigmet?format=json
 *       US convective SIGMETs / AIRMETs. Emits: icaoId, airSigmetType, hazard,
 *       severity, altitudeLow1/Hi1 (may be null), movementDir/Spd,
 *       validTimeFrom/To, rawAirSigmet.
 *   awc-gairmet    https://aviationweather.gov/api/data/gairmet?format=json
 *       Graphical AIRMET forecast areas. Emits: tag, forecastHour, validTime,
 *       hazard, product, level/top/base/fzltop/fzlbase, due_to, status,
 *       issueTime, expireTime.
 *   awc-cwa        https://aviationweather.gov/api/data/cwa?format=json
 *       ARTCC Center Weather Advisories. Emits: cwsu, name, hazard, qualifier,
 *       base, top, seriesId, validTimeFrom/To, rawText.
 *
 * GEOMETRY (R2): the ONLY location source is the per-bulletin `coords` array
 * of {lat,lon} vertices, turned into a real Polygon/LineString/Point according
 * to the bulletin's own `geom`/`geometryType` hint. Bulletins that come back
 * with no `coords` are text-only; those rows are emitted with has_geo=0. We
 * never fall back to the FIR centroid, and never to the issuing office —
 * quoting the work list, "never substitute the issuing office (KKCI = Kansas
 * City) as a location", and for CWA "`cwsu` is the issuing ARTCC identifier,
 * NOT a position".
 * TRAP handled in av_dbl(): isigmet/airsigmet return coords lat/lon as JSON
 * numbers, cwa/gairmet return the same values as STRINGS.
 * R3: an empty array is a normal quiet period → return 0.
 *
 * Licence: US Government work (NOAA/NWS Aviation Weather Center), public
 * domain. Keyless documented JSON API.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define AWC_LICENSE \
  "US Government work (NOAA/NWS Aviation Weather Center) — public domain. " \
  "Keyless documented JSON API."

/* Copy a whitelist of real upstream fields into props. */
static void awc_copy_all(cJSON *p, const cJSON *b, const char *const *keys) {
  for (int i = 0; keys[i]; i++) av_copy(p, b, keys[i]);
}

/* Shared bulletin → Feature. `geom_key` names the AREA/LINE/POINT hint. */
static void awc_feature(cJSON *feats, const cJSON *b, const char *record_type,
                        const char *geom_key, const char *title,
                        const char *uid, const char *const *keys) {
  const cJSON *gv = cJSON_GetObjectItem(b, geom_key);
  const char *ghint = (gv && cJSON_IsString(gv)) ? gv->valuestring : NULL;
  cJSON *geom = av_coords_geom(cJSON_GetObjectItem(b, "coords"), ghint);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "uid", uid);
  cJSON_AddStringToObject(p, "title", title);
  awc_copy_all(p, b, keys);

  /* published_at from the bulletin's own validity start (epoch seconds). */
  double vf = 0;
  char iso[32];
  if (av_dbl(b, "validTimeFrom", &vf)) {
    const char *s = av_epoch_iso(vf, iso, sizeof iso);
    if (s) cJSON_AddStringToObject(p, "published_at", s);
  }
  double vt = 0;
  char iso2[32];
  if (av_dbl(b, "validTimeTo", &vt)) {
    const char *s = av_epoch_iso(vt, iso2, sizeof iso2);
    if (s) cJSON_AddStringToObject(p, "valid_until", s);
  }
  if (!geom)
    cJSON_AddStringToObject(p, "geometry_note",
                            "bulletin is text-only: upstream returned no coords");
  cJSON_AddStringToObject(p, "record_type", record_type);
  cJSON_AddStringToObject(p, "source", "NWS Aviation Weather Center");
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("airspace-hazard"));
  cJSON_AddItemToObject(p, "tags", tags);

  cJSON_AddItemToArray(feats, av_feature(geom, p));
}

static cJSON *awc_fetch_array(const source_ctx *ctx, const char *url,
                              const char *tag) {
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, url, hdrs, tag);
  if (!body) return NULL;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (doc && !cJSON_IsArray(doc)) { cJSON_Delete(doc); return NULL; }
  return doc;
}

/* -------------------------------------------------------------- isigmet -- */
static const char *ISIG_KEYS[] = {
  "icaoId","firId","firName","seriesId","hazard","qualifier","base","top",
  "geom","dir","spd","chng","rawSigmet","receiptTime", NULL };

static int run_isigmet(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = awc_fetch_array(ctx,
      "https://aviationweather.gov/api/data/isigmet?format=json", "awc-isigmet");
  if (!doc) { fprintf(stderr, "[awc-isigmet] fetch/parse failed\n"); return -1; }
  cJSON *feats = cJSON_CreateArray();
  cJSON *b;
  cJSON_ArrayForEach(b, doc) {
    if (!cJSON_IsObject(b)) continue;
    const char *icao = jo_sv(b, "icaoId");
    const char *fir  = jo_sv(b, "firName");
    const char *haz  = jo_sv(b, "hazard");
    const char *qual = jo_sv(b, "qualifier");
    const char *ser  = jo_sv(b, "seriesId");
    if (!haz && !fir && !icao) continue;
    double vf = 0; av_dbl(b, "validTimeFrom", &vf);
    char uid[160];
    snprintf(uid, sizeof uid, "%s|%s|%.0f", icao ? icao : "?", ser ? ser : "?", vf);
    char title[288];
    snprintf(title, sizeof title, "SIGMET %s%s%s — %s",
             qual ? qual : "", qual ? " " : "", haz ? haz : "HAZARD",
             fir ? fir : (icao ? icao : "FIR"));
    awc_feature(feats, b, "sigmet-international", "geom", title, uid, ISIG_KEYS);
  }
  int n = geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  fprintf(stderr, "[awc-isigmet] emitted %d\n", n);
  return 0;
}

/* ------------------------------------------------------------ airsigmet -- */
static const char *AIRSIG_KEYS[] = {
  "icaoId","alphaChar","seriesId","airSigmetType","hazard","severity",
  "altitudeLow1","altitudeLow2","altitudeHi1","altitudeHi2","movementDir",
  "movementSpd","rawAirSigmet","receiptTime", NULL };

static int run_airsigmet(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = awc_fetch_array(ctx,
      "https://aviationweather.gov/api/data/airsigmet?format=json",
      "awc-airsigmet");
  if (!doc) { fprintf(stderr, "[awc-airsigmet] fetch/parse failed\n"); return -1; }
  cJSON *feats = cJSON_CreateArray();
  cJSON *b;
  cJSON_ArrayForEach(b, doc) {
    if (!cJSON_IsObject(b)) continue;
    const char *icao = jo_sv(b, "icaoId");
    const char *ty   = jo_sv(b, "airSigmetType");
    const char *haz  = jo_sv(b, "hazard");
    const char *ser  = jo_sv(b, "seriesId");
    if (!haz && !ty) continue;
    double vf = 0; av_dbl(b, "validTimeFrom", &vf);
    char uid[160];
    snprintf(uid, sizeof uid, "%s|%s|%.0f", icao ? icao : "?", ser ? ser : "?", vf);
    char title[288];
    snprintf(title, sizeof title, "%s %s%s%s — issued %s",
             ty ? ty : "AIRSIGMET", ser ? ser : "", ser ? " " : "",
             haz ? haz : "", icao ? icao : "?");
    /* `geom` is not published on this product; av_coords_geom falls back to a
     * LineString/Polygon decision from the vertex count. */
    awc_feature(feats, b, "sigmet-us", "geom", title, uid, AIRSIG_KEYS);
  }
  int n = geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  fprintf(stderr, "[awc-airsigmet] emitted %d\n", n);
  return 0;
}

/* -------------------------------------------------------------- gairmet -- */
static const char *GAIR_KEYS[] = {
  "tag","forecastHour","validTime","hazard","geometryType","severity",
  "frequency","due_to","status","level","top","base","fzltop","fzlbase",
  "product","geom","issueTime","expireTime", NULL };

static int run_gairmet(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = awc_fetch_array(ctx,
      "https://aviationweather.gov/api/data/gairmet?format=json", "awc-gairmet");
  if (!doc) { fprintf(stderr, "[awc-gairmet] fetch/parse failed\n"); return -1; }
  cJSON *feats = cJSON_CreateArray();
  cJSON *b;
  cJSON_ArrayForEach(b, doc) {
    if (!cJSON_IsObject(b)) continue;
    const char *tag  = jo_sv(b, "tag");
    const char *haz  = jo_sv(b, "hazard");
    const char *prod = jo_sv(b, "product");
    const char *vt   = jo_sv(b, "validTime");
    if (!haz) continue;
    double fh = 0; av_dbl(b, "forecastHour", &fh);
    double issue = 0; av_dbl(b, "issueTime", &issue);
    char uid[192];
    snprintf(uid, sizeof uid, "%s|%s|%s|%.0f|%.0f", prod ? prod : "?",
             tag ? tag : "?", haz, fh, issue);
    char title[288];
    snprintf(title, sizeof title, "G-AIRMET %s %s F+%.0fh%s%s", haz,
             tag ? tag : "", fh, prod ? " · " : "", prod ? prod : "");

    /* geometryType (AREA/LINE) is this product's own hint; `geom` mirrors it. */
    const cJSON *gt = cJSON_GetObjectItem(b, "geometryType");
    const char *hint = (gt && cJSON_IsString(gt)) ? "geometryType" : "geom";
    awc_feature(feats, b, "gairmet", hint, title, uid, GAIR_KEYS);

    /* validTime is an ISO string on this product (not epoch), so publish it. */
    if (vt) {
      cJSON *f = cJSON_GetArrayItem(feats, cJSON_GetArraySize(feats) - 1);
      cJSON *p = f ? cJSON_GetObjectItem(f, "properties") : NULL;
      if (p && !cJSON_GetObjectItem(p, "published_at"))
        cJSON_AddStringToObject(p, "published_at", vt);
    }
  }
  int n = geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  fprintf(stderr, "[awc-gairmet] emitted %d\n", n);
  return 0;
}

/* ------------------------------------------------------------------ cwa -- */
static const char *CWA_KEYS[] = {
  "cwsu","name","seriesId","hazard","qualifier","base","top","geom",
  "rawText","receiptTime", NULL };

static int run_cwa(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = awc_fetch_array(ctx,
      "https://aviationweather.gov/api/data/cwa?format=json", "awc-cwa");
  if (!doc) { fprintf(stderr, "[awc-cwa] fetch/parse failed\n"); return -1; }
  cJSON *feats = cJSON_CreateArray();
  cJSON *b;
  cJSON_ArrayForEach(b, doc) {
    if (!cJSON_IsObject(b)) continue;
    const char *cwsu = jo_sv(b, "cwsu");
    const char *name = jo_sv(b, "name");
    const char *haz  = jo_sv(b, "hazard");
    const char *qual = jo_sv(b, "qualifier");
    const char *ser  = jo_sv(b, "seriesId");
    if (!haz && !ser) continue;
    double vf = 0; av_dbl(b, "validTimeFrom", &vf);
    char uid[160];
    snprintf(uid, sizeof uid, "%s|%s|%.0f", cwsu ? cwsu : "?", ser ? ser : "?", vf);
    char title[288];
    snprintf(title, sizeof title, "CWA %s %s%s%s — %s%s%s",
             ser ? ser : "", haz ? haz : "", qual ? " " : "", qual ? qual : "",
             cwsu ? cwsu : "", name ? " " : "", name ? name : "");
    awc_feature(feats, b, "center-weather-advisory", "geom", title, uid, CWA_KEYS);
  }
  int n = geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  /* Zero live advisories is a normal quiet period, not an error (R3). */
  fprintf(stderr, "[awc-cwa] emitted %d\n", n);
  return 0;
}

static const source_def av_awc_isigmet_def = {
  .id = "awc-isigmet", .collector = "transport",
  .name = "AWC International SIGMETs",
  .update_interval_sec = 900, .run = run_isigmet,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/isigmet?format=json",
  .description = "Every international SIGMET currently in force worldwide — severe turbulence, icing, volcanic ash, thunderstorms, tropical cyclones — as named FIR, hazard type, altitude band, validity window and the actual hazard polygon.",
  .license = AWC_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_isigmet_def)

static const source_def av_awc_airsigmet_def = {
  .id = "awc-airsigmet", .collector = "transport",
  .name = "AWC US SIGMET / AIRMET Bulletins",
  .update_interval_sec = 900, .run = run_airsigmet,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/airsigmet?format=json",
  .description = "Active US convective SIGMETs and AIRMETs with hazard type, altitude band, movement vector and area polygon; complements the international feed for CONUS/Alaska airspace disruption.",
  .license = AWC_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_airsigmet_def)

static const source_def av_awc_gairmet_def = {
  .id = "awc-gairmet", .collector = "transport",
  .name = "AWC Graphical AIRMETs (G-AIRMET)",
  .update_interval_sec = 3600, .run = run_gairmet,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/gairmet?format=json",
  .description = "Graphical AIRMET forecast areas for icing, turbulence, freezing level, IFR conditions and mountain obscuration, by forecast hour — the polygon form of the US airspace-impact picture.",
  .license = AWC_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_gairmet_def)

static const source_def av_awc_cwa_def = {
  .id = "awc-cwa", .collector = "transport",
  .name = "AWC Center Weather Advisories",
  .update_interval_sec = 900, .run = run_cwa,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/cwa?format=json",
  .description = "Center Weather Advisories issued by each ARTCC's weather unit — short-fuse, sector-level hazards that drive real-time ATC reroutes. Low volume, high operational signal.",
  .license = AWC_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_cwa_def)
