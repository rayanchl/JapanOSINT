/* Verified-live zm_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on the IATI identifier "aid" (see vsrc17_za_aid_1.c for the
 * measurement); sweep: 2000 emitted, 1789 stored. The trans row below is
 * NOT keyed on aid: one activity has many transactions and d-portal's
 * trans_id is not unique on its own (live: 100 rows, 68 distinct
 * aid|trans_id), so it stays on the content-disambiguated hash. */
VJSON_KEYED(af_dportal_act_zm, "af-dportal-act-zm", "d-portal (IATI) — aid activities in Zambia", "d-portal (IATI) — aid activities in Zambia",
  "zm_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=ZM&limit=5000&form=json",
  "rows",
  "en", "[\"zm\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Zambia as recipient, carrying reporting organisation, funder reference, description, status and committed/spent amounts. Detail by aid.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

/* Keyed on the transaction tuple, as af-dportal-trans-tz is. An act⋈trans row
 * carries no id, so the generic VJSON key folded different transactions of one
 * activity onto each other: a 1800 s run emitted 100,000 and stored 99,007
 * (993 lost). On a 20,000-row sample (live 2026-09-15) this tuple is unique on
 * every byte-distinct row. `+` composes. */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(af_dportal_trans_zm, "af-dportal-trans-zm", "d-portal (IATI) — aid transactions in Zambia", "d-portal (IATI) — aid transactions in Zambia",
  "zm_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=ZM&limit=5000&form=json",
  "rows",
  "en", "[\"zm\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Zambia activity-plus-transaction rows with trans_ref and trans_description; detail by aid returns all transactions with amounts and codes.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid+trans_id+trans_ref+trans_day+trans_value+trans_code+trans_sector+trans_sector_group+trans_country");
