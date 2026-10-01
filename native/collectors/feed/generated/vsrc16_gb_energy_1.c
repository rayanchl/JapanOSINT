/* Verified-live gb_energy sources (4), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(uk_elexon_bmunits_reference, "uk-elexon-bmunits-reference", "Elexon BM unit master reference", "Elexon BM unit master reference",
  "gb_energy", "energy",
  "https://data.elexon.co.uk/bmrs/api/v1/reference/bmunits/all",
  "",
  "en", "[\"gb\",\"energy\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Every balancing mechanism unit in GB (389 KB): nationalGridBmUnit, elexonBmUnit, EIC, fuelType, leadPartyName and leadPartyId (the company legally responsible), bmUnitType, bmUnitName (plant name), demandCapacity, generationCapacity, transmissionLossFactor, credit assessment import/export capabilities, gspGroupId/gspGroupName and interconnectorId. Resolves any BM unit code seen in REMIT/BOALF/PN to its owning company and physical station.");

/* uk-elexon-disbsad: two defects, both measured 2026-09-14.
 *
 * 1. A FROZEN WINDOW. The row shipped `from=2026-08-15T00:00Z&to=…T02:00Z`,
 *    so every daily run re-fetched the same two hours of August forever. It
 *    now asks for the previous UTC day ({{today-1}} → {{today}}), which a daily
 *    interval covers once each.
 * 2. IDENTITY. `id` is a per-settlement-period sequence number (1, 2, 3 …), not
 *    a record key: a day's 475 records carry 109 distinct `id` and 49 distinct
 *    (settlementDate, settlementPeriod), but 475 distinct (settlementDate,
 *    settlementPeriod, id) — and 475 byte-distinct. On the frozen window the
 *    sweep measured 5 emitted, 1 stored. The key is that triple; the
 *    upstream's own `id` is kept on the record as `record_id`. */
static int run_uk_elexon_disbsad(const source_ctx *c, intel_sink *s) {
  static const char *tpl =
    "https://data.elexon.co.uk/bmrs/api/v1/datasets/DISBSAD?from={{today-1}}T00:00Z&to={{today}}T00:00Z";
  char *url = vsrc_url_dates(tpl);
  cJSON *doc = feed_get_json(c->http, url ? url : tpl, 25000);
  free(url);
  if (!doc) { fprintf(stderr, "[uk-elexon-disbsad] fetch failed\n"); return -1; }
  cJSON *arr = cJSON_GetObjectItemCaseSensitive(doc, "data");
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    if (!cJSON_IsObject(rec)) continue;
    cJSON *sd  = cJSON_GetObjectItemCaseSensitive(rec, "settlementDate");
    cJSON *sp  = cJSON_GetObjectItemCaseSensitive(rec, "settlementPeriod");
    cJSON *rid = cJSON_GetObjectItemCaseSensitive(rec, "id");
    if (!cJSON_IsString(sd) || !sd->valuestring ||
        !cJSON_IsNumber(sp) || !cJSON_IsNumber(rid)) continue;
    char buf[96];
    snprintf(buf, sizeof buf, "%s_%d_%.17g", sd->valuestring,
             (int)sp->valuedouble, rid->valuedouble);
    if (!cJSON_GetObjectItemCaseSensitive(rec, "record_id"))
      cJSON_AddNumberToObject(rec, "record_id", rid->valuedouble);
    cJSON_DeleteItemFromObjectCaseSensitive(rec, "id");
    cJSON_AddStringToObject(rec, "id", buf);
  }
  int n = jsonlist_emit_ex(s, "uk-elexon-disbsad", doc, "data", "energy", "en",
                           "[\"gb\",\"energy\",\"batch16\",\"high-penetrancy\"]",
                           NULL);
  cJSON_Delete(doc);
  return n < 0 ? -1 : 0;
}
static const source_def uk_elexon_disbsad = {
  .id = "uk-elexon-disbsad", .collector = "gb_energy",
  .name = "Elexon disaggregated balancing services adjustment",
  .name_ja = "Elexon disaggregated balancing services adjustment",
  .update_interval_sec = 86400, .run = run_uk_elexon_disbsad,
  .category = "energy", .type = "api",
  .url = "https://data.elexon.co.uk/bmrs/api/v1/datasets/DISBSAD?from={{today-1}}T00:00Z&to={{today}}T00:00Z",
  .description = "Non-BM balancing actions with cost and volume, soFlag/storFlag, partyId, assetId, isTendered and service name — i.e. which counterparty was paid for what ancillary service in each settlement period.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(uk_elexon_disbsad);

VJSON(uk_elexon_interconnectors_reference, "uk-elexon-interconnectors-reference", "Elexon interconnector reference", "Elexon interconnector reference",
  "gb_energy", "energy",
  "https://data.elexon.co.uk/bmrs/api/v1/reference/interconnectors/all",
  "",
  "en", "[\"gb\",\"energy\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Every GB electricity interconnector with interconnectorId, interconnectorName and the counterparty biddingZone (Eleclink/France, Greenlink/Ireland, Moyle/Northern Ireland, BritNed/Netherlands, NSL/Norway, Viking/Denmark).");

VJSON(uk_elexon_remit_list, "uk-elexon-remit-list", "Elexon BMRS REMIT message list", "Elexon BMRS REMIT message list",
  "gb_energy", "energy",
  "https://data.elexon.co.uk/bmrs/api/v1/remit/list/by-event?from=2026-08-10T00:00Z&to=2026-08-16T00:00Z",
  "data",
  "en", "[\"gb\",\"energy\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Index of REMIT inside-information filings (generation unavailability notices): id, mrid, revisionNumber, createdTime, publishTime and a direct url to the full message. Max 7-day window. This is the list half of the highest-value energy disclosure feed in GB.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");
