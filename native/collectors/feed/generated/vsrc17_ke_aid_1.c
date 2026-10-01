/* Verified-live ke_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on "aid" (the IATI activity identifier; present on all 100 live rows,
 * 100 distinct, only 48 distinct title|day_start — see vsrc17_tn_aid_1.c). */
VJSON_KEYED(af_dportal_act_ke, "af-dportal-act-ke", "d-portal (IATI) — aid activities in Kenya", "d-portal (IATI) — aid activities in Kenya",
  "ke_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=KE&limit=5000&form=json",
  "rows",
  "en", "[\"ke\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Kenya as recipient: aid, reporting org and reporting_ref, funder_ref, title, description, status, start/end days, commitment and spend in four currencies. Detail hop by aid returns the activity record.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(af_dportal_trans_ke, "af-dportal-trans-ke", "d-portal (IATI) — aid transactions in Kenya", "d-portal (IATI) — aid transactions in Kenya",
  "ke_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=KE&limit=5000&form=json",
  "rows",
  "en", "[\"ke\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Kenya activity-plus-transaction rows with trans_ref and trans_description; detail by aid returns the per-activity transaction ledger with values, currencies, flow and finance codes.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
