/* Verified-live et_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on "aid" (the IATI activity identifier; present on all 100 live rows,
 * 100 distinct, only 60 distinct title|day_start — see vsrc17_tn_aid_1.c). The
 * act,trans row below has a different record shape and is left as is. */
VJSON_KEYED(af_dportal_act_et, "af-dportal-act-et", "d-portal (IATI) — aid activities in Ethiopia", "d-portal (IATI) — aid activities in Ethiopia",
  "et_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=ET&limit=5000&form=json",
  "rows",
  "en", "[\"et\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Ethiopia as recipient, carrying reporting organisation, funder reference, description, status and committed/spent amounts. Detail by aid.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

/* act JOIN trans rows: one row per (activity, transaction, sector/country
 * split), so neither aid nor trans_id is a key (53 distinct per 100 rows).
 * Measured over a 2,000-row walk 2026-09-15 (ET and NA): the 8-field tuple
 * below is 2,000/2,000 distinct, equal to the full-row distinct count, and
 * deliberately excludes the currency-converted amounts, which move with FX
 * and would re-key the same transaction on every run. */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(af_dportal_trans_et, "af-dportal-trans-et", "d-portal (IATI) — aid transactions in Ethiopia", "d-portal (IATI) — aid transactions in Ethiopia",
  "et_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=ET&limit=5000&form=json",
  "rows",
  "en", "[\"et\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Ethiopia activity-plus-transaction rows with trans_ref and trans_description; detail by aid returns all transactions with amounts, currencies and sector codes.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid+trans_id+trans_ref+trans_day+trans_value+trans_code+trans_sector+trans_country");
