/* EMODnet Bathymetry depth contours as vector geometry — seabed depth lines for
 * European and adjacent seas, usable directly as a maritime map layer.
 *
 * Endpoint (keyless):
 *   https://ows.emodnet-bathymetry.eu/wfs?service=WFS&version=2.0.0
 *   &request=GetFeature&typeName=emodnet:contours&count=200
 *   &outputFormat=application/json
 * Emits per contour: the feature id, every real scalar attribute the layer
 *   carries (contour depth and friends) and the full LineString geometry.
 * Licence: EMODnet (European Commission DG MARE) data are free to use, CC BY
 *   4.0, with attribution to EMODnet Bathymetry and the data originators.
 *
 * parse_notes honoured:
 *  - GEOMETRY IS LineString, NOT Point: `coordinates` is an ARRAY OF [lon,lat]
 *    PAIRS, so an emitter that reads coordinates[0] as a longitude gets an
 *    array back. geoeo_geom() branches on the declared type; the whole line is
 *    stored as geometry_geojson and the pin comes from its first vertex.
 *  - WFS 2.0 uses 'count', NOT 'maxFeatures' (1.0.0 uses maxFeatures) — passing
 *    the wrong one for the version silently returns the WHOLE layer, so the
 *    version and the paging parameter are kept matched here.
 *  - Roughly 3 KB per contour, so the count stays modest.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL =
    "https://ows.emodnet-bathymetry.eu/wfs?service=WFS&version=2.0.0"
    "&request=GetFeature&typeName=emodnet:contours&count=200"
    "&outputFormat=application/json";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 60000);
  if (!doc) {
    fprintf(stderr, "[emodnet-bathymetry] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) {
    fprintf(stderr, "[emodnet-bathymetry] no features array (WFS exception?)\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *DEPTHK[] = { "depth", "elevation", "contour", "level",
                                  "value", "DEPTH", NULL };
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    const char *fid = geoeo_str(f, "id");
    cJSON *p = cJSON_GetObjectItem(f, "properties");

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);
    if (!fid && !geo) continue;

    double depth = 0;
    int has_depth = 0;
    for (int i = 0; DEPTHK[i] && !has_depth; i++)
      has_depth = geoeo_numlax(p, DEPTHK[i], &depth);

    cJSON *props = cJSON_CreateObject();
    geoeo_add_str(props, "feature_id", fid);
    if (p) geoeo_copy_scalars(props, p);
    if (geo) {
      cJSON_AddNumberToObject(props, "pin_latitude", lat);
      cJSON_AddNumberToObject(props, "pin_longitude", lon);
      cJSON_AddStringToObject(props, "geo_precision", "contour-first-vertex");
    }
    cJSON_AddStringToObject(props, "provider", "EMODnet Bathymetry");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[256];
    if (has_depth)
      snprintf(title, sizeof title, "Bathymetric contour %.0f m — %s", depth,
               fid ? fid : "EMODnet");
    else
      snprintf(title, sizeof title, "Bathymetric contour — %s",
               fid ? fid : "EMODnet");

    char uid[320];
    snprintf(uid, sizeof uid, "emodnet-bathymetry|%s", fid ? fid : title);

    intel_item it = {0};
    it.uid = uid;
    it.remote_key = fid;
    it.title = title;
    it.lang = "en";
    it.record_type = "bathymetric-contour";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"bathymetry\",\"maritime\",\"contour\",\"emodnet\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-bathymetry] emitted %d contours\n", n);
  return 0;
}

static const source_def geoeo_emodnet_def = {
  .id = "emodnet-bathymetry", .collector = "geospatial",
  .name = "EMODnet Bathymetry Depth Contours (WFS)",
  .update_interval_sec = 604800, .run = run,
  .category = "geospatial", .type = "api",
  .url = "https://ows.emodnet-bathymetry.eu/wfs",
  .description = "European Marine Observation and Data Network bathymetry contours as vector geometry — seabed depth lines for European and adjacent seas.",
  .license = "CC BY 4.0 (EMODnet / European Commission DG MARE); attribute EMODnet Bathymetry and the data originators.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_emodnet_def)
