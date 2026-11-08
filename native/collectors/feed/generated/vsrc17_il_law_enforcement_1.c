/* Verified-live il_law-enforcement sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* datastore_search rows below carry `sort=_id asc` (2026-11-07). An offset
 * walk with no declared order relies on the backend's default; CKAN's
 * datastore `_id` is its own sequential row id. Measured one or two rows per
 * host (izmir, data.gov.il, data.gov.au, QLD, VIC, Mauritius, Panama,
 * data.gov.sg): `_id desc` answers the opposite end, a bogus sort is refused
 * (409, or a 404 page on data.gov.il), and the total does not move. The
 * unsorted first page was already _id 1,2,3 on every one, so this is
 * STABILISING, not a recovery: 0 records were measured lost without it. */
VJSON(il_human_trafficking_cases, "il-human-trafficking-cases", "Israel — anti human-trafficking coordination unit case file", "Israel — anti human-trafficking coordination unit case file",
  "il_law-enforcement", "law-enforcement",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=329b3f63-fa09-44e7-bddf-286c7f8e3999&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"law-enforcement\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Cases handled by the National Anti-Trafficking Coordinator: case status and status date, opening and closing dates, case type, referral status and date, case subject (e.g. slavery) and the referring body.");

VJSON(il_police_crime_records, "il-police-crime-records", "Israel Police — crime case records by area and quarter", "Israel Police — crime case records by area and quarter",
  "il_law-enforcement", "law-enforcement",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=e311b6a1-be5a-4a82-8298-f3afbee07b6b&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"law-enforcement\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Case-level crime records with a fictive case id, year and quarter, locality code and name, and the full police hierarchy: district code and name, region code and name, station code and name, plus offence grouping.");
