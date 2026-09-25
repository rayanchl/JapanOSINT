/* Verified-live ml_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on the IATI activity identifier `aid` (50/50 distinct per live page
 * 2026-09-15). Sweep 2026-09-14: 1,000 emitted, 988 stored under the default
 * (title, link, date) hash. VJSON_IDKEYS rather than VJSON_KEYED: the latter's
 * pw_walk does not add the offset this URL lacks and stopped after page 1. */
#include "_vjson_idkeys.inc"
VJSON_IDKEYS(dportal_iati_ml, "dportal-iati-ml", "d-portal IATI activities - Mali", "d-portal IATI activities - Mali",
  "ml_aid", "aid",
  "https://d-portal.org/q?country_code=ML&limit=5000&form=json&orderby=aid", /* without an ORDER BY the LIMIT/OFFSET pages come back in a different order on every fetch: 1,000 rows walked held 865 distinct aid, the rest byte-identical re-serves of rows already seen (so others were skipped). orderby=aid puts ORDER BY aid in d-portal's own dquery; refetch-stable, 999-1,000 distinct of 1,000 (2026-09-15) */
  "rows",
  "en", "[\"ml\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Mali: reporting organisation, funder ref, title (some redaction notices from USAID are themselves informative), description, commitment/spend and dates. Transactions behind the detail hop.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_ml, "undp-projects-ml", "UNDP Open Data projects - Mali", "UNDP Open Data projects - Mali",
  "ml_aid", "aid",
  "https://api.open.undp.org/api/units/MLI.json",
  "projects",
  "fr", "[\"ml\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Mali projects: id, title, per-project outputs with sector and SDG, geolocated sites and purchase orders.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
