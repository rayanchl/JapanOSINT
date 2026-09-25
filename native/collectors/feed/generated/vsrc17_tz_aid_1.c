/* Verified-live tz_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON_KEYED(af_dportal_act_tz, "af-dportal-act-tz", "d-portal (IATI) — aid activities in Tanzania", "d-portal (IATI) — aid activities in Tanzania",
  "tz_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=TZ&limit=5000&form=json",
  "rows",
  "en", "[\"tz\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Tanzania as recipient: aid, reporting org, funder_ref, title, description, status, dates and amounts. Detail by aid returns the activity.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

/* act⋈trans rows have no single key (one activity `aid` carries many
 * transactions, and trans_id repeats across activities: 100 sample rows held 2
 * aids and 57 trans_ids, 0 byte-identical). Keyed on the BW/ET/NA siblings'
 * composite PLUS trans_sector_group: measured 2026-09-15 over 205,000 TZ rows
 * (41 offset pages), 204,875 byte-distinct rows, 204,513 distinct on the
 * sibling composite — 362 rows differ only in trans_sector_group (one
 * transaction split across sector groups) and would fold. See _vjson_idkeys.inc. */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(af_dportal_trans_tz, "af-dportal-trans-tz", "d-portal (IATI) — aid transactions in Tanzania", "d-portal (IATI) — aid transactions in Tanzania",
  "tz_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=TZ&limit=5000&form=json",
  "rows",
  "en", "[\"tz\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Tanzania activity-plus-transaction rows carrying trans_ref and trans_description; detail hop by aid returns the per-activity transaction ledger.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid+trans_id+trans_ref+trans_day+trans_value+trans_code+trans_sector+trans_sector_group+trans_country");
