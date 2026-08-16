/* Three public ArcGIS REST FeatureServer/MapServer layers queried with
 * f=geojson. One shared fetch guard, three row builders.
 *
 * Endpoints (all keyless, no token):
 *   NIFC WFIGS current US wildfire incident locations
 *     https://services3.arcgis.com/T4QMspbfLg3qTGWY/arcgis/rest/services/
 *     WFIGS_Incident_Locations_Current/FeatureServer/0/query?where=1%3D1&
 *     outFields=*&f=geojson&resultRecordCount=200
 *   NOAA NCEI multibeam bathymetric survey tracklines
 *     https://gis.ngdc.noaa.gov/arcgis/rest/services/web_mercator/
 *     multibeam_dynamic/MapServer/0/query?...&f=geojson&resultRecordCount=100
 *   USGS 3DEP lidar coverage index (MapServer layer 24)
 *     https://index.nationalmap.gov/arcgis/rest/services/3DEPElevationIndex/
 *     MapServer/24/query?...&f=geojson&resultRecordCount=100
 * Emits: every real scalar attribute the layer returned, plus the upstream
 *        geometry (Point for wildfires, MultiLineString for survey tracks,
 *        MultiPolygon for lidar project footprints).
 * Licences: NIFC/BLM open data (US Government, public domain, served from the
 *   public ArcGIS Online org, no token); NOAA NCEI public domain; USGS public
 *   domain.
 *
 * parse_notes honoured:
 *  - ArcGIS returns ERRORS WITH HTTP 200: {"error":{"code":400,...}}. The
 *    'error' key is checked BEFORE 'features', otherwise the run silently emits
 *    zero rows and reports success.
 *  - outFields must be '*' on the WFIGS layer; naming a subset is what triggers
 *    that 400.
 *  - exceededTransferLimit is surfaced in the log when the page was truncated.
 *  - The NCEI service is named web_mercator, but f=geojson reprojects to WGS84
 *    degrees (verified: -72.42, 39.60 is off New Jersey). NO mercator inverse is
 *    applied — doing so would move every track.
 *  - Geometry is MultiLineString / MultiPolygon, i.e. nested three or four
 *    levels deep; geoeo_geom() branches on the declared type rather than
 *    assuming coordinates[0] is a longitude.
 *  - Many attributes are JSON null on active fires (ContainmentDateTime,
 *    FinalAcres) — nulls are dropped, never stringified.
 *  - 3DEP MapServer layer 24 is one of many sub-layers; the layer number is a
 *    named constant here so the choice is explicit and revisable rather than
 *    buried in a URL.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

#define NIFC_URL                                                              \
  "https://services3.arcgis.com/T4QMspbfLg3qTGWY/arcgis/rest/services/"       \
  "WFIGS_Incident_Locations_Current/FeatureServer/0/query?where=1%3D1"        \
  "&outFields=*&f=geojson&resultRecordCount=200"

#define NCEI_MB_URL                                                           \
  "https://gis.ngdc.noaa.gov/arcgis/rest/services/web_mercator/"              \
  "multibeam_dynamic/MapServer/0/query?where=1%3D1&outFields=*&f=geojson"     \
  "&resultRecordCount=100"

/* Layer 24 of the 3DEPElevationIndex MapServer — enumerate
 * /3DEPElevationIndex/MapServer?f=json to pick a different product. */
#define TDEP_LAYER "24"
#define TDEP_URL                                                              \
  "https://index.nationalmap.gov/arcgis/rest/services/3DEPElevationIndex/"    \
  "MapServer/" TDEP_LAYER "/query?where=1%3D1&outFields=*&f=geojson"          \
  "&resultRecordCount=100"

/* Fetch + the HTTP-200-error guard. Returns the doc or NULL. */
static cJSON *arcgis_fetch(const source_ctx *ctx, const char *sid,
                           const char *url) {
  cJSON *doc = feed_get_json(ctx->http, url, 60000);
  if (!doc) {
    fprintf(stderr, "[%s] fetch/parse failed\n", sid);
    return NULL;
  }
  cJSON *err = cJSON_GetObjectItem(doc, "error");
  if (err) {                      /* HTTP 200 + an error body — a hard fail */
    const char *msg = geoeo_str(err, "message");
    fprintf(stderr, "[%s] ArcGIS error: %s\n", sid, msg ? msg : "(no message)");
    cJSON_Delete(doc);
    return NULL;
  }
  if (!cJSON_IsArray(cJSON_GetObjectItem(doc, "features"))) {
    fprintf(stderr, "[%s] no features array\n", sid);
    cJSON_Delete(doc);
    return NULL;
  }
  cJSON *props = cJSON_GetObjectItem(doc, "properties");
  if (props && cJSON_IsTrue(cJSON_GetObjectItem(props, "exceededTransferLimit")))
    fprintf(stderr, "[%s] page truncated (exceededTransferLimit)\n", sid);
  return doc;
}

