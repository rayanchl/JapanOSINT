/* collectors/social/sources/jp_news_rss.c — port of server/src/collectors/jpNewsRss.js
 * Aggregated JP news / government RSS bundle. JS loops FEEDS calling
 * fetchFeed+feedItemToIntel per feed with per-feed language + tags
 * ['news','publisher:<name>']. Ported faithfully as a per-feed rss_collect
 * loop; each feed carries its own publisher tag. The intel-envelope/_meta and
 * the publisher-hashed uid refinement (intelHashKey) are not portable through
 * the shared rss_collect helper (no uid-override hook) — uid precedence
 * guid→link→sha1(title|pubDate) mirrors intelUid for the common case. */
#include "source.h"
#include "lib/rss_atom.h"
#include <stdio.h>

struct feed { const char *url; const char *lang; const char *tags; };

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const struct feed FEEDS[] = {
    { "https://asia.nikkei.com/rss/feed/nar",     "en", "[\"news\",\"publisher:Nikkei Asia\"]" },
    { "https://www.japantimes.co.jp/feed/",       "en", "[\"news\",\"publisher:Japan Times\"]" },
    { "https://www3.nhk.or.jp/rss/news/cat0.xml", "ja", "[\"news\",\"publisher:NHK World (top)\"]" },
    { "https://jp.reuters.com/rss",               "ja", "[\"news\",\"publisher:Reuters Japan\"]" },
    { "https://japan.kantei.go.jp/rss.xml",       "en", "[\"news\",\"publisher:Kantei (PMO)\"]" },
    { "https://www.ndl.go.jp/en/news/index.rss",  "en", "[\"news\",\"publisher:NDL Press Releases\"]" },
  };
  int total = 0, ok = 0;
  for (unsigned i = 0; i < sizeof(FEEDS) / sizeof(FEEDS[0]); i++) {
    int n = rss_collect(ctx, sink, FEEDS[i].url, FEEDS[i].lang, FEEDS[i].tags);
    if (n >= 0) { total += n; ok = 1; }
  }
  fprintf(stderr, "[jp-news-rss] emitted %d\n", total);
  return ok ? 0 : -1;
}

static const source_def jp_news_rss_def = {
  .id = "jp-news-rss", .collector = "social",
  .name = "JP news RSS bundle", .name_ja = "日本ニュース RSS バンドル",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(jp_news_rss_def)
