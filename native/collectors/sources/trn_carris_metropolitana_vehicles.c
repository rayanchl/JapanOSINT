/* Carris Metropolitana Lisbon — live bus fleet positions.
 * Endpoint: https://api.carrismetropolitana.pt/vehicles
 * Emits: one intel row per bus — vehicle id, licence plate, make, model,
 * propulsion, total capacity and wheelchair accessibility, together with the
 * live current_status, bearing, speed, line/route/pattern and trip ids, pinned
 * on the vehicle's own lat/lon. Keyless.
 * Licence: Carris Metropolitana / Área Metropolitana de Lisboa open API,
 * keyless and documented for public reuse.
 *
 * Parse note (trap): the FIRST element of the array is a null/placeholder
 * record with id "|undefined" and no lat/lon. Rows without usable coordinates
 * are skipped outright rather than emitted at a depot or city centre (R2).
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define CARRIS_URL "https://api.carrismetropolitana.pt/vehicles"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CARRIS_URL, 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[carris-metropolitana-vehicles] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *v;
  cJSON_ArrayForEach(v, doc) {
    if (!cJSON_IsObject(v)) continue;
    const char *vid = jo_sv(v, "id");
    double la, lo;
    /* the placeholder first record has no coordinates — skip it here */
    if (!vid || !trn_num(v, "lat", &la) || !trn_num(v, "lon", &lo)) continue;
    if (!trn_geo_ok(la, lo)) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Carris Metropolitana");
    cJSON_AddStringToObject(pr, "vehicle_id", vid);
    trn_put_str(pr, "license_plate", jo_sv(v, "license_plate"));
    trn_put_str(pr, "make", jo_sv(v, "make"));
    trn_put_str(pr, "model", jo_sv(v, "model"));
    trn_put_str(pr, "propulsion", jo_sv(v, "propulsion"));
    trn_put_num(pr, "capacity_total", v, "capacity_total");
    trn_put_num(pr, "capacity_seated", v, "capacity_seated");
    trn_put_num(pr, "capacity_standing", v, "capacity_standing");
    trn_put_bool(pr, "wheelchair_accessible", v, "wheelchair_accessible");
    trn_put_str(pr, "current_status", jo_sv(v, "current_status"));
    trn_put_str(pr, "line_id", jo_sv(v, "line_id"));
    trn_put_str(pr, "route_id", jo_sv(v, "route_id"));
    trn_put_str(pr, "pattern_id", jo_sv(v, "pattern_id"));
    trn_put_str(pr, "trip_id", jo_sv(v, "trip_id"));
    trn_put_str(pr, "stop_id", jo_sv(v, "stop_id"));
    trn_put_num(pr, "bearing", v, "bearing");
    trn_put_num(pr, "speed", v, "speed");
    char isots[40]; isots[0] = 0;
    double ts;
    if (trn_num(v, "timestamp", &ts)) {
      trn_iso_from_unix(ts, isots, sizeof isots);
      trn_put_str(pr, "timestamp", isots);
    }
    cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256], summary[192];
    const char *line = jo_sv(v, "line_id");
    const char *plate = jo_sv(v, "license_plate");
    if (line) snprintf(title, sizeof title, "Carris bus %s on line %s",
                       vid, line);
    else snprintf(title, sizeof title, "Carris bus %s", vid);
    const char *st = jo_sv(v, "current_status");
    if (plate && st) snprintf(summary, sizeof summary, "%s · %s", plate, st);
    else if (plate)  snprintf(summary, sizeof summary, "%s", plate);
    else if (st)     snprintf(summary, sizeof summary, "%s", st);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = vid;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "pt";
    it.published_at    = isots[0] ? isots : NULL;
    it.record_type     = "transit-vehicle";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"bus\",\"lisbon\",\"live\"]";
    it.has_geo = 1; it.lat = la; it.lon = lo;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[carris-metropolitana-vehicles] emitted %d\n", n);
  return 0;
}

static const source_def trn_carris_metropolitana_vehicles_def = {
  .id = "carris-metropolitana-vehicles", .collector = "transport",
  .name = "Carris Metropolitana Lisbon — live bus fleet positions",
  .update_interval_sec = 60, .run = run,
  .category = "transport", .type = "api",
  .url = CARRIS_URL,
  .description = "Live position of every bus in the Lisbon metropolitan area, enriched with the physical vehicle record — plate, manufacturer, model, propulsion and capacity.",
  .license = "Carris Metropolitana / Area Metropolitana de Lisboa open API, keyless, documented for public reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_carris_metropolitana_vehicles_def)
