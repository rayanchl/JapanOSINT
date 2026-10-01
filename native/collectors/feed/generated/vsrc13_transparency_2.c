/* Verified-live transparency sources (1), part 2.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(cyb_usaspending_recipient_count, "cyb-usaspending-recipient-count", "USAspending — award recipients by state", "USAspending 州別受給者",
  "transparency", "procurement",
  "https://api.usaspending.gov/api/v2/recipient/state/",
  "",
  "en", "[\"usa\",\"procurement\",\"spending\",\"contracts\"]", 86400,
  "US federal award totals aggregated by state and territory.");
