/* Verified-live bf_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on the IATI activity identifier `aid` (live 2026-09-07: 50/50 distinct
 * per page); the default id precedence found no id field and hashed (title,
 * link, date), so same-titled activities collided across pages. */
VJSON_KEYED(dportal_iati_bf, "dportal-iati-bf", "d-portal IATI activities - Burkina Faso", "d-portal IATI activities - Burkina Faso",
  "bf_aid", "aid",
  "https://d-portal.org/q?country_code=BF&limit=5000&form=json",
  "rows",
  "en", "[\"bf\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Burkina Faso with reporting org, funder, title, description, financial commitment and spend and activity dates; transaction detail available.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_bf, "undp-projects-bf", "UNDP Open Data projects - Burkina Faso", "UNDP Open Data projects - Burkina Faso",
  "bf_aid", "aid",
  "https://api.open.undp.org/api/units/BFA.json",
  "projects",
  "en", "[\"bf\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Burkina Faso projects with award id, title, outputs (sector, description, SDG), subnational locations and purchase orders.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
