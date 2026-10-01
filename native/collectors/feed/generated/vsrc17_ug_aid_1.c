/* Verified-live ug_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on the IATI activity identifier `aid` (live 2026-09-07: 100/100
 * distinct per page); the default id precedence found no id field and hashed
 * (title, link, date), so same-titled activities collided across pages. */
VJSON_KEYED(af_dportal_act_ug, "af-dportal-act-ug", "d-portal (IATI) — aid activities in Uganda", "d-portal (IATI) — aid activities in Uganda",
  "ug_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=UG&limit=5000&form=json",
  "rows",
  "en", "[\"ug\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Uganda as recipient, with reporting organisation, funder reference, description, status and multi-currency commitment/spend. Detail hop by aid.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(af_dportal_trans_ug, "af-dportal-trans-ug", "d-portal (IATI) — aid transactions in Uganda", "d-portal (IATI) — aid transactions in Uganda",
  "ug_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=UG&limit=5000&form=json",
  "rows",
  "en", "[\"ug\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Uganda activity-plus-transaction rows with trans_ref and trans_description; detail by aid returns transaction values, currencies, flow/finance/sector codes.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
