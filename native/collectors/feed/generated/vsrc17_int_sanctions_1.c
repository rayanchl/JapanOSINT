/* Verified-live int_sanctions sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* worldbank-debarred-firms moved to collectors/pivot/table/hp3_worldbank_sanctions.c on
 * 2026-10-19: the endpoint needs the apikey the World Bank's own sanctions page sends, and
 * this fleet has no key_env, so the key had been written into the URL. hpengine reads it
 * from WORLDBANK_SANCTIONS_APIKEY and files a "needs credential" notice when it is unset. */
