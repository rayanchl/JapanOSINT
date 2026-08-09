/* Reseaux Energies (RTE) eCO2mix - French national grid, real time.
 * Endpoint: https://opendata.reseaux-energies.fr/api/explore/v2.1/catalog/
 *           datasets/eco2mix-national-tr/records?limit=48
 *           &order_by=date_heure%20desc   (the space MUST stay percent-encoded)
 * Emits, per 15-minute row: date_heure, consommation, the generation mix
 * (nucleaire, gaz, charbon, fioul, eolien, eolien_offshore, solaire,
 * hydraulique, bioenergies, pompage), taux_co2 and the cross-border exchange
 * columns - every scalar the API returned for that row.
 * Keyless. Licence: Licence Ouverte v2.0; RTE open-data terms permit reuse
 * with attribution. */
#include "od_shared.c"

#define SID "ods-rte-eco2mix"
static const char *URL =
  "https://opendata.reseaux-energies.fr/api/explore/v2.1/catalog/datasets/"
  "eco2mix-national-tr/records?limit=48&order_by=date_heure%20desc";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *arr = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0, skipped = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    const char *when = od_s(r, "date_heure");
    if (!when) continue;

    /* R3/R1 TRAP (recorded in verification): the newest rows are
     * pre-published placeholders in which consommation and every generation
     * column are null - only prevision_j1 is set. Coercing those nulls to 0
     * would assert that France consumed 0 MW, so the row is skipped instead. */
    double cons = 0;
    if (!od_numv(r, "consommation", &cons)) { skipped++; continue; }

    char title[220];
    snprintf(title, sizeof title, "France consumption %s = %.0f MW", when, cons);

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, r);          /* nulls are skipped, never zeroed */

    intel_item it = {0};
    it.title = title;
    it.remote_key = when;
    it.published_at = when;
    it.link = URL;
    it.lang = "fr";
    it.record_type = "grid-generation-mix";
    it.tags_json = "[\"opendata\",\"energy\",\"grid\",\"fr\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] skipped %d pre-published rows with null consommation\n",
          SID, skipped);
  return od_rc(SID, n);
}

static const source_def od_ods_rte_eco2mix_def = {
  .id = SID, .collector = "government",
  .name = "Reseaux Energies (RTE) eCO2mix national real-time",
  .update_interval_sec = 1800, .run = run,
  .category = "government", .type = "api",
  .url = "https://opendata.reseaux-energies.fr/api/explore/v2.1/catalog/datasets/eco2mix-national-tr/records?limit=48&order_by=date_heure%20desc",
  .description = "French national electricity consumption, generation mix by fuel, CO2 intensity and cross-border exchanges at 15-minute resolution",
  .license = "Licence Ouverte v2.0 (RTE open-data terms)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_rte_eco2mix_def)
