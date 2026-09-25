/* Think-tank / research-institute analysis feeds (best-effort real RSS) — geopolitical and security analysis, via rss_collect. */

/* 2026-09-08 EMITS_NOTHING triage of the 11 dead rows in this file. Every URL below was
   fetched live from this host with a full browser User-Agent and browser Accept headers.
   Two were repaired in place (see the per-row notes on tank_rusi and tank_sipri).
   The other nine are diagnosed and NOT repairable by changing the URL:

     brookings        302 -> "/" (x-redirect-by: Safe Redirect Manager). The feed was
                      retired; the homepage's own link rel=alternate still names the dead
                      /feed/. No replacement is advertised.
     carnegie-endow   client-rendered app: /posts/rss/, /rss, /feed and /rss/pubs all
     carnegie-china   return the same ~69 KB HTML shell with HTTP 200; /china/rss/ too.
                      No link rel=alternate anywhere on the site.
     cnas             /rss, /feed, /rss/publications, /press/rss all 404 (Craft CMS);
                      homepage advertises no feed.
     wilson-center    /rss.xml, /rss, /feed, /rss/articles all 404; no feed advertised.
     chatham-house    HTTP 403, Cloudflare "Attention Required" interstitial.
     fpri             HTTP 403, Cloudflare "Attention Required" interstitial.
     iiss             HTTP 403, Cloudflare "Attention Required" on /rss, /rss.xml, /feed.
     mei              HTTP 403, Cloudflare "Just a moment" JS challenge.

   The four 403s are bot walls, not dead paths — the URL may well still be right, and a
   fetch path that can pass a Cloudflare challenge would revive them unchanged. The five
   404/redirect rows need a non-RSS collector (site scrape or CMS JSON API) instead. */
#include "source.h"
#include "lib/rss_atom.h"

#include "_source_macros.inc"

RSSX(tank_csis, "csis", "CSIS Analysis", "CSIS Analysis", "osint", "news",
  "https://www.csis.org/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
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

/* 2026-09-08 EMITS_NOTHING fix: rusi.org/rss.xml 404s (the site moved to Gatsby and
   serves an SPA shell for unknown paths). The homepage advertises four real feeds via
   link rel=alternate; whats-new.xml is the superset (commentary + publications + events).
   Verified live: HTTP 200, application/xml, RSS 2.0 with populated item elements. */
/* NOT re-pointed at whats-new.xml after all. That feed is already collected by
 * `sec-rusi-whatsnew` (collectors/feed/generated/vsrc_geopolitics_2.c), so
 * moving this row there would have two registered sources fetching one
 * endpoint on their own schedules and storing the same items under two
 * source_ids — `make lint-sources` caught it as a dup-endpoint regression.
 * The old /rss.xml really is gone, so this row stays dead and is reported as
 * such; the CONTENT is not lost, it arrives via sec-rusi-whatsnew. Retire this
 * row or alias it to that id — both are decisions, not repairs. */
RSSX(tank_rusi, "rusi", "RUSI", "RUSI", "osint", "news",
  "https://www.rusi.org/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "RUSI — geopolitical/security research and analysis");

RSSX(tank_iiss, "iiss", "IISS", "IISS", "osint", "news",
  "https://www.iiss.org/rss", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "IISS — geopolitical/security research and analysis");

/* 2026-09-08 EMITS_NOTHING fix: /rss.xml 404s. SIPRI's own /rss landing page links to
   /rss/combined.xml, which is the real feed. Verified live: HTTP 200,
   application/rss+xml, 10 populated item elements. */
RSSX(tank_sipri, "sipri", "SIPRI", "SIPRI", "osint", "news",
  "https://www.sipri.org/rss/combined.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
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
  "https://merics.org/en/rss", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
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
  "https://www.hudson.org/rss.xml", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
  "Hudson Institute — geopolitical/security research and analysis");

RSSX(tank_heritage, "heritage", "Heritage Foundation", "Heritage Foundation", "osint", "news",
  "https://www.heritage.org/rss", "en", "[\"osint\",\"think-tank\",\"analysis\"]", 10800,
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
