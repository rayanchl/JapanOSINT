/* Verified-live hk_registry sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* hk-companies-registry-new-nonhk-companies removed 2026-10-06. It read
 * RNC063F_20250303.csv — one week, frozen since batch 17 — because the list is
 * republished weekly under a new date and VCSV has no way to follow it.
 * JO39_CN_HK_CR_NEW_NONHK_COMPANIES (collectors/pivot/table/hp3b39_cn39.c)
 * reads the newest week with a {date…} token and date_back. */
