/* Verified-live ci_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON_KEYED(dportal_iati_ci, "dportal-iati-ci", "d-portal IATI activities - Cote d'Ivoire", "d-portal IATI activities - Cote d'Ivoire",
  "ci_aid", "aid",
  "https://d-portal.org/q?country_code=CI&limit=5000&form=json",
  "rows",
  "fr", "[\"ci\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Cote d'Ivoire, many reported in French, with donor, title, description, financial commitment and spend, status and dates; per-activity transactions behind the detail hop.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_ci, "undp-projects-ci", "UNDP Open Data projects - Cote d'Ivoire", "UNDP Open Data projects - Cote d'Ivoire",
  "ci_aid", "aid",
  "https://api.open.undp.org/api/units/CIV.json",
  "projects",
  "fr", "[\"ci\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Cote d'Ivoire projects: id, French title, outputs with sector/SDG/description, geolocated sites and purchase orders.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
