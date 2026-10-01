/* Verified-live in_markets sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(in_mfapi_scheme_search, "in-mfapi-scheme-search", "India mutual fund scheme name search", "India mutual fund scheme name search",
  "in_markets", "markets",
  "https://api.mfapi.in/mf/search?q=infrastructure",
  "",
  "en", "[\"in\",\"markets\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Name-pivot over the Indian mutual fund universe: returns schemeCode and schemeName for every scheme matching a free-text query. Dispatchable with a fund house or sponsor name to enumerate all of its schemes.");
