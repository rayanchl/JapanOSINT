/* Two keyless point elevation services. Both are COORDINATE-PIVOT lookups: the
 * caller supplies the point, so update_interval_sec is 0 (on-demand /
 * OSINT-dispatched) rather than scheduled. ctx->entity is read as "lat,lon";
 * with no entity the collector logs one line and returns 0.
 *
 * Endpoints (both keyless):
 *   https://api.opentopodata.org/v1/<dataset>?locations=<lat>,<lon>
 *   https://epqs.nationalmap.gov/v1/json?x=<lon>&y=<lat>&wkid=4326&units=Meters&includeDate=false
 * Emits: the dataset queried, the elevation/depth in metres, the source DEM
 *        resolution (EPQS) and the coordinate the SERVICE echoed back.
 * Licences: OpenTopoData is open-source; the public instance is free with a
 *   documented rate limit of 1 call/sec and 1,000 calls/day and asks heavy
 *   users to self-host — hence at most two calls per pivot here and no
 *   batching. Underlying datasets (SRTM/NASA, GEBCO) are open. USGS 3DEP/EPQS
 *   is US Government public domain.
 *
 * GEOMETRY HONESTY: the position on these rows is the coordinate the SERVICE
 * returned in its own response (results[].location for OpenTopoData,
 * location.x/y for EPQS) — i.e. the echoed query. That is real fetched data,
 * but it is the caller's point, not a discovery, and it is labelled
 * geo_precision "echoed-query" so nobody reads it as an observation.
 *
 * parse_notes honoured:
 *  - OpenTopoData: the dataset is a PATH segment (/v1/srtm90m, /v1/gebco2020);
 *    the echoed coordinate uses 'lng', NOT 'lon'; `elevation` is JSON null over
 *    ocean for land-only datasets and null is the honest answer, not an error;
 *    `status` must be "OK". Both srtm90m (land) and gebco2020 (bathymetry) are
 *    tried, so one collector covers land and sea.
 *  - EPQS: PARAMETER ORDER AND SET MATTER — x,y,wkid,units,includeDate in that
 *    order works; dropping wkid returns the PLAIN-TEXT error "Invalid or
 *    missing input parameters." and putting units before wkid returns a WKID
 *    complaint, both with HTTP 200 and a NON-JSON body. A non-JSON body is
 *    therefore treated as a fetch failure. `value` is a STRING, not a number,
 *    and `resolution` is the source DEM resolution in metres (1 = lidar) —
 *    real intelligence about data quality. Coverage is US only.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

/* "35.36,138.73" (or "35.36 138.73") → lat, lon. */
static int parse_entity_ll(const char *e, double *lat, double *lon) {
  if (!e || !*e) return 0;
  char buf[128];
  snprintf(buf, sizeof buf, "%s", e);
  char *sep = strpbrk(buf, ",; \t/");
  if (!sep) return 0;
  *sep = 0;
  if (!geoeo_strtod(buf, lat)) return 0;
  if (!geoeo_strtod(sep + 1, lon)) return 0;
  return geoeo_ll_ok(*lat, *lon);
}

/* ---- OpenTopoData ---- */

static int otd_query(const source_ctx *ctx, intel_sink *sink,
                     const char *dataset, double qlat, double qlon,
                     int *emitted) {
  char url[256];
  snprintf(url, sizeof url,
           "https://api.opentopodata.org/v1/%s?locations=%.6f,%.6f", dataset,
           qlat, qlon);
  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) return -1;

  const char *status = geoeo_str(doc, "status");
  if (!status || strcmp(status, "OK") != 0) {
    fprintf(stderr, "[opentopodata-elevation] status=%s\n",
            status ? status : "(missing)");
    cJSON_Delete(doc);
    return -1;
  }
  cJSON *results = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(results)) { cJSON_Delete(doc); return -1; }

  int had_value = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, results) {
    /* Echoed coordinate: note 'lng', not 'lon'. */
    cJSON *loc = cJSON_GetObjectItem(r, "location");
    double lat = 0, lon = 0;
    int geo = loc && geoeo_num(loc, "lat", &lat) &&
              geoeo_num(loc, "lng", &lon) && geoeo_ll_ok(lat, lon);

    double elev = 0;
    /* null elevation is the honest answer over ocean for a land-only DEM. */
    int has_elev = geoeo_num(r, "elevation", &elev);
    const char *ds = geoeo_str(r, "dataset");
    if (!has_elev) continue;
    had_value = 1;

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "dataset", ds ? ds : dataset);
    cJSON_AddNumberToObject(props, "elevation_m", elev);
    if (geo) {
      cJSON_AddNumberToObject(props, "latitude", lat);
      cJSON_AddNumberToObject(props, "longitude", lon);
      cJSON_AddStringToObject(props, "geo_precision", "echoed-query");
    }
    cJSON_AddStringToObject(props, "service", "OpenTopoData");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char title[224];
    snprintf(title, sizeof title, "%s: %.1f m at %.5f,%.5f",
             ds ? ds : dataset, elev, lat, lon);

    char key[160];
    snprintf(key, sizeof key, "%s|%.5f,%.5f", ds ? ds : dataset, lat, lon);

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = elev < 0 ? "below sea level (bathymetry)" : NULL;
    it.lang = "en";
    it.record_type = "elevation-point";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"elevation\",\"terrain\",\"bathymetry\",\"opentopodata\"]";
    if (sink->emit(sink, &it) >= 0) (*emitted)++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  return had_value;
}

