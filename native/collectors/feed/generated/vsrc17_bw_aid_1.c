/* Verified-live bw_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON_KEYED(af_dportal_act_bw, "af-dportal-act-bw", "d-portal (IATI) — aid activities in Botswana", "d-portal (IATI) — aid activities in Botswana",
  "bw_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=BW&limit=5000&form=json",
  "rows",
  "en", "[\"bw\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Botswana as recipient, with reporting organisation, funder reference, description, status and amounts. Detail hop by aid.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

/* act JOIN trans rows have no single key; see af-dportal-trans-et in
 * vsrc17_et_aid_1.c for the measured 8-field tuple. */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(af_dportal_trans_bw, "af-dportal-trans-bw", "d-portal (IATI) — aid transactions in Botswana", "d-portal (IATI) — aid transactions in Botswana",
  "bw_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=BW&limit=5000&form=json",
  "rows",
  "en", "[\"bw\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Botswana activity-plus-transaction rows with trans_ref and trans_description; detail by aid returns per-transaction values and codes.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid+trans_id+trans_ref+trans_day+trans_value+trans_code+trans_sector+trans_country");
