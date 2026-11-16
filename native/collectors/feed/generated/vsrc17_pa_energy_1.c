/* Verified-live pa_energy sources (2), part 1.
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
VJSON(pa_asep_concesiones_2025, "pa-asep-concesiones-2025", "ASEP - Concesiones en Operacion Comercial 2025 (Panama)", "ASEP - Concesiones en Operacion Comercial 2025 (Panama)",
  "pa_energy", "energy",
  "https://www.datosabiertos.gob.pa/api/3/action/datastore_search?resource_id=aa5ec6df-3035-4502-b759-4a274ec5bafb&limit=50&sort=_id%20asc",
  "result.records",
  "es", "[\"pa\",\"energy\",\"batch17\",\"high-penetrancy\"]", 10800,
  "Prior-year concession register, 540 rows, same fields - reveals changes of concession holder between years.");

VJSON(pa_asep_concesiones_2026, "pa-asep-concesiones-2026", "ASEP - Concesiones electricas en operacion 2026 (Panama)", "ASEP - Concesiones electricas en operacion 2026 (Panama)",
  "pa_energy", "energy",
  "https://www.datosabiertos.gob.pa/api/3/action/datastore_search?resource_id=db368996-b5fd-49ea-acd7-2197f85db29e&limit=50&sort=_id%20asc",
  "result.records",
  "es", "[\"pa\",\"energy\",\"batch17\",\"high-penetrancy\"]", 10800,
  "Generation concessions in commercial operation, 405 rows: month, CONCESSION-HOLDING COMPANY, project name, resource exploited, province, district, corregimiento, installed MW, start of operation and current condition - company-to-asset mapping for Panama's power sector.");
