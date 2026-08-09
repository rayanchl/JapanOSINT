/* Fintraffic Digitraffic — Finnish rail network station registry.
 * Endpoint: https://rata.digitraffic.fi/api/v1/metadata/stations
 * Emits: one intel row per station, stopping point or turnout — stationName,
 * stationShortCode, stationUICCode, countryCode, passengerTraffic flag and the
 * type enum, pinned on the site's own latitude/longitude. Keyless.
 * Licence: Fintraffic Digitraffic, CC BY 4.0. A Digitraffic-User header
 * identifying the client is requested by the publisher and is sent here.
 *
 * Parse notes: plain JSON array (no GeoJSON envelope); latitude/longitude are
 * top-level numbers. `type` distinguishes STATION / STOPPING_POINT /
 * TURNOUT_IN_THE_OPEN_LINE, which is what makes the freight-only sites that
 * passenger timetables omit visible here.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static const char *const DT_HDRS[] = {
  "Digitraffic-User: japanosint-native", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://rata.digitraffic.fi/api/v1/metadata/stations", DT_HDRS, 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[digitraffic-rail-stations] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, doc) {
    const char *nm   = jo_sv(s, "stationName");
    const char *code = jo_sv(s, "stationShortCode");
    if (!nm && !code) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Fintraffic / Vayla");
    trn_put_str(pr, "station_name", nm);
    trn_put_str(pr, "station_short_code", code);
    trn_put_num(pr, "station_uic_code", s, "stationUICCode");
    trn_put_str(pr, "country_code", jo_sv(s, "countryCode"));
    trn_put_str(pr, "type", jo_sv(s, "type"));
    trn_put_bool(pr, "passenger_traffic", s, "passengerTraffic");
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256], summary[128];
    snprintf(title, sizeof title, "%s", nm ? nm : code);
    const char *ty = jo_sv(s, "type");
    if (code && ty) snprintf(summary, sizeof summary, "%s · %s", code, ty);
    else if (ty)    snprintf(summary, sizeof summary, "%s", ty);
    else            summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = code ? code : nm;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "fi";
    it.record_type     = "rail-station";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"finland\",\"infrastructure\"]";
    double la, lo;
    if (trn_num(s, "latitude", &la) && trn_num(s, "longitude", &lo) &&
        trn_geo_ok(la, lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-rail-stations] emitted %d\n", n);
  return 0;
}

static const source_def trn_digitraffic_rail_stations_def = {
  .id = "digitraffic-rail-stations", .collector = "transport",
  .name = "Fintraffic Digitraffic — Finnish rail network stations",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "api",
  .url = "https://rata.digitraffic.fi/api/v1/metadata/stations",
  .description = "Every station, stopping point and turnout on the Finnish rail network with UIC code and exact coordinates, including the freight-only sites passenger timetables omit.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_digitraffic_rail_stations_def)
