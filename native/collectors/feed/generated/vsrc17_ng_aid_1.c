/* Verified-live ng_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* "aid" is the IATI activity identifier and the row's real key, but it is not
 * in jsonlist's id precedence, so rows fell back to a hash of (title, link,
 * date) — and aid titles repeat heavily across reporting organisations
 * ("Donate to Educate Nigerian Girls…" is published by several funders).
 * Live-verified 2026-09-07: one page of 100 rows carries 100 distinct aid
 * values and only 99 distinct titles. */
VJSON_KEYED(af_dportal_act_ng, "af-dportal-act-ng", "d-portal (IATI) — aid activities in Nigeria", "d-portal (IATI) — aid activities in Nigeria",
  "ng_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=NG&limit=5000&form=json",
  "rows",
  "en", "[\"ng\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Every IATI-published aid activity with Nigeria as recipient. Row: aid (activity identifier), reporting org name, reporting_ref, funder_ref, title, slug, status_code, day_start/day_end/day_length, description, commitment and spend in USD/EUR/GBP/CAD, flags. Detail by aid returns the single activity; the same aid also drives ?from=trans&aid= for individual transactions.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(af_dportal_trans_ng, "af-dportal-trans-ng", "d-portal (IATI) — aid transactions in Nigeria", "d-portal (IATI) — aid transactions in Nigeria",
  "ng_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=NG&limit=5000&form=json",
  "rows",
  "en", "[\"ng\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Activity rows joined to their individual transactions for Nigeria — adds trans_ref and trans_description on top of the activity fields. Detail hop by aid returns the full transaction ledger for that activity: trans_day, trans_currency, trans_value, USD/EUR/GBP/CAD equivalents, trans_code, trans_flow_code, trans_finance_code, trans_sector, trans_id.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
