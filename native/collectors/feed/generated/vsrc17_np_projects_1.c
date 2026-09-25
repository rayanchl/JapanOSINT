/* Verified-live np_projects sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(np_bipad_project, "np-bipad-project", "Nepal BIPAD - DRR projects register", "Nepal BIPAD - DRR projects register",
  "np_projects", "projects",
  "https://bipadportal.gov.np/api/v1/project/?format=json&limit=50",
  "results",
  "en", "[\"np\",\"projects\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Registered disaster risk reduction projects: title, description of the intervention, the implementing organization id and the wards it covers.");
