/* Regional OSINT outlets for gap regions (Africa, Caucasus, Central/Emerging Europe, SE Asia, Gulf, LatAm, Caribbean, Pacific) — real RSS, via rss_collect. */
#include "source.h"
#include "lib/rss_atom.h"

#include "_source_macros.inc"

RSSX(reg_allafrica, "allafrica", "AllAfrica Headlines", "AllAfrica Headlines", "osint", "news",
  "https://allafrica.com/tools/headlines/rdf/latest/headlines.rdf", "en", "[\"news\",\"africa\",\"regional-outlet\"]", 3600,
  "AllAfrica Headlines — regional coverage (africa)");

RSSX(reg_iss_africa, "iss-africa", "ISS Africa", "ISS Africa", "osint", "news",
  "https://issafrica.org/rss/recent.xml", "en", "[\"news\",\"africa\",\"regional-outlet\"]", 3600,
  "ISS Africa — regional coverage (africa)");

/* audit-09: /section/politics/feed/ sits behind Cloudflare (403 "Just a
 * moment"); the site-wide feed is served without the challenge. */
RSSX(reg_the_continent, "the-continent", "The Africa Report", "The Africa Report", "osint", "news",
  "https://www.theafricareport.com/feed/", "en", "[\"news\",\"africa\",\"regional-outlet\"]", 3600,
  "The Africa Report — regional coverage (africa)");

/* audit-09: Semafor retired the per-vertical feeds (404). Only the site-wide
 * rss.xml exists, so this is no longer Africa-only — labelled accordingly
 * rather than tagging global copy as African. */
RSSX(reg_semafor_africa_2, "semafor-africa-2", "Semafor (all verticals)", "Semafor (all verticals)", "osint", "news",
  "https://www.semafor.com/rss.xml", "en", "[\"news\",\"global\",\"regional-outlet\"]", 3600,
  "Semafor — all verticals (the Africa-only feed was retired upstream)");

RSSX(reg_oc_media, "oc-media", "OC Media (Caucasus)", "OC Media (Caucasus)", "osint", "news",
  "https://oc-media.org/feed/", "en", "[\"news\",\"caucasus\",\"regional-outlet\"]", 3600,
  "OC Media (Caucasus) — regional coverage (caucasus)");

RSSX(reg_bne_intellinews, "bne-intellinews", "bne IntelliNews", "bne IntelliNews", "osint", "news",
  "https://www.intellinews.com/feed", "en", "[\"news\",\"emerging-europe\",\"regional-outlet\"]", 3600,
  "bne IntelliNews — regional coverage (emerging-europe)");

RSSX(reg_emerging_europe, "emerging-europe", "Emerging Europe", "Emerging Europe", "osint", "news",
  "https://emerging-europe.com/feed/", "en", "[\"news\",\"emerging-europe\",\"regional-outlet\"]", 3600,
  "Emerging Europe — regional coverage (emerging-europe)");

RSSX(reg_novaya_europe, "novaya-europe", "Novaya Gazeta Europe", "Novaya Gazeta Europe", "osint", "news",
  "https://novayagazeta.eu/feed/rss", "en", "[\"news\",\"russia\",\"regional-outlet\"]", 3600,
  "Novaya Gazeta Europe — regional coverage (russia)");

RSSX(reg_meduza_2, "meduza-2", "Meduza Features", "Meduza Features", "osint", "news",
  "https://meduza.io/rss/en/all", "en", "[\"news\",\"russia\",\"regional-outlet\"]", 3600,
  "Meduza Features — regional coverage (russia)");

/* audit-09: rss2.xml 404s. The site's live endpoint is the Arc outbound feed
 * below — it answers 200 but currently carries ZERO items (BenarNews stopped
 * publishing). Kept pointed at the real endpoint so the source reports an
 * honest empty instead of erroring and being quarantined. */
RSSX(reg_benar_news, "benar-news", "BenarNews (SE Asia)", "BenarNews (SE Asia)", "osint", "news",
  "https://www.benarnews.org/arc/outboundfeeds/english/rss/", "en", "[\"news\",\"southeast-asia\",\"regional-outlet\"]", 3600,
  "BenarNews (SE Asia) — regional coverage (southeast-asia)");

RSSX(reg_frontier_myanmar, "frontier-myanmar", "Frontier Myanmar", "Frontier Myanmar", "osint", "news",
  "https://www.frontiermyanmar.net/en/feed/", "en", "[\"news\",\"myanmar\",\"regional-outlet\"]", 3600,
  "Frontier Myanmar — regional coverage (myanmar)");

