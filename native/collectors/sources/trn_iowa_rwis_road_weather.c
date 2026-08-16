/* Iowa RWIS — road weather sensor observations (Iowa Environmental Mesonet).
 * Endpoint:
 *   https://mesonet.agron.iastate.edu/api/1/currents.geojson?network=IA_RWIS
 * Emits: one intel row per RWIS station observation — station id, name, county,
 * state, network, the UTC valid time, air temperature, dew point, relative
 * humidity, visibility, wind speed and direction, and the pavement sensor
 * temperatures c1tmpf…c5tmpf, pinned on the station's own coordinates. Keyless.
 * Licence: Iowa Environmental Mesonet, Iowa State University — data provided
 * freely for public and research use.
 *
 * Parse notes: ask for .geojson, NOT .json — the .json variant returns a
 * pandas-style {schema, data} envelope with no geometry at all, while .geojson
 * gives a proper FeatureCollection with Point coordinates. Many sensor fields
 * are null on any given station, so nulls are skipped rather than emitted as
 * zeros (a 0 °F pavement reading is a very different claim from "no sensor").
 * The network= parameter swaps in other US state RWIS networks.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define IA_RWIS_URL \
  "https://mesonet.agron.iastate.edu/api/1/currents.geojson?network=IA_RWIS"

/* Copy a numeric measurement only when the upstream actually reported one. */
static void put_meas(cJSON *pr, const cJSON *props, const char *key) {
  const cJSON *v = cJSON_GetObjectItem(props, key);
  if (!v || cJSON_IsNull(v)) return;             /* null → omit, not zero */
  double d;
  if (trn_numv(v, &d)) cJSON_AddNumberToObject(pr, key, d);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, IA_RWIS_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[iowa-rwis-road-weather] fetch/parse failed\n");
    return -1;
  }

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, cJSON_GetObjectItem(doc, "features")) {
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    const char *station = jo_sv(props, "station");
    const char *name = jo_sv(props, "name");
    if (!station && !name) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator",
                            "Iowa Environmental Mesonet / Iowa DOT");
    trn_put_str(pr, "station", station);
    trn_put_str(pr, "name", name);
    trn_put_str(pr, "county", jo_sv(props, "county"));
    trn_put_str(pr, "state", jo_sv(props, "state"));
    trn_put_str(pr, "network", jo_sv(props, "network"));
    trn_put_str(pr, "utc_valid", jo_sv(props, "utc_valid"));
    put_meas(pr, props, "tmpf");
    put_meas(pr, props, "dwpf");
    put_meas(pr, props, "relh");
    put_meas(pr, props, "vsby");
    put_meas(pr, props, "sknt");
    put_meas(pr, props, "drct");
    put_meas(pr, props, "c1tmpf");
    put_meas(pr, props, "c2tmpf");
    put_meas(pr, props, "c3tmpf");
    put_meas(pr, props, "c4tmpf");
    put_meas(pr, props, "c5tmpf");
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256], summary[160];
    if (name && station) snprintf(title, sizeof title, "%s (%s)", name, station);
    else snprintf(title, sizeof title, "%s", name ? name : station);
    double t;
    const cJSON *tv = cJSON_GetObjectItem(props, "tmpf");
    if (tv && !cJSON_IsNull(tv) && trn_numv(tv, &t))
      snprintf(summary, sizeof summary, "%.1f F air temperature", t);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = station ? station : name;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.published_at    = jo_sv(props, "utc_valid");
    it.record_type     = "road-weather-observation";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"road\",\"iowa\",\"weather\"]";
    double la, lo;
    if (trn_geom_first_pos(cJSON_GetObjectItem(f, "geometry"), &la, &lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[iowa-rwis-road-weather] emitted %d\n", n);
  return 0;
}

static const source_def trn_iowa_rwis_road_weather_def = {
  .id = "iowa-rwis-road-weather", .collector = "transport",
  .name = "Iowa RWIS — road weather sensor observations",
  .update_interval_sec = 1800, .run = run,
  .category = "transport", .type = "api",
  .url = IA_RWIS_URL,
  .description = "Live observations from Iowa DOT's roadside weather information system, with pavement-sensor temperatures alongside air temperature, visibility and wind — the data that drives winter road closures.",
  .license = "Iowa Environmental Mesonet, Iowa State University — free for public and research use.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_iowa_rwis_road_weather_def)
