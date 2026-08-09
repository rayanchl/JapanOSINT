/* collectors/sources/av_awc_bbox.c
 * NOAA/NWS Aviation Weather Center — the four bounding-box query endpoints.
 * All keyless, all returning a bare top-level JSON ARRAY.
 *
 *   awc-pirep         /api/data/pirep?format=json&age=3&bbox=minLat,minLon,maxLat,maxLon
 *       Airborne pilot reports. Emits: icaoId (reporting station), acType, lat,
 *       lon, fltLvl, fltLvlType, obsTime, clouds/visib/temp/wdir/wspd, icing
 *       (icgInt1/icgType1/icgBas1/icgTop1), turbulence (tbInt1/tbType1/
 *       tbBas1/tbTop1), pirepType, rawOb.
 *       NOTE the bbox parameter is MANDATORY: without it the API answers HTTP
 *       400 {"status":"error","error":"Must specify bounding box or stations
 *       ID and radial distance"}.
 *   awc-airport-info  /api/data/airport?bbox=…&format=json
 *       Airport reference records. Emits: icaoId, iataId, faaId, name, state,
 *       country, type, lat, lon, elev, magdec, owner, tower, beacon,
 *       operations, passengers, freqs and the nested runways[] (id, dimension,
 *       surface, alignment).
 *   awc-station-info  /api/data/stationinfo?bbox=…&format=json
 *       The station table behind every METAR/TAF. Emits: id, icaoId, iataId,
 *       faaId, wmoId, site, lat, lon, elev, state, country, siteType[].
 *   awc-obstacles     /api/data/obstacle?bbox=…&format=json
 *       Charted vertical obstructions. Emits: name, type, lat, lon, elev,
 *       height (elev/height come back as STRINGS).
 *
 * GEOMETRY (R2): the only coordinate used is the record's own top-level
 * `lat`/`lon`. PIREPs whose /OV field could not be resolved come back with
 * null lat/lon — those rows are emitted with has_geo=0 and NEVER placed at
 * `icaoId`, because a pilot report is by definition somewhere other than the
 * reporting station. Station rows with icaoId null (buoys such as
 * "44022 Execution Rocks") are still valid stations and are keyed on `id`.
 *
 * COVERAGE (explicit, not silent): every one of these endpoints silently caps
 * its result set — ~50 rows for /airport, ~400 for /pirep, /stationinfo and
 * /obstacle. Quoting the work list: "page by tiling the bbox, not by assuming
 * full coverage." Each source below therefore runs a breadth-first quadtree
 * sweep (lib helper av_bbox_sweep): any tile that comes back AT the cap is
 * split into four and re-queried, until either the tile is under the cap, a
 * documented maximum depth is reached, or a documented per-run request budget
 * is spent. The budget and the resulting partial coverage are logged on every
 * run. Duplicate rows across parent/child tiles collapse on the sink's uid
 * upsert.
 *
 * R3: an empty tile is an honest empty. A source returns -1 only when its very
 * first tile request fails, i.e. the endpoint itself is unreachable.
 *
 * Licence: US Government work (NOAA/NWS AWC; obstacles derived from the FAA
 * Digital Obstacle File, airports from FAA NASR), public domain, keyless.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define AWCB_LICENSE \
  "US Government work (NOAA/NWS Aviation Weather Center; obstacles from the " \
  "FAA Digital Obstacle File, airports from FAA NASR) — public domain, keyless."

typedef struct { int emitted; const char *tag; } awc_acc;

/* Fetch one bbox tile as a JSON array. NULL on any non-200 / unparseable. */
static cJSON *awc_tile(const source_ctx *ctx, const char *path,
                       const char *extra, double a, double b, double c,
                       double d, const char *tag) {
  char url[320];
  snprintf(url, sizeof url,
           "https://aviationweather.gov/api/data/%s?format=json%s"
           "&bbox=%.4f,%.4f,%.4f,%.4f", path, extra ? extra : "", a, b, c, d);
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, url, hdrs, tag);
  if (!body) return NULL;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (doc && !cJSON_IsArray(doc)) { cJSON_Delete(doc); return NULL; }
  return doc;
}

/* Point geometry from the record's own lat/lon, or NULL (→ has_geo=0). */
static cJSON *awc_point(const cJSON *r) {
  double lat, lon;
  if (!av_dbl(r, "lat", &lat) || !av_dbl(r, "lon", &lon)) return NULL;
  if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return NULL;
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  return g;
}

static void awc_keys(cJSON *p, const cJSON *r, const char *const *keys) {
  for (int i = 0; keys[i]; i++) av_copy(p, r, keys[i]);
}

