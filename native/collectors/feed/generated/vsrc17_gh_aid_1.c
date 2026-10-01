/* Verified-live gh_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on the IATI activity identifier `aid`, as af-dportal-act-et/-na/-bw
 * already are. Sweep 2026-09-14: 2,000 emitted, 1,973 stored — the default
 * precedence found no id and hashed (title, link, date). A 20-page walk
 * 2026-09-15 returned 2,000 distinct aid values. VJSON_IDKEYS rather than
 * VJSON_KEYED: the latter's pw_walk does not add the offset this URL lacks, and
 * measured it stopped after page 1 (100 of the 2,000 VJSON's walk reaches). */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(af_dportal_act_gh, "af-dportal-act-gh", "d-portal (IATI) — aid activities in Ghana", "d-portal (IATI) — aid activities in Ghana",
  "gh_aid", "aid",
  "https://d-portal.org/q?from=act&country_code=GH&limit=5000&form=json&orderby=aid", /* unordered LIMIT/OFFSET pages re-serve and skip rows (an unordered 20-page walk held 1,998 distinct of 2,000); orderby=aid makes d-portal's query ORDER BY aid and the walk refetch-stable — measured on the same endpoint for Mali, see dportal-iati-ml */
  "rows",
  "en", "[\"gh\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "IATI aid activities with Ghana as recipient, carrying funder reference, reporting organisation, description, status and committed/spent amounts. Detail by aid returns the activity.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(af_dportal_trans_gh, "af-dportal-trans-gh", "d-portal (IATI) — aid transactions in Ghana", "d-portal (IATI) — aid transactions in Ghana",
  "gh_aid", "aid",
  "https://d-portal.org/q?from=act,trans&trans_country_code=GH&limit=5000&form=json",
  "rows",
  "en", "[\"gh\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Ghana activity-plus-transaction rows carrying trans_ref and trans_description; detail hop by aid returns every transaction with amounts, currency and sector codes.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