RSSX(reg_the_wire_india, "the-wire-india", "The Wire (India)", "The Wire (India)", "osint", "news",
  "https://thewire.in/rss", "en", "[\"news\",\"india\",\"regional-outlet\"]", 3600,
  "The Wire (India) — regional coverage (india)");

/* audit-09: scroll.in/feeds/all.rss 404s; the outlet's published feed is the
 * FeedBurner one linked from the site. */
RSSX(reg_scroll_in, "scroll-in", "Scroll.in (India)", "Scroll.in (India)", "osint", "news",
  "https://feeds.feedburner.com/ScrollinArticles", "en", "[\"news\",\"india\",\"regional-outlet\"]", 3600,
  "Scroll.in (India) — regional coverage (india)");

RSSX(reg_the_new_arab, "the-new-arab", "The New Arab", "The New Arab", "osint", "news",
  "https://www.newarab.com/rss", "en", "[\"news\",\"middle-east\",\"regional-outlet\"]", 3600,
  "The New Arab — regional coverage (middle-east)");

RSSX(reg_amwaj_media, "amwaj-media", "Amwaj.media (Gulf/Iran)", "Amwaj.media (Gulf/Iran)", "osint", "news",
  "https://amwaj.media/rss", "en", "[\"news\",\"middle-east\",\"regional-outlet\"]", 3600,
  "Amwaj.media (Gulf/Iran) — regional coverage (middle-east)");

/* audit-09: /investigations/feed/ now 302s to a single article page (0 items).
 * The site feed is /feed/. */
RSSX(reg_insight_crime_2, "insight-crime-2", "InSight Crime", "InSight Crime", "osint", "news",
  "https://insightcrime.org/feed/", "en", "[\"news\",\"latam\",\"regional-outlet\"]", 3600,
  "InSight Crime — regional coverage (latam)");

RSSX(reg_dialogo_americas, "dialogo-americas", "Diálogo Américas", "Diálogo Américas", "osint", "news",
  "https://dialogo-americas.com/feed/", "en", "[\"news\",\"latam\",\"regional-outlet\"]", 3600,
  "Diálogo Américas — regional coverage (latam)");

RSSX(reg_caribbean_news, "caribbean-news", "Caribbean National Weekly", "Caribbean National Weekly", "osint", "news",
  "https://www.caribbeannationalweekly.com/feed/", "en", "[\"news\",\"caribbean\",\"regional-outlet\"]", 3600,
  "Caribbean National Weekly — regional coverage (caribbean)");

RSSX(reg_rnz_pacific, "rnz-pacific", "RNZ Pacific", "RNZ Pacific", "osint", "news",
  "https://www.rnz.co.nz/rss/pacific.xml", "en", "[\"news\",\"pacific\",\"regional-outlet\"]", 3600,
  "RNZ Pacific — regional coverage (pacific)");

RSSX(reg_islands_business, "islands-business", "Islands Business (Pacific)", "Islands Business (Pacific)", "osint", "news",
  "https://islandsbusiness.com/feed/", "en", "[\"news\",\"pacific\",\"regional-outlet\"]", 3600,
  "Islands Business (Pacific) — regional coverage (pacific)");

/* audit-09: the per-tag feed 404s after the Next.js rebuild. The only feed the
 * site advertises (<link rel=alternate>) is the news-archive one. */
RSSX(reg_kyiv_indep_3, "kyiv-indep-3", "Kyiv Independent", "Kyiv Independent", "osint", "news",
  "https://kyivindependent.com/news-archive/rss/", "en", "[\"news\",\"ukraine\",\"regional-outlet\"]", 3600,
  "Kyiv Independent — regional coverage (ukraine)");

/* audit-09: the /category/investigations/ feed 404s; the site feed works. */
RSSX(reg_balkan_insight_2, "balkan-insight-2", "BIRN Balkan Insight", "BIRN Balkan Insight", "osint", "news",
  "https://balkaninsight.com/feed/", "en", "[\"news\",\"balkans\",\"regional-outlet\"]", 3600,
  "BIRN Balkan Insight — regional coverage (balkans)");

RSSX(reg_hong_kong_fp, "hong-kong-fp", "Hong Kong Free Press", "Hong Kong Free Press", "osint", "news",
  "https://hongkongfp.com/feed/", "en", "[\"news\",\"hong-kong\",\"regional-outlet\"]", 3600,
  "Hong Kong Free Press — regional coverage (hong-kong)");

RSSX(reg_the_print_india, "the-print-india", "ThePrint (India)", "ThePrint (India)", "osint", "news",
  "https://theprint.in/feed/", "en", "[\"news\",\"india\",\"regional-outlet\"]", 3600,
  "ThePrint (India) — regional coverage (india)");
