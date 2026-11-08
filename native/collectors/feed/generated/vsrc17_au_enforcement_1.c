/* Verified-live au_enforcement sources (4), part 1.
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
VJSON(au_ndis_commission_compliance_actions, "au-ndis-commission-compliance-actions", "NDIS Quality and Safeguards Commission — compliance actions", "NDIS Quality and Safeguards Commission — compliance actions",
  "au_enforcement", "enforcement",
  "https://data.gov.au/data/api/3/action/datastore_search?resource_id=7e08bcc8-d3a0-403b-bac7-936ec4d48694&limit=100&sort=_id%20asc",
  "result.records",
  "en", "[\"au\",\"enforcement\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Regulatory actions against NDIS providers: Type (revocation of registration, banning order, compliance notice), Date effective from, Date no longer in force, provider Name including trading name, ABN, city/state/postcode, provider number and the registration groups affected. Names companies and the sanction against them.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");

VJSON(au_qld_environmental_enforcement_actions, "au-qld-environmental-enforcement-actions", "Queensland — environmental enforcement actions register", "Queensland — environmental enforcement actions register",
  "au_enforcement", "enforcement",
  "https://www.data.qld.gov.au/api/3/action/datastore_search?resource_id=7b334c2a-54dd-4ebe-a429-ad7123102cf2&limit=100&sort=_id%20asc",
  "result.records",
  "en", "[\"au\",\"enforcement\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Queensland environmental enforcement orders naming the recipient: Enforcement Reference, Enforcement Type (Environmental Enforcement Order, Transitional Environmental Program), Issued To (person or company), CC To, Issued Date, Status, Activities, Locations (lot/plan), related environmental authority, evaluator name and subsequent action.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
