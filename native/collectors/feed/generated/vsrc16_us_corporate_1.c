/* Verified-live us_corporate sources (8), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "_vjson_idkeys.inc"   /* VJSON_IDKEYS — us-cfpb-hmda-filers */

VJSON(global_peeringdb_carrier_detail, "global-peeringdb-carrier-detail", "PeeringDB carrier detail (depth=2)", "PeeringDB carrier detail (depth=2)",
  "us_corporate", "corporate",
  "https://www.peeringdb.com/api/carrier/1?depth=2",
  "data",
  "en", "[\"us\",\"corporate\",\"batch16\",\"high-penetrancy\"]", 86400,
  "One carrier with its owning org object inlined (legal address, lat/lon, all *_set id lists) and its facility footprint.");

VJSON(us_ca_dcc_cannabis_licensetypes, "us-ca-dcc-cannabis-licensetypes", "California DCC - licence type vocabulary", "California DCC - licence type vocabulary",
  "us_corporate", "corporate",
  "https://as-dcc-pub-cann-w-p-002.azurewebsites.net/licensetypes",
  "",
  "en", "[\"us\",\"corporate\",\"batch16\",\"high-penetrancy\"]", 86400,
  "id/name/description for all 34 licence categories (Retailer, Non-Storefront, Testing Laboratory, Microbusiness, Event Organizer, Distributor, Manufacturer Type 6/7/N/P...). Needed to drive AdvancedSearch facets.");

/* 2026-11-12: `years=2023` was frozen at authoring; the API publishes a year
 * only after the filing season (measured today: years=2023 1,220 filers, 2024
 * 1,146, 2025 1,122, 2026 HTTP 400; a list `years=2024,2025` answers the
 * FIRST year only, so no list can track two). `{date-22m:%Y}` renders the
 * previous year from 1 November and the year before that until then — 2025
 * today, measured to answer; the exact month a year is published is not
 * measurable from here, and 1 November is the earliest date it was seen
 * published. And the row keyed on VJSON's (title, link, date) fallback,
 * where the title is the lender NAME: 1,116 distinct names over 1,122
 * filers, so six lenders collapsed a run. `lei+period` is 1,122 of 1,122 and
 * keeps one row per lender-year across the yearly roll. */
VJSON_IDKEYS(us_cfpb_hmda_filers, "us-cfpb-hmda-filers", "CFPB/FFIEC HMDA filer list", "CFPB/FFIEC HMDA filer list",
  "us_corporate", "corporate",
  "https://ffiec.cfpb.gov/v2/data-browser-api/view/filers?years={date-22m:%Y}&states=CA",
  "institutions",
  "en", "[\"us\",\"corporate\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Every mortgage lender that reported HMDA loan records in California for the latest published year, with lei (the Legal Entity Identifier - the global corporate key), name, count of loan records and period. The LEI makes this joinable to GLEIF, SEC and FDIC data, and the count tells you a lender's origination volume in that state.",
  "lei+period");

VCSV(us_irs_eo_bmf_region4, "us-irs-eo-bmf-region4", "IRS Exempt Organizations BMF (region 4 / territories)", "IRS Exempt Organizations BMF (region 4 / territories)",
  "us_corporate", "corporate",
  "https://www.irs.gov/pub/irs-soi/eo4.csv",
  "en", "[\"us\",\"corporate\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Same 28-column schema as eo1.csv covering the remaining IRS regions and US territories (Puerto Rico etc.). Collecting eo1 through eo4 gives the complete national EO Business Master File - roughly 1.8 million exempt entities with officer care-of names, assets, income and revenue.");

VJSON(us_phl_carto_business_licenses, "us-phl-carto-business-licenses", "Philadelphia business licences", "Philadelphia business licences",
  "us_corporate", "corporate",
  "https://phl.carto.com/api/v2/sql?q=SELECT%20*%20FROM%20business_licenses%20LIMIT%205",
  "rows",
  "en", "[\"us\",\"corporate\",\"batch16\",\"high-penetrancy\"]", 86400,
  "posse_jobid, licensenum, licensetype, licensestatus, issuedate/expirationdate, business name and legalname, owner/legal entity type, address + unit, council_district, opa_account_num. Ties a trading business to a specific parcel and to its owner-of-record.");
