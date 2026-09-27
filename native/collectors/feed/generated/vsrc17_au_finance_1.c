/* Verified-live au_finance sources (8), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(au_acnc_annual_information_statement_2024, "au-acnc-annual-information-statement-2024", "ACNC — 2024 Annual Information Statement", "ACNC — 2024 Annual Information Statement",
  "au_finance", "finance",
  "https://data.gov.au/data/api/3/action/datastore_search?resource_id=710630ea-1202-4bbb-95f7-3973a972ddf8&limit=100",
  "result.records",
  "en", "[\"au\",\"finance\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "The financial and activity return each Australian charity files: abn, charity name, registration status, website, size, basic-religious-charity flag, AIS due date, dates the AIS and financial report were received, whether activities were conducted, and a long block of international-activity disclosures (transferring goods or services overseas, operating overseas).  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");

VJSON(au_qld_health_grant_recipients, "au-qld-health-grant-recipients", "Queensland Health — grant funding recipients", "Queensland Health — grant funding recipients",
  "au_finance", "finance",
  "https://www.data.qld.gov.au/api/3/action/datastore_search?resource_id=bad2b05f-10eb-4ff0-96a4-a9651b512dac&limit=100",
  "result.records",
  "en", "[\"au\",\"finance\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Organisations funded by Queensland Health: Organisation Name, Project Title, description of the funded services, Contract End Date and the funding amount per financial year. Follows public money to named non-government providers.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
