/* GNS Science / GeoNet New Zealand — two GeoJSON endpoints.
 *
 * Endpoints (keyless, no credential of any kind):
 *   https://api.geonet.org.nz/quake?MMI=3      → FeatureCollection of quakes
 *   https://api.geonet.org.nz/volcano/val      → FeatureCollection of the 12
 *                                                monitored volcanoes
 * Emits (all values fetched, nothing constructed):
 *   quakes   — publicID, time, depth(km), magnitude, mmi, locality, quality
 *              and the upstream Point geometry.
 *   volcanoes— volcanoID, volcanoTitle, level, acc (aviation colour code),
 *              activity, hazards and the upstream Point geometry.
 * Licence: GeoNet data is CC BY 3.0 NZ (GNS Science / EQC). Programmatic
 *          reuse explicitly permitted; attribution required.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static const char *Q_URL = "https://api.geonet.org.nz/quake?MMI=3";
static const char *V_URL = "https://api.geonet.org.nz/volcano/val";

static int quakes_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, Q_URL, 25000);
  if (!doc) {
    fprintf(stderr, "[geonet-nz-quakes] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) {
    fprintf(stderr, "[geonet-nz-quakes] no features array\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *pid = geoeo_str(p, "publicID");
    if (!pid) continue;                          /* no upstream id → no row */

    double mag = 0, dep = 0, mmi = 0;
    int has_mag = geoeo_num(p, "magnitude", &mag);
    int has_dep = geoeo_num(p, "depth", &dep);
    int has_mmi = geoeo_num(p, "mmi", &mmi);
    const char *loc = geoeo_str(p, "locality");
    const char *when = geoeo_str(p, "time");

    /* parse_notes: "Every one of the 100 features probed carried geometry;
     * still guard for geometry==null before dereferencing coordinates." */
    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    /* magnitude/depth come back with excessive precision
     * (2.6322321953010395) — round for the human-readable title only, the
     * full-precision value stays in properties. */
    char title[320];
    if (has_mag)
      snprintf(title, sizeof title, "M%.1f %s", mag, loc ? loc : "New Zealand");
    else
      snprintf(title, sizeof title, "GeoNet quake %s", pid);

    char summary[256];
    int sw = 0;
    summary[0] = 0;
    if (has_dep)
      sw += snprintf(summary + sw, sizeof summary - (size_t)sw,
                     "depth %.1f km", dep);
    if (has_mmi && sw < (int)sizeof summary)
      snprintf(summary + sw, sizeof summary - (size_t)sw, "%sMMI %.0f",
               sw ? " · " : "", mmi);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "publicID", pid);
    geoeo_add_str(props, "locality", loc);
    geoeo_add_str(props, "time", when);
    geoeo_add_str(props, "quality", geoeo_str(p, "quality"));
    if (has_mag) cJSON_AddNumberToObject(props, "magnitude", mag);
    if (has_dep) cJSON_AddNumberToObject(props, "depth_km", dep);
    if (has_mmi) cJSON_AddNumberToObject(props, "mmi", mmi);
    cJSON_AddStringToObject(props, "network", "GeoNet (GNS Science)");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char link[256];
    snprintf(link, sizeof link, "https://www.geonet.org.nz/earthquake/%s", pid);

    intel_item it = {0};
    it.remote_key = pid;
    it.title = title;
    it.summary = summary;
    it.link = link;
    it.lang = "en";
    it.published_at = when;
    it.record_type = "earthquake";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"earthquake\",\"seismic\",\"new-zealand\",\"geonet\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[geonet-nz-quakes] emitted %d\n", n);
  return 0;
}

static int volcano_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, V_URL, 25000);
  if (!doc) {
    fprintf(stderr, "[geonet-volcano-alerts] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) {
    fprintf(stderr, "[geonet-volcano-alerts] no features array\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *vid = geoeo_str(p, "volcanoID");
    if (!vid) continue;
    const char *vtitle = geoeo_str(p, "volcanoTitle");
    const char *acc = geoeo_str(p, "acc");
    const char *activity = geoeo_str(p, "activity");
    double level = 0;
    int has_level = geoeo_num(p, "level", &level);

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    char title[256];
    if (has_level)
      snprintf(title, sizeof title, "%s — Volcanic Alert Level %d (%s)",
               vtitle ? vtitle : vid, (int)level, acc ? acc : "n/a");
    else
      snprintf(title, sizeof title, "%s — Volcanic Alert Level n/a",
               vtitle ? vtitle : vid);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "volcanoID", vid);
    geoeo_add_str(props, "volcanoTitle", vtitle);
    geoeo_add_str(props, "aviation_colour_code", acc);
    geoeo_add_str(props, "activity", activity);
    geoeo_add_str(props, "hazards", geoeo_str(p, "hazards"));
    if (has_level) cJSON_AddNumberToObject(props, "level", level);
    cJSON_AddStringToObject(props, "network", "GeoNet (GNS Science)");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char link[256];
    snprintf(link, sizeof link, "https://www.geonet.org.nz/volcano/%s", vid);

    intel_item it = {0};
    it.remote_key = vid;
    it.title = title;
    it.summary = activity;
    it.body = geoeo_str(p, "hazards");
    it.link = link;
    it.lang = "en";
    it.record_type = "volcano-alert-level";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"volcano\",\"alert\",\"new-zealand\",\"geonet\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  /* Quiet state is level 0 / Green for every volcano — that is a valid
   * non-empty return, and even a genuinely empty one is R3 return 0. */
  fprintf(stderr, "[geonet-volcano-alerts] emitted %d\n", n);
  return 0;
}

static const source_def geoeo_geonet_quakes_def = {
  .id = "geonet-nz-quakes", .collector = "environment",
  .name = "GeoNet New Zealand Earthquakes",
  .update_interval_sec = 300, .run = quakes_run,
  .category = "environment", .type = "api",
  .url = "https://api.geonet.org.nz/quake?MMI=3",
  .description = "GNS Science GeoNet real-time earthquake catalogue for New Zealand filtered by Modified Mercalli intensity — carries felt-shaking MMI, which USGS feeds do not.",
  .license = "CC BY 3.0 NZ (GNS Science / EQC); attribution required.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_geonet_quakes_def)

static const source_def geoeo_geonet_volcano_def = {
  .id = "geonet-volcano-alerts", .collector = "environment",
  .name = "GeoNet New Zealand Volcanic Alert Levels",
  .update_interval_sec = 3600, .run = volcano_run,
  .category = "environment", .type = "api",
  .url = "https://api.geonet.org.nz/volcano/val",
  .description = "Current Volcanic Alert Level and Aviation Colour Code for all 12 monitored New Zealand volcanoes, each with its official coordinate.",
  .license = "CC BY 3.0 NZ (GNS Science); reuse permitted with attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_geonet_volcano_def)
