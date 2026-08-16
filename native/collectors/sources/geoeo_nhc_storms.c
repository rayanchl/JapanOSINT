/* NOAA National Hurricane Center — machine-readable current storms.
 *
 * Endpoint (keyless): https://www.nhc.noaa.gov/CurrentStorms.json
 * Emits: storm id, name, classification, intensity (kt), pressure (mb),
 *        position, movement direction and speed, lastUpdate and the public /
 *        forecast advisory URLs. One row per active Atlantic, Eastern Pacific
 *        and Central Pacific tropical cyclone.
 * Licence: NOAA/NWS public domain.
 *
 * parse_notes honoured:
 *  - The pin uses latitudeNumeric / longitudeNumeric (real numbers, west
 *    negative). The sibling `latitude`/`longitude` fields are DISPLAY STRINGS
 *    ("23.3N", "135.0W") and would parse to a positive longitude in the wrong
 *    hemisphere, so they are carried as text only and never used as ordinates.
 *  - `intensity` and `pressure` are strings ("40", "1003") — parsed with a
 *    full-string numeric parse, not coerced.
 *  - activeStorms is [] out of season: a valid empty, R3 return 0.
 *  - Distinct endpoint from the nhc-atlantic / nhc-cpac RSS sources, which
 *    carry advisory prose and links but no structured position.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL = "https://www.nhc.noaa.gov/CurrentStorms.json";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 25000);
  if (!doc) {
    fprintf(stderr, "[nhc-current-storms] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "activeStorms");
  if (!cJSON_IsArray(arr)) {
    if (cJSON_IsArray(doc)) {
      arr = doc;
    } else {
      fprintf(stderr, "[nhc-current-storms] no activeStorms array\n");
      cJSON_Delete(doc);
      return -1;
    }
  }

  static const char *KEYS[] = { "id", "binNumber", "name", "classification",
                                "isActive", "lastUpdate", "publicAdvisory",
                                "forecastAdvisory", "movementDir",
                                "movementSpeed", "latitude", "longitude",
                                NULL };
  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    const char *sid = geoeo_str(s, "id");
    const char *name = geoeo_str(s, "name");
    if (!sid && !name) continue;
    const char *cls = geoeo_str(s, "classification");
    const char *upd = geoeo_str(s, "lastUpdate");

    double lat = 0, lon = 0;
    int geo = geoeo_numlax(s, "latitudeNumeric", &lat) &&
              geoeo_numlax(s, "longitudeNumeric", &lon) &&
              geoeo_ll_ok(lat, lon);

    double kt = 0, mb = 0, mdir = 0, mspd = 0;
    int has_kt = geoeo_numlax(s, "intensity", &kt);
    int has_mb = geoeo_numlax(s, "pressure", &mb);
    int has_dir = geoeo_numlax(s, "movementDir", &mdir);
    int has_spd = geoeo_numlax(s, "movementSpeed", &mspd);

    char title[256];
    if (has_kt)
      snprintf(title, sizeof title, "%s %s — %.0f kt",
               cls ? cls : "Tropical cyclone", name ? name : (sid ? sid : ""),
               kt);
    else
      snprintf(title, sizeof title, "%s %s", cls ? cls : "Tropical cyclone",
               name ? name : (sid ? sid : ""));

    char summary[256];
    int w = 0;
    summary[0] = 0;
    if (has_mb)
      w += snprintf(summary + w, sizeof summary - (size_t)w, "%.0f mb", mb);
    if (has_dir && has_spd && w < (int)sizeof summary)
      snprintf(summary + w, sizeof summary - (size_t)w,
               "%smoving %.0f deg at %.0f kt", w ? " · " : "", mdir, mspd);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, s, KEYS);
    if (has_kt) cJSON_AddNumberToObject(props, "intensity_kt", kt);
    if (has_mb) cJSON_AddNumberToObject(props, "pressure_mb", mb);
    if (geo) { cJSON_AddNumberToObject(props, "latitudeNumeric", lat);
               cJSON_AddNumberToObject(props, "longitudeNumeric", lon); }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    /* The advisory URL, when present, is a real fetched field on the storm. */
    const char *link = NULL;
    cJSON *pub = cJSON_GetObjectItem(s, "publicAdvisory");
    if (cJSON_IsObject(pub)) link = geoeo_str(pub, "url");

    char key[128];
    snprintf(key, sizeof key, "%s|%s", sid ? sid : name, upd ? upd : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.link = link;
    it.lang = "en";
    it.published_at = upd;
    it.record_type = "tropical-cyclone";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"cyclone\",\"hurricane\",\"nhc\",\"noaa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[nhc-current-storms] emitted %d active storms\n", n);
  return 0;
}

static const source_def geoeo_nhc_def = {
  .id = "nhc-current-storms", .collector = "environment",
  .name = "NOAA NHC Current Storms (JSON)",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://www.nhc.noaa.gov/CurrentStorms.json",
  .description = "Structured position, intensity, pressure and motion vector for every active Atlantic, Eastern and Central Pacific tropical cyclone.",
  .license = "NOAA/NWS public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_nhc_def)
