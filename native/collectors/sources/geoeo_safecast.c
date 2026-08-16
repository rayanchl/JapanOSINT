/* Safecast — the open, crowdsourced radiation dataset born from post-Fukushima
 * citizen monitoring and now global.
 *
 * Endpoint (keyless):
 *   https://api.safecast.org/measurements.json?distance=<m>&latitude=<lat>&longitude=<lon>
 * Emits per measurement: id, value, unit, captured_at, latitude, longitude,
 *   device_id, user_id, location_name and height, exactly as returned.
 * Licence: Safecast data are released CC0 (public domain dedication) and the
 *   read API is keyless.
 *
 * STALENESS WARNING (from parse_notes): the default ordering is NOT
 * newest-first — a radius query around Tokyo returned 2014 measurements, and
 * adding 'order=captured_at desc' did NOT change that. A naive collector would
 * present twelve-year-old readings as current. So:
 *   - captured_at is carried in the title AND in properties, so the age of
 *     every reading is visible at a glance;
 *   - the row is typed 'radiation-measurement' and explicitly NOT labelled
 *     live, and a 'freshness_note' property says the archive is not ordered by
 *     recency.
 * 'unit' varies (cpm vs usv) and comparing values across units is meaningless,
 * so the unit is always carried with the value. location_name, device_id and
 * height are usually null and are dropped rather than stringified.
 *
 * The centre of the radius query is the probed Tokyo point by default and can
 * be overridden with a "lat,lon" entity on an OSINT pivot. Whatever the query
 * centre, the COORDINATES ON EACH ROW are the ones the measurement itself
 * carried — never the query centre.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

#define SAFECAST_DEFAULT_LAT 35.6
#define SAFECAST_DEFAULT_LON 139.7
#define SAFECAST_RADIUS_M 10000

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

static int run(const source_ctx *ctx, intel_sink *sink) {
  double qlat = SAFECAST_DEFAULT_LAT, qlon = SAFECAST_DEFAULT_LON;
  parse_entity_ll(ctx->entity, &qlat, &qlon);

  char url[288];
  snprintf(url, sizeof url,
           "https://api.safecast.org/measurements.json?distance=%d"
           "&latitude=%.5f&longitude=%.5f",
           SAFECAST_RADIUS_M, qlat, qlon);

  cJSON *doc = feed_get_json(ctx->http, url, 40000);
  if (!doc) {
    fprintf(stderr, "[safecast-radiation] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc
                                  : cJSON_GetObjectItem(doc, "measurements");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[safecast-radiation] response is not a measurement array\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *KEYS[] = { "unit", "captured_at", "device_id", "user_id",
                                "location_name", "height", "channel_id",
                                "sensor_id", NULL };
  int n = 0;
  cJSON *m;
  cJSON_ArrayForEach(m, arr) {
    double mid = 0;
    if (!geoeo_numlax(m, "id", &mid)) continue;
    double value = 0;
    int has_value = geoeo_numlax(m, "value", &value);
    const char *unit = geoeo_str(m, "unit");
    const char *when = geoeo_str(m, "captured_at");

    /* The measurement's own coordinates, not the query centre. */
    double lat = 0, lon = 0;
    int geo = geoeo_numlax(m, "latitude", &lat) &&
              geoeo_numlax(m, "longitude", &lon) && geoeo_ll_ok(lat, lon);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddNumberToObject(props, "id", mid);
    geoeo_copy_all(props, m, KEYS);
    if (has_value) cJSON_AddNumberToObject(props, "value", value);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddNumberToObject(props, "query_radius_m", SAFECAST_RADIUS_M);
    cJSON_AddStringToObject(props, "freshness_note",
                            "the Safecast archive is NOT ordered newest-first; "
                            "read captured_at before treating a value as current");
    cJSON_AddStringToObject(props, "licence", "CC0");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char title[256];
    if (has_value)
      snprintf(title, sizeof title, "Radiation %.2f %s (%s)", value,
               unit ? unit : "?", when ? when : "date unknown");
    else
      snprintf(title, sizeof title, "Safecast measurement %lld (%s)",
               (long long)mid, when ? when : "date unknown");

    char key[64];
    snprintf(key, sizeof key, "%lld", (long long)mid);

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = geoeo_str(m, "location_name");
    it.lang = "en";
    it.published_at = when;
    it.record_type = "radiation-measurement";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"radiation\",\"environment\",\"safecast\",\"crowdsourced\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[safecast-radiation] emitted %d measurements around "
                  "%.4f,%.4f\n", n, qlat, qlon);
  return 0;
}

static const source_def geoeo_safecast_def = {
  .id = "safecast-radiation", .collector = "environment",
  .name = "Safecast Radiation Measurements",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.safecast.org/measurements.json",
  .description = "The Safecast open radiation dataset — crowdsourced gamma readings with position, the largest independent environmental radiation archive. Readings are archival: check captured_at.",
  .license = "CC0 public domain dedication (Safecast); read API keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_safecast_def)
