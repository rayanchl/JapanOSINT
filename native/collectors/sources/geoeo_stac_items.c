/* Four public STAC catalogues, queried for ITEMS (never the catalog or
 * collection document — a catalog is metadata about the service, not a record).
 *
 * Endpoints (all keyless):
 *   CDSE  https://stac.dataspace.copernicus.eu/v1/search?collections=sentinel-1-grd&limit=20
 *   BDC   https://data.inpe.br/bdc/stac/v1/search?limit=20
 *   DEA   https://explorer.digitalearth.africa/stac/collections/s2_l2a/items?limit=20
 *   SWISS https://data.geo.admin.ch/api/stac/v0.9/collections/ch.swisstopo.swissimage-dop10/items?limit=50
 * Emits per item: id, collection, properties.datetime, platform/constellation,
 *   instruments, gsd, the bbox and the item's own footprint geometry.
 * Licences:
 *   CDSE  — Copernicus open and free data policy (Regulation EU 1159/2013):
 *           free, full and open access including redistribution. Contributing
 *           mission collections (ccm-*) carry stricter ESA licences and are
 *           EXCLUDED below.
 *   BDC   — INPE open data (Brazilian government); CBERS/Amazonia products are
 *           explicitly free for any use. The per-item licence field is carried
 *           through when present.
 *   DEA   — Digital Earth Africa products are CC BY 4.0, explicitly free for
 *           any use including commercial.
 *   SWISS — swisstopo open government data (OGD): free use, reuse and
 *           redistribution with source attribution.
 *
 * parse_notes honoured:
 *  - CDSE's /v1/search REQUIRES a collections parameter (HTTP 400
 *    SearchValidationError without one — a hard fail, not an empty result), so
 *    it is always supplied. The resto path
 *    (catalogue.dataspace.copernicus.eu/resto/api/...) returned HTTP 403 and is
 *    NOT used, and stac.dataspace.copernicus.eu is a different host and API from
 *    the OData endpoint already wired.
 *  - BDC's bare /search with only limit works; 'context.matched' is the corpus
 *    total and is logged, never emitted as a per-record value.
 *  - DEA's /stac/search path was flaky at probe time; the
 *    /collections/<id>/items path is used instead (feed_get_json already
 *    retries on transport errors).
 *  - swisstopo is STAC 0.9, older than the 1.x used by the other three: the
 *    FeatureCollection carries a 'timeStamp' field and items declare
 *    stac_version 0.9.0. Only the shared Feature shape is relied on.
 *  - Geometry is a Polygon FOOTPRINT, so it is stored whole as
 *    geometry_geojson and the pin is derived from its own vertices; a
 *    geo_precision is recorded so nobody mistakes the pin for a target.
 *  - Items are large (70 KB each with assets); limits are kept low and the
 *    asset blobs are not copied into the row.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/pagewalk.h"
#include "geoeo_common.inc"

/* These four all answered `limit` with exactly `limit` items and published a
 * STAC `links:[{"rel":"next","href":…}]` alongside, so the limit was a discard
 * and the continuation was sitting in the response. Measured 2026-10-18:
 * CDSE/BDC/DEA 20 -> 200 and swisstopo 50 -> 100 when asked for ten times as
 * many, which is the shortfall ONE page was dropping. pw_walk follows the
 * server's own next link (lib/pagewalk.c selects it by `rel`, never by
 * position) and discloses whatever the page ceiling leaves behind, so the
 * `limit` in each URL is now a page size rather than a cap. */
typedef struct {
  const char *record_type, *tags, *geo_precision;
  int exclude_ccm;
} stac_page_ud;

/* STAC search can be slow to answer; keep the 60 s this collector always used
 * rather than inherit pw_fetch_json's 25 s. */
static cJSON *stac_fetch(const source_ctx *c, const char *url, void *ud) {
  (void)ud;
  return feed_get_json(c->http, url, 60000);
}

