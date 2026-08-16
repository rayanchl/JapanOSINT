/* Berlin VBB/BVG — public transport stops near a coordinate.
 * Endpoint: https://v6.bvg.transport.rest/locations/nearby
 *           ?latitude=<lat>&longitude=<lon>&results=100
 * Emits: one intel row per stop — the stop id, name, the national IFOPT
 * identifier, the mode flags (S-Bahn, U-Bahn, tram, bus, ferry, regional,
 * express) and the distance in metres from the query point, pinned on the
 * stop's own location.latitude/location.longitude. Keyless.
 * Licence: v6.bvg.transport.rest is a community REST wrapper over VBB's HAFAS
 * endpoint, free and keyless with a published 100 req/min limit; the underlying
 * data is VBB open data. NOT an operator-official endpoint.
 *
 * The API only answers "near a point", so this sweeps a fixed grid of anchor
 * coordinates across Berlin (and honours ctx->entity as "lat,lon" on an OSINT
 * pivot). The anchors decide which stops are fetched; every emitted value,
 * including every coordinate, comes from the API response.
 *
 * Parse notes: top-level array. /stops/nearby is NOT a valid path (it answers
 * "id must be an IBNR") — the working path is /locations/nearby. The sibling
 * v6.db.transport.rest (Deutsche Bahn) has the same shape but was returning
 * HTTP 503 when this was written.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

/* Anchor points spread across Berlin's districts. */
static const double ANCHORS[][2] = {
  { 52.5200, 13.4050 },   /* Mitte            */
  { 52.5065, 13.3320 },   /* Charlottenburg   */
  { 52.4880, 13.4260 },   /* Neukoelln        */
  { 52.5450, 13.3550 },   /* Wedding          */
  { 52.5230, 13.4640 },   /* Friedrichshain   */
  { 52.4650, 13.3200 },   /* Steglitz         */
  { 52.5600, 13.4300 },   /* Pankow           */
  { 52.4300, 13.5300 },   /* Koepenick        */
  { 52.5400, 13.5800 },   /* Marzahn          */
  { 52.4700, 13.2200 },   /* Zehlendorf       */
};

static int emit_nearby(const source_ctx *ctx, intel_sink *sink,
                       double qlat, double qlon) {
  char url[256];
  snprintf(url, sizeof url,
      "https://v6.bvg.transport.rest/locations/nearby"
      "?latitude=%.6f&longitude=%.6f&results=100", qlat, qlon);
  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc || !cJSON_IsArray(doc)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, doc) {
    const char *sid = jo_sv(s, "id");
    const char *nm  = jo_sv(s, "name");
    if (!sid && !nm) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "VBB / BVG");
    trn_put_str(pr, "stop_id", sid);
    trn_put_str(pr, "name", nm);
    {
      const cJSON *ids = cJSON_GetObjectItem(s, "ids");
      if (ids) trn_put_str(pr, "ifopt", jo_sv(ids, "ifopt"));
    }
    {
      const cJSON *pd = cJSON_GetObjectItem(s, "products");
      static const char *const MODES[] = { "suburban", "subway", "tram", "bus",
                                           "ferry", "express", "regional", NULL };
      cJSON *modes = cJSON_CreateArray();
      for (int i = 0; pd && MODES[i]; i++)
        if (trn_bool(pd, MODES[i]) == 1)
          cJSON_AddItemToArray(modes, cJSON_CreateString(MODES[i]));
      cJSON_AddItemToObject(pr, "modes", modes);
    }
    trn_put_num(pr, "distance_m", s, "distance");
    char *pj = cJSON_PrintUnformatted(pr);

    intel_item it = {0};
    it.remote_key      = sid ? sid : nm;
    it.title           = nm ? nm : sid;
    it.lang            = "de";
    it.record_type     = "transit-stop";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"transit\",\"berlin\"]";
    const cJSON *loc = cJSON_GetObjectItem(s, "location");
    double la, lo;
    if (loc && trn_num(loc, "latitude", &la) &&
        trn_num(loc, "longitude", &lo) && trn_geo_ok(la, lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0;
  if (ctx->entity && ctx->entity[0]) {
    double la, lo;
    if (sscanf(ctx->entity, "%lf , %lf", &la, &lo) == 2 && trn_geo_ok(la, lo))
      n += emit_nearby(ctx, sink, la, lo);
    else
      return 0;                    /* entity is not a coordinate → nothing */
  } else {
    int k = (int)(sizeof ANCHORS / sizeof ANCHORS[0]);
    for (int i = 0; i < k; i++)
      n += emit_nearby(ctx, sink, ANCHORS[i][0], ANCHORS[i][1]);
  }
  fprintf(stderr, "[bvg-berlin-stops-nearby] emitted %d\n", n);
  return 0;
}

static const source_def trn_bvg_berlin_stops_nearby_def = {
  .id = "bvg-berlin-stops-nearby", .collector = "transport",
  .name = "Berlin VBB/BVG — stops near a coordinate",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = "https://v6.bvg.transport.rest/locations/nearby",
  .description = "Berlin and Brandenburg public-transport stops with mode flags and the national IFOPT identifier, resolved around a coordinate.",
  .license = "Community REST wrapper over VBB HAFAS; free, 100 req/min, underlying data is VBB open data.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_bvg_berlin_stops_nearby_def)
