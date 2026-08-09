/* PACER docket feed — US District Court, Southern District of New York.
 *
 * Endpoint : https://ecf.nysd.uscourts.gov/cgi-bin/rss_outside.pl
 * Format   : RSS 2.0 declared ISO-8859-1, HTML-escaped anchor inside <description>.
 * Verified : HTTP 200, live docket entries (lastBuildDate 2026-08-01T19:05 GMT; sample
 *            "1:26-cv-03600 Pacheco Jr. v. Commissioner of Social Security").
 * Keyless  : yes.
 * Emits    : title (case number + caption), link, docket-entry type, case_number, criminal
 *            flag, pubDate as ISO-8601 UTC.
 * Geometry : NONE (R2).
 * Licence  : US Courts public RSS, free tier of PACER.
 *
 * Notes    : SDNY restricts its feed to an explicit entry-type list (published in the channel
 *            <description>), so it is NARROWER than CAND's "all" — a quiet run here is normal
 *            and returns 0, not -1. This is the district that hears US securities-fraud,
 *            sanctions-evasion and terrorism-finance prosecutions.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "pacer-docket-entry",
    .tags_json = "[\"court\",\"docket\",\"pacer\",\"us\"]",
    .lang = "en",
    .enrich = sanc_pacer_enrich,
  };
  int n = sanc_feed_run(ctx, sink, "https://ecf.nysd.uscourts.gov/cgi-bin/rss_outside.pl",
                        &cfg, "pacer-nysd");
  return n < 0 ? -1 : 0;
}

static const source_def sanc_pacer_nysd_def = {
  .id = "pacer-rss-nysd", .collector = "sanctions",
  .name = "PACER docket feed — S.D. New York",
  .update_interval_sec = 900, .run = run,
  .category = "government", .type = "api",
  .url = "https://ecf.nysd.uscourts.gov/cgi-bin/rss_outside.pl",
  .description = "The docket of the district that hears US securities-fraud, sanctions-evasion and terrorism-finance prosecutions.",
  .license = "US Courts public RSS (free tier of PACER).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_pacer_nysd_def)
