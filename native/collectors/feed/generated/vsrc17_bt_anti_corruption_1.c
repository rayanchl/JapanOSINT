/* Verified-live bt_anti-corruption sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(bt_acc_search, "bt-acc-search", "Bhutan Anti-Corruption Commission - site-wide search", "Bhutan Anti-Corruption Commission - site-wide search",
  "bt_anti-corruption", "anti-corruption",
  "https://www.acc.org.bt/wp-json/wp/v2/search?search=corruption&per_page=50",
  "",
  "en", "[\"bt\",\"anti-corruption\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Free-text pivot over Bhutan's ACC: title, url, id, type/subtype. Reaches National Integrity Assessment findings presented against named agencies, investigation outcomes, asset-declaration notices and debarment announcements.");
