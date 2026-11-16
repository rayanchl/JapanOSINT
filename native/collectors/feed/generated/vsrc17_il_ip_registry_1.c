/* Verified-live il_ip-registry sources (5), part 1.
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
VJSON(il_patent_applicants, "il-patent-applicants", "Israel Patent Office — applicants behind patent applications", "Israel Patent Office — applicants behind patent applications",
  "il_ip-registry", "ip-registry",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=b2b87f6c-832c-410d-a753-c52ec15cd793&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"ip-registry\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Applicant entity per application: application number, invention title in Hebrew and English, APPLICANT NAME and APPLICANT ADDRESS in Hebrew and English. Links inventions to named companies and firms.");

VJSON(il_patent_applications, "il-patent-applications", "Israel Patent Office — patent applications", "Israel Patent Office — patent applications",
  "il_ip-registry", "ip-registry",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=adca9a88-bf91-462a-8a49-62381af9b588&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"ip-registry\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Patent applications back to 1917: application number, request and filing dates, invention title in Hebrew and English, international application number and publication number.");

VJSON(il_patent_inventors, "il-patent-inventors", "Israel Patent Office — named inventors", "Israel Patent Office — named inventors",
  "il_ip-registry", "ip-registry",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=abd0e8c3-0b0d-4581-9166-923a3b21bdda&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"ip-registry\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Inventor names attached to each patent application, in Hebrew and English, with the invention title. Sample record is an electromagnetic gun system — includes defence-adjacent filings.");

VJSON(il_trademark_holders, "il-trademark-holders", "Israel Trademark Office — holders and licensees", "Israel Trademark Office — holders and licensees",
  "il_ip-registry", "ip-registry",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=d4a365c7-357a-4172-8815-1a6650e848e2&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"ip-registry\",\"batch17\",\"high-penetrancy\"]", 86400,
  "The owner-behind-the-mark table: national mark number, client type (individual / foreign corporation), client number, client name in Hebrew and English, client standing (owner vs licensee) and lead-client flag.");

VJSON(il_trademarks, "il-trademarks", "Israel Trademark Office — trademark register", "Israel Trademark Office — trademark register",
  "il_ip-registry", "ip-registry",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=6284d4a9-fdd4-45c9-a1d6-58143c7d8127&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"ip-registry\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Trademark records: mark number, figurative flag, link to the official extract on trademarks.justice.gov.il, legal state, national vs international, international mark number, and mark description in Hebrew and English.");
