/* PACER docket feed — US District Court, District of Columbia.
 *
 * Endpoint : https://ecf.dcd.uscourts.gov/cgi-bin/rss_outside.pl
 * Format   : RSS 2.0 declared ISO-8859-1, HTML-escaped anchor inside <description>.
 * Verified : HTTP 200, live docket entries (lastBuildDate 2026-08-01T20:04 GMT; sample
 *            "1:25-cr-00363-1 USA v. CUNNINGHAM - [Response to motion]").
 * Keyless  : yes.
 * Emits    : title (case number + caption), link, docket-entry type, case_number, criminal
 *            flag, pubDate as ISO-8601 UTC.
 * Geometry : NONE (R2).
 * Licence  : US Courts public RSS, free tier of PACER.
 *
 * Notes    : DCD publishes "all" entry types. "USA v. X" captions identify criminal
 *            prosecutions cleanly, and the "-cr-" test on the case number flags them in
 *            properties. This is where federal agency litigation, FOIA suits, national
 *            security cases and many DOJ prosecutions land.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "pacer-docket-entry",
    .tags_json = "[\"court\",\"docket\",\"pacer\",\"us\"]",
    .lang = "en",
    .enrich = sanc_pacer_enrich,
  };
  int n = sanc_feed_run(ctx, sink, "https://ecf.dcd.uscourts.gov/cgi-bin/rss_outside.pl",
                        &cfg, "pacer-dcd");
  return n < 0 ? -1 : 0;
}

static const source_def sanc_pacer_dcd_def = {
  .id = "pacer-rss-dcd", .collector = "sanctions",
  .name = "PACER docket feed — District of Columbia",
  .update_interval_sec = 900, .run = run,
  .category = "government", .type = "api",
  .url = "https://ecf.dcd.uscourts.gov/cgi-bin/rss_outside.pl",
  .description = "DC district docket — where federal agency litigation, FOIA suits, national-security cases and many DOJ prosecutions land.",
  .license = "US Courts public RSS (free tier of PACER).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_pacer_dcd_def)
