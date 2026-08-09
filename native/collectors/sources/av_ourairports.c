/* collectors/sources/av_ourairports.c
 * OurAirports — the three global CSV tables the fleet was missing. The
 * pre-existing OURAIRPORTS source only pulls airports.csv, so runway-level,
 * navaid-level and frequency-level detail was absent entirely.
 *
 *   ourairports-runways      https://davidmegginson.github.io/ourairports-data/runways.csv
 *       Emits: id, airport_ident, length_ft, width_ft, surface, lighted,
 *       closed, le_ident/he_ident, le/he heading_degT, le/he elevation_ft,
 *       le/he displaced_threshold_ft and, when surveyed, the two threshold
 *       coordinates.
 *   ourairports-navaids      .../navaids.csv
 *       Emits: ident, name, type, frequency_khz, elevation_ft, iso_country,
 *       dme_frequency_khz, dme_channel, magnetic_variation_deg, usageType,
 *       power, associated_airport, plus the transmitter coordinate.
 *   ourairports-frequencies  .../airport-frequencies.csv
 *       Emits: id, airport_ident, type, description, frequency_mhz.
 *
 * GEOMETRY (R2):
 *  - runways: le_latitude_deg/le_longitude_deg (low-numbered threshold) and
 *    he_latitude_deg/he_longitude_deg (high end). THESE ARE FREQUENTLY EMPTY —
 *    in the first data rows (00A, 00AK, 00AL) all four are blank. A runway
 *    with both thresholds surveyed is emitted as a LineString between the two
 *    REAL threshold positions; with one surveyed, as that single Point; with
 *    neither, with has_geo=0. The parent airport's coordinate is NEVER
 *    inherited as a threshold position.
 *  - navaids: latitude_deg/longitude_deg = the transmitter site. The separate
 *    dme_latitude_deg/dme_longitude_deg (a co-located DME sitting elsewhere)
 *    are usually blank and are NEVER back-filled from the main position; when
 *    present they are carried as attributes, not as the row's geometry.
 *  - frequencies: NO COORDINATES IN THE FILE AT ALL. has_geo is always 0 and
 *    no join to airports.csv is performed here.
 *
 * The files are quoted CSV with embedded empty fields, so lib/csv.h is used
 * rather than strtok.
 *
 * R3: a fetch/parse failure returns -1; a file that parsed but yielded no
 * usable row returns 0.
 *
 * Licence: OurAirports data is released into the PUBLIC DOMAIN by its
 * maintainers. The GitHub Pages mirror (davidmegginson.github.io/
 * ourairports-data) is the maintainer's own canonical daily export. Keyless.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define OA_LICENSE \
  "OurAirports data — released into the PUBLIC DOMAIN by its maintainers; " \
  "mirror davidmegginson.github.io/ourairports-data is the maintainer's own " \
  "canonical daily export. Keyless."
#define OA_FLUSH 1000

/* csv_parse yields every cell as a STRING ("" when the field was empty).
 * jo_sv already maps "" → NULL, and av_dbl parses a numeric string. */
static cJSON *oa_fetch(const source_ctx *ctx, const char *url, const char *tag) {
  char *text = feed_get_text(ctx->http, url, 60000);
  if (!text || strlen(text) < 32) {
    free(text);
    fprintf(stderr, "[%s] fetch failed\n", tag);
    return NULL;
  }
  cJSON *rows = csv_parse(text, 1);
  free(text);
  if (!cJSON_IsArray(rows) || cJSON_GetArraySize(rows) == 0) {
    cJSON_Delete(rows);
    fprintf(stderr, "[%s] no rows parsed\n", tag);
    return NULL;
  }
  return rows;
}

static void oa_flush(const source_ctx *ctx, intel_sink *sink, cJSON **feats,
                     int *emitted) {
  if (cJSON_GetArraySize(*feats) == 0) return;
  *emitted += geojson_emit_features(sink, ctx->source_id, *feats);
  cJSON_Delete(*feats);
  *feats = cJSON_CreateArray();
}

static cJSON *oa_point(double lon, double lat) {
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  return g;
}

