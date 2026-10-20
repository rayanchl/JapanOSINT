/* Thematic global OSINT feeds — maritime, aviation, space, energy, health, human-rights, conflict and intelligence domains, via rss_collect. */
#include "source.h"
#include "lib/rss_atom.h"

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
#include "_source_macros.inc"

RSSX(them_gcaptain, "gcaptain", "gCaptain (Maritime)", "gCaptain (Maritime)", "trade_press", "news",
  "https://gcaptain.com/feed/", "en", "[\"osint\",\"maritime\",\"thematic\",\"global\"]", 3600,
  "Global maritime shipping and incident reporting");

RSSX(them_maritime_bulletin, "maritime-bulletin", "Maritime Bulletin", "Maritime Bulletin", "trade_press", "news",
  "https://maritimebulletin.net/feed/", "en", "[\"osint\",\"maritime\",\"thematic\",\"global\"]", 3600,
  "Ship casualties, seizures and maritime security");

RSSX(them_splash247, "splash247", "Splash 247 (Shipping)", "Splash 247 (Shipping)", "trade_press", "news",
  "https://splash247.com/feed/", "en", "[\"osint\",\"maritime\",\"thematic\",\"global\"]", 3600,
  "Global shipping industry and vessel news");

RSSX(them_avherald, "avherald", "The Aviation Herald", "The Aviation Herald", "trade_press", "news",
  "https://avherald.com/rss/", "en", "[\"osint\",\"aviation\",\"thematic\",\"global\"]", 3600,
  "Aviation incidents and accidents worldwide");

RSSX(them_flightglobal, "flightglobal", "FlightGlobal", "FlightGlobal", "trade_press", "news",
  "https://www.flightglobal.com/rss/", "en", "[\"osint\",\"aviation\",\"thematic\",\"global\"]", 3600,
  "Aviation industry and defense aerospace news");

RSSX(them_spacenews, "spacenews", "SpaceNews", "SpaceNews", "trade_press", "news",
  "https://spacenews.com/feed/", "en", "[\"osint\",\"space\",\"thematic\",\"global\"]", 3600,
  "Space industry, launches and military space");

RSSX(them_nasaspaceflight, "nasaspaceflight", "NASASpaceflight", "NASASpaceflight", "trade_press", "news",
  "https://www.nasaspaceflight.com/feed/", "en", "[\"osint\",\"space\",\"thematic\",\"global\"]", 3600,
  "Launch vehicle and orbital activity reporting");

RSSX(them_oilprice, "oilprice", "OilPrice.com (Energy)", "OilPrice.com (Energy)", "trade_press", "news",
  "https://oilprice.com/rss/main", "en", "[\"osint\",\"energy\",\"thematic\",\"global\"]", 3600,
  "Global oil, gas and energy market intelligence");

RSSX(them_rigzone, "rigzone", "Rigzone (Oil & Gas)", "Rigzone (Oil & Gas)", "trade_press", "news",
  "https://www.rigzone.com/news/rss/rigzone_latest.aspx", "en", "[\"osint\",\"energy\",\"thematic\",\"global\"]", 3600,
  "Upstream oil and gas industry news");

RSSX(them_argus_energy, "argus-energy", "Reccessary Energy", "Reccessary Energy", "trade_press", "news",
  "https://www.reccessary.com/en/news/list/re-energy?rssfeed=1", "en", "[\"osint\",\"energy\",\"thematic\",\"global\"]", 3600,
  "Asia energy transition and power markets");

RSSX(them_statnews, "statnews", "STAT News (Health)", "STAT News (Health)", "trade_press", "news",
  "https://www.statnews.com/feed/", "en", "[\"osint\",\"health\",\"thematic\",\"global\"]", 3600,
  "Global health, pharma and biotech intelligence");

RSSX(them_cidrap, "cidrap", "CIDRAP (Infectious Disease)", "CIDRAP (Infectious Disease)", "trade_press", "news",
  "https://www.cidrap.umn.edu/rss.xml", "en", "[\"osint\",\"health\",\"thematic\",\"global\"]", 3600,
  "Infectious-disease outbreak and biosecurity news");

RSSX(them_hrw_news, "hrw-news", "Human Rights Watch", "Human Rights Watch", "trade_press", "news",
  "https://www.hrw.org/rss/news", "en", "[\"osint\",\"rights\",\"thematic\",\"global\"]", 3600,
  "Human-rights investigations worldwide");

RSSX(them_amnesty, "amnesty", "Amnesty International", "Amnesty International", "trade_press", "news",
  "https://www.amnesty.org/en/rss/", "en", "[\"osint\",\"rights\",\"thematic\",\"global\"]", 3600,
  "Human-rights abuse reporting and campaigns");

RSSX(them_crisis_group, "crisis-group", "International Crisis Group", "International Crisis Group", "trade_press", "news",
  "https://www.crisisgroup.org/rss.xml", "en", "[\"osint\",\"conflict\",\"thematic\",\"global\"]", 3600,
  "Conflict analysis and early-warning briefings");

RSSX(them_acled_blog, "acled-blog", "ACLED Analysis", "ACLED Analysis", "trade_press", "news",
  "https://acleddata.com/feed/", "en", "[\"osint\",\"conflict\",\"thematic\",\"global\"]", 3600,
  "Armed conflict and political violence analysis");

RSSX(them_small_wars, "small-wars", "Small Wars Journal", "Small Wars Journal", "trade_press", "news",
  /* /rss.xml 404s since the WordPress migration; /feed/ is the live feed */
  "https://smallwarsjournal.com/feed/", "en", "[\"osint\",\"conflict\",\"thematic\",\"global\"]", 3600,
  "Irregular warfare and counterinsurgency analysis");

RSSX(them_bellingcat_2, "bellingcat-2", "Bellingcat Investigations", "Bellingcat Investigations", "trade_press", "news",
  "https://www.bellingcat.com/feed/", "en", "[\"osint\",\"osint\",\"thematic\",\"global\"]", 3600,
  "Open-source investigation and geolocation");

RSSX(them_intelnews, "intelnews", "IntelNews", "IntelNews", "trade_press", "news",
  "https://intelnews.org/feed/", "en", "[\"osint\",\"intelligence\",\"thematic\",\"global\"]", 3600,
  "Intelligence-community and espionage reporting");

RSSX(them_lawfare, "lawfare", "Lawfare", "Lawfare", "trade_press", "news",
  "https://www.lawfaremedia.org/feed/rss.xml", "en", "[\"osint\",\"security\",\"thematic\",\"global\"]", 3600,
  "National-security law and policy analysis");
