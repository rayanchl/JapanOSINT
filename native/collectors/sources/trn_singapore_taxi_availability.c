/* Singapore — live positions of every available taxi.
 * Endpoint: https://api.data.gov.sg/v1/transport/taxi-availability
 * Emits: one intel row per available taxi position, carrying the feed's own
 * taxi_count and observation timestamp, pinned on the [lon,lat] pair LTA
 * published for that vehicle. Keyless.
 * Licence: data.gov.sg / LTA — Singapore Open Data Licence v1.0, reuse
 * permitted with attribution.
 *
 * Parse note (trap): this is NOT one Feature per taxi. The response is a single
 * Feature whose geometry is a MultiPoint holding thousands of [lon, lat] pairs,
 * so a per-feature GeoJSON emitter yields exactly one row. The MultiPoint is
 * expanded here.
 *
 * The feed is anonymous — there is no vehicle identifier — so the row key is
 * the position's index in the MultiPoint. Each row therefore means "one
 * available taxi", which is what the data actually says; it does not track an
 * individual cab between polls. Coordinate precision varies between vehicles
 * (5 to 10 decimals), so rows are tagged geo_precision="vehicle-gps".
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define SG_TAXI_URL "https://api.data.gov.sg/v1/transport/taxi-availability"
#define SG_TAXI_CAP 4000

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SG_TAXI_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[singapore-taxi-availability] fetch/parse failed\n");
    return -1;
  }
  cJSON *feat = cJSON_GetArrayItem(cJSON_GetObjectItem(doc, "features"), 0);
  if (!feat) {
    cJSON_Delete(doc);
    fprintf(stderr, "[singapore-taxi-availability] no features returned\n");
    return 0;                    /* fetched fine, nothing published (R3) */
  }
  cJSON *props = cJSON_GetObjectItem(feat, "properties");
  const char *ts = props ? jo_sv(props, "timestamp") : NULL;
  double taxi_count = 0;
  int have_count = props ? trn_num(props, "taxi_count", &taxi_count) : 0;

  cJSON *coords = cJSON_GetObjectItem(
      cJSON_GetObjectItem(feat, "geometry"), "coordinates");

  int n = 0, idx = 0;
  cJSON *p;
  cJSON_ArrayForEach(p, coords) {
    if (n >= SG_TAXI_CAP) break;
    cJSON *x = cJSON_GetArrayItem(p, 0), *y = cJSON_GetArrayItem(p, 1);
    double lo, la;
    if (!trn_numv(x, &lo) || !trn_numv(y, &la)) { idx++; continue; }
    if (!trn_geo_ok(la, lo)) { idx++; continue; }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "LTA Singapore (data.gov.sg)");
    cJSON_AddNumberToObject(pr, "slot_index", idx);
    if (have_count) cJSON_AddNumberToObject(pr, "taxi_count", taxi_count);
    trn_put_str(pr, "timestamp", ts);
    cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
    char *pj = cJSON_PrintUnformatted(pr);

    char rk[64], title[96];
    snprintf(rk, sizeof rk, "taxi-slot-%d", idx);
    snprintf(title, sizeof title, "Available taxi #%d", idx);

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.lang            = "en";
    it.published_at    = ts;
    it.record_type     = "taxi-available";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"taxi\",\"singapore\",\"live\"]";
    it.has_geo = 1; it.lat = la; it.lon = lo;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
    idx++;
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[singapore-taxi-availability] emitted %d\n", n);
  return 0;
}

static const source_def trn_singapore_taxi_availability_def = {
  .id = "singapore-taxi-availability", .collector = "transport",
  .name = "Singapore — live available-taxi positions",
  .update_interval_sec = 120, .run = run,
  .category = "transport", .type = "api",
  .url = SG_TAXI_URL,
  .description = "The live position of every available taxi in Singapore — a real-time, city-wide proxy for street-level activity and demand distribution.",
  .license = "Singapore Open Data Licence v1.0 (data.gov.sg / LTA), attribution required.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_singapore_taxi_availability_def)
