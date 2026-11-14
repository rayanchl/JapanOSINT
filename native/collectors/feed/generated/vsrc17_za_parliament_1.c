/* Verified-live za_parliament sources (10), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* af-pmg-daily-schedule and af-pmg-committee-meetings: MOVED 2026-11-12 to
 * collectors/feed/generated/hp17_za_parliament_1.c (same ids), because a VJSON
 * row cannot declare `page_resume` and both stopped at the 20-page default —
 * 1,000 records of the 3,757 and 35,240 the API counted that day. The rows
 * there read each register over a cycle. */
