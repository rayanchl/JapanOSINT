/* CWFIS Canada — satellite thermal fire hotspots from the last 24 hours, with
 * fire radiative power, estimated area and the full Canadian Fire Weather Index
 * suite per detection. A keyless alternative to NASA FIRMS (which needs a
 * MAP_KEY).
 *
 * Endpoint (keyless):
 *   https://cwfis.cfs.nrcan.gc.ca/geoserver/public/wfs?service=WFS&version=1.0.0
 *   &request=GetFeature&typeName=public:hotspots_last24hrs
 *   &outputFormat=application/json&maxFeatures=500
 * Emits: lat, lon, rep_date, source, sensor, satellite, frp, temp, fwi,
 *        estarea, fuel, elev — all from properties.
 * Licence: Natural Resources Canada / Canadian Forest Service, Open Government
 *          Licence — Canada. Reuse permitted with attribution.
 *
 * CRITICAL PROJECTION TRAP (from parse_notes): geometry.coordinates on this
 * layer are in a PROJECTED CRS (Lambert / EPSG:3978-style METRES, e.g.
 * [-2221627.49, 2656793.22]). Feeding them to a map as lon/lat puts every fire
 * off the planet. The correct WGS84 values are in properties.lat and
 * properties.lon (64.27583, -148.33371). This collector therefore IGNORES
 * geometry entirely and builds its Point from the properties — and the
 * WGS84 sanity gate would reject a metre value anyway, so a change upstream
 * degrades to has_geo=0 rather than to a wrong pin.
 *
 * typeName must be exactly 'public:hotspots_last24hrs' — 'public:m3_hotspots'
 * and 'public:activefires_current' both return a WFS ServiceException with
 * HTTP 200, which is why the features array is validated before use.
 * frp/temp can be null on low-confidence detections and are dropped, not zeroed.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL =
    "https://cwfis.cfs.nrcan.gc.ca/geoserver/public/wfs?service=WFS"
    "&version=1.0.0&request=GetFeature&typeName=public:hotspots_last24hrs"
    "&outputFormat=application/json&maxFeatures=500";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 45000);
  if (!doc) {
    fprintf(stderr, "[cwfis-hotspots] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) {
    fprintf(stderr, "[cwfis-hotspots] no features array (WFS exception?)\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *KEYS[] = { "rep_date", "source", "sensor", "satellite",
                                "fuel", "agency", "hotspot_id", NULL };
  static const char *NUMS[] = { "frp", "temp", "fwi", "estarea", "elev",
                                "ffmc", "dmc", "dc", "isi", "bui", "greenup",
                                "sfc", "tfc", "ros", "cfb", NULL };
  int n = 0, dropped = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    if (!p) continue;

    /* WGS84 from the PROPERTIES; the geometry is projected metres. */
    double lat = 0, lon = 0;
    int geo = geoeo_numlax(p, "lat", &lat) && geoeo_numlax(p, "lon", &lon) &&
              geoeo_ll_ok(lat, lon);
    if (!geo) dropped++;

    const char *rep = geoeo_str(p, "rep_date");
    const char *sat = geoeo_str(p, "satellite");
    const char *sens = geoeo_str(p, "sensor");
    double frp = 0, area = 0;
    int has_frp = geoeo_numlax(p, "frp", &frp);
    int has_area = geoeo_numlax(p, "estarea", &area);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, p, KEYS);
    for (int i = 0; NUMS[i]; i++) {
      double d = 0;
      if (geoeo_numlax(p, NUMS[i], &d)) cJSON_AddNumberToObject(props, NUMS[i], d);
    }
    if (geo) { cJSON_AddNumberToObject(props, "lat", lat);
               cJSON_AddNumberToObject(props, "lon", lon);
               cJSON_AddStringToObject(props, "crs_note",
                   "WGS84 from properties.lat/lon; layer geometry is a "
                   "projected (metre) CRS and is deliberately not used"); }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char title[224];
    if (has_frp)
      snprintf(title, sizeof title, "Fire hotspot %.2f MW — %s%s%s", frp,
               sens ? sens : "satellite", sat ? "/" : "", sat ? sat : "");
    else
      snprintf(title, sizeof title, "Fire hotspot — %s%s%s",
               sens ? sens : "satellite", sat ? "/" : "", sat ? sat : "");

    char summary[192];
    if (has_area)
      snprintf(summary, sizeof summary, "est. area %.2f ha%s%s", area,
               rep ? " · " : "", rep ? rep : "");
    else
      snprintf(summary, sizeof summary, "%s", rep ? rep : "");

    /* No stable upstream id on this layer: key on the detection's own
     * position + report time, both fetched values. */
    char key[160];
    const char *fid = geoeo_str(f, "id");
    if (fid) snprintf(key, sizeof key, "%s", fid);
    else snprintf(key, sizeof key, "%.5f,%.5f|%s", lat, lon, rep ? rep : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "en";
    it.published_at = rep;
    it.record_type = "fire-hotspot";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"wildfire\",\"hotspot\",\"satellite\",\"canada\",\"cwfis\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[cwfis-hotspots] emitted %d hotspots (%d without usable "
                  "WGS84 properties)\n", n, dropped);
  return 0;
}

static const source_def geoeo_cwfis_def = {
  .id = "cwfis-hotspots", .collector = "satellite",
  .name = "CWFIS Canada Fire Hotspots (last 24h)",
  .update_interval_sec = 3600, .run = run,
  .category = "satellite", .type = "api",
  .url = "https://cwfis.cfs.nrcan.gc.ca/geoserver/public/wfs",
  .description = "Canadian Wildland Fire Information System satellite thermal hotspots from the last 24 hours with fire radiative power, estimated area and the full Fire Weather Index suite per detection.",
  .license = "Open Government Licence — Canada (NRCan / Canadian Forest Service); attribution required.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_cwfis_def)