/* Copy a nested array/object field verbatim when the upstream returned one. */
static void awc_copy_node(cJSON *p, const cJSON *r, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(r, k);
  if (!v || cJSON_IsNull(v)) return;
  if (cJSON_IsArray(v) || cJSON_IsObject(v))
    cJSON_AddItemToObject(p, k, cJSON_Duplicate(v, 1));
}

/* ------------------------------------------------------------- awc-pirep -- */
static const char *PIREP_KEYS[] = {
  "icaoId","acType","fltLvl","fltLvlType","pirepType","rawOb","clouds","visib",
  "wxString","temp","wdir","wspd","icgBas1","icgTop1","icgInt1","icgType1",
  "tbBas1","tbTop1","tbInt1","tbType1","tbFreq1","brkAction","vertGust",
  "receiptTime", NULL };

static int pirep_tile(const source_ctx *ctx, intel_sink *sink, double a,
                      double b, double c, double d, void *user) {
  awc_acc *acc = (awc_acc *)user;
  cJSON *doc = awc_tile(ctx, "pirep", "&age=3", a, b, c, d, acc->tag);
  if (!doc) return -1;
  int raw = cJSON_GetArraySize(doc);
  cJSON *feats = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (!cJSON_IsObject(r)) continue;
    const char *ac = jo_sv(r, "acType");
    const char *st = jo_sv(r, "icaoId");
    const char *raw_ob = jo_sv(r, "rawOb");
    if (!raw_ob && !ac && !st) continue;
    double ot = 0; av_dbl(r, "obsTime", &ot);
    char uid[224];
    snprintf(uid, sizeof uid, "%s|%.0f|%.180s", st ? st : "?", ot,
             raw_ob ? raw_ob : (ac ? ac : "pirep"));
    const char *ty = jo_sv(r, "pirepType");
    char title[288];
    snprintf(title, sizeof title, "%s%s%s%s%s", ty ? ty : "PIREP",
             ac ? " " : "", ac ? ac : "", st ? " via " : "", st ? st : "");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", uid);
    cJSON_AddStringToObject(p, "title", title);
    awc_keys(p, r, PIREP_KEYS);
    char iso[32];
    const char *s = av_epoch_iso(ot, iso, sizeof iso);   /* obsTime = epoch s */
    if (s) cJSON_AddStringToObject(p, "published_at", s);
    cJSON *geom = awc_point(r);
    if (!geom)
      cJSON_AddStringToObject(p, "geometry_note",
        "AWC could not decode the /OV field; the reporting station is NOT the "
        "report's position, so no coordinate is asserted");
    cJSON_AddStringToObject(p, "record_type", "pirep");
    cJSON_AddStringToObject(p, "source", "NWS Aviation Weather Center");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("pirep"));
    cJSON_AddItemToObject(p, "tags", tags);
    cJSON_AddItemToArray(feats, av_feature(geom, p));
  }
  acc->emitted += geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  return raw;
}

static int run_pirep(const source_ctx *ctx, intel_sink *sink) {
  awc_acc acc = { 0, "awc-pirep" };
  int ok = 1;
  /* Root tile spans the region AWC decodes PIREPs for (CONUS, Alaska, Canada,
   * the Caribbean and the adjacent oceanic FIRs). Cap 400, max depth 3,
   * budget 60 requests. */
  int spent = av_bbox_sweep(ctx, sink, 15, -170, 75, -50, 3, 400, 60,
                            pirep_tile, &acc, &ok);
  if (!ok) { fprintf(stderr, "[awc-pirep] endpoint unreachable\n"); return -1; }
  fprintf(stderr, "[awc-pirep] emitted %d over %d tiles\n", acc.emitted, spent);
  return 0;
}

/* ------------------------------------------------------ awc-airport-info -- */
static const char *AIRPORT_KEYS[] = {
  "icaoId","iataId","faaId","name","state","country","source","type","elev",
  "magdec","owner","rwyNum","rwyLength","rwyType","services","tower","beacon",
  "operations","passengers","freqs","priority", NULL };

