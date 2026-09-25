/* Verified-live vn_finance sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(vn_vietqr_banks, "vn-vietqr-banks", "Vietnam Bank Directory (BIN, SWIFT and short name)", "Vietnam Bank Directory (BIN, SWIFT and short name)",
  "vn_finance", "finance",
  "https://api.vietqr.io/v2/banks",
  "data",
  "vi", "[\"vn\",\"finance\",\"batch17\",\"high-penetrancy\"]", 86400,
  "65 Vietnamese banks. Fields: id, name (full legal Vietnamese name), code, bin (6-digit Napas BIN, e.g. 970415), shortName, swift_code (e.g. ICBVVNVX), transferSupported, lookupSupported. Resolves BIN/SWIFT seen in Vietnamese payment artefacts to a named institution.");
