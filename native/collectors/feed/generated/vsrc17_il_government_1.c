/* Verified-live il_government sources (3), part 1.
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
VJSON(il_databases_registry, "il-databases-registry", "Israel Privacy Protection Authority — registry of personal databases", "Israel Privacy Protection Authority — registry of personal databases",
  "il_government", "government",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=fd56bf5b-7918-4906-99e4-b0e5102ae268&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Registered personal-data databases: database number, OWNER ID NUMBER (corporate registration number), owner full name, database name, and the owner's city, street, house number and postbox. Maps which company holds which personal dataset.");

VJSON(il_knesset_gov_ministry, "il-knesset-gov-ministry", "Knesset — government ministries (KNS_GovMinistry)", "Knesset — government ministries (KNS_GovMinistry)",
  "il_government", "government",
  "https://knesset.gov.il/Odata/ParliamentInfo.svc/KNS_GovMinistry?$top=100&$format=json",
  "value",
  "he", "[\"il\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Ministry register with IsActive flag; resolves GovMinistryID appearing on questions, laws and position records.");

VJSON(il_political_parties, "il-political-parties", "Israel Registrar of Political Parties", "Israel Registrar of Political Parties",
  "il_government", "government",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=1dbfc053-e92f-4354-92d6-1b99aadb20d7&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Registered political parties: party name, registrar party number and the party's full declared objects text.");