static int airport_tile(const source_ctx *ctx, intel_sink *sink, double a,
                        double b, double c, double d, void *user) {
  awc_acc *acc = (awc_acc *)user;
  cJSON *doc = awc_tile(ctx, "airport", NULL, a, b, c, d, acc->tag);
  if (!doc) return -1;
  int raw = cJSON_GetArraySize(doc);
  cJSON *feats = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (!cJSON_IsObject(r)) continue;
    const char *icao = jo_sv(r, "icaoId");
    const char *faa  = jo_sv(r, "faaId");
    char nbuf[192];
    const char *name = av_trim(jo_sv(r, "name"), nbuf, sizeof nbuf);
    if (!icao && !faa && !name) continue;
    char title[288];
    snprintf(title, sizeof title, "%s%s%s", icao ? icao : (faa ? faa : ""),
             (icao || faa) && name ? " — " : "", name ? name : "");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", icao ? icao : (faa ? faa : name));
    cJSON_AddStringToObject(p, "title", title);
    awc_keys(p, r, AIRPORT_KEYS);
    /* runways[] carry dimension/surface/alignment but NOT per-threshold
     * coordinates — those come from RunwayLine / OurAirports runways.csv. */
    awc_copy_node(p, r, "runways");
    cJSON *geom = awc_point(r);          /* lat/lon = Airport Reference Point */
    if (geom) cJSON_AddStringToObject(p, "geo_precision", "airport-reference-point");
    cJSON_AddStringToObject(p, "record_type", "airport");
    cJSON_AddStringToObject(p, "source", "NWS Aviation Weather Center");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("airport"));
    cJSON_AddItemToObject(p, "tags", tags);
    cJSON_AddItemToArray(feats, av_feature(geom, p));
  }
  acc->emitted += geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  return raw;
}

static int run_airport(const source_ctx *ctx, intel_sink *sink) {
  awc_acc acc = { 0, "awc-airport-info" };
  int ok = 1;
  /* Whole-world root; the endpoint caps at ~50 rows, so cap 50, max depth 6,
   * budget 250 requests per weekly run. Coverage is therefore BOUNDED, not
   * complete — the count and tile spend are logged every run. */
  int spent = av_bbox_sweep(ctx, sink, -90, -180, 90, 180, 6, 50, 250,
                            airport_tile, &acc, &ok);
  if (!ok) { fprintf(stderr, "[awc-airport-info] endpoint unreachable\n"); return -1; }
  fprintf(stderr, "[awc-airport-info] emitted %d over %d tiles (bounded sweep)\n",
          acc.emitted, spent);
  return 0;
}

/* ------------------------------------------------------ awc-station-info -- */
static const char *STATION_KEYS[] = {
  "id","icaoId","iataId","faaId","wmoId","site","elev","state","country",
  "priority", NULL };

static int station_tile(const source_ctx *ctx, intel_sink *sink, double a,
                        double b, double c, double d, void *user) {
  awc_acc *acc = (awc_acc *)user;
  cJSON *doc = awc_tile(ctx, "stationinfo", NULL, a, b, c, d, acc->tag);
  if (!doc) return -1;
  int raw = cJSON_GetArraySize(doc);
  cJSON *feats = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (!cJSON_IsObject(r)) continue;
    /* Rows with icaoId null (buoys, non-airport sites) are valid stations —
     * key on `id`, which every row carries. */
    const char *id = jo_sv(r, "id");
    char sbuf[192];
    const char *site = av_trim(jo_sv(r, "site"), sbuf, sizeof sbuf);
    if (!id && !site) continue;
    char title[288];
    snprintf(title, sizeof title, "%s%s%s", id ? id : "",
             id && site ? " — " : "", site ? site : "");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", id ? id : site);
    cJSON_AddStringToObject(p, "title", title);
    awc_keys(p, r, STATION_KEYS);
    awc_copy_node(p, r, "siteType");     /* ["METAR","TAF"] etc. */
    cJSON *geom = awc_point(r);
    if (geom) cJSON_AddStringToObject(p, "geo_precision", "station");
    cJSON_AddStringToObject(p, "record_type", "observing-station");
    cJSON_AddStringToObject(p, "source", "NWS Aviation Weather Center");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("station"));
    cJSON_AddItemToObject(p, "tags", tags);
    cJSON_AddItemToArray(feats, av_feature(geom, p));
  }
  acc->emitted += geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  return raw;
}

static int run_station(const source_ctx *ctx, intel_sink *sink) {
  awc_acc acc = { 0, "awc-station-info" };
  int ok = 1;
  /* Whole world; cap 400, max depth 5, budget 250 requests per weekly run. */
  int spent = av_bbox_sweep(ctx, sink, -90, -180, 90, 180, 5, 400, 250,
                            station_tile, &acc, &ok);
  if (!ok) { fprintf(stderr, "[awc-station-info] endpoint unreachable\n"); return -1; }
  fprintf(stderr, "[awc-station-info] emitted %d over %d tiles\n",
          acc.emitted, spent);
  return 0;
}

