/* OpenTopography lidar dataset catalogue — which high-resolution terrain
 * surveys exist for an area of interest, who flew them and when.
 *
 * Endpoint (keyless):
 *   https://portal.opentopography.org/API/otCatalog?productFormat=PointCloud
 *   &detail=false&outputFormat=json&include_federated=false
 * Emits per survey: name, the OpenTopography identifier value, alternateName,
 *   the DOI url, fileFormat, dateCreated, temporalCoverage, keywords and the
 *   survey bounding box as a footprint polygon.
 * Licence: the OpenTopography catalogue API is keyless and the catalogue
 *   metadata is openly published (NSF-funded, UC San Diego). Individual
 *   datasets carry their own licences (mostly CC BY). NOTE the separate
 *   globaldem/lidar DOWNLOAD APIs DO require an API key — only this catalogue
 *   is keyless, and only the catalogue is read here.
 *
 * parse_notes honoured:
 *  - Schema.org JSON-LD flavour with DOUBLED nesting: {"Datasets":[{"Dataset":
 *    {...}}]} — each array element is a wrapper object with a single 'Dataset'
 *    key.
 *  - 'identifier' is an OBJECT with @type/propertyID/value, not a scalar.
 *  - AXIS ORDER TRAP: spatialCoverage.geo.box is a SPACE-SEPARATED
 *    "minlat minlon maxlat maxlon" STRING — the schema.org GeoShape convention,
 *    LATITUDE FIRST, the opposite of GeoJSON's [lon,lat]. Copy-pasting a
 *    GeoJSON bbox reader here silently transposes every survey, so the four
 *    values are named explicitly on the way in and swapped on the way out.
 *  - A survey whose box does not parse into four sane ordinates is emitted
 *    with has_geo=0.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL =
    "https://portal.opentopography.org/API/otCatalog?productFormat=PointCloud"
    "&detail=false&outputFormat=json&include_federated=false";

/* "minlat minlon maxlat maxlon" (schema.org GeoShape, LAT FIRST). */
static int parse_geo_box(const char *s, double *minlat, double *minlon,
                         double *maxlat, double *maxlon) {
  if (!s) return 0;
  double v[4];
  const char *p = s;
  for (int i = 0; i < 4; i++) {
    while (*p == ' ' || *p == ',' || *p == '\t') p++;
    char *e = NULL;
    v[i] = strtod(p, &e);
    if (e == p || !isfinite(v[i])) return 0;
    p = e;
  }
  *minlat = v[0];
  *minlon = v[1];
  *maxlat = v[2];
  *maxlon = v[3];
  return geoeo_ll_ok(*minlat, *minlon) && geoeo_ll_ok(*maxlat, *maxlon);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 60000);
  if (!doc) {
    fprintf(stderr, "[opentopography-catalog] fetch/parse failed\n");
    return -1;
  }
  cJSON *sets = cJSON_GetObjectItem(doc, "Datasets");
  if (!cJSON_IsArray(sets)) {
    fprintf(stderr, "[opentopography-catalog] no Datasets array\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *KEYS[] = { "name", "alternateName", "url", "fileFormat",
                                "dateCreated", "temporalCoverage",
                                "description", "citation", NULL };
  int n = 0;
  cJSON *wrap;
  cJSON_ArrayForEach(wrap, sets) {
    cJSON *d = cJSON_GetObjectItem(wrap, "Dataset");   /* doubled nesting */
    if (!cJSON_IsObject(d)) continue;
    const char *name = geoeo_str(d, "name");

    /* 'identifier' is an object: {@type, propertyID, value}. */
    const char *otid = NULL;
    cJSON *ident = cJSON_GetObjectItem(d, "identifier");
    if (cJSON_IsObject(ident)) otid = geoeo_str(ident, "value");
    else if (cJSON_IsString(ident)) otid = ident->valuestring;
    if (!name && !otid) continue;

    double minlat = 0, minlon = 0, maxlat = 0, maxlon = 0;
    int geo = 0;
    cJSON *sc = cJSON_GetObjectItem(d, "spatialCoverage");
    cJSON *g = sc ? cJSON_GetObjectItem(sc, "geo") : NULL;
    const char *box = g ? geoeo_str(g, "box") : NULL;
    if (parse_geo_box(box, &minlat, &minlon, &maxlat, &maxlon)) geo = 1;

    char *gj = NULL;
    double plat = 0, plon = 0;
    if (geo) {
      /* GeoJSON wants [lon, lat]; the source gave lat first. */
      cJSON *poly = geoeo_mk_bbox(minlon, minlat, maxlon, maxlat);
      gj = cJSON_PrintUnformatted(poly);
      cJSON_Delete(poly);
      plat = (minlat + maxlat) / 2;
      plon = (minlon + maxlon) / 2;
    }

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, d, KEYS);
    geoeo_add_str(props, "ot_identifier", otid);
    geoeo_add_str(props, "geo_box_raw", box);
    cJSON *kw = cJSON_GetObjectItem(d, "keywords");
    if (cJSON_IsString(kw) && kw->valuestring[0])
      cJSON_AddStringToObject(props, "keywords", kw->valuestring);
    if (geo) {
      cJSON_AddNumberToObject(props, "min_latitude", minlat);
      cJSON_AddNumberToObject(props, "min_longitude", minlon);
      cJSON_AddNumberToObject(props, "max_latitude", maxlat);
      cJSON_AddNumberToObject(props, "max_longitude", maxlon);
      cJSON_AddStringToObject(props, "geo_precision", "survey-extent");
    }
    cJSON_AddStringToObject(props, "catalogue", "OpenTopography");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[416];
    snprintf(title, sizeof title, "Lidar survey — %s", name ? name : otid);

    char summary[256];
    const char *tc = geoeo_str(d, "temporalCoverage");
    const char *ff = geoeo_str(d, "fileFormat");
    snprintf(summary, sizeof summary, "%s%s%s", tc ? tc : "",
             (tc && ff) ? " · " : "", ff ? ff : "");

    intel_item it = {0};
    it.remote_key = otid ? otid : name;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.link = geoeo_str(d, "url");            /* the DOI */
    it.lang = "en";
    it.published_at = geoeo_str(d, "dateCreated");
    it.record_type = "lidar-survey";
    it.has_geo = geo;
    it.lat = plat;
    it.lon = plon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"lidar\",\"elevation\",\"survey\",\"opentopography\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[opentopography-catalog] emitted %d surveys\n", n);
  return 0;
}

static const source_def geoeo_ot_catalog_def = {
  .id = "opentopography-catalog", .collector = "geospatial",
  .name = "OpenTopography Lidar Dataset Catalogue",
  .update_interval_sec = 604800, .run = run,
  .category = "geospatial", .type = "api",
  .url = "https://portal.opentopography.org/API/otCatalog?productFormat=PointCloud&detail=false&outputFormat=json&include_federated=false",
  .description = "OpenTopography's catalogue of high-resolution lidar and point-cloud surveys worldwide with DOIs, acquisition dates and bounding boxes.",
  .license = "OpenTopography (NSF / UC San Diego) catalogue metadata openly published; individual datasets carry their own licences (mostly CC BY).",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ot_catalog_def)
