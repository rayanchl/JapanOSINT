/* Verified-live lk_energy sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(lk_energymin_search, "lk-energymin-search", "Sri Lanka Ministry of Energy - site-wide search", "Sri Lanka Ministry of Energy - site-wide search",
  "lk_energy", "energy",
  "https://www.energymin.gov.lk/wp-json/wp/v2/search?search=fuel&per_page=50",
  "",
  "en", "[\"lk\",\"energy\",\"batch17\",\"high-penetrancy\"]", 10800,
  "Search over the energy ministry: title, url, id, type/subtype. Carries dated administered fuel-price revisions and petroleum/power sector notices.");