/* --------------------------------------------------------- awc-obstacles -- */
static const char *OBST_KEYS[] = { "name","type","elev","height", NULL };

static int obstacle_tile(const source_ctx *ctx, intel_sink *sink, double a,
                         double b, double c, double d, void *user) {
  awc_acc *acc = (awc_acc *)user;
  cJSON *doc = awc_tile(ctx, "obstacle", NULL, a, b, c, d, acc->tag);
  if (!doc) return -1;
  int raw = cJSON_GetArraySize(doc);
  cJSON *feats = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (!cJSON_IsObject(r)) continue;
    const char *name = jo_sv(r, "name");
    const char *type = jo_sv(r, "type");
    cJSON *geom = awc_point(r);
    if (!geom) continue;      /* this endpoint is a spatial query: a row with
                               * no coordinate carries nothing to emit */
    double lat = 0, lon = 0;
    av_dbl(r, "lat", &lat); av_dbl(r, "lon", &lon);
    const char *hgt = jo_sv(r, "height");     /* strings in this feed */
    char uid[128];
    snprintf(uid, sizeof uid, "%.5f,%.5f|%s", lat, lon, type ? type : "OBST");
    char title[224];
    snprintf(title, sizeof title, "%s%s%s%s", name ? name : "Obstruction",
             hgt ? " · " : "", hgt ? hgt : "", hgt ? " ft AGL" : "");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", uid);
    cJSON_AddStringToObject(p, "title", title);
    awc_keys(p, r, OBST_KEYS);
    cJSON_AddStringToObject(p, "record_type", "vertical-obstruction");
    cJSON_AddStringToObject(p, "source", "NWS AWC (FAA Digital Obstacle File)");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("obstacle"));
    cJSON_AddItemToObject(p, "tags", tags);
    cJSON_AddItemToArray(feats, av_feature(geom, p));
  }
  acc->emitted += geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  return raw;
}

static int run_obstacle(const source_ctx *ctx, intel_sink *sink) {
  awc_acc acc = { 0, "awc-obstacles" };
  int ok = 1;
  /* CONUS + Alaska + Hawaii root (the DOF is FAA territory only); cap 400,
   * max depth 5, budget 250 requests. Bounded, logged, not complete. */
  int spent = av_bbox_sweep(ctx, sink, 17, -180, 72, -64, 5, 400, 250,
                            obstacle_tile, &acc, &ok);
  if (!ok) { fprintf(stderr, "[awc-obstacles] endpoint unreachable\n"); return -1; }
  fprintf(stderr, "[awc-obstacles] emitted %d over %d tiles (bounded sweep)\n",
          acc.emitted, spent);
  return 0;
}

static const source_def av_awc_pirep_def = {
  .id = "awc-pirep", .collector = "transport",
  .name = "AWC Pilot Reports (PIREP)",
  .update_interval_sec = 900, .run = run_pirep,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/pirep?format=json",
  .description = "Airborne pilot reports — turbulence, icing, cloud tops, urgent (UUA) reports — each with the AWC-decoded position, flight level and aircraft type. Crowd-sourced observations from aircraft in flight.",
  .license = AWCB_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_pirep_def)

static const source_def av_awc_airport_def = {
  .id = "awc-airport-info", .collector = "transport",
  .name = "AWC Airport Reference Data",
  .update_interval_sec = 604800, .run = run_airport,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/airport?format=json",
  .description = "Airport reference records — reference-point coordinates, elevation, magnetic variation, ownership, tower/beacon presence, published frequencies and per-runway dimension/surface/alignment. Swept by adaptive bbox tiling; coverage per run is bounded and logged.",
  .license = AWCB_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_airport_def)

static const source_def av_awc_station_def = {
  .id = "awc-station-info", .collector = "transport",
  .name = "AWC Observing Station Index",
  .update_interval_sec = 604800, .run = run_station,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/stationinfo?format=json",
  .description = "The station table behind every METAR/TAF: ICAO, IATA, FAA and WMO identifiers cross-referenced with real coordinates and which products each site issues — the lookup that makes any raw METAR/TAF mappable.",
  .license = AWCB_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_station_def)

static const source_def av_awc_obstacle_def = {
  .id = "awc-obstacles", .collector = "transport",
  .name = "AWC Vertical Obstructions",
  .update_interval_sec = 604800, .run = run_obstacle,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/obstacle?format=json",
  .description = "Charted vertical obstructions (towers, tanks, stacks, buildings) with AGL height and AMSL elevation, queried by bounding box — a lightweight view onto the FAA Digital Obstacle File. Coverage per run is bounded and logged.",
  .license = AWCB_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_obstacle_def)
