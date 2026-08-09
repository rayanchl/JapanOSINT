/* Fintraffic Digitraffic — road traffic measurement (TMS) station network.
 * Endpoint: https://tie.digitraffic.fi/api/tms/v1/stations
 * Emits: one intel row per inductive-loop counting station — Digitraffic id,
 * tmsNumber, station name, collectionStatus, state and dataUpdatedTime, pinned
 * on the station's own published coordinates. Keyless.
 * Licence: Fintraffic Digitraffic, CC BY 4.0. A Digitraffic-User header
 * identifying the client is requested by the publisher and is sent here.
 *
 * Parse notes: GeoJSON FeatureCollection; coordinates are [lon, lat, elevation]
 * (three values) — only the first two are read.
 *
 * Trap: at least one station (id 89) is published at [80.24, 18.97], which is
 * in the Indian Ocean, not Finland. Every coordinate is therefore checked
 * against Finland's bounding box before has_geo is set; a station that fails
 * the check is still emitted, just without a position (R2 — better no location
 * than a wrong one).
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static const char *const DT_HDRS[] = {
  "Digitraffic-User: japanosint-native", NULL };

/* Finland incl. the Åland islands and the northern tip, with a small margin. */
#define FI_LAT0 59.0
#define FI_LAT1 70.6
#define FI_LON0 19.0
#define FI_LON1 32.0

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://tie.digitraffic.fi/api/tms/v1/stations", DT_HDRS, 30000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-tms-stations] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  int n = 0, dropped_geo = 0;
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
    trn_put_num(pr, "tms_number", props, "tmsNumber");
    trn_put_str(pr, "collection_status", jo_sv(props, "collectionStatus"));
    trn_put_str(pr, "state", jo_sv(props, "state"));
    trn_put_str(pr, "road_station_state", jo_sv(props, "roadStationState"));
    trn_put_str(pr, "data_updated_time", jo_sv(props, "dataUpdatedTime"));
    trn_put_str(pr, "municipality", jo_sv(props, "municipality"));
    trn_put_str(pr, "province", jo_sv(props, "province"));

    double la, lo; int geo = 0;
    if (trn_geom_first_pos(cJSON_GetObjectItem(f, "geometry"), &la, &lo)) {
      if (trn_in_bbox(la, lo, FI_LAT0, FI_LAT1, FI_LON0, FI_LON1)) geo = 1;
      else { dropped_geo++;
             cJSON_AddStringToObject(pr, "coordinate_rejected",
                                     "outside Finland — upstream error"); }
    }
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256];
    if (nm) snprintf(title, sizeof title, "%s", nm);
    else    snprintf(title, sizeof title, "TMS station %s", sid);

    intel_item it = {0};
    it.remote_key      = sid[0] ? sid : nm;
    it.title           = title;
    it.summary         = jo_sv(props, "collectionStatus");
    it.lang            = "fi";
    it.published_at    = jo_sv(props, "dataUpdatedTime");
    it.record_type     = "traffic-count-station";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"road\",\"finland\",\"sensor\"]";
    if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-tms-stations] emitted %d (%d rejected bad coords)\n",
          n, dropped_geo);
  return 0;
}

static const source_def trn_digitraffic_tms_stations_def = {
  .id = "digitraffic-tms-stations", .collector = "transport",
  .name = "Fintraffic Digitraffic — road traffic measurement (TMS) stations",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = "https://tie.digitraffic.fi/api/tms/v1/stations",
  .description = "Every inductive-loop traffic counting station on the Finnish road network with exact coordinates and collection status.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_digitraffic_tms_stations_def)
