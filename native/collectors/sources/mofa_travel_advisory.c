/* collectors/government/sources/mofa_travel_advisory.c — port of
 * server/src/collectors/mofaTravelAdvisory.js. MOFA 海外安全ホームページ
 * travel advisories. The three original feeds (spotinfo.xml, dangerinfo.xml,
 * info.xml) now 404 to a maintenance placeholder page (confirmed live) — MOFA
 * consolidated them into one feed, rss/news.xml (confirmed live, real
 * <item>s, e.g. category "スポット情報"). That feed carries no machine-stable
 * spot/danger split rss_collect can key off, so the old 4th tag is dropped
 * rather than guessed; each entry still gets 'advisory','mofa','travel'. */
#include "source.h"
#include "lib/rss_atom.h"
#include <stdio.h>

struct feed { const char *url; const char *tags; };

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const struct feed FEEDS[] = {
    { "https://www.anzen.mofa.go.jp/rss/news.xml",
      "[\"advisory\",\"mofa\",\"travel\"]" },
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
