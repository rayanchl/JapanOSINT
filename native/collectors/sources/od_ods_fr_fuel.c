/* data.economie.gouv.fr - instantaneous fuel prices at French filling stations.
 * Endpoint: https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/
 *           prix-des-carburants-en-france-flux-instantane-v2/records?limit=100&offset=0&order_by=id
 * Row tier (results[]), 9,814 stations, updated continuously. Emits, per
 * station, every scalar the API returned: id, cp, ville, adresse, pop,
 * services, horaires (an embedded JSON document that upstream ships as a
 * string - copied verbatim, not re-interpreted), and the per-fuel price and
 * maj (update-time) columns.
 * Keyless. Licence: Licence Ouverte (Etalab) - reuse with attribution.
 *
 * PAGED 2026-10-12. This made ONE request for 100 stations of the 9,814 the
 * flux holds (total_count, measured) -- the seventh discard shape
 * (tools/audit_url_limits.py), and a live price feed where 99% of the country
 * was missing every hour. `offset=0` is in the URL so lib/pagewalk.c advances
 * it; 120 pages of 100 covers 12,000, and the dataset sits just inside the
 * ODS API's own refusal of offset+limit past 10,000. */
#include "od_shared.inc"
#include "lib/pagewalk.h"

#define SID "ods-fr-fuel-prices"
static const char *URL =
  "https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/"
  "prix-des-carburants-en-france-flux-instantane-v2/records?limit=100&offset=0&order_by=id";
/* 9,814 stations at 100 a page = 99; 120 is that with headroom. */
/* ORDER_BY IS NOT COSMETIC ON AN OFFSET WALK (measured 2026-10-12). ODS
 * returns records in no guaranteed order, and this dataset updates live, so
 * paging by `offset` without a stable sort re-serves some records on a later
 * page and SKIPS others entirely. The skips are invisible; the re-serves show
 * up as a UID-COLLISION on the run line, which is how this was found: Velib
 * reported 1,519 emitted / 1,508 stored, and a `where=stationcode="13022"`
 * query proved the dataset holds exactly ONE record for the code that had
 * supposedly appeared twice. A collision that is really a skip is the worst
 * shape in this file -- the visible half is harmless and the invisible half is
 * lost data. `order_by` on the row's own id makes the walk a stable sequence.
 */
#define FUEL_PAGE_MAX 120  /* exhaustive-ok: page ceiling handed to pw_walk_max(); a stop at it files a collector-truncation-notice */

static int fuel_page(const source_ctx *ctx, intel_sink *sink, const char *sid,
                     cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)sid; (void)ud;
  const cJSON *arr = cJSON_GetObjectItem(doc, "results");
  *seen = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
  if (!cJSON_IsArray(arr)) return 0;

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
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, pw_walk_max(ctx, sink, SID, URL, pw_fetch_json, fuel_page,
                                NULL, FUEL_PAGE_MAX));
}

static const source_def od_ods_fr_fuel_def = {
  .id = SID, .collector = "government",
  .name = "data.economie.gouv.fr - instantaneous fuel prices",
  .update_interval_sec = 3600, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.economie.gouv.fr/api/explore/v2.1/catalog/datasets/prix-des-carburants-en-france-flux-instantane-v2/records?limit=100&offset=0&order_by=id",
  .description = "Live per-station fuel prices at French filling stations with address, opening hours and decoded station coordinates",
  .license = "Licence Ouverte (Etalab) - reuse with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_fr_fuel_def)
