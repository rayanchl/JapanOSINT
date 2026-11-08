/* Verified-live tr_government sources (2), part 1.
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
VJSON(tr_izmir_council_members, "tr-izmir-council-members", "Izmir Metropolitan Municipality — council members", "Izmir Metropolitan Municipality — council members",
  "tr_government", "government",
  "https://acikveri.bizizmir.com/api/3/action/datastore_search?resource_id=ecc81700-8701-482e-8298-4a333f74a945&limit=100&sort=_id%20asc",
  "result.records",
  "tr", "[\"tr\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Named elected council members: serial number, FULL NAME, office held (e.g. Metropolitan Mayor), profession and level of education. New host, not previously in the tree.");

VJSON(tr_izmir_sister_cities, "tr-izmir-sister-cities", "Izmir — sister-city and cooperation protocols", "Izmir — sister-city and cooperation protocols",
  "tr_government", "government",
  "https://acikveri.bizizmir.com/api/3/action/datastore_search?resource_id=c351510c-107c-4ab5-bc62-dc6b27834ca7&limit=100&sort=_id%20asc",
  "result.records",
  "tr", "[\"tr\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Municipal foreign relations: agreement type (cooperation protocol / sister city), domestic vs foreign, partner COUNTRY and CITY, council resolution number and council resolution date.");