/* Copy a CSV cell verbatim when non-empty. */
static void oa_copy(cJSON *p, const cJSON *r, const char *k) {
  const char *v = jo_sv(r, k);
  if (v) cJSON_AddStringToObject(p, k, v);
}

/* --------------------------------------------------- ourairports-runways -- */
static const char *RWY_KEYS[] = {
  "id","airport_ref","airport_ident","length_ft","width_ft","surface",
  "lighted","closed","le_ident","le_elevation_ft","le_heading_degT",
  "le_displaced_threshold_ft","he_ident","he_elevation_ft","he_heading_degT",
  "he_displaced_threshold_ft", NULL };

static int run_runways(const source_ctx *ctx, intel_sink *sink) {
  cJSON *rows = oa_fetch(ctx,
      "https://davidmegginson.github.io/ourairports-data/runways.csv",
      "ourairports-runways");
  if (!rows) return -1;
  cJSON *feats = cJSON_CreateArray();
  int emitted = 0, seen = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *apt = jo_sv(r, "airport_ident");
    const char *id  = jo_sv(r, "id");
    if (!apt || !id) continue;
    seen++;
    const char *le = jo_sv(r, "le_ident"), *he = jo_sv(r, "he_ident");
    const char *len = jo_sv(r, "length_ft"), *wid = jo_sv(r, "width_ft");
    const char *surf = jo_sv(r, "surface");

    double lelat, lelon, helat, helon;
    /* Range-gate every ordinate. OurAirports is community-maintained and the
     * runways file contains rows whose threshold longitude is a stray figure
     * such as 1912324.13216 — almost certainly a length or elevation typed
     * into the wrong column upstream. tests/contract/source_contract.py
     * caught it across 48,141 rows. Parsing successfully is not the same as
     * the value being a coordinate: an out-of-range pair must be dropped, not
     * persisted, or the map renders a runway somewhere off the planet.
     * has_geo=0 with the raw values still in properties is the honest result. */
    int lok = av_dbl(r, "le_latitude_deg", &lelat) &&
              av_dbl(r, "le_longitude_deg", &lelon) &&
              lelat >= -90.0 && lelat <= 90.0 &&
              lelon >= -180.0 && lelon <= 180.0;
    int hok = av_dbl(r, "he_latitude_deg", &helat) &&
              av_dbl(r, "he_longitude_deg", &helon) &&
              helat >= -90.0 && helat <= 90.0 &&
              helon >= -180.0 && helon <= 180.0;
    cJSON *geom = NULL;
    const char *prec = NULL;
    if (lok && hok) {
      /* Both thresholds surveyed: the centreline runs between the two REAL
       * positions. No third point is interpolated. */
      geom = cJSON_CreateObject();
      cJSON_AddStringToObject(geom, "type", "LineString");
      cJSON *c = cJSON_CreateArray();
      cJSON *a = cJSON_CreateArray();
      cJSON_AddItemToArray(a, cJSON_CreateNumber(lelon));
      cJSON_AddItemToArray(a, cJSON_CreateNumber(lelat));
      cJSON *b = cJSON_CreateArray();
      cJSON_AddItemToArray(b, cJSON_CreateNumber(helon));
      cJSON_AddItemToArray(b, cJSON_CreateNumber(helat));
      cJSON_AddItemToArray(c, a);
      cJSON_AddItemToArray(c, b);
      cJSON_AddItemToObject(geom, "coordinates", c);
      prec = "surveyed-thresholds";
    } else if (lok) {
      geom = oa_point(lelon, lelat); prec = "low-threshold-only";
    } else if (hok) {
      geom = oa_point(helon, helat); prec = "high-threshold-only";
    }

    char title[256];
    snprintf(title, sizeof title, "%s runway %s%s%s%s%s%s%s", apt,
             le ? le : "", (le && he) ? "/" : "", he ? he : "",
             len ? " · " : "", len ? len : "", len && wid ? "x" : "",
             (len && wid) ? wid : "");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", id);
    cJSON_AddStringToObject(p, "title", title);
    for (int k = 0; RWY_KEYS[k]; k++) oa_copy(p, r, RWY_KEYS[k]);
    if (lok) { cJSON_AddNumberToObject(p, "le_latitude_deg", lelat);
               cJSON_AddNumberToObject(p, "le_longitude_deg", lelon); }
    if (hok) { cJSON_AddNumberToObject(p, "he_latitude_deg", helat);
               cJSON_AddNumberToObject(p, "he_longitude_deg", helon); }
    if (prec) cJSON_AddStringToObject(p, "geo_precision", prec);
    else cJSON_AddStringToObject(p, "geometry_note",
           "OurAirports has no surveyed threshold coordinates for this runway; "
           "the parent airport's position is deliberately not inherited");
    if (surf) cJSON_AddStringToObject(p, "surface", surf);
    cJSON_AddStringToObject(p, "record_type", "runway");
    cJSON_AddStringToObject(p, "source", "OurAirports");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("runway"));
    cJSON_AddItemToObject(p, "tags", tags);
    cJSON_AddItemToArray(feats, av_feature(geom, p));
    if (cJSON_GetArraySize(feats) >= OA_FLUSH)
      oa_flush(ctx, sink, &feats, &emitted);
  }
  oa_flush(ctx, sink, &feats, &emitted);
  cJSON_Delete(feats);
  cJSON_Delete(rows);
  fprintf(stderr, "[ourairports-runways] emitted %d of %d rows\n", emitted, seen);
  return 0;
}

