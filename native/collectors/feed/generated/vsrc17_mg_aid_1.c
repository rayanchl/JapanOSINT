/* Verified-live mg_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on the IATI identifier "aid" (see vsrc17_za_aid_1.c for the
 * measurement). Sweep: 1000 emitted, 981 stored. */
VJSON_KEYED(dportal_iati_mg, "dportal-iati-mg", "d-portal IATI activities - Madagascar", "d-portal IATI activities - Madagascar",
  "mg_aid", "aid",
  "https://d-portal.org/q?country_code=MG&limit=5000&form=json",
  "rows",
  "en", "[\"mg\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Madagascar: reporting organisation and ref, funder, title, description, commitment/spend and dates; transaction detail per activity.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_mg, "undp-projects-mg", "UNDP Open Data projects - Madagascar", "UNDP Open Data projects - Madagascar",
  "mg_aid", "aid",
  "https://api.open.undp.org/api/units/MDG.json",
  "projects",
  "fr", "[\"mg\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Madagascar projects with award id, title, outputs, geolocated activity sites and purchase orders.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
