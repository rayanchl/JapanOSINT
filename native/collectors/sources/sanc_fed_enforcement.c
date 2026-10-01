/* US Federal Reserve Board enforcement actions.
 *
 * Endpoint : https://www.federalreserve.gov/feeds/press_enforcement.xml
 * Format   : RSS 2.0, CDATA-wrapped link/description.
 * Verified : HTTP 200, feed of recent enforcement actions (top item 2026-07-30,
 *            "Federal Reserve Board issues enforcement action with Iuka Bancshares, Inc.").
 * Keyless  : yes.
 * Emits    : title (the named bank holding company / bank / individual), link, pubDate
 *            (normalised to ISO-8601 UTC), description, guid.
 * Geometry : NONE (R2) — an enforcement order is not a place.
 * Licence  : US Government work, public domain.
 *
 * Notes    : the repo already reads federalreserve.gov/feeds/press_all.xml; this is the
 *            enforcement-only feed, a strict subset with far less noise. The body carries a
 *            UTF-8 BOM BEFORE the XML declaration — sanc_skip_bom() strips it inside
 *            sanc_feed_run() before a single tag is scanned.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "fed-enforcement-action",
    .tags_json = "[\"enforcement\",\"regulatory\",\"banking\",\"us\"]",
    .lang = "en",
  };
  int n = sanc_feed_run(ctx, sink, "https://www.federalreserve.gov/feeds/press_enforcement.xml",
                        &cfg, "fed-enforcement");
  return n < 0 ? -1 : 0;   /* R3: fetched-but-quiet is 0, not -1 */
}

static const source_def sanc_fed_enforcement_def = {
  .id = "fed-enforcement-actions", .collector = "sanctions",
  .name = "US Federal Reserve enforcement actions",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = "https://www.federalreserve.gov/feeds/press_enforcement.xml",
  .description = "Named US bank-holding companies, banks and individuals hit with Federal Reserve cease-and-desist orders, prohibition orders and civil money penalties.",
  .license = "US Government work, public domain.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_fed_enforcement_def)
