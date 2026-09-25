/* Verified-live cm_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on "aid" (the IATI activity identifier; present on all 100 live rows,
 * 100 distinct, 88 distinct title|day_start — see vsrc17_tn_aid_1.c). */
VJSON_KEYED(dportal_iati_cm, "dportal-iati-cm", "d-portal IATI activities - Cameroon", "d-portal IATI activities - Cameroon",
  "cm_aid", "aid",
  "https://d-portal.org/q?country_code=CM&limit=5000&form=json",
  "rows",
  "en", "[\"cm\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Cameroon with reporting organisation, funder ref, title, description, committed and spent amounts and activity period; transaction detail per activity.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_cm, "undp-projects-cm", "UNDP Open Data projects - Cameroon", "UNDP Open Data projects - Cameroon",
  "cm_aid", "aid",
  "https://api.open.undp.org/api/units/CMR.json",
  "projects",
  "fr", "[\"cm\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Cameroon projects with award id, title, per-project outputs, subnational locations and purchase orders.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
