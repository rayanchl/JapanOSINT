/* port of server/src/collectors/nhkNewsRss.js */
#include "source.h"
#include "lib/rss_atom.h"
static int run(const source_ctx *c, intel_sink *s) {
  return rss_collect(c, s, "https://www3.nhk.or.jp/rss/news/cat0.xml",
                     "ja", "[\"news\",\"nhk\"]") >= 0 ? 0 : -1; }
static const source_def nhk_news_rss_def = {
  .id="nhk-news-rss", .collector="news", .name="NHK News",
  .name_ja="NHKニュース",  .update_interval_sec=900, .run=run };
REGISTER_SOURCE(nhk_news_rss_def)
