/* Verified-live sn_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON_KEYED(dportal_iati_sn, "dportal-iati-sn", "d-portal IATI activities - Senegal", "d-portal IATI activities - Senegal",
  "sn_aid", "aid",
  "https://d-portal.org/q?country_code=SN&limit=5000&form=json",
  "rows",
  "en", "[\"sn\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Senegal: IATI activity id, reporting org and ref, funder ref, title, full description, commitment/spend in USD and EUR, status and dates. Detail hop lists the activity's transactions.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_sn, "undp-projects-sn", "UNDP Open Data projects - Senegal", "UNDP Open Data projects - Senegal",
  "sn_aid", "aid",
  "https://api.open.undp.org/api/units/SEN.json",
  "projects",
  "fr", "[\"sn\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Senegal projects with award id, title, nested outputs (sector, description, SDG, markers), subnational geolocations and purchase orders.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
