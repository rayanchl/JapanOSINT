/* data.economie.gouv.fr - instantaneous fuel prices at French filling stations.
 * Endpoint: https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/
 *           prix-des-carburants-en-france-flux-instantane-v2/records?limit=100
 * Row tier (results[]), 9,804 stations, updated continuously. Emits, per
 * station, every scalar the API returned: id, cp, ville, adresse, pop,
 * services, horaires (an embedded JSON document that upstream ships as a
 * string - copied verbatim, not re-interpreted), and the per-fuel price and
 * maj (update-time) columns.
 * Keyless. Licence: Licence Ouverte (Etalab) - reuse with attribution. */
#include "od_shared.c"

#define SID "ods-fr-fuel-prices"
static const char *URL =
  "https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/"
  "prix-des-carburants-en-france-flux-instantane-v2/records?limit=100";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *arr = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    char idb[64];
    const char *id = od_scalar(r, "id", idb, sizeof idb);
    const char *ville = od_s(r, "ville");
    const char *adr = od_s(r, "adresse");
    char title[320];
    if (adr && ville)      snprintf(title, sizeof title, "%s, %s", adr, ville);
    else if (adr)          snprintf(title, sizeof title, "%s", adr);
    else if (ville)        snprintf(title, sizeof title, "%s", ville);
    else if (id)           snprintf(title, sizeof title, "station %s", id);
    else continue;                       /* nothing identifying -> no row */

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, r);

    /* R2 TRAP (recorded in verification): latitude/longitude arrive as STRINGS
     * in units of 1e-5 degrees - "5004475" is 50.04475 and "152562" is
     * 1.52562. Emitting them verbatim would drop every French station into
     * the Indian Ocean, so divide by 100000 and replace the raw pair in the
     * properties with the decoded degrees. */
    double la = 0, lo = 0;
    int geo = 0;
    if (od_numv(r, "latitude", &la) && od_numv(r, "longitude", &lo)) {
      la /= 100000.0;
      lo /= 100000.0;
      geo = od_ll_ok(la, lo);
    }
    cJSON_DeleteItemFromObject(props, "latitude");
    cJSON_DeleteItemFromObject(props, "longitude");
    if (geo) {
      cJSON_AddNumberToObject(props, "latitude", la);
      cJSON_AddNumberToObject(props, "longitude", lo);
    }

    intel_item it = {0};
    it.title = title;
    it.summary = od_s(r, "cp");
    it.remote_key = id;
    it.link = URL;
    it.lang = "fr";
    it.record_type = "fuel-station-price";
    it.tags_json = "[\"opendata\",\"energy\",\"fr\"]";
    if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_ods_fr_fuel_def = {
  .id = SID, .collector = "government",
  .name = "data.economie.gouv.fr - instantaneous fuel prices",
  .update_interval_sec = 3600, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/prix-des-carburants-en-france-flux-instantane-v2/records?limit=100",
  .description = "Live per-station fuel prices at French filling stations with address, opening hours and decoded station coordinates",
  .license = "Licence Ouverte (Etalab) - reuse with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_fr_fuel_def)
