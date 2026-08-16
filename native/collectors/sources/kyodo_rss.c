/* port of server/src/collectors/kyodoRss.js (createRssCollector) */
#include "source.h"
#include "lib/rss_atom.h"
/* /rss/news.xml has 404'd since the site rebuild (verified: 404 text/html for
 * both the collector UA and a browser UA). The homepage still advertises one
 * feed — /list/feed/rss4kyodonews-fzone — which serves text/xml with 25
 * <item>s of English Kyodo copy. That is the surviving channel. */
static int run(const source_ctx *c, intel_sink *s) {
  return rss_collect(c, s,
                     "https://english.kyodonews.net/list/feed/rss4kyodonews-fzone",
                     "en", "[\"news\",\"kyodo\"]") >= 0 ? 0 : -1; }
static const source_def kyodo_rss_def = {
  .id="kyodo-rss", .collector="news", .name="Kyodo News",
  .name_ja="共同通信",  .update_interval_sec=1800, .run=run };
REGISTER_SOURCE(kyodo_rss_def)