static int stac_emit_page(const source_ctx *ctx, intel_sink *sink,
                          const char *sid, cJSON *doc, void *ud, int *seen) {
  const stac_page_ud *u = (const stac_page_ud *)ud;
  const char *record_type = u->record_type, *tags = u->tags,
             *geo_precision = u->geo_precision;
  const int exclude_ccm = u->exclude_ccm;
  (void)ctx;
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) {
    fprintf(stderr, "[%s] no features array (not a STAC ItemCollection)\n", sid);
    return 0;
  }
  /* What the page CONTAINED, not what we emitted: the ccm filter below refuses
   * records, and driving "was this page full" off the emitted count would make
   * a full page of excluded scenes look short — stopping the walk AND
   * suppressing its own truncation notice (lib/pagewalk.h). */
  *seen = cJSON_GetArraySize(feats);
  cJSON *context = cJSON_GetObjectItem(doc, "context");
  double matched = 0;
  if (context && geoeo_num(context, "matched", &matched))
    fprintf(stderr, "[%s] catalogue reports %.0f matching items\n", sid, matched);

  static const char *PKEYS[] = { "datetime", "start_datetime", "end_datetime",
                                 "platform", "constellation", "instruments",
                                 "gsd", "created", "updated", "license",
                                 "sar:instrument_mode", "sar:polarizations",
                                 "eo:cloud_cover", "proj:epsg",
                                 "product_type", "title", NULL };
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    const char *iid = geoeo_str(f, "id");
    if (!iid) continue;
    const char *coll = geoeo_str(f, "collection");
    /* Contributing-mission collections carry stricter ESA licences. */
    if (exclude_ccm && coll && strncmp(coll, "ccm", 3) == 0) continue;

    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *when = geoeo_str(p, "datetime");
    if (!when) when = geoeo_str(p, "start_datetime");

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "item_id", iid);
    geoeo_add_str(props, "collection", coll);
    if (p) geoeo_copy_all(props, p, PKEYS);
    cJSON *bbox = cJSON_GetObjectItem(f, "bbox");
    if (cJSON_IsArray(bbox)) {
      cJSON *b = cJSON_AddArrayToObject(props, "bbox");
      cJSON *v;
      cJSON_ArrayForEach(v, bbox)
        if (cJSON_IsNumber(v))
          cJSON_AddItemToArray(b, cJSON_CreateNumber(v->valuedouble));
    }
    if (geo) {
      cJSON_AddNumberToObject(props, "pin_latitude", lat);
      cJSON_AddNumberToObject(props, "pin_longitude", lon);
      cJSON_AddStringToObject(props, "geo_precision", geo_precision);
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[416];
    snprintf(title, sizeof title, "%s%s%s", iid, coll ? " · " : "",
             coll ? coll : "");

    char summary[224];
    const char *plat = geoeo_str(p, "platform");
    snprintf(summary, sizeof summary, "%s%s%s", plat ? plat : "",
             (plat && when) ? " · " : "", when ? when : "");

    char uid[320];
    snprintf(uid, sizeof uid, "%s|%s", sid, iid);

    intel_item it = {0};
    it.uid = uid;
    it.remote_key = iid;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "en";
    it.published_at = when;
    it.record_type = record_type;
    it.sub_source_id = coll;
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  return n;
}

static int stac_collect(const source_ctx *ctx, intel_sink *sink,
                        const char *sid, const char *url,
                        const char *record_type, const char *tags,
                        const char *geo_precision, int exclude_ccm) {
  stac_page_ud u = { record_type, tags, geo_precision, exclude_ccm };
  int n = pw_walk(ctx, sink, sid, url, stac_fetch, stac_emit_page, &u);
  if (n < 0) {                        /* only the FIRST fetch failing is an error */
    fprintf(stderr, "[%s] fetch/parse failed\n", sid);
    return -1;
  }
  fprintf(stderr, "[%s] emitted %d STAC items\n", sid, n);
  return 0;
}

static int cdse_run(const source_ctx *ctx, intel_sink *sink) {
  return stac_collect(
      ctx, sink, "cdse-stac",
      "https://stac.dataspace.copernicus.eu/v1/search?collections=sentinel-1-grd&limit=20",
      "satellite-scene",
      "[\"satellite\",\"sar\",\"sentinel\",\"copernicus\",\"stac\"]",
      "scene-footprint", 1);
}

