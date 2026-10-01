/* Hong Kong Citybus — route network and stop coordinates.
 * Endpoints (all verified keyless):
 *   https://rt.data.gov.hk/v2/transport/citybus/route/CTB            (routes)
 *   https://rt.data.gov.hk/v2/transport/citybus/route-stop/CTB/<route>/<dir>
 *   https://rt.data.gov.hk/v2/transport/citybus/stop/<stopId>
 * Emits: one intel row per Citybus route (route number, bilingual origin and
 * destination, data_timestamp) and one row per resolved stop (stop id,
 * bilingual stop name) pinned on the stop's own coordinates.
 * Keyless.
 * Licence: HKSAR Government DATA.GOV.HK real-time transport API — the Terms of
 * Condition for Using Public Sector Information permit reuse with attribution.
 *
 * Parse notes: the stop coordinates come from a THREE-HOP model — routes, then
 * the ordered stop ids per route and direction, then one lookup per stop. In
 * the per-stop payload the longitude key is "long", NOT "lon", and both lat and
 * long are STRINGS. Resolving every stop of every route would be thousands of
 * requests per run, so the routes are all emitted (they carry no coordinates,
 * so they go out with has_geo = 0) and stop geography is resolved for a bounded
 * number of routes and stops per run — no coordinate is ever guessed for the
 * rest (R2).
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define HK_ROUTES  "https://rt.data.gov.hk/v2/transport/citybus/route/CTB"
#define HK_MAX_ROUTES_RESOLVED 10
#define HK_MAX_STOPS_RESOLVED  300

/* Have we already emitted this stop id in this run? Tiny linear set — the cap
 * keeps it well under a few hundred entries. */
static int seen(char seen_ids[][32], int count, const char *id) {
  for (int i = 0; i < count; i++)
    if (strcmp(seen_ids[i], id) == 0) return 1;
  return 0;
}

static int emit_stop(const source_ctx *ctx, intel_sink *sink,
                     const char *stop_id) {
  char url[192];
  snprintf(url, sizeof url,
           "https://rt.data.gov.hk/v2/transport/citybus/stop/%s", stop_id);
  cJSON *doc = feed_get_json(ctx->http, url, 15000);
  if (!doc) return 0;
  cJSON *d = cJSON_GetObjectItem(doc, "data");
  const char *en = d ? jo_sv(d, "name_en") : NULL;
  if (!d || !en) { cJSON_Delete(doc); return 0; }

  cJSON *pr = cJSON_CreateObject();
  cJSON_AddStringToObject(pr, "operator", "Citybus (CTB), Hong Kong");
  cJSON_AddStringToObject(pr, "stop_id", stop_id);
  trn_put_str(pr, "name_en", en);
  trn_put_str(pr, "name_tc", jo_sv(d, "name_tc"));
  trn_put_str(pr, "name_sc", jo_sv(d, "name_sc"));
  trn_put_str(pr, "data_timestamp", jo_sv(d, "data_timestamp"));
  char *pj = cJSON_PrintUnformatted(pr);

  char rk[96];
  snprintf(rk, sizeof rk, "stop|%s", stop_id);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = en;
  it.summary         = jo_sv(d, "name_tc");
  it.lang            = "en";
  it.record_type     = "transit-stop";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"transport\",\"bus\",\"hong-kong\"]";
  /* the longitude key is "long", and both values are STRINGS */
  double la, lo;
  if (trn_num(d, "lat", &la) && trn_num(d, "long", &lo) && trn_geo_ok(la, lo)) {
    it.has_geo = 1; it.lat = la; it.lon = lo;
  }
  int emitted = (sink->emit(sink, &it) >= 0) ? 1 : 0;
  free(pj);
  cJSON_Delete(pr);
  cJSON_Delete(doc);
  return emitted;
}

