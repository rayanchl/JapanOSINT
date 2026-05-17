/* collectors/safety/sources/saigai_info.c
 * Port of server/src/collectors/saigaiInfo.js — JMA public disaster-
 * prevention Atom feed (extra.xml). Each <entry> → one intel item. The
 * shared rss_atom toolkit ports intelHelpers.parseFeed + feedItemToIntel
 * + intelUid (uid precedence guid → link → sha1(title|pubDate), matching
 * the JS intelUid(SOURCE_ID,id,link,intelHashKey(title,updated))).
 * Honest empty (rss_collect → -1) on fetch/parse failure (rule 8). */
#include "../../../source.h"
#include "../../../lib/rss_atom.h"
#include <stdio.h>

#define FEED_URL "https://www.data.jma.go.jp/developer/xml/feed/extra.xml"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = rss_collect(ctx, sink, FEED_URL, "ja",
                       "[\"disaster\",\"jma\",\"saigai\"]");
  fprintf(stderr, "[saigai-info] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def saigai_info_def = {
  .id = "saigai-info", .collector = "safety",
  .name = "Cabinet Office Disaster", .name_ja = "内閣府 災害情報",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(saigai_info_def)
