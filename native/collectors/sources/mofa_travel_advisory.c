/* collectors/government/sources/mofa_travel_advisory.c — port of
 * server/src/collectors/mofaTravelAdvisory.js. MOFA 海外安全ホームページ
 * travel advisories: three RSS feeds (spotinfo, dangerinfo, info), each entry
 * → intel with language 'ja' and tags ['advisory','mofa','travel',<danger|
 * spot>] where the 4th tag is 'danger' iff the feed url ends 'dangerinfo.xml'
 * else 'spot' (mirrors JS). JS also sets properties.feed_url and slices to 200
 * combined; those refinements are not portable through the shared rss_collect
 * helper. The _meta/seed envelope is dropped. */
#include "source.h"
#include "lib/rss_atom.h"
#include <stdio.h>

struct feed { const char *url; const char *tags; };

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const struct feed FEEDS[] = {
    { "https://www.anzen.mofa.go.jp/rss/spotinfo.xml",
      "[\"advisory\",\"mofa\",\"travel\",\"spot\"]" },
    { "https://www.anzen.mofa.go.jp/rss/dangerinfo.xml",
      "[\"advisory\",\"mofa\",\"travel\",\"danger\"]" },
    { "https://www.anzen.mofa.go.jp/rss/info.xml",
      "[\"advisory\",\"mofa\",\"travel\",\"spot\"]" },
  };
  int total = 0, ok = 0;
  for (unsigned i = 0; i < sizeof(FEEDS) / sizeof(FEEDS[0]); i++) {
    int n = rss_collect(ctx, sink, FEEDS[i].url, "ja", FEEDS[i].tags);
    if (n >= 0) { total += n; ok = 1; }
  }
  fprintf(stderr, "[mofa-travel-advisory] emitted %d\n", total);
  return ok ? 0 : -1;
}

static const source_def mofa_travel_advisory_def = {
  .id = "mofa-travel-advisory", .collector = "government",
  .name = "MOFA Travel Advisory", .name_ja = "外務省 海外安全情報",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(mofa_travel_advisory_def)
