/* Verified-live rw_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on `aid`, the IATI activity identifier (100 of 100 distinct per
 * page, live 2026-09-06; titles only 87 distinct). */
VJSON_KEYED(af_dportal_act_rw, "af-dportal-act-rw", "d-portal (IATI) — aid activities in Rwanda", "d-portal (IATI) — aid activities in Rwanda",
  "rw_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=RW&limit=5000&form=json",
  "rows",
  "en", "[\"rw\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Rwanda as recipient: aid, reporting org, funder_ref, title, description, status, dates, commitment/spend in four currencies. Detail by aid.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(af_dportal_trans_rw, "af-dportal-trans-rw", "d-portal (IATI) — aid transactions in Rwanda", "d-portal (IATI) — aid transactions in Rwanda",
  "rw_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=RW&limit=5000&form=json",
  "rows",
  "en", "[\"rw\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Rwanda activity-plus-transaction rows with trans_ref and trans_description; detail hop by aid returns the transaction ledger.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
