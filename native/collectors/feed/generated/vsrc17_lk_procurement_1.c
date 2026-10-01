/* Verified-live lk_procurement sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(lk_agrimin_search, "lk-agrimin-search", "Sri Lanka Ministry of Agriculture - site-wide search", "Sri Lanka Ministry of Agriculture - site-wide search",
  "lk_procurement", "procurement",
  "https://www.agrimin.gov.lk/wp-json/wp/v2/search?search=tender&per_page=50",
  "",
  "en", "[\"lk\",\"procurement\",\"batch17\",\"high-penetrancy\"]", 21600,
  "Search pivot returning title, url, id, type/subtype. Hits include tender notices and named-officer appointment lists (e.g. permanent appointments at the Land Reform Commission).");
