/* Verified-live tn_aid sources (2), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* d-portal activity rows carry the IATI activity identifier as "aid", which
 * is not on jsonlist's id list, so the uid fell back to sha(title, link,
 * day) — and aid programmes reuse titles ("Technical assistance…") on the
 * same start day: 49 distinct title|day_start per 50 rows here, 60 per 100
 * for Ethiopia (live 2026-09-06), colliding across pages. aid is present on
 * every row and unique (0 overlap between pages); key on it. */
VJSON_KEYED(dportal_iati_tn, "dportal-iati-tn", "d-portal IATI activities - Tunisia", "d-portal IATI activities - Tunisia",
  "tn_aid", "aid",
  "https://d-portal.org/q?country_code=TN&limit=5000&form=json",
  "rows",
  "en", "[\"tn\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Aid activities in Tunisia with reporting org, funder ref, title, narrative description, commitment/spend figures and activity status; detail hop yields per-activity transactions.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.",
  "aid");

VJSON(undp_projects_tn, "undp-projects-tn", "UNDP Open Data projects - Tunisia", "UNDP Open Data projects - Tunisia",
  "tn_aid", "aid",
  "https://api.open.undp.org/api/units/TUN.json",
  "projects",
  "fr", "[\"tn\",\"aid\",\"batch17\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "UNDP Tunisia project portfolio: project id, title, outputs with sector/SDG/description, geolocated subnational sites (e.g. Tunis, with lat/lon and focus area), purchase orders and attached documents.  A per-record detail endpoint was verified for this source; see the batch's detail-hops side-car. This collector fetches the list endpoint only.");
