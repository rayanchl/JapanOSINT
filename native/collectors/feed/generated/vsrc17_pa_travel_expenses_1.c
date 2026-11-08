/* Verified-live pa_travel-expenses sources (2), part 1.
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
VJSON(pa_migracion_viaticos_2023, "pa-migracion-viaticos-2023", "Servicio Nacional de Migracion - Viaticos internacionales 2023 (Panama)", "Servicio Nacional de Migracion - Viaticos internacionales 2023 (Panama)",
  "pa_travel-expenses", "travel-expenses",
  "https://www.datosabiertos.gob.pa/api/3/action/datastore_search?resource_id=879128e2-a373-4ed9-ba57-8c1859e76313&limit=50&sort=_id%20asc",
  "result.records",
  "es", "[\"pa\",\"travel-expenses\",\"batch17\",\"high-penetrancy\"]", 86400,
  "International travel allowances of immigration service officials: cedula, name, surname, post, destination, details, start and end dates, total, per-diem and report file - places named immigration officials in specific countries on specific dates.");

VJSON(pa_sbp_misiones_2025, "pa-sbp-misiones-2025", "Superintendencia de Bancos - Misiones Oficiales Nacionales 2025 (Panama)", "Superintendencia de Bancos - Misiones Oficiales Nacionales 2025 (Panama)",
  "pa_travel-expenses", "travel-expenses",
  "https://www.datosabiertos.gob.pa/api/3/action/datastore_search?resource_id=093e41ba-1891-48f4-87d5-218ea2ea1a1b&limit=50&sort=_id%20asc",
  "result.records",
  "es", "[\"pa\",\"travel-expenses\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Official missions of banking-regulator staff: start and end dates, name, surname, cedula, destination, per-diem amount and details - who from the bank supervisor travelled where and when.");
