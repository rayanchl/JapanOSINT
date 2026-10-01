/* US CFTC enforcement actions.
 *
 * Endpoint : https://www.cftc.gov/RSS/RSSENF/rssenf.xml
 * Format   : RSS 2.0.
 * Verified : HTTP 200, newest item 2026-07-31 ("CFTC Orders George Santos to Pay $35,000 for
 *            Manipulative Trading of State-of-the-Union Event Contract").
 * Keyless  : yes.
 * Emits    : title (respondent is named in it), link, pubDate (ISO-8601 UTC), guid.
 * Geometry : NONE (R2).
 * Licence  : US Government work, public domain.
 *
 * Notes    : <description> is empty on this feed — the payload IS the <title>, so rows carry
 *            no summary and that is correct, not a parse miss. The channel title says "Press
 *            Releases" but the RSSENF path is the enforcement filter.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "cftc-enforcement-action",
    .tags_json = "[\"enforcement\",\"regulatory\",\"derivatives\",\"crypto\",\"us\"]",
    .lang = "en",
  };
  int n = sanc_feed_run(ctx, sink, "https://www.cftc.gov/RSS/RSSENF/rssenf.xml",
                        &cfg, "cftc-enforcement");
  return n < 0 ? -1 : 0;
}

static const source_def sanc_cftc_enforcement_def = {
  .id = "cftc-enforcement-actions", .collector = "sanctions",
  .name = "US CFTC enforcement actions",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = "https://www.cftc.gov/RSS/RSSENF/rssenf.xml",
  .description = "Derivatives and commodities enforcement: fraud and manipulation orders, registration bans and the CFTC's crypto-fraud actions, with the respondent named in the title.",
  .license = "US Government work, public domain.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_cftc_enforcement_def)