static int bdc_run(const source_ctx *ctx, intel_sink *sink) {
  return stac_collect(ctx, sink, "bdc-stac",
                      "https://data.inpe.br/bdc/stac/v1/search?limit=20",
                      "satellite-scene",
                      "[\"satellite\",\"brazil\",\"inpe\",\"stac\"]",
                      "item-extent", 0);
}

static int dea_run(const source_ctx *ctx, intel_sink *sink) {
  return stac_collect(
      ctx, sink, "dea-stac",
      "https://explorer.digitalearth.africa/stac/collections/s2_l2a/items?limit=20",
      "satellite-scene",
      "[\"satellite\",\"africa\",\"sentinel-2\",\"stac\"]",
      "scene-footprint", 0);
}

static int swiss_run(const source_ctx *ctx, intel_sink *sink) {
  return stac_collect(
      ctx, sink, "swisstopo-stac",
      "https://data.geo.admin.ch/api/stac/v0.9/collections/"
      "ch.swisstopo.swissimage-dop10/items?limit=50",
      "aerial-imagery-tile",
      "[\"aerial\",\"orthophoto\",\"switzerland\",\"swisstopo\",\"stac\"]",
      "tile-footprint", 0);
}

static const source_def geoeo_cdse_def = {
  .id = "cdse-stac", .collector = "satellite",
  .name = "Copernicus Data Space Ecosystem STAC API",
  .update_interval_sec = 3600, .run = cdse_run,
  .category = "satellite", .type = "api",
  .url = "https://stac.dataspace.copernicus.eu/v1/search?collections=sentinel-1-grd&limit=20",
  .description = "ESA's official Copernicus Data Space STAC API returning real Sentinel-1 SAR scene records within minutes of acquisition — keyless discovery of the largest open EO archive.",
  .license = "Copernicus open and free data policy (Regulation EU 1159/2013) — free, full and open access including redistribution; ccm-* collections excluded.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_cdse_def)

static const source_def geoeo_bdc_def = {
  .id = "bdc-stac", .collector = "satellite",
  .name = "Brazil Data Cube STAC (INPE)",
  .update_interval_sec = 21600, .run = bdc_run,
  .category = "satellite", .type = "api",
  .url = "https://data.inpe.br/bdc/stac/v1/search?limit=20",
  .description = "Brazil's National Institute for Space Research STAC catalogue — CBERS, Amazonia-1, Landsat and Sentinel data cubes plus downscaled climate projections over South America.",
  .license = "INPE open data (Brazilian government); CBERS/Amazonia products explicitly free for any use.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_bdc_def)

static const source_def geoeo_dea_def = {
  .id = "dea-stac", .collector = "satellite",
  .name = "Digital Earth Africa STAC",
  .update_interval_sec = 21600, .run = dea_run,
  .category = "satellite", .type = "api",
  .url = "https://explorer.digitalearth.africa/stac/collections/s2_l2a/items?limit=20",
  .description = "Digital Earth Africa's analysis-ready data catalogue — continental-scale Sentinel-2 L2A and related products over Africa, free and keyless.",
  .license = "Digital Earth Africa open data programme — CC BY 4.0, free for any use including commercial.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_dea_def)

static const source_def geoeo_swisstopo_def = {
  .id = "swisstopo-stac", .collector = "satellite",
  .name = "swisstopo STAC Aerial Imagery Index",
  .update_interval_sec = 86400, .run = swiss_run,
  .category = "satellite", .type = "api",
  .url = "https://data.geo.admin.ch/api/stac/v0.9/collections/ch.swisstopo.swissimage-dop10/items?limit=50",
  .description = "The Swiss federal geodata STAC — 10 cm orthophoto tiles as discoverable STAC items with footprints and direct download assets.",
  .license = "swisstopo open government data (OGD): free use, reuse and redistribution with source attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_swisstopo_def)