/* Resolve the ordered stop ids for one route/direction and emit each stop. */
static int emit_route_stops(const source_ctx *ctx, intel_sink *sink,
                            const char *route, const char *dir,
                            char seen_ids[][32], int *seen_count) {
  char url[224];
  snprintf(url, sizeof url,
           "https://rt.data.gov.hk/v2/transport/citybus/route-stop/CTB/%s/%s",
           route, dir);
  cJSON *doc = feed_get_json(ctx->http, url, 15000);
  if (!doc) return 0;

  int n = 0;
  cJSON *rs;
  cJSON_ArrayForEach(rs, cJSON_GetObjectItem(doc, "data")) {
    const char *sid = jo_sv(rs, "stop");
    if (!sid || strlen(sid) >= 32) continue;
    if (*seen_count >= HK_MAX_STOPS_RESOLVED) break;
    if (seen(seen_ids, *seen_count, sid)) continue;
    snprintf(seen_ids[*seen_count], 32, "%s", sid);
    (*seen_count)++;
    n += emit_stop(ctx, sink, sid);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, HK_ROUTES, 30000);
  if (!doc) {
    fprintf(stderr, "[hongkong-citybus-network] route fetch/parse failed\n");
    return -1;
  }
  cJSON *routes = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(routes)) {
    fprintf(stderr, "[hongkong-citybus-network] unexpected route envelope\n");
    cJSON_Delete(doc);
    return -1;
  }

  char seen_ids[HK_MAX_STOPS_RESOLVED][32];
  int seen_count = 0, resolved_routes = 0, n = 0;

  cJSON *r;
  cJSON_ArrayForEach(r, routes) {
    const char *route = jo_sv(r, "route");
    if (!route) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Citybus (CTB), Hong Kong");
    cJSON_AddStringToObject(pr, "route", route);
    trn_put_str(pr, "orig_en", jo_sv(r, "orig_en"));
    trn_put_str(pr, "orig_tc", jo_sv(r, "orig_tc"));
    trn_put_str(pr, "dest_en", jo_sv(r, "dest_en"));
    trn_put_str(pr, "dest_tc", jo_sv(r, "dest_tc"));
    trn_put_str(pr, "co", jo_sv(r, "co"));
    trn_put_str(pr, "data_timestamp", jo_sv(r, "data_timestamp"));
    char *pj = cJSON_PrintUnformatted(pr);

    char rk[96], title[384];
    snprintf(rk, sizeof rk, "route|%s", route);
    const char *oe = jo_sv(r, "orig_en"), *de = jo_sv(r, "dest_en");
    if (oe && de) snprintf(title, sizeof title, "Citybus %s: %s to %s",
                           route, oe, de);
    else snprintf(title, sizeof title, "Citybus route %s", route);

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = jo_sv(r, "dest_tc");
    it.lang            = "en";
    it.record_type     = "bus-route";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"bus\",\"hong-kong\"]";
    /* a route is not a point; the routes endpoint has no geometry (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);

    if (resolved_routes < HK_MAX_ROUTES_RESOLVED &&
        seen_count < HK_MAX_STOPS_RESOLVED) {
      n += emit_route_stops(ctx, sink, route, "inbound",
                            seen_ids, &seen_count);
      n += emit_route_stops(ctx, sink, route, "outbound",
                            seen_ids, &seen_count);
      resolved_routes++;
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[hongkong-citybus-network] emitted %d (%d stops resolved)\n",
          n, seen_count);
  return 0;
}

static const source_def trn_hongkong_citybus_network_def = {
  .id = "hongkong-citybus-network", .collector = "transport",
  .name = "Hong Kong Citybus — route and stop network",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = HK_ROUTES,
  .description = "The full Citybus route network in Hong Kong with bilingual origin and destination, plus resolved per-stop coordinates from the government real-time transport API.",
  .license = "HKSAR DATA.GOV.HK Terms of Condition for Using Public Sector Information — reuse with attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_hongkong_citybus_network_def)
