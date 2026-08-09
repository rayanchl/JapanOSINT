/* MBTA Boston — live vehicle positions (V3 JSON:API).
 * Endpoint: https://api-v3.mbta.com/vehicles
 * Emits: one intel row per vehicle — vehicle id and label, live latitude,
 * longitude and bearing, current_status, current_stop_sequence, occupancy
 * status, the route and stop it is related to, and the upstream updated_at.
 * Keyless (a free API key only raises the rate limit).
 * Licence: MassDOT/MBTA open data, reuse permitted.
 *
 * Parse notes: JSON:API envelope — the real values live under
 * data[].attributes, and route/stop are id references under
 * data[].relationships.<name>.data.id rather than inline fields. This JSON
 * endpoint is used in place of the protobuf GTFS-RT feed because this platform
 * has no protobuf decoder. carriages[] carries per-car occupancy when present.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

/* relationships.<rel>.data.id */
static const char *rel_id(const cJSON *v, const char *rel) {
  const cJSON *r = cJSON_GetObjectItem(
      cJSON_GetObjectItem(cJSON_GetObjectItem(v, "relationships"), rel), "data");
  return r ? jo_sv(r, "id") : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://api-v3.mbta.com/vehicles", 30000);
  if (!doc) {
    fprintf(stderr, "[mbta-vehicle-positions] fetch/parse failed\n");
    return -1;
  }
  cJSON *data = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(data)) {
    fprintf(stderr, "[mbta-vehicle-positions] unexpected envelope\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *v;
  cJSON_ArrayForEach(v, data) {
    const char *vid = jo_sv(v, "id");
    cJSON *at = cJSON_GetObjectItem(v, "attributes");
    if (!vid || !at) continue;

    const char *route = rel_id(v, "route");
    const char *stop  = rel_id(v, "stop");
    const char *trip  = rel_id(v, "trip");

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "MBTA");
    cJSON_AddStringToObject(pr, "vehicle_id", vid);
    trn_put_str(pr, "label", jo_sv(at, "label"));
    trn_put_str(pr, "current_status", jo_sv(at, "current_status"));
    trn_put_num(pr, "current_stop_sequence", at, "current_stop_sequence");
    trn_put_num(pr, "bearing", at, "bearing");
    trn_put_num(pr, "speed", at, "speed");
    trn_put_str(pr, "occupancy_status", jo_sv(at, "occupancy_status"));
    trn_put_num(pr, "direction_id", at, "direction_id");
    trn_put_str(pr, "route_id", route);
    trn_put_str(pr, "stop_id", stop);
    trn_put_str(pr, "trip_id", trip);
    trn_put_str(pr, "updated_at", jo_sv(at, "updated_at"));
    cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256], summary[192];
    if (route) snprintf(title, sizeof title, "MBTA %s on route %s", vid, route);
    else       snprintf(title, sizeof title, "MBTA vehicle %s", vid);
    const char *cs = jo_sv(at, "current_status");
    if (cs && stop) snprintf(summary, sizeof summary, "%s stop %s", cs, stop);
    else if (cs)    snprintf(summary, sizeof summary, "%s", cs);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = vid;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.published_at    = jo_sv(at, "updated_at");
    it.record_type     = "transit-vehicle";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"transit\",\"boston\",\"live\"]";
    double la, lo;
    if (trn_num(at, "latitude", &la) && trn_num(at, "longitude", &lo) &&
        trn_geo_ok(la, lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[mbta-vehicle-positions] emitted %d\n", n);
  return 0;                   /* an empty overnight fleet is an honest 0 (R3) */
}

static const source_def trn_mbta_vehicle_positions_def = {
  .id = "mbta-vehicle-positions", .collector = "transport",
  .name = "MBTA Boston — live vehicle positions (V3 API)",
  .update_interval_sec = 60, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api-v3.mbta.com/vehicles",
  .description = "Live positions, bearings, occupancy and current stop for every MBTA subway, light-rail, commuter-rail and bus vehicle in Boston.",
  .license = "MassDOT/MBTA open data — keyless access permitted, rate limited.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_mbta_vehicle_positions_def)
