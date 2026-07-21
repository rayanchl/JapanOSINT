/* collectors/sources/intel_gov_disaster.c — real-time government, disaster and
 * geo-hazard open feeds. Official advisories (travel/security, law enforcement),
 * multilateral humanitarian updates, and live geophysical alert streams (global
 * earthquakes, disaster events, volcanic activity). Real RSS/Atom via
 * rss_collect; each becomes an FTS-indexed intel row the alert hooks can raise
 * on. These extend the JP-domestic disaster collectors (jma, saigai, bosai) to
 * worldwide coverage. Scheduled on tighter intervals for the live hazard feeds. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#define COLL "government"

#define RSS_I(SYM, ID, NAME, NAMEJA, CAT, URL, LANG, TAGS, DESC, IVAL)       \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

#define RSS(SYM, ID, NAME, NAMEJA, CAT, URL, LANG, TAGS, DESC) \
  RSS_I(SYM, ID, NAME, NAMEJA, CAT, URL, LANG, TAGS, DESC, 3600)

/* --- Live geo-hazard streams (frequent polling) --- */
RSS_I(gd_gdacs, "gdacs", "GDACS Global Disaster Alerts",
  "GDACS 災害アラート", "environment",
  "https://www.gdacs.org/xml/rss.xml", "en",
  "[\"disaster\",\"alert\",\"global\",\"osint\"]",
  "Global Disaster Alert and Coordination System — earthquakes, cyclones, floods, volcanoes",
  900);

RSS_I(gd_usgs_sig, "usgs-quake-significant", "USGS Significant Earthquakes (7d)",
  "USGS 主要地震", "environment",
  "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/significant_week.atom", "en",
  "[\"earthquake\",\"usgs\",\"global\"]",
  "USGS significant earthquakes worldwide, past 7 days",
  900);

RSS_I(gd_usgs_25, "usgs-quake-m25-day", "USGS M2.5+ Earthquakes (24h)",
  "USGS M2.5+ 地震", "environment",
  "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/2.5_day.atom", "en",
  "[\"earthquake\",\"usgs\",\"global\"]",
  "USGS magnitude 2.5+ earthquakes worldwide, past 24 hours",
  600);

RSS_I(gd_usgs_45, "usgs-quake-m45-week", "USGS M4.5+ Earthquakes (7d)",
  "USGS M4.5+ 地震", "environment",
  "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_week.atom", "en",
  "[\"earthquake\",\"usgs\",\"global\"]",
  "USGS magnitude 4.5+ earthquakes worldwide, past 7 days",
  1800);

RSS_I(gd_volcano, "gvp-volcano-activity", "Smithsonian Weekly Volcanic Activity",
  "スミソニアン 火山活動週報", "environment",
  "https://volcano.si.edu/news/WeeklyVolcanoRSS.xml", "en",
  "[\"volcano\",\"hazard\",\"global\"]",
  "Smithsonian Global Volcanism Program — weekly volcanic activity report",
  21600);

RSS_I(gd_nws, "nws-alerts-us", "US NWS Weather Alerts",
  "米国 気象警報", "environment",
  "https://alerts.weather.gov/cap/us.php?x=0", "en",
  "[\"weather\",\"alert\",\"usa\"]",
  "US National Weather Service active CAP alerts (all hazards, nationwide)",
  900);

RSS_I(gd_reliefweb, "reliefweb", "ReliefWeb Updates (UN OCHA)",
  "ReliefWeb 更新", "environment",
  "https://reliefweb.int/updates/rss.xml", "en",
  "[\"humanitarian\",\"disaster\",\"un\"]",
  "UN OCHA ReliefWeb — humanitarian situation reports and disaster updates",
  3600);

/* --- Government / security / law-enforcement advisories --- */
RSS(gd_state_travel, "us-state-travel", "US State Dept Travel Advisories",
  "米国務省 渡航情報", "government",
  "https://travel.state.gov/_res/rss/TAsTWs.xml", "en",
  "[\"advisory\",\"travel\",\"us-gov\"]",
  "US Department of State travel advisories and warnings by country");

RSS(gd_fcdo, "uk-fcdo-travel", "UK FCDO Travel Advice",
  "英国外務省 渡航情報", "government",
  "https://www.gov.uk/foreign-travel-advice.atom", "en",
  "[\"advisory\",\"travel\",\"uk-gov\"]",
  "UK Foreign, Commonwealth & Development Office foreign travel advice updates");

RSS(gd_fbi, "fbi-press", "FBI National Press Releases",
  "FBI 報道発表", "government",
  "https://www.fbi.gov/feeds/national-press-releases/rss.xml", "en",
  "[\"law-enforcement\",\"fbi\",\"us-gov\"]",
  "US FBI national press releases — investigations, charges and advisories");

RSS(gd_europol, "europol-news", "Europol Newsroom",
  "ユーロポール", "government",
  "https://www.europol.europa.eu/rss.xml", "en",
  "[\"law-enforcement\",\"europol\",\"eu\"]",
  "Europol newsroom — EU cross-border law-enforcement operations");

RSS(gd_ofac, "ofac-recent-actions", "US Treasury OFAC Recent Actions",
  "OFAC 制裁措置", "government",
  "https://ofac.treasury.gov/system/files/126/ofac.xml", "en",
  "[\"sanctions\",\"ofac\",\"us-gov\"]",
  "US Treasury OFAC recent sanctions actions and SDN list updates");

RSS(gd_unnews, "un-news", "UN News",
  "国連ニュース", "government",
  "https://news.un.org/feed/subscribe/en/news/all/rss.xml", "en",
  "[\"un\",\"news\",\"global\"]",
  "United Nations News — official global UN system reporting");

RSS(gd_nato, "nato-news", "NATO News",
  "NATO ニュース", "government",
  "https://www.nato.int/cps/en/natohq/news.rss", "en",
  "[\"nato\",\"defense\",\"news\"]",
  "NATO official news releases and statements");

RSS(gd_who_don, "who-outbreak-news", "WHO Disease Outbreak News",
  "WHO 疾病アウトブレイク", "health",
  "https://www.who.int/feeds/entity/csr/don/en/rss.xml", "en",
  "[\"health\",\"outbreak\",\"who\"]",
  "World Health Organization Disease Outbreak News — global health emergencies");

RSS(gd_cdc, "cdc-newsroom", "CDC Newsroom",
  "CDC ニュース", "health",
  "https://tools.cdc.gov/api/v2/resources/media/403372.rss", "en",
  "[\"health\",\"cdc\",\"usa\"]",
  "US CDC newsroom — outbreak, advisory and public-health releases");

RSS(gd_ecdc, "ecdc-threats", "ECDC Communicable Disease Threats",
  "ECDC 感染症脅威", "health",
  "https://www.ecdc.europa.eu/en/taxonomy/term/2942/feed", "en",
  "[\"health\",\"ecdc\",\"eu\"]",
  "European CDC communicable disease threat reports and risk assessments");

RSS(gd_firms, "nasa-firms-blog", "NASA Earth Observatory",
  "NASA 地球観測", "environment",
  "https://earthobservatory.nasa.gov/feeds/earth-observatory.rss", "en",
  "[\"satellite\",\"earth-observation\",\"nasa\"]",
  "NASA Earth Observatory — imagery-driven natural-event and environmental reporting");
