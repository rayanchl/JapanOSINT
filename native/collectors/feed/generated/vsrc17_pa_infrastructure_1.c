/* Verified-live pa_infrastructure sources (1), part 1.
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
VJSON(pa_idaan_tomas_agua, "pa-idaan-tomas-agua", "IDAAN - Listado de tomas de agua cruda (Panama)", "IDAAN - Listado de tomas de agua cruda (Panama)",
  "pa_infrastructure", "infrastructure",
  "https://www.datosabiertos.gob.pa/api/3/action/datastore_search?resource_id=90b6d8b4-67b3-47ec-a3b4-58b830299c81&limit=50&sort=_id%20asc",
  "result.records",
  "es", "[\"pa\",\"infrastructure\",\"batch17\",\"high-penetrancy\"]", 43200,
  "Raw-water intake points of the national water utility: intake name, easting/northing coordinates, province, district, corregimiento, watershed, source and type - named critical water infrastructure with coordinates.");
