/* IOC-UNESCO Sea Level Monitoring Facility — the global tide gauge network that
 * underpins tsunami detection outside the US DART array.
 *
 * Endpoint (keyless):
 *   https://www.ioc-sealevelmonitoring.org/service.php?query=stationlist&format=json
 * Emits per station: Code, Location, country, Lat, Lon, type, GlossID, WMO,
 *   the number of sensors registered, the most recent 'last' observation
 *   timestamp seen across those sensors, the observation count and status.
 * Licence: IOC-UNESCO / VLIZ operated facility, open access. Data providers
 *   retain rights over individual station data; the station LIST itself is
 *   openly published, and that is all this collector takes.
 *
 * parse_notes honoured:
 *  - Bare JSON array, ~2 MB.
 *  - Coordinate keys are CAPITALISED 'Lat'/'Lon' (real numbers with float
 *    noise, 56.149999999999999); a lowercase lookup finds nothing.
 *  - Many stations appear MULTIPLE TIMES, once per sensorid. Rows are
 *    de-duplicated on Code so the map shows stations, not sensors — otherwise
 *    a dozen markers stack on one gauge.
 *  - DCP_ID, WMO, GlossID, Har and url are commonly null and are dropped
 *    rather than stringified.
 *  - A station with observations 0 or a stale 'last' is offline, which is
 *    itself intelligence, so both are carried rather than filtered out.
 *  - Per-station sea level VALUES need a second call (query=data&code=<Code>)
 *    and are deliberately not fetched here.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL =
    "https://www.ioc-sealevelmonitoring.org/service.php?query=stationlist&format=json";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 60000);
  if (!doc) {
    fprintf(stderr, "[ioc-sealevel-stations] fetch/parse failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[ioc-sealevel-stations] top level is not an array\n");
    cJSON_Delete(doc);
    return -1;
  }

  int total = cJSON_GetArraySize(doc);
  char **seen = (char **)calloc((size_t)(total > 0 ? total : 1), sizeof(char *));
  if (!seen) { cJSON_Delete(doc); return -1; }
  int nseen = 0, n = 0;

  static const char *KEYS[] = { "Code", "Location", "country", "type",
                                "GlossID", "WMO", "status", "statday",
                                "UTCOffset", NULL };
  cJSON *s;
  cJSON_ArrayForEach(s, doc) {
    const char *code = geoeo_str(s, "Code");
    if (!code) continue;

    int dup = 0;
    for (int i = 0; i < nseen && !dup; i++)
      if (strcmp(seen[i], code) == 0) dup = 1;
    if (dup) continue;
    seen[nseen++] = (char *)code;                 /* borrowed, not owned */

    /* Aggregate across every sensor row for this station. */
    int sensors = 0;
    double obs_total = 0;
    const char *newest = NULL;
    cJSON *o;
    cJSON_ArrayForEach(o, doc) {
      const char *oc = geoeo_str(o, "Code");
      if (!oc || strcmp(oc, code) != 0) continue;
      sensors++;
      double ob = 0;
      if (geoeo_numlax(o, "observations", &ob)) obs_total += ob;
      const char *last = geoeo_str(o, "last");
      if (last && (!newest || strcmp(last, newest) > 0)) newest = last;
    }

    double lat = 0, lon = 0;
    int geo = geoeo_numlax(s, "Lat", &lat) && geoeo_numlax(s, "Lon", &lon) &&
              geoeo_ll_ok(lat, lon);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, s, KEYS);
    cJSON_AddNumberToObject(props, "sensor_count", sensors);
    cJSON_AddNumberToObject(props, "observations_total", obs_total);
    geoeo_add_str(props, "last_observation", newest);
    if (geo) { cJSON_AddNumberToObject(props, "Lat", lat);
               cJSON_AddNumberToObject(props, "Lon", lon); }
    cJSON_AddStringToObject(props, "network", "IOC-UNESCO Sea Level Monitoring");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    const char *loc = geoeo_str(s, "Location");
    const char *ctry = geoeo_str(s, "country");
    char title[288];
    snprintf(title, sizeof title, "Tide gauge %s — %s%s%s%s", code,
             loc ? loc : "", ctry ? " (" : "", ctry ? ctry : "",
             ctry ? ")" : "");

    char summary[192];
    snprintf(summary, sizeof summary, "%d sensor(s)%s%s", sensors,
             newest ? " · last " : "", newest ? newest : "");

    char link[192];
    snprintf(link, sizeof link,
             "https://www.ioc-sealevelmonitoring.org/station.php?code=%s", code);

    intel_item it = {0};
    it.remote_key = code;
    it.title = title;
    it.summary = summary;
    it.link = link;
    it.lang = "en";
    it.published_at = newest;
    it.record_type = "tide-gauge";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"sea-level\",\"tide-gauge\",\"tsunami\",\"ioc\",\"unesco\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }

  free(seen);
  cJSON_Delete(doc);
  fprintf(stderr, "[ioc-sealevel-stations] emitted %d stations from %d sensor "
                  "records\n", n, total);
  return 0;
}

static const source_def geoeo_ioc_def = {
  .id = "ioc-sealevel-stations", .collector = "environment",
  .name = "IOC Sea Level Monitoring Station Network",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://www.ioc-sealevelmonitoring.org/service.php?query=stationlist&format=json",
  .description = "UNESCO/IOC global sea level station monitoring facility — tide gauges worldwide with position, operator, GLOSS id and a per-station data-availability health record.",
  .license = "IOC-UNESCO / VLIZ open access; the station list is openly published (individual station data rights stay with providers).",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ioc_def)
