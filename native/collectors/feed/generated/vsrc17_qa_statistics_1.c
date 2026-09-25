/* Verified-live qa_statistics sources (1), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(qa_environmental_monitoring, "qa-environmental-monitoring", "Qatar — coastal environmental monitoring by location", "Qatar — coastal environmental monitoring by location",
  "qa_statistics", "statistics",
  "https://www.data.gov.qa/api/explore/v2.1/catalog/datasets/plastic-debris-density0/records?limit=100",
  "results",
  "en", "[\"qa\",\"statistics\",\"batch17\",\"high-penetrancy\"]", 86400,
  "728 measurements: named parameter (English and Arabic), named monitoring location including the national permissible maximum, measured value and year.");
