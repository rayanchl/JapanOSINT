/* Verified-live mu_government sources (1), part 1.
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
VJSON(mauritius_accident_prone_areas, "mauritius-accident-prone-areas", "Mauritius open data - accident-prone road segments", "Mauritius open data - accident-prone road segments",
  "mu_government", "government",
  "https://data.govmu.org/api/3/action/datastore_search?resource_id=3e12e27b-71c5-47b6-8585-f498e82d32cc&limit=100&sort=_id%20asc",
  "result.records",
  "en", "[\"mu\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "48 officially designated accident-prone road segments in Mauritius, each with _id, a named road descriptor ('M1: Belle Village up to Caudan') and the_geom as a MULTILINESTRING of WGS84 coordinates. The only resource on data.govmu.org whose datastore records carry a real label - the rest are raw spreadsheet dumps with 'Back to Table of Contents' headers.");
