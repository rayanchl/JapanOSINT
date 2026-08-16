/* GLIMS — Global Land Ice Measurements from Space, served as WFS. Satellite
 * derived glacier locations, names, areas and analysis dates worldwide.
 *
 * Endpoint (keyless):
 *   https://www.glims.org/geoserver/GLIMS/ows?service=WFS&version=1.0.0
 *   &request=GetFeature&typeName=GLIMS:GLIMS_Points
 *   &outputFormat=application/json&maxFeatures=500
 * Emits per glacier: glac_id, glac_name, src_date, db_area (km2), anlys_id,
 *   release_date and the upstream Point geometry.
 * Licence: GLIMS (NSIDC / University of Colorado) glacier database, open for
 *   research and general reuse with citation of GLIMS and the RGI.
 *
 * parse_notes honoured:
 *  - typeName is CASE-SENSITIVE and must be exactly 'GLIMS:GLIMS_Points'; the
 *    lowercase 'GLIMS:glims_points' returns a WFS ServiceExceptionReport with
 *    HTTP 200 — a silent failure, so the features array is validated before use
 *    and its absence is a hard -1, not an honest empty.
 *  - COORDINATES HAVE THREE ORDINATES: [8.134, 46.298, 0] — the trailing 0 is a
 *    dummy z. A strict 2-element length check would drop every record;
 *    geoeo_geom() reads the first two ordinates and ignores the rest.
 *  - glac_name is null for the majority of the world's glaciers (only about one
 *    in ten is named), so rows are keyed on glac_id, never on the name, and the
 *    null name is dropped rather than stringified.
 *  - Other layers on the same service (GLIMS_Glacier_Outlines, GLIMS_RC_Outlines,
 *    FOG_points, RGI_<region> for 19 regions) are enumerable with
 *    request=DescribeFeatureType; this collector deliberately takes the point
 *    layer only.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL =
    "https://www.glims.org/geoserver/GLIMS/ows?service=WFS&version=1.0.0"
    "&request=GetFeature&typeName=GLIMS:GLIMS_Points"
    "&outputFormat=application/json&maxFeatures=500";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 60000);
  if (!doc) {
    fprintf(stderr, "[glims-glaciers] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) {
    fprintf(stderr, "[glims-glaciers] no features array (WFS exception?)\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *KEYS[] = { "glac_id", "glac_name", "src_date",
                                "release_date", "anlys_id", "anlys_time",
                                "wgms_id", "local_id", "geog_area", NULL };
  int n = 0, named = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *gid = geoeo_str(p, "glac_id");
    if (!gid) continue;                       /* key on glac_id, not name */
    const char *gname = geoeo_str(p, "glac_name");
    if (gname) named++;

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    double area = 0;
    int has_area = geoeo_numlax(p, "db_area", &area);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, p, KEYS);
    if (has_area) cJSON_AddNumberToObject(props, "db_area_km2", area);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "database", "GLIMS (NSIDC/CU Boulder)");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[288];
    if (gname)
      snprintf(title, sizeof title, "Glacier %s (%s)", gname, gid);
    else
      snprintf(title, sizeof title, "Glacier %s (unnamed)", gid);

    char summary[192];
    const char *src = geoeo_str(p, "src_date");
    if (has_area)
      snprintf(summary, sizeof summary, "%.4f km2%s%s", area,
               src ? " · imagery " : "", src ? src : "");
    else
      snprintf(summary, sizeof summary, "%s", src ? src : "");

    intel_item it = {0};
    it.remote_key = gid;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "en";
    it.published_at = geoeo_str(p, "src_date");
    it.record_type = "glacier";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"glacier\",\"cryosphere\",\"glims\",\"satellite\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[glims-glaciers] emitted %d glaciers (%d named)\n", n, named);
  return 0;
}

static const source_def geoeo_glims_def = {
  .id = "glims-glaciers", .collector = "geospatial",
  .name = "GLIMS Global Land Ice Measurements from Space",
  .update_interval_sec = 604800, .run = run,
  .category = "geospatial", .type = "dataset",
  .url = "https://www.glims.org/geoserver/GLIMS/ows",
  .description = "The GLIMS glacier database served as WFS — satellite-derived glacier locations, names, areas and analysis dates worldwide.",
  .license = "GLIMS (NSIDC / University of Colorado); open for research and general reuse with citation of GLIMS and the RGI.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_glims_def)
