/* Verified-live bj_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* "aid" is the IATI activity identifier; jsonlist's id precedence does not
 * know the name, so rows fell back to a hash of (title, link, date) and repeated
 * project titles collapsed. Live-verified 2026-09-07: 50 rows, 50 distinct aid
 * values, only 35 distinct titles. */
VJSON_KEYED(dportal_iati_bj, "dportal-iati-bj", "d-portal IATI activities - Benin", "d-portal IATI activities - Benin",
  "bj_aid", "aid",
  "https://d-portal.org/q?country_code=BJ&limit=5000&form=json",
  "rows",
  "en", "[\"bj\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Benin: donor and reporting refs, project title, description, commitment/spend, status codes and dates. Detail hop returns transactions.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_bj, "undp-projects-bj", "UNDP Open Data projects - Benin", "UNDP Open Data projects - Benin",
  "bj_aid", "aid",
  "https://api.open.undp.org/api/units/BEN.json",
  "projects",
  "en", "[\"bj\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Benin projects: project id, title, outputs with sector and SDG mapping, geolocated activity sites, purchase orders and documents.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
