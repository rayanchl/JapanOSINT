/* collectors/sources/google_trends_jp.c
 * Google Trends — Daily Search Trends (Japan).
 * RSS 2.0 feed at https://trends.google.com/trending/rss?geo=JP
 * returns trending search terms with approx_traffic and associated news items.
 * Keyless, scheduled every 30 minutes. Uses rss_collect for standard RSS
 * parsing (guid→link→sha1 uid, all items emitted exhaustively). */
#include "source.h"
#include "lib/rss_atom.h"
#include <stdio.h>

#define FEED_URL "https://trends.google.com/trending/rss?geo=JP"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = rss_collect(ctx, sink, FEED_URL, "ja",
                      "[\"google-trends\",\"trending\",\"japan\"]");
  fprintf(stderr, "[google-trends-jp] emitted %d\n", n);
  return n < 0 ? -1 : 0;
}

static const source_def google_trends_jp_def = {
  .id = "google-trends-jp", .collector = "social",
  .name = "Google Trends (Japan)", .name_ja = "Googleトレンド（日本）",
  .update_interval_sec = 1800, .run = run,
  .category = "media", .type = "web_request",
  .url = FEED_URL,
  .description = "Daily trending search terms in Japan from Google Trends RSS",
  .free_tier = 1,
};
REGISTER_SOURCE(google_trends_jp_def)
