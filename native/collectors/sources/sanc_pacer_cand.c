/* PACER docket feed — US District Court, Northern District of California.
 *
 * Endpoint : https://ecf.cand.uscourts.gov/cgi-bin/rss_outside.pl
 * Format   : RSS 2.0 declared ISO-8859-1 with an HTML-escaped anchor inside <description>.
 * Verified : HTTP 200, live docket entries (lastBuildDate 2026-08-01T20:30 GMT; sample
 *            "3:26-cv-01132 Yakovlev v. The Superior Court of the State of California …").
 * Keyless  : yes — the public court RSS, no PACER account.
 * Emits    : title (case number + caption), link (DktRpt.pl?caseid), docket-entry type parsed
 *            from the bracketed prefix of <description>, case_number (leading title token),
 *            criminal flag ("-cr-" in the case number), pubDate as ISO-8601 UTC.
 * Geometry : NONE (R2).
 * Licence  : US Courts public RSS, free tier of PACER. Fetch politely — the feed is
 *            regenerated on a fixed cycle, so polling faster than ~15 min gains nothing.
 *
 * Notes    : the body is not valid UTF-8; sanc_feed_run() transcodes Latin-1 -> UTF-8 before
 *            any field is extracted so no undecodable byte reaches the DB or its FTS mirror.
 *            The same cgi path exists on most district courts — see the nysd/dcd siblings.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "pacer-docket-entry",
    .tags_json = "[\"court\",\"docket\",\"pacer\",\"us\"]",
    .lang = "en",
    .enrich = sanc_pacer_enrich,
  };
  int n = sanc_feed_run(ctx, sink, "https://ecf.cand.uscourts.gov/cgi-bin/rss_outside.pl",
                        &cfg, "pacer-cand");
  return n < 0 ? -1 : 0;
}

static const source_def sanc_pacer_cand_def = {
  .id = "pacer-rss-cand", .collector = "sanctions",
  .name = "PACER docket feed — N.D. California",
  .update_interval_sec = 900, .run = run,
  .category = "government", .type = "api",
  .url = "https://ecf.cand.uscourts.gov/cgi-bin/rss_outside.pl",
  .description = "Live federal docket activity — new civil and criminal filings and the entries made on them — from the district that hears most US tech litigation. Keyless, no PACER account needed.",
  .license = "US Courts public RSS (free tier of PACER).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_pacer_cand_def)
