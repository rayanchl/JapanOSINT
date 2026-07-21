/* port of server/src/collectors/nhkWorldRss.js */
#include "../../source.h"
#include "../../lib/rss_atom.h"
static int run(const source_ctx *c, intel_sink *s) {
  return rss_collect(c, s, "https://www3.nhk.or.jp/nhkworld/en/news/feeds/",
                     "en", "[\"news\",\"nhk-world\"]") >= 0 ? 0 : -1; }
static const source_def nhk_world_rss_def = {
  .id="nhk-world-rss", .collector="news", .name="NHK World",
  .name_ja="NHKワールド",  .update_interval_sec=1800, .run=run };
REGISTER_SOURCE(nhk_world_rss_def)
