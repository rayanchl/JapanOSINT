/* Fintraffic Digitraffic — roadside weather station network.
 * Endpoint: https://tie.digitraffic.fi/api/weather/v1/stations
 * Emits: one intel row per roadside weather sensor station — station id, name,
 * collectionStatus, the sensor-health `state` enum and dataUpdatedTime, pinned
 * on the station's own published coordinates. Keyless.
 * Licence: Fintraffic Digitraffic, CC BY 4.0. A Digitraffic-User header
 * identifying the client is requested by the publisher and is sent here.
 *
 * Parse notes: GeoJSON FeatureCollection, coordinates [lon, lat, elev]. The
 * `state` field carries the sensor health enum (e.g. OK_FAULT_DOUBT_CANCELLED),
 * which is what tells you whether a station's readings can be trusted.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static const char *const DT_HDRS[] = {
  "Digitraffic-User: japanosint-native", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://tie.digitraffic.fi/api/weather/v1/stations", DT_HDRS, 30000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-road-weather-stations] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    char sid[64];
    trn_idfield(f, "id", sid, sizeof sid);
    if (!sid[0]) trn_idfield(props, "id", sid, sizeof sid);
    const char *nm = jo_sv(props, "name");
    if (!sid[0] && !nm) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Fintraffic");
    trn_put_str(pr, "station_id", sid);
    trn_put_str(pr, "name", nm);
    trn_put_str(pr, "collection_status", jo_sv(props, "collectionStatus"));
    trn_put_str(pr, "state", jo_sv(props, "state"));
    trn_put_str(pr, "road_station_state", jo_sv(props, "roadStationState"));
    trn_put_str(pr, "data_updated_time", jo_sv(props, "dataUpdatedTime"));
    trn_put_str(pr, "municipality", jo_sv(props, "municipality"));
    trn_put_str(pr, "province", jo_sv(props, "province"));
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256];
    if (nm) snprintf(title, sizeof title, "%s", nm);
    else    snprintf(title, sizeof title, "Road weather station %s", sid);

    intel_item it = {0};
    it.remote_key      = sid[0] ? sid : nm;
    it.title           = title;
    it.summary         = jo_sv(props, "collectionStatus");
    it.lang            = "fi";
    it.published_at    = jo_sv(props, "dataUpdatedTime");
    it.record_type     = "road-weather-station";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"road\",\"finland\",\"weather\"]";
    double la, lo;
    if (trn_geom_first_pos(cJSON_GetObjectItem(f, "geometry"), &la, &lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-road-weather-stations] emitted %d\n", n);
  return 0;
}

static const source_def trn_digitraffic_road_weather_def = {
  .id = "digitraffic-road-weather-stations", .collector = "transport",
  .name = "Fintraffic Digitraffic — road weather station network",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = "https://tie.digitraffic.fi/api/weather/v1/stations",
  .description = "Roadside weather sensor stations across Finland with exact coordinates and sensor-health state.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_digitraffic_road_weather_def)