/* --------------------------------------------------- ourairports-navaids -- */
static const char *NAV_KEYS[] = {
  "id","ident","name","type","frequency_khz","elevation_ft","iso_country",
  "dme_frequency_khz","dme_channel","dme_elevation_ft","slaved_variation_deg",
  "magnetic_variation_deg","usageType","power","associated_airport", NULL };

static int run_navaids(const source_ctx *ctx, intel_sink *sink) {
  cJSON *rows = oa_fetch(ctx,
      "https://davidmegginson.github.io/ourairports-data/navaids.csv",
      "ourairports-navaids");
  if (!rows) return -1;
  cJSON *feats = cJSON_CreateArray();
  int emitted = 0, seen = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *id = jo_sv(r, "id");
    const char *ident = jo_sv(r, "ident");
    const char *name = jo_sv(r, "name");
    if (!id || (!ident && !name)) continue;
    seen++;
    const char *type = jo_sv(r, "type");
    const char *freq = jo_sv(r, "frequency_khz");
    const char *cc   = jo_sv(r, "iso_country");

    double lat, lon;
    cJSON *geom = NULL;
    if (av_dbl(r, "latitude_deg", &lat) && av_dbl(r, "longitude_deg", &lon) &&
        lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180)
      geom = oa_point(lon, lat);

    char title[288];
    snprintf(title, sizeof title, "%s%s%s%s%s%s%s%s%s",
             ident ? ident : "", (ident && name) ? " " : "", name ? name : "",
             type ? " (" : "", type ? type : "", type ? ")" : "",
             freq ? " · " : "", freq ? freq : "", freq ? " kHz" : "");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", id);
    cJSON_AddStringToObject(p, "title", title);
    for (int k = 0; NAV_KEYS[k]; k++) oa_copy(p, r, NAV_KEYS[k]);
    /* A co-located DME sitting elsewhere: carried as attributes only, never
     * back-filled from the main transmitter position. */
    oa_copy(p, r, "dme_latitude_deg");
    oa_copy(p, r, "dme_longitude_deg");
    if (cc) cJSON_AddStringToObject(p, "country", cc);
    if (!geom) cJSON_AddStringToObject(p, "geometry_note",
                 "no transmitter coordinate published for this navaid");
    cJSON_AddStringToObject(p, "record_type", "navaid");
    cJSON_AddStringToObject(p, "source", "OurAirports");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("navaid"));
    cJSON_AddItemToObject(p, "tags", tags);
    cJSON_AddItemToArray(feats, av_feature(geom, p));
    if (cJSON_GetArraySize(feats) >= OA_FLUSH)
      oa_flush(ctx, sink, &feats, &emitted);
  }
  oa_flush(ctx, sink, &feats, &emitted);
  cJSON_Delete(feats);
  cJSON_Delete(rows);
  fprintf(stderr, "[ourairports-navaids] emitted %d of %d rows\n", emitted, seen);
  return 0;
}