static int otd_run(const source_ctx *ctx, intel_sink *sink) {
  double lat = 0, lon = 0;
  if (!parse_entity_ll(ctx->entity, &lat, &lon)) {
    fprintf(stderr, "[opentopodata-elevation] no 'lat,lon' entity supplied — "
                    "nothing to look up\n");
    return 0;
  }
  int n = 0;
  int land = otd_query(ctx, sink, "srtm90m", lat, lon, &n);
  if (land < 0) {
    fprintf(stderr, "[opentopodata-elevation] srtm90m lookup failed\n");
    return -1;
  }
  /* No land elevation here → ask the bathymetry DEM (same schema). */
  if (land == 0) otd_query(ctx, sink, "gebco2020", lat, lon, &n);
  fprintf(stderr, "[opentopodata-elevation] emitted %d\n", n);
  return 0;
}

/* ---- USGS EPQS (3DEP) ---- */

static int epqs_run(const source_ctx *ctx, intel_sink *sink) {
  double lat = 0, lon = 0;
  if (!parse_entity_ll(ctx->entity, &lat, &lon)) {
    fprintf(stderr, "[usgs-epqs-elevation] no 'lat,lon' entity supplied — "
                    "nothing to look up\n");
    return 0;
  }

  /* x,y,wkid,units,includeDate — this order and this set, nothing else. */
  char url[288];
  snprintf(url, sizeof url,
           "https://epqs.nationalmap.gov/v1/json?x=%.6f&y=%.6f&wkid=4326"
           "&units=Meters&includeDate=false",
           lon, lat);
  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) {
    /* A plain-text error body with HTTP 200 lands here — a fetch failure. */
    fprintf(stderr, "[usgs-epqs-elevation] non-JSON or failed response\n");
    return -1;
  }

  cJSON *loc = cJSON_GetObjectItem(doc, "location");
  double rx = 0, ry = 0;
  int geo = loc && geoeo_numlax(loc, "x", &rx) && geoeo_numlax(loc, "y", &ry) &&
            geoeo_ll_ok(ry, rx);

  double val = 0;
  int has_val = geoeo_numlax(doc, "value", &val);   /* 'value' is a STRING */
  if (has_val && val < -100000) has_val = 0;        /* out-of-coverage sentinel */
  if (!has_val) {
    fprintf(stderr, "[usgs-epqs-elevation] no elevation at %.5f,%.5f "
                    "(US coverage only)\n", lat, lon);
    cJSON_Delete(doc);
    return 0;
  }

  double res = 0;
  int has_res = geoeo_numlax(doc, "resolution", &res);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddNumberToObject(props, "elevation_m", val);
  if (has_res) cJSON_AddNumberToObject(props, "dem_resolution_m", res);
  geoeo_copy(props, doc, "rasterId");
  if (geo) {
    cJSON_AddNumberToObject(props, "latitude", ry);
    cJSON_AddNumberToObject(props, "longitude", rx);
    cJSON_AddStringToObject(props, "geo_precision", "echoed-query");
  }
  cJSON_AddStringToObject(props, "service", "USGS 3DEP EPQS");
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char *gj = NULL;
  if (geo) {
    cJSON *g = geoeo_mk_point(rx, ry);
    gj = cJSON_PrintUnformatted(g);
    cJSON_Delete(g);
  }

  char title[224];
  snprintf(title, sizeof title, "3DEP elevation %.2f m at %.5f,%.5f", val,
           geo ? ry : lat, geo ? rx : lon);

  char summary[128];
  if (has_res)
    snprintf(summary, sizeof summary, "source DEM resolution %.0f m", res);
  else summary[0] = 0;

  char key[128];
  snprintf(key, sizeof key, "3dep|%.5f,%.5f", geo ? ry : lat, geo ? rx : lon);

  intel_item it = {0};
  it.remote_key = key;
  it.title = title;
  it.summary = summary[0] ? summary : NULL;
  it.lang = "en";
  it.record_type = "elevation-point";
  it.has_geo = geo;
  it.lat = ry;
  it.lon = rx;
  it.geometry_geojson = gj;
  it.properties_json = pj ? pj : "{}";
  it.tags_json = "[\"elevation\",\"terrain\",\"lidar\",\"3dep\",\"usgs\"]";
  int n = (sink->emit(sink, &it) >= 0) ? 1 : 0;
  free(pj);
  free(gj);
  cJSON_Delete(doc);
  fprintf(stderr, "[usgs-epqs-elevation] emitted %d\n", n);
  return 0;
}

static const source_def geoeo_otd_def = {
  .id = "opentopodata-elevation", .collector = "geospatial",
  .name = "OpenTopoData Global Elevation and Bathymetry",
  .update_interval_sec = 0, .run = otd_run,
  .category = "geospatial", .type = "api",
  .url = "https://api.opentopodata.org/v1/srtm90m",
  .description = "Keyless point elevation and bathymetry lookup over SRTM, ASTER, EU-DEM, NED and GEBCO 2020 — turns a coordinate into a real terrain height or ocean depth. Entity: 'lat,lon'.",
  .license = "OpenTopoData public instance free with a 1 call/sec, 1,000 calls/day limit; underlying SRTM/GEBCO datasets are open.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_otd_def)

static const source_def geoeo_epqs_def = {
  .id = "usgs-epqs-elevation", .collector = "geospatial",
  .name = "USGS Elevation Point Query Service (3DEP)",
  .update_interval_sec = 0, .run = epqs_run,
  .category = "geospatial", .type = "api",
  .url = "https://epqs.nationalmap.gov/v1/json",
  .description = "USGS 3D Elevation Program point query — metre-resolution lidar-derived elevation for the United States, with the source DEM resolution. Entity: 'lat,lon'.",
  .license = "USGS public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_epqs_def)
