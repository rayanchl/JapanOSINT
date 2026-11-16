/* Verified-live pa_procurement sources (2), part 1.
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
VJSON(pa_sertv_contrataciones_2024, "pa-sertv-contrataciones-2024", "SERTV - Contrataciones Publicas 2024 (Panama)", "SERTV - Contrataciones Publicas 2024 (Panama)",
  "pa_procurement", "procurement",
  "https://www.datosabiertos.gob.pa/api/3/action/datastore_search?resource_id=c44dbb5d-3691-4700-af4b-1f20724793b3&limit=50&sort=_id%20asc",
  "result.records",
  "es", "[\"pa\",\"procurement\",\"batch17\",\"high-penetrancy\"]", 21600,
  "Same award register for 2024, 798 rows - award-by-award supplier history for one contracting entity across years.");

VJSON(pa_sertv_contrataciones_2026, "pa-sertv-contrataciones-2026", "SERTV - Contrataciones Publicas 2026 (Panama)", "SERTV - Contrataciones Publicas 2026 (Panama)",
  "pa_procurement", "procurement",
  "https://www.datosabiertos.gob.pa/api/3/action/datastore_search?resource_id=46079db0-b7b2-483a-a2de-5b6811856e51&limit=50&sort=_id%20asc",
  "result.records",
  "es", "[\"pa\",\"procurement\",\"batch17\",\"high-penetrancy\"]", 21600,
  "Awarded public tenders of the state broadcaster, 245 rows: public act number, act name, purchase type, AWARDED COMPANY, act date, item description, quantity, unit value and purchase value. Records also carry a mojibake 'Nombre del Acto P£blico Adjudicado' column; '_id' is the safe exact label key.");
