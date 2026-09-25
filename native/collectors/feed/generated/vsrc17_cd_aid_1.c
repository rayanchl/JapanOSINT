/* Verified-live cd_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* Keyed on `aid`, the IATI activity identifier (50 of 50 distinct per page,
 * live 2026-09-06); titles repeat across activities (34 distinct of 50), so
 * the hash fallback was merging distinct activities across pages. */
VJSON_KEYED(dportal_iati_cd, "dportal-iati-cd", "d-portal IATI activities - DR Congo", "d-portal IATI activities - DR Congo",
  "cd_aid", "aid",
  "https://d-portal.org/q?country_code=CD&limit=5000&form=json",
  "rows",
  "en", "[\"cd\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in the Democratic Republic of the Congo with donor, reporting ref, title, description, committed and disbursed amounts, status and period; per-activity transactions.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_cd, "undp-projects-cd", "UNDP Open Data projects - DR Congo", "UNDP Open Data projects - DR Congo",
  "cd_aid", "aid",
  "https://api.open.undp.org/api/units/COD.json",
  "projects",
  "fr", "[\"cd\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Largest of the set: 308 UNDP DR Congo projects with award id, title, outputs (sector, description, SDG, policy markers), geolocated subnational sites, purchase orders and documents.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
