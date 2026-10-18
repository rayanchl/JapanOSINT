/* OpenAerialMap — the Humanitarian OpenStreetMap Team's open imagery index of
 * drone and aerial surveys, mostly flown for disaster response and
 * humanitarian mapping.
 *
 * Endpoint (keyless): https://api.openaerialmap.org/meta?limit=2000&page=1
 * Emits per image: _id, title, uuid (the COG URL), gsd, acquisition_start /
 *   acquisition_end, provider, platform, per-image license, and the image
 *   footprint.
 * Licence: the service metadata declares "license": "CC-BY 4.0" at the response
 *   root; individual images carry their own licence field, which is carried
 *   through per row so the specific terms travel with the record.
 *
 * parse_notes honoured:
 *  - Records live under 'results'; 'meta.found' is the corpus total and is
 *    logged, never emitted as a per-record value. It is also the number the
 *    page size was chosen against — see the comment on URL.
 *  - THREE representations of the same geometry are present: `bbox` (an
 *    array), `footprint` (a WKT POLYGON STRING — deliberately NOT JSON-parsed,
 *    it is carried as text) and `geojson` (a real GeoJSON Polygon object). The
 *    geojson object is the one used for geometry.
 *  - 'projection' is a full WKT CRS string with embedded quotes that would
 *    bloat any row it is copied into, so it is not copied.
 *  - acquisition_start/end are null on older uploads and are dropped rather
 *    than stringified.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/pagewalk.h"
#include "geoeo_common.inc"

/* PAGED. `meta.found` was already being LOGGED on every run — 21,455 images —
 * beside a page of 50, which is the shape rule 2 calls a discard that announces
 * itself to nobody. Measured 2026-10-18: `page` is honoured (page=2 shares no
 * image with page=1) and `limit=2000` is served at 7.2 MB a page, so
 * 21,455 / 2,000 = 11 pages completes the index inside the 20-page ceiling. At
 * limit=50 it would have needed 430.
 *
 * `page=1` is in the URL deliberately: pw_walk advances a parameter the author
 * wrote and never invents one. It is a page NUMBER, so it needs no stride — the
 * walk advances by one and uses the server's own first page as the "was that
 * page full" yardstick. */
static const char *URL = "https://api.openaerialmap.org/meta?limit=2000&page=1";

static cJSON *oam_fetch(const source_ctx *c, const char *url, void *ud) {
  (void)ud;
  return feed_get_json(c->http, url, 120000);   /* 7.2 MB a page */
}

/* pw_emit_fn. `seen` is the page's image count, not the emitted count: an entry
 * with neither _id nor title is skipped below, and reporting the emitted count
 * would make a full page look short — stopping the walk and suppressing its own
 * truncation notice (lib/pagewalk.h). */
static int oam_emit_page(const source_ctx *ctx, intel_sink *sink,
                         const char *sid, cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)ud;
  cJSON *results = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(results)) {
    fprintf(stderr, "[%s] no results array\n", sid);
    *seen = 0;
    return 0;
  }
  *seen = cJSON_GetArraySize(results);
  cJSON *meta = cJSON_GetObjectItem(doc, "meta");
  double found = 0;
  if (meta && geoeo_num(meta, "found", &found))
    fprintf(stderr, "[%s] index reports %.0f images\n", sid, found);

  static const char *KEYS[] = { "_id", "title", "uuid", "gsd", "provider",
                                "platform", "license", "acquisition_start",
                                "acquisition_end", "uploaded_at", "contact",
                                "footprint", NULL };
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, results) {
    const char *id = geoeo_str(r, "_id");
    const char *title = geoeo_str(r, "title");
    if (!id && !title) continue;

    /* The real GeoJSON object — not `footprint`, which is a WKT string. */
    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(r, "geojson"), &lat, &lon, &gj);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, r, KEYS);
    cJSON *bbox = cJSON_GetObjectItem(r, "bbox");
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
      cJSON_AddStringToObject(props, "geo_precision", "image-footprint");
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    double gsd = 0;
    int has_gsd = geoeo_numlax(r, "gsd", &gsd);
    const char *prov = geoeo_str(r, "provider");

    char t[384];
    snprintf(t, sizeof t, "Aerial imagery — %s", title ? title : id);

    char summary[256];
    if (has_gsd)
      snprintf(summary, sizeof summary, "%.2f m GSD%s%s", gsd,
               prov ? " · " : "", prov ? prov : "");
    else
      snprintf(summary, sizeof summary, "%s", prov ? prov : "");

    intel_item it = {0};
    it.remote_key = id ? id : title;
    it.title = t;
    it.summary = summary[0] ? summary : NULL;
    it.link = geoeo_str(r, "uuid");        /* the COG URL, as published */
    it.author = prov;
    it.lang = "en";
    it.published_at = geoeo_str(r, "acquisition_start");
    it.record_type = "aerial-imagery";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"imagery\",\"aerial\",\"drone\",\"humanitarian\",\"oam\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = pw_walk(ctx, sink, "openaerialmap-imagery", URL,
                  oam_fetch, oam_emit_page, NULL);
  if (n < 0) {
    fprintf(stderr, "[openaerialmap-imagery] fetch/parse failed\n");
    return -1;
  }
  return 0;
}

static const source_def geoeo_oam_def = {
  .id = "openaerialmap-imagery", .collector = "satellite",
  .name = "OpenAerialMap Imagery Index",
  .update_interval_sec = 86400, .run = run,
  .category = "satellite", .type = "api",
  /* The scanner reads a `page=` in any URL literal as a fetch site. This one is
   * source_def.url — the human-facing endpoint this row documents — and the
   * request is made by run() above, which walks it with pw_walk. Measured
   * 2026-10-18: 21,455 images across 11 pages. */
  .url = "https://api.openaerialmap.org/meta?limit=2000&page=1",  /* exhaustive-ok: metadata field, not a fetch; run() walks it with pw_walk */
  .description = "The Humanitarian OpenStreetMap Team's open imagery index — drone and aerial surveys flown for disaster response and humanitarian mapping, each with a footprint, GSD and a direct COG URL.",
  .license = "Service metadata declares CC-BY 4.0; individual images carry their own licence field, carried per row.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_oam_def)
