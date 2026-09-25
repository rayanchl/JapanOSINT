/* Verified-live cm_statistics sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VRSS(ins_cameroun_rss, "ins-cameroun-rss", "INS Cameroun statistics institute feed", "INS Cameroun statistics institute feed",
  "cm_statistics", "statistics",
  "https://ins-cameroun.cm/feed/",
  "fr", "[\"cm\",\"statistics\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Cameroon national statistics institute feed with report and survey announcements (e.g. 'CAMEROON JOINT MONITORING REPORT/RAPPORT DE SUIVI CONJOINT DU CAMEROUN') carrying title, link, dc:creator, pubDate and full content:encoded body.");
