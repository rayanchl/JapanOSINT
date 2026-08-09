/* Maxar Open Data Program — sub-metre WorldView imagery released for major
 * disasters, organised as a STATIC STAC catalogue of events. Knowing which
 * disasters have new high-resolution collects is itself the intelligence.
 *
 * Endpoints (keyless, static JSON — no /search, no query parameters):
 *   level 1  https://maxar-opendata.s3.amazonaws.com/events/catalog.json
 *   level 2  <event>/collection.json   (reached via the level-1 child links)
 * Emits per event collection: the collection id, title/description, its
 *   declared licence, extent.temporal.interval and extent.spatial.bbox as a
 *   real fetched footprint polygon.
 *
 * LICENCE — READ BEFORE EXTENDING: the catalog root declares
 * "license": "CC-BY-NC-4.0" — NON-COMMERCIAL — and the individual acquisition
 * collections declare "license": "proprietary". Programmatic access and
 * redistribution are permitted for NON-COMMERCIAL use only. This collector
 * therefore indexes ONLY the event list (event name, date range, bbox) and
 * links out; it deliberately does NOT mirror imagery, walk the per-tile item
 * documents, or copy asset URLs.
 *
 * parse_notes honoured:
 *  - Static catalogue: four levels exist (events/catalog.json → <event>/
 *    collection.json → ard/acquisition_collections/<id>_collection.json → the
 *    item .json). The cheap, high-value read is the event list plus the event
 *    collection's own extent, which is what is fetched here — real geometry
 *    without walking thousands of tiles.
 *  - Relative hrefs ("./…", "../…") are resolved against the parent URL.
 *  - The number of event fetches is capped per run.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

#define MAXAR_ROOT "https://maxar-opendata.s3.amazonaws.com/events/catalog.json"
#define MAXAR_BASE "https://maxar-opendata.s3.amazonaws.com/events/"
#define MAXAR_MAX_EVENTS 40

/* Resolve a STAC relative href against a base directory URL. */
static void resolve_href(const char *base_dir, const char *href, char *out,
                         size_t n) {
  if (!href) { out[0] = 0; return; }
  if (strncmp(href, "http://", 7) == 0 || strncmp(href, "https://", 8) == 0) {
    snprintf(out, n, "%s", href);
    return;
  }
  const char *h = href;
  char base[512];
  snprintf(base, sizeof base, "%s", base_dir);
  while (1) {
    if (strncmp(h, "./", 2) == 0) { h += 2; continue; }
    if (strncmp(h, "../", 3) == 0) {
      h += 3;
      size_t bl = strlen(base);
      if (bl > 1 && base[bl - 1] == '/') base[bl - 1] = 0;
      char *slash = strrchr(base, '/');
      if (slash && slash > base + 8) slash[1] = 0;
      continue;
    }
    break;
  }
  snprintf(out, n, "%s%s", base, h);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *root = feed_get_json(ctx->http, MAXAR_ROOT, 40000);
  if (!root) {
    fprintf(stderr, "[maxar-opendata] event catalog fetch failed\n");
    return -1;
  }
  cJSON *links = cJSON_GetObjectItem(root, "links");
  if (!cJSON_IsArray(links)) {
    fprintf(stderr, "[maxar-opendata] catalog has no links array\n");
    cJSON_Delete(root);
    return -1;
  }
  const char *root_license = geoeo_str(root, "license");

  int n = 0, fetched = 0;
  cJSON *l;
  cJSON_ArrayForEach(l, links) {
    if (fetched >= MAXAR_MAX_EVENTS) break;
    const char *rel = geoeo_str(l, "rel");
    const char *href = geoeo_str(l, "href");
    if (!rel || !href) continue;
    if (strcmp(rel, "child") != 0 && strcmp(rel, "item") != 0) continue;

    char url[640];
    resolve_href(MAXAR_BASE, href, url, sizeof url);
    if (!url[0]) continue;

    cJSON *coll = feed_get_json(ctx->http, url, 30000);
    fetched++;
    if (!coll) continue;

    const char *cid = geoeo_str(coll, "id");
    const char *ctitle = geoeo_str(coll, "title");
    const char *cdesc = geoeo_str(coll, "description");
    const char *clic = geoeo_str(coll, "license");
    if (!cid && !ctitle) { cJSON_Delete(coll); continue; }

    /* extent.spatial.bbox = [[west, south, east, north]] */
    double w = 0, s = 0, e = 0, nn = 0;
    int geo = 0;
    cJSON *extent = cJSON_GetObjectItem(coll, "extent");
    cJSON *sp = extent ? cJSON_GetObjectItem(extent, "spatial") : NULL;
    cJSON *bb = sp ? cJSON_GetObjectItem(sp, "bbox") : NULL;
    cJSON *b0 = cJSON_IsArray(bb) ? cJSON_GetArrayItem(bb, 0) : NULL;
    if (cJSON_IsArray(b0) && cJSON_GetArraySize(b0) >= 4) {
      cJSON *a0 = cJSON_GetArrayItem(b0, 0), *a1 = cJSON_GetArrayItem(b0, 1);
      cJSON *a2 = cJSON_GetArrayItem(b0, 2), *a3 = cJSON_GetArrayItem(b0, 3);
      if (cJSON_IsNumber(a0) && cJSON_IsNumber(a1) && cJSON_IsNumber(a2) &&
          cJSON_IsNumber(a3)) {
        w = a0->valuedouble; s = a1->valuedouble;
        e = a2->valuedouble; nn = a3->valuedouble;
        geo = geoeo_ll_ok(s, w) && geoeo_ll_ok(nn, e);
      }
    }

    /* extent.temporal.interval = [[start, end]] */
    const char *tstart = NULL, *tend = NULL;
    cJSON *tp = extent ? cJSON_GetObjectItem(extent, "temporal") : NULL;
    cJSON *iv = tp ? cJSON_GetObjectItem(tp, "interval") : NULL;
    cJSON *i0 = cJSON_IsArray(iv) ? cJSON_GetArrayItem(iv, 0) : NULL;
    if (cJSON_IsArray(i0)) {
      cJSON *a = cJSON_GetArrayItem(i0, 0), *b = cJSON_GetArrayItem(i0, 1);
      if (cJSON_IsString(a)) tstart = a->valuestring;
      if (cJSON_IsString(b)) tend = b->valuestring;
    }

    char *gj = NULL;
    double plat = 0, plon = 0;
    if (geo) {
      cJSON *g = geoeo_mk_bbox(w, s, e, nn);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
      plat = (s + nn) / 2;
      plon = (w + e) / 2;
    }

    cJSON *props = cJSON_CreateObject();
    geoeo_add_str(props, "collection_id", cid);
    geoeo_add_str(props, "title", ctitle);
    geoeo_add_str(props, "description", cdesc);
    geoeo_add_str(props, "license", clic ? clic : root_license);
    geoeo_add_str(props, "temporal_start", tstart);
    geoeo_add_str(props, "temporal_end", tend);
    cJSON_AddStringToObject(props, "collection_url", url);
    if (geo) {
      cJSON *b = cJSON_AddArrayToObject(props, "bbox");
      cJSON_AddItemToArray(b, cJSON_CreateNumber(w));
      cJSON_AddItemToArray(b, cJSON_CreateNumber(s));
      cJSON_AddItemToArray(b, cJSON_CreateNumber(e));
      cJSON_AddItemToArray(b, cJSON_CreateNumber(nn));
      cJSON_AddStringToObject(props, "geo_precision", "event-extent");
    }
    cJSON_AddStringToObject(props, "usage_note",
                            "event index only — non-commercial licence; "
                            "imagery and asset URLs are not mirrored");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[384];
    snprintf(title, sizeof title, "Maxar Open Data event — %s",
             ctitle ? ctitle : cid);

    char summary[256];
    snprintf(summary, sizeof summary, "%s%s%s", tstart ? tstart : "",
             (tstart && tend) ? " → " : "", tend ? tend : "");

    intel_item it = {0};
    it.remote_key = cid ? cid : url;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.body = cdesc;
    it.link = url;
    it.lang = "en";
    it.published_at = tstart;
    it.record_type = "disaster-imagery-event";
    it.has_geo = geo;
    it.lat = plat;
    it.lon = plon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"imagery\",\"satellite\",\"disaster\",\"maxar\",\"stac\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
    cJSON_Delete(coll);
  }

  cJSON_Delete(root);
  fprintf(stderr, "[maxar-opendata] emitted %d events (%d collections fetched)\n",
          n, fetched);
  return 0;
}

static const source_def geoeo_maxar_def = {
  .id = "maxar-opendata", .collector = "satellite",
  .name = "Maxar Open Data Disaster Imagery (STAC)",
  .update_interval_sec = 86400, .run = run,
  .category = "satellite", .type = "dataset",
  .url = "https://maxar-opendata.s3.amazonaws.com/events/catalog.json",
  .description = "Maxar's Open Data Program event index — which disasters have sub-metre WorldView collects, with each event's date range and footprint. Event list only; imagery is not mirrored.",
  .license = "CC-BY-NC-4.0 (catalog root) — NON-COMMERCIAL; acquisition collections declare 'proprietary'. Event metadata indexed and linked, imagery not redistributed.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_maxar_def)