/* OBJECTID (or any object id) as a stable key fragment. */
static void arcgis_key(cJSON *feat, cJSON *p, char *out, size_t n) {
  static const char *IDK[] = { "OBJECTID", "objectid", "OBJECTID_1", "FID",
                               "GlobalID", "globalid", NULL };
  for (int i = 0; IDK[i]; i++) {
    const cJSON *v = cJSON_GetObjectItem(p, IDK[i]);
    if (cJSON_IsNumber(v)) { snprintf(out, n, "%lld", (long long)v->valuedouble); return; }
    if (cJSON_IsString(v) && v->valuestring[0]) { snprintf(out, n, "%s", v->valuestring); return; }
  }
  const cJSON *fid = cJSON_GetObjectItem(feat, "id");
  if (cJSON_IsString(fid)) { snprintf(out, n, "%s", fid->valuestring); return; }
  if (cJSON_IsNumber(fid)) { snprintf(out, n, "%lld", (long long)fid->valuedouble); return; }
  out[0] = 0;
}

/* Shared emit for the generic-attribute layers. */
static int arcgis_emit(intel_sink *sink, cJSON *feats, const char *sid,
                       const char *record_type, const char *tags,
                       const char *const *title_keys, const char *title_prefix,
                       const char *const *time_keys) {
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    char key[128];
    arcgis_key(f, p, key, sizeof key);
    const char *name = geoeo_first_str(p, title_keys);
    if (!name && !key[0]) continue;

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    char iso[48];
    int has_iso = 0;
    for (int i = 0; time_keys && time_keys[i] && !has_iso; i++)
      has_iso = geoeo_epoch_ms_iso(p, time_keys[i], iso, sizeof iso);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_scalars(props, p);
    if (has_iso) cJSON_AddStringToObject(props, "observed_at", iso);
    if (geo) { cJSON_AddNumberToObject(props, "pin_latitude", lat);
               cJSON_AddNumberToObject(props, "pin_longitude", lon); }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[384];
    snprintf(title, sizeof title, "%s%s", title_prefix,
             name ? name : key);

    char uid[192];
    snprintf(uid, sizeof uid, "%s|%s", sid, key[0] ? key : (name ? name : "?"));

    intel_item it = {0};
    it.uid = uid;
    it.remote_key = key[0] ? key : NULL;
    it.title = title;
    it.lang = "en";
    it.published_at = has_iso ? iso : NULL;
    it.record_type = record_type;
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

/* ---- NIFC wildfires: richer title/summary from known WFIGS attributes ---- */
static int nifc_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = arcgis_fetch(ctx, "nifc-wildfires", NIFC_URL);
  if (!doc) return -1;

  static const char *NAMEK[] = { "IncidentName", "attr_IncidentName",
                                 "poly_IncidentName", NULL };
  static const char *TIMEK[] = { "FireDiscoveryDateTime",
                                 "attr_FireDiscoveryDateTime",
                                 "ModifiedOnDateTime_dt", NULL };
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, cJSON_GetObjectItem(doc, "features")) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    char key[128];
    arcgis_key(f, p, key, sizeof key);
    const char *name = geoeo_first_str(p, NAMEK);
    if (!name && !key[0]) continue;

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    double acres = 0;
    int has_acres = geoeo_numlax(p, "IncidentSize", &acres) ||
                    geoeo_numlax(p, "DiscoveryAcres", &acres);
    const char *state = geoeo_str(p, "POOState");
    const char *behav = geoeo_str(p, "FireBehaviorGeneral");

    char iso[48];
    int has_iso = 0;
    for (int i = 0; TIMEK[i] && !has_iso; i++)
      has_iso = geoeo_epoch_ms_iso(p, TIMEK[i], iso, sizeof iso);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_scalars(props, p);
    if (has_iso) cJSON_AddStringToObject(props, "discovered_at", iso);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[352];
    snprintf(title, sizeof title, "Wildfire — %s%s%s", name ? name : key,
             state ? " · " : "", state ? state : "");

    char summary[224];
    int w = 0;
    summary[0] = 0;
    if (has_acres)
      w += snprintf(summary + w, sizeof summary - (size_t)w, "%.0f acres",
                    acres);
    if (behav && w < (int)sizeof summary)
      snprintf(summary + w, sizeof summary - (size_t)w, "%s%s", w ? " · " : "",
               behav);

    char uid[192];
    snprintf(uid, sizeof uid, "nifc-wildfires|%s", key[0] ? key : name);

    intel_item it = {0};
    it.uid = uid;
    it.remote_key = key[0] ? key : NULL;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "en";
    it.published_at = has_iso ? iso : NULL;
    it.record_type = "wildfire-incident";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"wildfire\",\"incident\",\"usa\",\"nifc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[nifc-wildfires] emitted %d incidents\n", n);
  return 0;
}

