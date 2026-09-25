/* Verified-live dz_aid sources (2), part 1.
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
VJSON_KEYED(dportal_iati_dz, "dportal-iati-dz", "d-portal IATI activities - Algeria", "d-portal IATI activities - Algeria",
  "dz_aid", "aid",
  "https://d-portal.org/q?country_code=DZ&limit=5000&form=json",
  "rows",
  "en", "[\"dz\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Algeria: donor, implementing/reporting organisation, title, description, budget and disbursement amounts, dates and status. Detail hop returns transaction-level records.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_dz, "undp-projects-dz", "UNDP Open Data projects - Algeria", "UNDP Open Data projects - Algeria",
  "dz_aid", "aid",
  "https://api.open.undp.org/api/units/DZA.json",
  "projects",
  "fr", "[\"dz\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Algeria projects with award id, French titles, per-project outputs (sector, description, SDG targets), subnational locations and purchase orders. One of the very few open machine-readable Algerian project datasets.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
