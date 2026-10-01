/* Verified-live mu_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Same shape as the Benin row: "aid" is the IATI activity identifier and is not
 * in jsonlist's id precedence, so rows keyed on a hash of (title, link, date).
 * Live-verified 2026-09-07: 50 rows, 50 distinct aid values, 47 distinct
 * titles. */
VJSON_KEYED(dportal_iati_mu, "dportal-iati-mu", "d-portal IATI activities - Mauritius", "d-portal IATI activities - Mauritius",
  "mu_aid", "aid",
  "https://d-portal.org/q?country_code=MU&limit=5000&form=json",
  "rows",
  "en", "[\"mu\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Mauritius with donor and reporting refs, project title, description, financial commitment and spend, and activity dates; transactions behind the detail hop.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_mu, "undp-projects-mu", "UNDP Open Data projects - Mauritius", "UNDP Open Data projects - Mauritius",
  "mu_aid", "aid",
  "https://api.open.undp.org/api/units/MUS.json",
  "projects",
  "en", "[\"mu\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Mauritius projects: id, title, outputs with sector/SDG, subnational locations and purchase orders.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
