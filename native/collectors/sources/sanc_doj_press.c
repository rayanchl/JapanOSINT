/* US DOJ press releases — prosecutions, convictions and settlements.
 *
 * Endpoint : https://www.justice.gov/news/rss?type=press_release
 * Format   : RSS 2.0.
 * Verified : HTTP 200, newest item 2026-07-31 ("Diversity Visa Lottery Winner Found Guilty of
 *            Visa Fraud", usao-ndia).
 * Keyless  : yes.
 * Emits    : title (named defendant), link, description, pubDate (ISO-8601 UTC), guid, and
 *            the publishing DOJ component parsed out of the link path (usao-ndia, opa, …).
 * Geometry : NONE (R2) — a prosecution is not a coordinate.
 * Licence  : US Government work, public domain.
 *
 * Notes    : the legacy path www.justice.gov/feeds/opa/justice-news.xml now serves a Drupal
 *            "Page not found" body, which parses to zero items rather than failing loudly —
 *            exactly the HTTP-200-but-empty trap. /news/rss?type=press_release is the live one.
 */
#include "sanc_common.inc"

/* The DOJ component is the first path segment of the article URL. Nothing is
 * synthesised: this only re-reads bytes already present in the fetched link. */
static void doj_enrich(cJSON *props, const sanc_feed_item *fi) {
  const char *link = fi->link;
  if (!link) return;
  const char *host = strstr(link, "justice.gov/");
  if (!host) return;
  const char *seg = host + strlen("justice.gov/");
  const char *slash = strchr(seg, '/');
  if (!slash || slash == seg) return;
  size_t l = (size_t)(slash - seg);
  if (l >= 64) return;
  char comp[64];
  memcpy(comp, seg, l);
  comp[l] = 0;
  sanc_add(props, "doj_component", comp);
  if (strncmp(comp, "usao-", 5) == 0)
    sanc_add(props, "us_attorney_district", comp + 5);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const sanc_feed_cfg cfg = {
    .record_type = "doj-press-release",
    .tags_json = "[\"enforcement\",\"prosecution\",\"doj\",\"us\"]",
    .lang = "en",
    .enrich = doj_enrich,
  };
  int n = sanc_feed_run(ctx, sink, "https://www.justice.gov/news/rss?type=press_release",
                        &cfg, "doj-press");
  return n < 0 ? -1 : 0;
}

static const source_def sanc_doj_press_def = {
  .id = "doj-press-enforcement", .collector = "sanctions",
  .name = "US DOJ press releases (prosecutions and settlements)",
  .update_interval_sec = 10800, .run = run,
  .category = "government", .type = "api",
  .url = "https://www.justice.gov/news/rss?type=press_release",
  .description = "Named defendants in US federal indictments, convictions, FCPA and sanctions-evasion cases, and civil settlements, from Main Justice and the US Attorneys' offices.",
  .license = "US Government work, public domain.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_doj_press_def)
