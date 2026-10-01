/* Verified-live za_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* d-portal activity rows identify themselves by the IATI identifier "aid"
 * (live 2026-09-06: 100 of 100 distinct on a page), a name not on the fixed
 * id precedence list; the (title, link, date) hash fallback collapsed
 * same-titled activities across pages (sweep: 2000 emitted, 1787 stored). */
VJSON_KEYED(af_dportal_act_za, "af-dportal-act-za", "d-portal (IATI) — aid activities in South Africa", "d-portal (IATI) — aid activities in South Africa",
  "za_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=ZA&limit=5000&form=json",
  "rows",
  "en", "[\"za\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with South Africa as recipient: aid, reporting org, funder_ref, title, description, status, dates, commitment/spend. Detail by aid returns the activity record.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(af_dportal_trans_za, "af-dportal-trans-za", "d-portal (IATI) — aid transactions in South Africa", "d-portal (IATI) — aid transactions in South Africa",
  "za_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=ZA&limit=5000&form=json",
  "rows",
  "en", "[\"za\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "South Africa activity-plus-transaction rows with trans_ref and trans_description; detail by aid returns the transaction ledger.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
