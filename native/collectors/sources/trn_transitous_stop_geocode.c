/* Transitous — cross-border public transport stop geocoder.
 * Endpoint: https://api.transitous.org/api/v1/geocode?text=<name>
 * Emits: one intel row per resolved place — the Transitous/GTFS id, the type
 * (STOP / PLACE / ADDRESS), the name, country, postcode, timezone, the served
 * modes, and the coordinates the geocoder returned. Keyless.
 * Licence: Transitous is a MOTIS-based community service aggregating openly
 * licensed GTFS feeds; free to use, attribute Transitous and the underlying
 * operators.
 *
 * The endpoint is a lookup, not a bulk feed, so this resolves ctx->entity when
 * the OSINT dispatcher pivots on a place name and otherwise sweeps a fixed list
 * of major international rail hubs. The query list only decides which records
 * are fetched — every field emitted, coordinates included, comes back from the
 * service.
 *
 * Parse notes: top-level array. Numbers are serialised in EXPONENT form
 * (5.252498E1 = 52.52498), which cJSON handles correctly but a hand-rolled
 * float scan would not. /api/v1/stops does not exist (404); the live endpoints
 * are /geocode, /plan and /stoptimes.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static const char *const HUBS[] = {
  "Berlin Hbf", "Wien Hbf", "Praha hlavni nadrazi", "Zurich HB", "Amsterdam Centraal",
  "Bruxelles-Midi", "Milano Centrale", "Barcelona Sants", "Warszawa Centralna",
  "Budapest-Keleti", "Kobenhavn H", "Stockholm Centralstation", "Oslo S",
  "Helsinki", "Lisboa Oriente", "Roma Termini", "Munchen Hbf", "Paris Gare de Lyon",
  NULL };

static int emit_query(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = trn_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
           "https://api.transitous.org/api/v1/geocode?text=%s", enc);
  free(enc);

  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc || !cJSON_IsArray(doc)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *id = jo_sv(r, "id");
    const char *nm = jo_sv(r, "name");
    if (!id && !nm) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Transitous");
    trn_put_str(pr, "place_id", id);
    trn_put_str(pr, "name", nm);
    trn_put_str(pr, "type", jo_sv(r, "type"));
    trn_put_str(pr, "country", jo_sv(r, "country"));
    trn_put_str(pr, "zip", jo_sv(r, "zip"));
    trn_put_str(pr, "timezone", jo_sv(r, "tz"));
    trn_put_num(pr, "level", r, "level");
    {
      const cJSON *modes = cJSON_GetObjectItem(r, "modes");
      if (cJSON_IsArray(modes))
        cJSON_AddItemToObject(pr, "modes", cJSON_Duplicate(modes, 1));
    }
    cJSON_AddStringToObject(pr, "matched_query", q);
    char *pj = cJSON_PrintUnformatted(pr);

    char summary[192];
    const char *ty = jo_sv(r, "type"), *co = jo_sv(r, "country");
    if (ty && co) snprintf(summary, sizeof summary, "%s · %s", ty, co);
    else if (ty)  snprintf(summary, sizeof summary, "%s", ty);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = id ? id : nm;
    it.title           = nm ? nm : id;
    it.summary         = summary[0] ? summary : NULL;
    it.record_type     = "transit-stop";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"transit\",\"geocode\"]";
    double la, lo;
    if (trn_num(r, "lat", &la) && trn_num(r, "lon", &lo) && trn_geo_ok(la, lo)) {
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
    n += emit_query(ctx, sink, ctx->entity);
  } else {
    for (int i = 0; HUBS[i]; i++) n += emit_query(ctx, sink, HUBS[i]);
  }
  fprintf(stderr, "[transitous-stop-geocode] emitted %d\n", n);
  return 0;
}

static const source_def trn_transitous_stop_geocode_def = {
  .id = "transitous-stop-geocode", .collector = "transport",
  .name = "Transitous — cross-border public transport stop geocoder",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.transitous.org/api/v1/geocode",
  .description = "A free, keyless community geocoder over the aggregated GTFS feeds of dozens of countries — resolves a stop or station name to coordinates, country, timezone and served modes.",
  .license = "Transitous (MOTIS-based community service over openly licensed GTFS feeds); free, attribute Transitous and the operators.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_transitous_stop_geocode_def)
