/* Trenitalia ViaggiaTreno — Italian rail station registry.
 * Endpoint:
 *   https://www.viaggiatreno.it/infomobilita/resteasy/viaggiatreno/elencoStazioni/0
 * Emits: one intel row per station — codiceStazione, the long and short station
 * names, the region code and tipoStazione, pinned on the station's own lat/lon.
 * Keyless.
 * Licence: CAUTION — this is Trenitalia's public web-app backend, not a
 * formally licensed open-data product, and no terms are published on the
 * endpoint. Reuse is widespread and non-commercial; treat the licence as
 * UNSTATED and keep polling low (this runs weekly).
 *
 * Parse notes: the path segment 0 is the region code, and 0 returns the
 * national list. Station names are uppercase upstream.
 *
 * Trap: latMappaCitta / lonMappaCitta are frequently 0.0 and must NOT be used
 * as a fallback when lat/lon are missing (R2) — this code reads only lat/lon,
 * and trn_geo_ok additionally rejects the (0,0) placeholder that some rows
 * carry in those real fields too.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define VT_URL "https://www.viaggiatreno.it/infomobilita/resteasy/viaggiatreno/" \
               "elencoStazioni/0"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, VT_URL, 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[viaggiatreno-stations] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, doc) {
    const char *code = jo_sv(s, "codiceStazione");
    const cJSON *loc = cJSON_GetObjectItem(s, "localita");
    const char *nome = loc ? jo_sv(loc, "nomeLungo") : NULL;
    if (!nome) nome = jo_sv(s, "nomeLungo");
    if (!code && !nome) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Trenitalia / RFI (ViaggiaTreno)");
    trn_put_str(pr, "station_code", code);
    trn_put_str(pr, "name", nome);
    if (loc) trn_put_str(pr, "short_name", jo_sv(loc, "nomeBreve"));
    trn_put_num(pr, "region_code", s, "codReg");
    trn_put_num(pr, "station_type", s, "tipoStazione");
    trn_put_num(pr, "zoom_level", s, "dettZoomStaz");
    char *pj = cJSON_PrintUnformatted(pr);

    intel_item it = {0};
    it.remote_key      = code ? code : nome;
    it.title           = nome ? nome : code;
    it.summary         = code;
    it.lang            = "it";
    it.record_type     = "rail-station";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"italy\",\"infrastructure\"]";
    double la, lo;
    if (trn_num(s, "lat", &la) && trn_num(s, "lon", &lo) && trn_geo_ok(la, lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[viaggiatreno-stations] emitted %d\n", n);
  return 0;
}

static const source_def trn_viaggiatreno_stations_def = {
  .id = "viaggiatreno-stations", .collector = "transport",
  .name = "Trenitalia ViaggiaTreno — Italian rail station registry",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "api",
  .url = VT_URL,
  .description = "The full Italian rail station registry behind ViaggiaTreno, with station code, name and coordinates — the key needed to query live departures per station.",
  .license = "Unstated: Trenitalia's public web-app backend, no published terms. Non-commercial reuse only, low poll rate.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_viaggiatreno_stations_def)
