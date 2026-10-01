/* Fintraffic Digitraffic — live GPS positions of Finnish trains.
 * Endpoint: https://rata.digitraffic.fi/api/v1/train-locations/latest/
 * Emits: one intel row per moving train — trainNumber, departureDate, speed in
 * km/h, GPS fix accuracy in metres and the fix timestamp, pinned on the
 * position the train itself reported. Keyless.
 * Licence: Fintraffic Digitraffic, CC BY 4.0. A Digitraffic-User header
 * identifying the client is requested by the publisher and is sent here.
 *
 * Parse notes (traps):
 *  - the TRAILING SLASH matters: /train-locations/latest (without it) 404s, and
 *    /trains/latest returns PARAMETER_WRONG_TYPE because "latest" is parsed as
 *    a date on that path. The URL below keeps the slash.
 *  - `location` is an embedded GeoJSON Point, so coordinates[0] is the
 *    LONGITUDE and coordinates[1] the latitude — the opposite order to the
 *    latitude/longitude field pair used by the sibling station metadata API.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static const char *const DT_HDRS[] = {
  "Digitraffic-User: japanosint-native", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://rata.digitraffic.fi/api/v1/train-locations/latest/",
      DT_HDRS, 20000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[digitraffic-train-locations] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *t;
  cJSON_ArrayForEach(t, doc) {
    char num[32];
    trn_idfield(t, "trainNumber", num, sizeof num);
    const char *dep = jo_sv(t, "departureDate");
    if (!num[0]) continue;

    /* location is a GeoJSON Point: coordinates = [lon, lat] */
    double la, lo;
    if (!trn_geom_first_pos(cJSON_GetObjectItem(t, "location"), &la, &lo))
      continue;                      /* no fix → no row for a position feed */

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Fintraffic");
    cJSON_AddStringToObject(pr, "train_number", num);
    trn_put_str(pr, "departure_date", dep);
    trn_put_num(pr, "speed_kmh", t, "speed");
    trn_put_num(pr, "accuracy_m", t, "accuracy");
    trn_put_str(pr, "timestamp", jo_sv(t, "timestamp"));
    cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
    char *pj = cJSON_PrintUnformatted(pr);

    char rk[64], title[128], summary[96];
    snprintf(rk, sizeof rk, "%s|%s", num, dep ? dep : "");
    snprintf(title, sizeof title, "Train %s", num);
    double spd;
    if (trn_num(t, "speed", &spd))
      snprintf(summary, sizeof summary, "%.0f km/h", spd);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "fi";
    it.published_at    = jo_sv(t, "timestamp");
    it.record_type     = "train-position";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"finland\",\"live\"]";
    it.has_geo = 1; it.lat = la; it.lon = lo;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-train-locations] emitted %d\n", n);
  return 0;              /* no trains moving at 03:00 is an honest 0 (R3) */
}

static const source_def trn_digitraffic_train_locations_def = {
  .id = "digitraffic-train-locations", .collector = "transport",
  .name = "Fintraffic Digitraffic — live Finnish train GPS positions",
  .update_interval_sec = 60, .run = run,
  .category = "transport", .type = "api",
  .url = "https://rata.digitraffic.fi/api/v1/train-locations/latest/",
  .description = "Live GPS position, speed and fix accuracy of every train currently moving in Finland.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_digitraffic_train_locations_def)
