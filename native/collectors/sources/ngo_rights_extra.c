/* Rights / investigative-NGO feeds (real RSS) — corruption, digital rights, press freedom, cross-border investigations, via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#include "_source_macros.inc"

RSSX(ngo_insight_crime, "insight-crime", "InSight Crime", "InSight Crime", "osint", "news",
  "https://insightcrime.org/feed/", "en", "[\"osint\",\"ngo\",\"organized-crime\"]", 10800,
  "InSight Crime — organized-crime investigations and reporting");

RSSX(ngo_citizen_lab, "citizen-lab", "Citizen Lab", "Citizen Lab", "osint", "news",
  "https://citizenlab.ca/feed/", "en", "[\"osint\",\"ngo\",\"digital-rights\"]", 10800,
  "Citizen Lab — digital-rights investigations and reporting");

RSSX(ngo_eff_deeplinks, "eff-deeplinks", "EFF Deeplinks", "EFF Deeplinks", "osint", "news",
  "https://www.eff.org/rss/updates.xml", "en", "[\"osint\",\"ngo\",\"digital-rights\"]", 10800,
  "EFF Deeplinks — digital-rights investigations and reporting");

RSSX(ngo_cpj, "cpj", "Committee to Protect Journalists", "Committee to Protect Journalists", "osint", "news",
  "https://cpj.org/feed/", "en", "[\"osint\",\"ngo\",\"press-freedom\"]", 10800,
  "Committee to Protect Journalists — press-freedom investigations and reporting");

RSSX(ngo_rsf, "rsf", "Reporters Without Borders", "Reporters Without Borders", "osint", "news",
  "https://rsf.org/en/rss.xml", "en", "[\"osint\",\"ngo\",\"press-freedom\"]", 10800,
  "Reporters Without Borders — press-freedom investigations and reporting");

RSSX(ngo_global_witness, "global-witness", "Global Witness", "Global Witness", "osint", "news",
  "https://www.globalwitness.org/en/rss/", "en", "[\"osint\",\"ngo\",\"corruption\"]", 10800,
  "Global Witness — corruption investigations and reporting");

RSSX(ngo_transparency_intl, "transparency-intl", "Transparency International", "Transparency International", "osint", "news",
  "https://www.transparency.org/en/rss", "en", "[\"osint\",\"ngo\",\"corruption\"]", 10800,
  "Transparency International — corruption investigations and reporting");

RSSX(ngo_access_now, "access-now", "Access Now", "Access Now", "osint", "news",
  "https://www.accessnow.org/feed/", "en", "[\"osint\",\"ngo\",\"digital-rights\"]", 10800,
  "Access Now — digital-rights investigations and reporting");

RSSX(ngo_article19, "article19", "ARTICLE 19", "ARTICLE 19", "osint", "news",
  "https://www.article19.org/feed/", "en", "[\"osint\",\"ngo\",\"free-expression\"]", 10800,
  "ARTICLE 19 — free-expression investigations and reporting");

RSSX(ngo_forbidden_stories, "forbidden-stories", "Forbidden Stories", "Forbidden Stories", "osint", "news",
  "https://forbiddenstories.org/feed/", "en", "[\"osint\",\"ngo\",\"investigation\"]", 10800,
  "Forbidden Stories — investigation investigations and reporting");

RSSX(ngo_freedom_house, "freedom-house", "Freedom House", "Freedom House", "osint", "news",
  "https://freedomhouse.org/rss.xml", "en", "[\"osint\",\"ngo\",\"democracy\"]", 10800,
  "Freedom House — democracy investigations and reporting");

RSSX(ngo_bellingcat_3, "bellingcat-3", "Bellingcat Long-Reads", "Bellingcat Long-Reads", "osint", "news",
  "https://www.bellingcat.com/category/resources/feed/", "en", "[\"osint\",\"ngo\",\"osint\"]", 10800,
  "Bellingcat Long-Reads — osint investigations and reporting");

RSSX(ngo_privacy_intl, "privacy-intl", "Privacy International", "Privacy International", "osint", "news",
  "https://privacyinternational.org/rss.xml", "en", "[\"osint\",\"ngo\",\"privacy\"]", 10800,
  "Privacy International — privacy investigations and reporting");
