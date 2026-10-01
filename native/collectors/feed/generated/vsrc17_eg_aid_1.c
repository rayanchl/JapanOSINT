/* Verified-live eg_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on the IATI identifier "aid" (live 2026-09-06: 50 of 50 distinct on
 * a page; see vsrc17_za_aid_1.c). Sweep: 1000 emitted, 980 stored. */
VJSON_KEYED(dportal_iati_eg, "dportal-iati-eg", "d-portal IATI activities - Egypt", "d-portal IATI activities - Egypt",
  "eg_aid", "aid",
  "https://d-portal.org/q?country_code=EG&limit=5000&form=json",
  "rows",
  "en", "[\"eg\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Egypt with reporting organisation, funder, title, description, commitment and spend, and activity dates; transactions available per activity.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_eg, "undp-projects-eg", "UNDP Open Data projects - Egypt", "UNDP Open Data projects - Egypt",
  "eg_aid", "aid",
  "https://api.open.undp.org/api/units/EGY.json",
  "projects",
  "en", "[\"eg\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Egypt project portfolio: project id, title, outputs with sector and SDG mapping, geolocated activity sites, purchase orders and documents.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
