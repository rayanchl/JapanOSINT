/* Science / space / technology feeds (real RSS) — emerging-tech and dual-use research signal, via rss_collect. */
#include "source.h"
#include "lib/rss_atom.h"

#include "_source_macros.inc"

RSSX(sci_esa_topnews, "esa-topnews", "ESA Top News", "ESA Top News", "science_press", "news",
  "https://www.esa.int/rssfeed/TopNews", "en", "[\"science\",\"space\",\"research\"]", 7200,
  "ESA Top News — space science/technology feed");

RSSX(sci_nature_news, "nature-news", "Nature News", "Nature News", "science_press", "news",
  "https://www.nature.com/nature.rss", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "Nature News — science science/technology feed");

RSSX(sci_science_aaas, "science-aaas", "Science (AAAS) News", "Science (AAAS) News", "science_press", "news",
  "https://www.science.org/rss/news_current.xml", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "Science (AAAS) News — science science/technology feed");

RSSX(sci_mit_tech_review, "mit-tech-review", "MIT Technology Review", "MIT Technology Review", "science_press", "news",
  "https://www.technologyreview.com/feed/", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "MIT Technology Review — tech science/technology feed");

RSSX(sci_ieee_spectrum, "ieee-spectrum", "IEEE Spectrum", "IEEE Spectrum", "science_press", "news",
  "https://spectrum.ieee.org/rss/fulltext", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "IEEE Spectrum — tech science/technology feed");

RSSX(sci_ars_technica, "ars-technica", "Ars Technica", "Ars Technica", "science_press", "news",
  "https://feeds.arstechnica.com/arstechnica/index", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "Ars Technica — tech science/technology feed");

RSSX(sci_wired, "wired", "WIRED", "WIRED", "science_press", "news",
  "https://www.wired.com/feed/rss", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "WIRED — tech science/technology feed");

RSSX(sci_the_register, "the-register", "The Register", "The Register", "science_press", "news",
  "https://www.theregister.com/headlines.atom", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "The Register — tech science/technology feed");

RSSX(sci_phys_org, "phys-org", "Phys.org", "Phys.org", "science_press", "news",
  "https://phys.org/rss-feed/", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "Phys.org — science science/technology feed");

RSSX(sci_sciencedaily, "sciencedaily", "ScienceDaily", "ScienceDaily", "science_press", "news",
  "https://www.sciencedaily.com/rss/all.xml", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "ScienceDaily — science science/technology feed");

RSSX(sci_space_com, "space-com", "Space.com", "Space.com", "science_press", "news",
  "https://www.space.com/feeds/all", "en", "[\"science\",\"space\",\"research\"]", 7200,
  "Space.com — space science/technology feed");

RSSX(sci_defense_one, "defense-one", "Defense One", "Defense One", "science_press", "news",
  "https://www.defenseone.com/rss/all/", "en", "[\"science\",\"defense\",\"research\"]", 7200,
  "Defense One — defense science/technology feed");

RSSX(sci_mit_news, "mit-news", "MIT News", "MIT News", "science_press", "news",
  "https://news.mit.edu/rss/feed", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "MIT News — science science/technology feed");

RSSX(sci_quanta, "quanta", "Quanta Magazine", "Quanta Magazine", "science_press", "news",
  "https://www.quantamagazine.org/feed/", "en", "[\"science\",\"science\",\"research\"]", 7200,
  "Quanta Magazine — science science/technology feed");

RSSX(sci_hn_frontpage, "hn-frontpage", "Hacker News Front Page", "Hacker News Front Page", "science_press", "news",
  "https://hnrss.org/frontpage", "en", "[\"science\",\"tech\",\"research\"]", 7200,
  "Hacker News Front Page — tech science/technology feed");