static int ncei_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = arcgis_fetch(ctx, "ncei-multibeam", NCEI_MB_URL);
  if (!doc) return -1;
  static const char *NAMEK[] = { "SURVEY_ID", "survey_id", "SURVEY", "NAME",
                                 "survey_name", "PLATFORM", "platform_name",
                                 "SHIP_NAME", NULL };
  static const char *TIMEK[] = { "START_TIME", "start_time", "SURVEY_YEAR",
                                 "DATE_ADDED", NULL };
  int n = arcgis_emit(sink, cJSON_GetObjectItem(doc, "features"),
                      "ncei-multibeam", "bathymetric-survey-track",
                      "[\"bathymetry\",\"survey\",\"maritime\",\"noaa\"]",
                      NAMEK, "Multibeam survey track — ", TIMEK);
  cJSON_Delete(doc);
  fprintf(stderr, "[ncei-multibeam] emitted %d tracklines\n", n);
  return 0;
}

static int tdep_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = arcgis_fetch(ctx, "usgs-3dep-index", TDEP_URL);
  if (!doc) return -1;
  static const char *NAMEK[] = { "project", "PROJECT", "workunit", "WORKUNIT",
                                 "project_id", "Project", "prod_name",
                                 "collection_name", NULL };
  static const char *TIMEK[] = { "collect_start", "COLLECT_START",
                                 "pub_date", "collect_end", NULL };
  int n = arcgis_emit(sink, cJSON_GetObjectItem(doc, "features"),
                      "usgs-3dep-index", "lidar-coverage",
                      "[\"lidar\",\"elevation\",\"3dep\",\"usgs\"]",
                      NAMEK, "3DEP lidar project — ", TIMEK);
  cJSON_Delete(doc);
  fprintf(stderr, "[usgs-3dep-index] emitted %d project footprints\n", n);
  return 0;
}

static const source_def geoeo_nifc_def = {
  .id = "nifc-wildfires", .collector = "environment",
  .name = "NIFC WFIGS Current US Wildfire Incidents",
  .update_interval_sec = 1800, .run = nifc_run,
  .category = "environment", .type = "api",
  .url = "https://services3.arcgis.com/T4QMspbfLg3qTGWY/arcgis/rest/services/WFIGS_Incident_Locations_Current/FeatureServer/0/query",
  .description = "Wildland Fire Interagency Geospatial Services current incident locations — every active US wildfire with discovery time, acreage, containment status and point of origin.",
  .license = "NIFC/BLM open data, US Government public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_nifc_def)

static const source_def geoeo_ncei_mb_def = {
  .id = "ncei-multibeam", .collector = "geospatial",
  .name = "NOAA NCEI Multibeam Bathymetric Survey Tracks",
  .update_interval_sec = 604800, .run = ncei_run,
  .category = "geospatial", .type = "api",
  .url = "https://gis.ngdc.noaa.gov/arcgis/rest/services/web_mercator/multibeam_dynamic/MapServer/0/query",
  .description = "NOAA NCEI's archive of multibeam sonar survey tracklines — where research and survey vessels have mapped the seafloor, with vessel, institution and survey date.",
  .license = "NOAA NCEI, US Government public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ncei_mb_def)

static const source_def geoeo_3dep_def = {
  .id = "usgs-3dep-index", .collector = "geospatial",
  .name = "USGS 3DEP Lidar Coverage Index",
  .update_interval_sec = 604800, .run = tdep_run,
  .category = "geospatial", .type = "api",
  .url = "https://index.nationalmap.gov/arcgis/rest/services/3DEPElevationIndex/MapServer/24/query",
  .description = "The USGS 3D Elevation Program coverage index — which lidar and IfSAR acquisition projects cover which ground, at what quality level and in what year.",
  .license = "USGS public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_3dep_def)