/* ----------------------------------------------- ourairports-frequencies -- */
static int run_freqs(const source_ctx *ctx, intel_sink *sink) {
  cJSON *rows = oa_fetch(ctx,
      "https://davidmegginson.github.io/ourairports-data/airport-frequencies.csv",
      "ourairports-frequencies");
  if (!rows) return -1;
  int n = 0, seen = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *id  = jo_sv(r, "id");
    const char *apt = jo_sv(r, "airport_ident");
    const char *type = jo_sv(r, "type");
    const char *mhz = jo_sv(r, "frequency_mhz");
    if (!id || !apt || !mhz) continue;
    seen++;
    const char *desc = jo_sv(r, "description");   /* often blank */

    cJSON *data = cJSON_CreateObject();
    oa_copy(data, r, "id");
    oa_copy(data, r, "airport_ref");
    oa_copy(data, r, "airport_ident");
    oa_copy(data, r, "type");
    oa_copy(data, r, "description");
    oa_copy(data, r, "frequency_mhz");
    cJSON_AddStringToObject(data, "source", "OurAirports");
    char *bj = cJSON_PrintUnformatted(data);

    cJSON *props = cJSON_Duplicate(data, 1);
    cJSON_AddStringToObject(props, "record_type", "airport-frequency");
    /* This file contains no coordinates at all; no join is performed. */
    cJSON_AddStringToObject(props, "geometry_note",
      "airport-frequencies.csv carries no coordinates; not geocoded here");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);
    cJSON_Delete(data);

    char title[224];
    snprintf(title, sizeof title, "%s %s %s MHz%s%s", apt,
             type ? type : "", mhz, desc ? " — " : "", desc ? desc : "");

    intel_item it = {0};
    it.remote_key      = id;
    it.title           = title;
    it.summary         = desc;
    it.body            = bj;
    it.lang            = "en";
    it.record_type     = "airport-frequency";
    it.properties_json = pj;
    it.tags_json       = "[\"aviation\",\"radio\",\"frequency\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
  }
  cJSON_Delete(rows);
  fprintf(stderr, "[ourairports-frequencies] emitted %d of %d rows\n", n, seen);
  return 0;
}

static const source_def av_oa_runways_def = {
  .id = "ourairports-runways", .collector = "transport",
  .name = "OurAirports Global Runway Inventory",
  .update_interval_sec = 604800, .run = run_runways,
  .category = "transport", .type = "dataset",
  .url = "https://davidmegginson.github.io/ourairports-data/runways.csv",
  .description = "Every runway at every airport worldwide: dimensions, surface, lighting, closed flag, both threshold identifiers with headings and, where surveyed, the threshold coordinates. Runway-level detail the airports-only OurAirports source lacks.",
  .license = OA_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_oa_runways_def)

static const source_def av_oa_navaids_def = {
  .id = "ourairports-navaids", .collector = "transport",
  .name = "OurAirports Global Navaid Database",
  .update_interval_sec = 604800, .run = run_navaids,
  .category = "transport", .type = "dataset",
  .url = "https://davidmegginson.github.io/ourairports-data/navaids.csv",
  .description = "Worldwide VOR/DME/TACAN/NDB/ILS navaids with frequency, power class, magnetic variation and associated airport — the global counterpart to the US-only FAA NAVAID layer, covering Japan, Europe, Africa and Asia.",
  .license = OA_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_oa_navaids_def)

static const source_def av_oa_freqs_def = {
  .id = "ourairports-frequencies", .collector = "transport",
  .name = "OurAirports Airport Radio Frequencies",
  .update_interval_sec = 604800, .run = run_freqs,
  .category = "transport", .type = "dataset",
  .url = "https://davidmegginson.github.io/ourairports-data/airport-frequencies.csv",
  .description = "Published tower, ground, approach, ATIS, CTAF and UNICOM frequencies for airports worldwide — directly useful for scanner correlation and for identifying which facility controls a given field. No coordinates in this file.",
  .license = OA_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_oa_freqs_def)
