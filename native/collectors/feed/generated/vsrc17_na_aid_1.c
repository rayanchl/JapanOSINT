/* Verified-live na_aid sources (2), part 1.
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
VJSON_KEYED(af_dportal_act_na, "af-dportal-act-na", "d-portal (IATI) — aid activities in Namibia", "d-portal (IATI) — aid activities in Namibia",
  "na_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=NA&limit=5000&form=json",
  "rows",
  "en", "[\"na\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Namibia as recipient: aid, reporting org, funder_ref, title, description, status, dates and multi-currency amounts. Detail by aid.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

/* act JOIN trans rows have no single key; see af-dportal-trans-et in
 * vsrc17_et_aid_1.c for the measured 8-field tuple (2,000/2,000 distinct on
 * this country's walk as well). */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(af_dportal_trans_na, "af-dportal-trans-na", "d-portal (IATI) — aid transactions in Namibia", "d-portal (IATI) — aid transactions in Namibia",
  "na_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=NA&limit=5000&form=json",
  "rows",
  "en", "[\"na\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Namibia activity-plus-transaction rows with trans_ref and trans_description; detail hop by aid returns the transaction ledger.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid+trans_id+trans_ref+trans_day+trans_value+trans_code+trans_sector+trans_country");
