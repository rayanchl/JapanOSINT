/* Science / space / technology feeds (real RSS) — emerging-tech and dual-use research signal, via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#include "_source_macros.inc"

RSSX(sci_esa_topnews, "esa-topnews", "ESA Top News", "ESA Top News", "osint", "news",
  "https://www.esa.int/rssfeed/TopNews", "en", "[\"science\",\"space\",\"research\"]", 7200,
  "ESA Top News — space science/technology feed");

RSSX(sci_nature_news, "nature-news", "Nature News", "Nature News", "osint", "news",
  "https://www.nature.com/nature.rss", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "Nature News — science science/technology feed");

RSSX(sci_science_aaas, "science-aaas", "Science (AAAS) News", "Science (AAAS) News", "osint", "news",
  "https://www.science.org/rss/news_current.xml", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "Science (AAAS) News — science science/technology feed");

RSSX(sci_mit_tech_review, "mit-tech-review", "MIT Technology Review", "MIT Technology Review", "osint", "news",
  "https://www.technologyreview.com/feed/", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "MIT Technology Review — tech science/technology feed");

RSSX(sci_ieee_spectrum, "ieee-spectrum", "IEEE Spectrum", "IEEE Spectrum", "osint", "news",
  "https://spectrum.ieee.org/rss/fulltext", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "IEEE Spectrum — tech science/technology feed");

RSSX(sci_ars_technica, "ars-technica", "Ars Technica", "Ars Technica", "osint", "news",
  "https://feeds.arstechnica.com/arstechnica/index", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "Ars Technica — tech science/technology feed");

RSSX(sci_wired, "wired", "WIRED", "WIRED", "osint", "news",
  "https://www.wired.com/feed/rss", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "WIRED — tech science/technology feed");

RSSX(sci_the_register, "the-register", "The Register", "The Register", "osint", "news",
  "https://www.theregister.com/headlines.atom", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "The Register — tech science/technology feed");

RSSX(sci_phys_org, "phys-org", "Phys.org", "Phys.org", "osint", "news",
  "https://phys.org/rss-feed/", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "Phys.org — science science/technology feed");

RSSX(sci_sciencedaily, "sciencedaily", "ScienceDaily", "ScienceDaily", "osint", "news",
  "https://www.sciencedaily.com/rss/all.xml", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "ScienceDaily — science science/technology feed");

RSSX(sci_space_com, "space-com", "Space.com", "Space.com", "osint", "news",
  "https://www.space.com/feeds/all", "en", "[\"science\",\"space\",\"research\"]", 7200,
  "Space.com — space science/technology feed");

RSSX(sci_defense_one, "defense-one", "Defense One", "Defense One", "osint", "news",
  "https://www.defenseone.com/rss/all/", "en", "[\"science\",\"defense\",\"research\"]", 7200,
  "Defense One — defense science/technology feed");

RSSX(sci_mit_news, "mit-news", "MIT News", "MIT News", "osint", "news",
  "https://news.mit.edu/rss/feed", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "MIT News — science science/technology feed");

RSSX(sci_quanta, "quanta", "Quanta Magazine", "Quanta Magazine", "osint", "news",
  "https://www.quantamagazine.org/feed/", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "Quanta Magazine — science science/technology feed");

RSSX(sci_hn_frontpage, "hn-frontpage", "Hacker News Front Page", "Hacker News Front Page", "osint", "news",
  "https://hnrss.org/frontpage", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "Hacker News Front Page — tech science/technology feed");
