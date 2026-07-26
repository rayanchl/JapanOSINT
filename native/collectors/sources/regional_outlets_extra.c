/* Regional OSINT outlets for gap regions (Africa, Caucasus, Central/Emerging Europe, SE Asia, Gulf, LatAm, Caribbean, Pacific) — real RSS, via rss_collect. */
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

RSSX(reg_allafrica, "allafrica", "AllAfrica Headlines", "AllAfrica Headlines", "osint", "news",
  "https://allafrica.com/tools/headlines/rdf/latest/headlines.rdf", "en", "[\"news\",\"africa\",\"regional-outlet\"]", 3600,
  "AllAfrica Headlines — regional coverage (africa)");

RSSX(reg_iss_africa, "iss-africa", "ISS Africa", "ISS Africa", "osint", "news",
  "https://issafrica.org/rss/recent.xml", "en", "[\"news\",\"africa\",\"regional-outlet\"]", 3600,
  "ISS Africa — regional coverage (africa)");

RSSX(reg_the_continent, "the-continent", "The Africa Report Politics", "The Africa Report Politics", "osint", "news",
  "https://www.theafricareport.com/section/politics/feed/", "en", "[\"news\",\"africa\",\"regional-outlet\"]", 3600,
  "The Africa Report Politics — regional coverage (africa)");

RSSX(reg_semafor_africa_2, "semafor-africa-2", "Semafor Africa Signals", "Semafor Africa Signals", "osint", "news",
  "https://www.semafor.com/vertical/africa/rss.xml", "en", "[\"news\",\"africa\",\"regional-outlet\"]", 3600,
  "Semafor Africa Signals — regional coverage (africa)");

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

RSSX(reg_benar_news, "benar-news", "BenarNews (SE Asia)", "BenarNews (SE Asia)", "osint", "news",
  "https://www.benarnews.org/english/rss2.xml", "en", "[\"news\",\"southeast-asia\",\"regional-outlet\"]", 3600,
  "BenarNews (SE Asia) — regional coverage (southeast-asia)");

RSSX(reg_frontier_myanmar, "frontier-myanmar", "Frontier Myanmar", "Frontier Myanmar", "osint", "news",
  "https://www.frontiermyanmar.net/en/feed/", "en", "[\"news\",\"myanmar\",\"regional-outlet\"]", 3600,
  "Frontier Myanmar — regional coverage (myanmar)");

RSSX(reg_the_wire_india, "the-wire-india", "The Wire (India)", "The Wire (India)", "osint", "news",
  "https://thewire.in/rss", "en", "[\"news\",\"india\",\"regional-outlet\"]", 3600,
  "The Wire (India) — regional coverage (india)");

RSSX(reg_scroll_in, "scroll-in", "Scroll.in (India)", "Scroll.in (India)", "osint", "news",
  "https://scroll.in/feeds/all.rss", "en", "[\"news\",\"india\",\"regional-outlet\"]", 3600,
  "Scroll.in (India) — regional coverage (india)");

RSSX(reg_the_new_arab, "the-new-arab", "The New Arab", "The New Arab", "osint", "news",
  "https://www.newarab.com/rss", "en", "[\"news\",\"middle-east\",\"regional-outlet\"]", 3600,
  "The New Arab — regional coverage (middle-east)");

RSSX(reg_amwaj_media, "amwaj-media", "Amwaj.media (Gulf/Iran)", "Amwaj.media (Gulf/Iran)", "osint", "news",
  "https://amwaj.media/rss", "en", "[\"news\",\"middle-east\",\"regional-outlet\"]", 3600,
  "Amwaj.media (Gulf/Iran) — regional coverage (middle-east)");

RSSX(reg_insight_crime_2, "insight-crime-2", "InSight Crime Investigations", "InSight Crime Investigations", "osint", "news",
  "https://insightcrime.org/investigations/feed/", "en", "[\"news\",\"latam\",\"regional-outlet\"]", 3600,
  "InSight Crime Investigations — regional coverage (latam)");

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

RSSX(reg_kyiv_indep_3, "kyiv-indep-3", "Kyiv Independent Business", "Kyiv Independent Business", "osint", "news",
  "https://kyivindependent.com/tag/business/feed/", "en", "[\"news\",\"ukraine\",\"regional-outlet\"]", 3600,
  "Kyiv Independent Business — regional coverage (ukraine)");

RSSX(reg_balkan_insight_2, "balkan-insight-2", "BIRN Balkan Insight Investigations", "BIRN Balkan Insight Investigations", "osint", "news",
  "https://balkaninsight.com/category/investigations/feed/", "en", "[\"news\",\"balkans\",\"regional-outlet\"]", 3600,
  "BIRN Balkan Insight Investigations — regional coverage (balkans)");

RSSX(reg_hong_kong_fp, "hong-kong-fp", "Hong Kong Free Press", "Hong Kong Free Press", "osint", "news",
  "https://hongkongfp.com/feed/", "en", "[\"news\",\"hong-kong\",\"regional-outlet\"]", 3600,
  "Hong Kong Free Press — regional coverage (hong-kong)");

RSSX(reg_the_print_india, "the-print-india", "ThePrint (India)", "ThePrint (India)", "osint", "news",
  "https://theprint.in/feed/", "en", "[\"news\",\"india\",\"regional-outlet\"]", 3600,
  "ThePrint (India) — regional coverage (india)");
