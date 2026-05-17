/* port of server/src/collectors/kyodoRss.js (createRssCollector) */
#include "../../../source.h"
#include "../../../lib/rss_atom.h"
static int run(const source_ctx *c, intel_sink *s) {
  return rss_collect(c, s, "https://english.kyodonews.net/rss/news.xml",
                     "en", "[\"news\",\"kyodo\"]") >= 0 ? 0 : -1; }
static const source_def kyodo_rss_def = {
  .id="kyodo-rss", .collector="news", .name="Kyodo News",
  .name_ja="共同通信",  .update_interval_sec=1800, .run=run };
REGISTER_SOURCE(kyodo_rss_def)
