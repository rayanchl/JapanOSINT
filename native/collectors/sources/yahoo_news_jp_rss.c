/* collectors/social/sources/yahoo_news_jp_rss.c — port of
 * server/src/collectors/yahooNewsJpRss.js. Yahoo! JAPAN News topics RSS,
 * multi-category. JS loops FEEDS calling fetchFeed+feedItemToIntel per feed
 * with language 'ja' + tags ['news','yahoo','category:<cat>']. Ported as a
 * per-feed rss_collect loop carrying each category tag. The intel envelope
 * and category-hashed uid refinement are not portable through the shared
 * rss_collect helper; uid precedence guid→link→sha1(title|pubDate) mirrors
 * intelUid for the common case. */
#include "../../source.h"
#include "../../lib/rss_atom.h"
#include <stdio.h>

struct feed { const char *url; const char *tags; };

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const struct feed FEEDS[] = {
    { "https://news.yahoo.co.jp/rss/topics/top-picks.xml", "[\"news\",\"yahoo\",\"category:top-picks\"]" },
    { "https://news.yahoo.co.jp/rss/topics/domestic.xml",  "[\"news\",\"yahoo\",\"category:domestic\"]" },
    { "https://news.yahoo.co.jp/rss/topics/world.xml",     "[\"news\",\"yahoo\",\"category:world\"]" },
    { "https://news.yahoo.co.jp/rss/topics/business.xml",  "[\"news\",\"yahoo\",\"category:business\"]" },
    { "https://news.yahoo.co.jp/rss/topics/it.xml",        "[\"news\",\"yahoo\",\"category:it\"]" },
  };
  int total = 0, ok = 0;
  for (unsigned i = 0; i < sizeof(FEEDS) / sizeof(FEEDS[0]); i++) {
    int n = rss_collect(ctx, sink, FEEDS[i].url, "ja", FEEDS[i].tags);
    if (n >= 0) { total += n; ok = 1; }
  }
  fprintf(stderr, "[yahoo-news-jp-rss] emitted %d\n", total);
  return ok ? 0 : -1;
}

static const source_def yahoo_news_jp_rss_def = {
  .id = "yahoo-news-jp-rss", .collector = "social",
  .name = "Yahoo! Japan News RSS", .name_ja = "Yahoo!ニュース RSS",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(yahoo_news_jp_rss_def)
