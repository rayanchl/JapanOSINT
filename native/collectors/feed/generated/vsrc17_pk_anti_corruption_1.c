/* Verified-live pk_anti-corruption sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(pk_nab_search, "pk-nab-search", "Pakistan National Accountability Bureau - site-wide search", "Pakistan National Accountability Bureau - site-wide search",
  "pk_anti-corruption", "anti-corruption",
  "https://www.nab.gov.pk/wp-json/wp/v2/search?search=corruption&per_page=50",
  "",
  "en", "[\"pk\",\"anti-corruption\",\"batch17\",\"high-penetrancy\"]", 86400,
  "High value: free-text pivot over Pakistan's principal anti-corruption prosecutor. Each hit gives title, url, id and type/subtype, reaching NAB press releases, reference filings, arrest and plea-bargain announcements and inter-agency actions. Dispatchable with a person or company name.");
