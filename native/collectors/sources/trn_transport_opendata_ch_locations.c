/* Swiss public transport — station and stop locations (transport.opendata.ch).
 * Endpoint: https://transport.opendata.ch/v1/locations?query=<name>
 * Emits: one intel row per stop returned — the official DiDok station id, the
 * stop name, the mode icon (train/tram/bus/…) and the stop's own coordinates.
 * Keyless.
 * Licence: transport.opendata.ch is a community API over SBB /
 * opentransportdata.swiss open data. Free and keyless with a published
 * fair-use limit of ~1000 requests/day per IP; attribute opendata.ch.
 *
 * The API is query-driven, so this sweeps a fixed list of Swiss hubs on a
 * schedule and additionally resolves ctx->entity when the OSINT dispatcher
 * pivots on a place name. Every emitted value comes from the API response —
 * the query list only decides which records are fetched.
 *
 * Parse note (TRAP): coordinate.x is the LATITUDE and coordinate.y is the
 * LONGITUDE, which is the opposite of the usual x=lon convention. Reading them
 * the conventional way drops every Swiss stop into the Indian Ocean.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static const char *const CH_QUERIES[] = {
  "Bern", "Zurich", "Geneve", "Basel", "Lausanne", "Luzern", "Lugano",
  "Winterthur", "St.Gallen", "Biel", "Chur", "Sion", "Fribourg", "Neuchatel",
  "Thun", "Zug", "Bellinzona", "Interlaken", "Brig", "Olten", NULL };

static int emit_query(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = trn_urlencode(q);
  if (!enc) return 0;
  char url[320];
  snprintf(url, sizeof url,
           "https://transport.opendata.ch/v1/locations?query=%s", enc);
  free(enc);

  cJSON *doc = feed_get_json(ctx->http, url, 20000);
  if (!doc) return 0;
  cJSON *stations = cJSON_GetObjectItem(doc, "stations");

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, stations) {
    char sid[64];
    trn_idfield(s, "id", sid, sizeof sid);
    const char *nm = jo_sv(s, "name");
    if (!sid[0] && !nm) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "opendata.ch / SBB");
    trn_put_str(pr, "station_id", sid);
    trn_put_str(pr, "name", nm);
    trn_put_str(pr, "icon", jo_sv(s, "icon"));
    trn_put_str(pr, "score", jo_sv(s, "score"));
    cJSON_AddStringToObject(pr, "matched_query", q);
    char *pj = cJSON_PrintUnformatted(pr);

    intel_item it = {0};
    it.remote_key      = sid[0] ? sid : nm;
    it.title           = nm ? nm : sid;
    it.summary         = jo_sv(s, "icon");
    it.lang            = "de";
    it.record_type     = "transit-stop";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"transit\",\"switzerland\"]";

    /* TRAP: coordinate.x is LATITUDE, coordinate.y is LONGITUDE here. */
    const cJSON *co = cJSON_GetObjectItem(s, "coordinate");
    double lat, lon;
    if (co && trn_num(co, "x", &lat) && trn_num(co, "y", &lon) &&
        trn_geo_ok(lat, lon)) {
      it.has_geo = 1; it.lat = lat; it.lon = lon;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0, calls = 0;
  if (ctx->entity && ctx->entity[0]) {
    n += emit_query(ctx, sink, ctx->entity);
    calls++;
  } else {
    for (int i = 0; CH_QUERIES[i]; i++) {
      n += emit_query(ctx, sink, CH_QUERIES[i]);
      calls++;
    }
  }
  if (calls == 0) return 0;
  fprintf(stderr, "[transport-opendata-ch-locations] emitted %d\n", n);
  return 0;
}

static const source_def trn_transport_opendata_ch_def = {
  .id = "transport-opendata-ch-locations", .collector = "transport",
  .name = "Swiss public transport — station and stop locations",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://transport.opendata.ch/v1/locations",
  .description = "Swiss public-transport stop lookup returning the official DiDok station id, coordinates and mode icon; also answers OSINT entity pivots on a place name.",
  .license = "Community API over SBB / opentransportdata.swiss open data; free, fair-use limited, attribute opendata.ch.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_transport_opendata_ch_def)
