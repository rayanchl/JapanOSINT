/* Verified-live lk_labour sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(lk_slbfe_search, "lk-slbfe-search", "Sri Lanka Bureau of Foreign Employment - site-wide search", "Sri Lanka Bureau of Foreign Employment - site-wide search",
  "lk_labour", "labour",
  "https://www.slbfe.lk/wp-json/wp/v2/search?search=agency&per_page=50",
  "",
  "en", "[\"lk\",\"labour\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Search over the regulator of Sri Lankan labour-migration agencies: title, url, id, type/subtype. Reaches licensed foreign-employment agency notices, suspensions and recruitment announcements - the register behind Sri Lanka's largest remittance channel.");
