/* Think-tank / research-institute analysis feeds (best-effort real RSS) — geopolitical and security analysis, via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#define RSSX(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC)  \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

RSSX(tank_csis, "csis", "CSIS Analysis", "CSIS Analysis", "osint", "news",
  "https://www.csis.org/analysis/feed", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "CSIS Analysis — geopolitical/security research and analysis");

RSSX(tank_brookings, "brookings", "Brookings Institution", "Brookings Institution", "osint", "news",
  "https://www.brookings.edu/feed/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Brookings Institution — geopolitical/security research and analysis");

RSSX(tank_atlantic_council, "atlantic-council", "Atlantic Council", "Atlantic Council", "osint", "news",
  "https://www.atlanticcouncil.org/feed/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Atlantic Council — geopolitical/security research and analysis");

RSSX(tank_carnegie_endow, "carnegie-endow", "Carnegie Endowment", "Carnegie Endowment", "osint", "news",
  "https://carnegieendowment.org/posts/rss/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Carnegie Endowment — geopolitical/security research and analysis");

RSSX(tank_chatham_house, "chatham-house", "Chatham House", "Chatham House", "osint", "news",
  "https://www.chathamhouse.org/rss/publications.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Chatham House — geopolitical/security research and analysis");

RSSX(tank_rusi, "rusi", "RUSI", "RUSI", "osint", "news",
  "https://rusi.org/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "RUSI — geopolitical/security research and analysis");

RSSX(tank_iiss, "iiss", "IISS", "IISS", "osint", "news",
  "https://www.iiss.org/rss", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "IISS — geopolitical/security research and analysis");

RSSX(tank_sipri, "sipri", "SIPRI", "SIPRI", "osint", "news",
  "https://www.sipri.org/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "SIPRI — geopolitical/security research and analysis");

RSSX(tank_fpri, "fpri", "Foreign Policy Research Institute", "Foreign Policy Research Institute", "osint", "news",
  "https://www.fpri.org/feed/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Foreign Policy Research Institute — geopolitical/security research and analysis");

RSSX(tank_wilson_center, "wilson-center", "Wilson Center", "Wilson Center", "osint", "news",
  "https://www.wilsoncenter.org/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Wilson Center — geopolitical/security research and analysis");

RSSX(tank_cnas, "cnas", "Center for a New American Security", "Center for a New American Security", "osint", "news",
  "https://www.cnas.org/rss", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Center for a New American Security — geopolitical/security research and analysis");

RSSX(tank_ecfr, "ecfr", "European Council on Foreign Relations", "European Council on Foreign Relations", "osint", "news",
  "https://ecfr.eu/feed/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "European Council on Foreign Relations — geopolitical/security research and analysis");

RSSX(tank_merics, "merics", "MERICS (China)", "MERICS (China)", "osint", "news",
  "https://merics.org/en/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "MERICS (China) — geopolitical/security research and analysis");

RSSX(tank_lowy_interpreter, "lowy-interpreter", "Lowy Institute Interpreter", "Lowy Institute Interpreter", "osint", "news",
  "https://www.lowyinstitute.org/the-interpreter/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Lowy Institute Interpreter — geopolitical/security research and analysis");

RSSX(tank_stimson, "stimson", "Stimson Center", "Stimson Center", "osint", "news",
  "https://www.stimson.org/feed/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Stimson Center — geopolitical/security research and analysis");

RSSX(tank_jamestown, "jamestown", "Jamestown Foundation", "Jamestown Foundation", "osint", "news",
  "https://jamestown.org/feed/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Jamestown Foundation — geopolitical/security research and analysis");

RSSX(tank_cepa, "cepa", "Center for European Policy Analysis", "Center for European Policy Analysis", "osint", "news",
  "https://cepa.org/feed/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Center for European Policy Analysis — geopolitical/security research and analysis");

RSSX(tank_hudson, "hudson", "Hudson Institute", "Hudson Institute", "osint", "news",
  "https://www.hudson.org/rss", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Hudson Institute — geopolitical/security research and analysis");

RSSX(tank_heritage, "heritage", "Heritage Foundation", "Heritage Foundation", "osint", "news",
  "https://www.heritage.org/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Heritage Foundation — geopolitical/security research and analysis");

RSSX(tank_rand_pubs, "rand-pubs", "RAND Corporation", "RAND Corporation", "osint", "news",
  "https://www.rand.org/pubs/new.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "RAND Corporation — geopolitical/security research and analysis");

RSSX(tank_csis_china, "csis-china", "CSIS ChinaPower", "CSIS ChinaPower", "osint", "news",
  "https://chinapower.csis.org/feed/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "CSIS ChinaPower — geopolitical/security research and analysis");

RSSX(tank_mei, "mei", "Middle East Institute", "Middle East Institute", "osint", "news",
  "https://www.mei.edu/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Middle East Institute — geopolitical/security research and analysis");

RSSX(tank_carnegie_china, "carnegie-china", "Carnegie China", "Carnegie China", "osint", "news",
  "https://carnegieendowment.org/china/rss/", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Carnegie China — geopolitical/security research and analysis");

RSSX(tank_gmfus, "gmfus", "German Marshall Fund", "German Marshall Fund", "osint", "news",
  "https://www.gmfus.org/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "German Marshall Fund — geopolitical/security research and analysis");
