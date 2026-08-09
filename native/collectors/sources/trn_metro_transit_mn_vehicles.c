/* Metro Transit Minneapolis–St Paul — live vehicle positions.
 * Endpoints: https://svc.metrotransit.org/nextrip/routes      (route index)
 *            https://svc.metrotransit.org/nextrip/vehicles/<route_id>
 * Emits: one intel row per in-service vehicle — trip_id, route_id and route
 * label, direction, bearing, speed, odometer and the location_time, pinned on
 * the vehicle's own latitude/longitude. Keyless.
 * Licence: Metro Transit NexTrip open API, keyless, published for public reuse.
 *
 * Parse notes: there is NO all-routes vehicle endpoint — the vehicle feed is
 * per route, so /nextrip/routes is called first and then fanned out. That is
 * ~120 requests per run, which is why this runs on a 5-minute cycle rather than
 * the 60 s a single-call feed would justify: it keeps the request rate on
 * svc.metrotransit.org modest. latitude/longitude are numbers.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define MT_ROUTES "https://svc.metrotransit.org/nextrip/routes"

static int emit_route(const source_ctx *ctx, intel_sink *sink,
                      const char *route_id, const char *route_label) {
  char url[256];
  snprintf(url, sizeof url, "https://svc.metrotransit.org/nextrip/vehicles/%s",
           route_id);
  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc || !cJSON_IsArray(doc)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *v;
  cJSON_ArrayForEach(v, doc) {
    double la, lo;
    if (!trn_num(v, "latitude", &la) || !trn_num(v, "longitude", &lo)) continue;
    if (!trn_geo_ok(la, lo)) continue;
    char trip[64];
    trn_idfield(v, "trip_id", trip, sizeof trip);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Metro Transit");
    cJSON_AddStringToObject(pr, "route_id", route_id);
    trn_put_str(pr, "route_label", route_label);
    trn_put_str(pr, "trip_id", trip);
    trn_put_str(pr, "direction", jo_sv(v, "direction"));
    trn_put_str(pr, "terminal", jo_sv(v, "terminal"));
    trn_put_num(pr, "bearing", v, "bearing");
    trn_put_num(pr, "speed", v, "speed");
    trn_put_num(pr, "odometer", v, "odometer");
    char isots[40]; isots[0] = 0;
    double lt;
    if (trn_num(v, "location_time", &lt)) {
      trn_iso_from_unix(lt, isots, sizeof isots);
      trn_put_str(pr, "location_time", isots);
    }
    cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
    char *pj = cJSON_PrintUnformatted(pr);

    char rk[160], title[256], summary[128];
    snprintf(rk, sizeof rk, "%s|%s", route_id, trip[0] ? trip : "?");
    snprintf(title, sizeof title, "Metro Transit route %s%s%s", route_id,
             route_label ? " " : "", route_label ? route_label : "");
    const char *dir = jo_sv(v, "direction");
    if (dir) snprintf(summary, sizeof summary, "%s", dir);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.published_at    = isots[0] ? isots : NULL;
    it.record_type     = "transit-vehicle";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"transit\",\"minneapolis\",\"live\"]";
    it.has_geo = 1; it.lat = la; it.lon = lo;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *routes = feed_get_json(ctx->http, MT_ROUTES, 20000);
  if (!routes || !cJSON_IsArray(routes)) {
    fprintf(stderr, "[metro-transit-mn-vehicles] route index fetch failed\n");
    cJSON_Delete(routes);
    return -1;
  }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, routes) {
    char rid[64];
    trn_idfield(r, "route_id", rid, sizeof rid);
    if (!rid[0]) continue;
    n += emit_route(ctx, sink, rid, jo_sv(r, "route_label"));
  }
  cJSON_Delete(routes);
  fprintf(stderr, "[metro-transit-mn-vehicles] emitted %d\n", n);
  return 0;
}

static const source_def trn_metro_transit_mn_vehicles_def = {
  .id = "metro-transit-mn-vehicles", .collector = "transport",
  .name = "Metro Transit Minneapolis–St Paul — live vehicle positions",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = MT_ROUTES,
  .description = "Live positions of every in-service vehicle on the Twin Cities Metro Transit network, fanned out across the published route index.",
  .license = "Metro Transit NexTrip open API, keyless, published for public reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_metro_transit_mn_vehicles_def)
