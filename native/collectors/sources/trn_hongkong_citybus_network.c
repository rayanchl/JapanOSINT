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
 * so they go out with has_geo = 0) and stop geography is resolved for a
 * cycling slice of routes per run (see HK_ROUTES_PER_RUN) — no coordinate is
 * ever guessed for the rest (R2).
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define HK_ROUTES  "https://rt.data.gov.hk/v2/transport/citybus/route/CTB"
/* A CYCLE over the route list, not "the first ten routes for ever"
 * (2026-11-07). The run resolved stops for routes 0..9 of the list — 410
 * routes measured that day — every day, and stopped at 300 stops, so the
 * stops of the other 400 routes were never fetched and nothing said so. It
 * now resolves HK_ROUTES_PER_RUN routes a run starting where the last run
 * stopped (source_ctx resume_in/resume_out, token "r=<index>"), every stop of
 * each (deduped with lib/seenset.c), and wraps at the end: ~41 daily runs a
 * cycle. Each run files a notice saying which slice it resolved. */
#define HK_ROUTES_PER_RUN 10  /* exhaustive-ok: a cycle slice; the position advances every run and is disclosed */
#include "lib/seenset.h"

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
                            seen_set *seen_ids, int *seen_count) {
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
    if (!sid) continue;
    if (!seen_add(seen_ids, sid)) continue;
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

  seen_set seen_ids = {0};
  int seen_count = 0, resolved_routes = 0, n = 0;
  int nroutes = cJSON_GetArraySize(routes);
  int start = 0;
  if (ctx->resume_in && strncmp(ctx->resume_in, "r=", 2) == 0)
    start = atoi(ctx->resume_in + 2);
  if (start < 0 || start >= nroutes) start = 0;
  int ri = -1;
  int last_ri = start - 1;   /* the last route index whose stops were resolved */

  cJSON *r;
  cJSON_ArrayForEach(r, routes) {
    ri++;
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

    if (ri >= start && resolved_routes < HK_ROUTES_PER_RUN) {
      n += emit_route_stops(ctx, sink, route, "inbound",
                            &seen_ids, &seen_count);
      n += emit_route_stops(ctx, sink, route, "outbound",
                            &seen_ids, &seen_count);
      resolved_routes++;
      last_ri = ri;
    }
  }
  cJSON_Delete(doc);
  seen_free(&seen_ids);
  /* The index AFTER the last route resolved — not start + resolved_routes,
   * which lags whenever the slice held a record with no `route` field (it is
   * skipped above but still occupies an index), so the next run would re-read
   * the tail of this slice instead of moving on. */
  int next = last_ri + 1;
  if (resolved_routes < HK_ROUTES_PER_RUN || next >= nroutes) next = 0;
  if (ctx->resume_out) {
    char tok[32];
    snprintf(tok, sizeof tok, "r=%d", next);
    ctx->resume_out(ctx, next ? tok : NULL);
  }
  if (resolved_routes < nroutes) {
    char why[200];
    snprintf(why, sizeof why,
             "stops were resolved for routes %d..%d of %d this run (a cycle; "
             "records count ROUTES whose stops were resolved)",
             start, last_ri, nroutes);
    jo_trunc_notice_scoped(sink, "hongkong-citybus-network", "stop-cycle",
                           HK_ROUTES, resolved_routes, nroutes, why,
                           "the next run resolves the next slice; raise "
                           "HK_ROUTES_PER_RUN to finish a cycle sooner");
  }
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
