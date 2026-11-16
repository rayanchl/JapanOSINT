/* Verified-live au_regulatory sources (2), part 1.
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
VJSON(au_apvma_pubcris_products, "au-apvma-pubcris-products", "APVMA PUBCRIS — registered agvet chemical products", "APVMA PUBCRIS — registered agvet chemical products",
  "au_regulatory", "regulatory",
  "https://data.gov.au/data/api/3/action/datastore_search?resource_id=a83d500d-b47a-4f03-9d77-4c0524217855&limit=100&sort=_id%20asc",
  "result.records",
  "en", "[\"au\",\"regulatory\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Registered agricultural and veterinary chemical products with pcode, regno and regdate. Thin: only three data columns, so the emitted label is the row number rather than a product name — useful as a registration-number index, not as a browsable list.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");

VJSON(au_qld_environmental_authorities, "au-qld-environmental-authorities", "Queensland — environmental authorities (permit holders)", "Queensland — environmental authorities (permit holders)",
  "au_regulatory", "regulatory",
  "https://www.data.qld.gov.au/api/3/action/datastore_search?resource_id=a9658145-87bd-4258-a689-5aec29d49792&limit=100&sort=_id%20asc",
  "result.records",
  "en", "[\"au\",\"regulatory\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Queensland environmental permits: Permit Reference, Permit Type, Permit Holder(s) (named company), Effective Date, Status (Granted/Surrendered), Condition Type, Industry, the specific Environmentally Relevant Activities authorised and locations. Facility-level regulatory footprint behind a corporate name.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
