/* Verified-live il_procurement sources (1), part 1.
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
VJSON(il_state_lawyer_engagements, "il-state-lawyer-engagements", "Israel — state engagements with external law firms", "Israel — state engagements with external law firms",
  "il_procurement", "procurement",
  "https://data.gov.il/api/3/action/datastore_search?resource_id=81108843-f82c-4d35-92fe-ddb9e9aad405&limit=100&sort=_id%20asc",
  "result.records",
  "he", "[\"il\",\"procurement\",\"batch17\",\"high-penetrancy\"]", 21600,
  "Committee for the engagement of external counsel: request id, the government body making the request, request date, and the NAMED law firms considered or selected with the selection method. Government-to-private-counsel money trail.");
