/* collectors/sources/intl_feeds_batch50.c
 * Batch 50 — 38 international (cross-border OSINT) government / institutional RSS-Atom feeds.
 *
 * Every row was fetched live on 2026-10-20: HTTP 200, an <rss>/<feed>/<rdf> root,
 * and at least one <item>/<entry>. URLs already registered anywhere in the tree
 * were dropped before probing.
 *
 * 0 of these were found by AUTODISCOVERY: the official homepage (from
 * Wikidata P856, or a host already in the tree) was fetched and its own
 * <link rel="alternate" type="application/rss+xml"> declarations were read —
 * the site was asked what it serves rather than probed at guessed paths.
 * Closed gov.uk bodies (gov.uk organisations API govuk_status=closed) poll weekly.
 * Per-row probe counts: docs/verified-sources-batch50.tsv. */
#include "_source_macros.inc"

/* ── cyber_advisory ──────────────────────────────────────────────────── */

RSSX(intl_b50_feeds_feedburner_com_eset_blog, "intl-b50-feeds-feedburner-com-eset-blog",
     "ESET Blog", "ESET Blog",
     "cyber_advisory", "cyber",
     "https://feeds.feedburner.com/eset/blog",
     "en", "[\"gov\",\"country:intl\",\"publisher:ESET Blog\",\"cyber\",\"batch:50\"]",
     3600,
     "ESET Blog — cyber feed (100 entries at probe)")

RSSX(intl_b50_threatpost_com, "intl-b50-threatpost-com",
     "Threatpost", "Threatpost",
     "cyber_advisory", "cyber",
     "https://threatpost.com/feed/",
     "en", "[\"gov\",\"country:intl\",\"publisher:Threatpost\",\"cyber\",\"batch:50\"]",
     3600,
     "Threatpost — cyber feed (10 entries at probe)")

/* ── defense ─────────────────────────────────────────────────────────── */

RSSX(intl_b50_scmp_com_16367, "intl-b50-scmp-com-16367",
     "SCMP China Military", "SCMP China Military",
     "defense", "defense",
     "https://www.scmp.com/rss/16367/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP China Military\",\"defense\",\"batch:50\"]",
     3600,
     "SCMP China Military — defense feed (20 entries at probe)")

/* ── government ──────────────────────────────────────────────────────── */

RSSX(intl_b50_france24_com_en_africa, "intl-b50-france24-com-en-africa",
     "France 24 EN Africa", "France 24 EN Africa",
     "government", "government",
     "https://www.france24.com/en/africa/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN Africa\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN Africa — government feed (30 entries at probe)")

RSSX(intl_b50_france24_com_en_americas, "intl-b50-france24-com-en-americas",
     "France 24 EN Americas", "France 24 EN Americas",
     "government", "government",
     "https://www.france24.com/en/americas/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN Americas\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN Americas — government feed (30 entries at probe)")

RSSX(intl_b50_france24_com_en_asia_pacific, "intl-b50-france24-com-en-asia-pacific",
     "France 24 EN Asia-Pacific", "France 24 EN Asia-Pacific",
     "government", "government",
     "https://www.france24.com/en/asia-pacific/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN Asia-Pacific\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN Asia-Pacific — government feed (30 entries at probe)")

RSSX(intl_b50_france24_com_en_culture, "intl-b50-france24-com-en-culture",
     "France 24 EN Culture", "France 24 EN Culture",
     "government", "government",
     "https://www.france24.com/en/culture/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN Culture\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN Culture — government feed (30 entries at probe)")

RSSX(intl_b50_france24_com_en_europe, "intl-b50-france24-com-en-europe",
     "France 24 EN Europe", "France 24 EN Europe",
     "government", "government",
     "https://www.france24.com/en/europe/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN Europe\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN Europe — government feed (30 entries at probe)")

RSSX(intl_b50_france24_com_en_france, "intl-b50-france24-com-en-france",
     "France 24 EN France", "France 24 EN France",
     "government", "government",
     "https://www.france24.com/en/france/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN France\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN France — government feed (30 entries at probe)")

RSSX(intl_b50_france24_com_en_middle_east, "intl-b50-france24-com-en-middle-east",
     "France 24 EN Middle East", "France 24 EN Middle East",
     "government", "government",
     "https://www.france24.com/en/middle-east/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN Middle East\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN Middle East — government feed (30 entries at probe)")

RSSX(intl_b50_france24_com_en_sport, "intl-b50-france24-com-en-sport",
     "France 24 EN Sport", "France 24 EN Sport",
     "government", "government",
     "https://www.france24.com/en/sport/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN Sport\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN Sport — government feed (30 entries at probe)")

RSSX(intl_b50_france24_com_en_tech, "intl-b50-france24-com-en-tech",
     "France 24 EN Tech", "France 24 EN Tech",
     "government", "government",
     "https://www.france24.com/en/tech/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:France 24 EN Tech\",\"government\",\"batch:50\"]",
     3600,
     "France 24 EN Tech — government feed (30 entries at probe)")

RSSX(intl_b50_scmp_com_100, "intl-b50-scmp-com-100",
     "SCMP Economy", "SCMP Economy",
     "government", "government",
     "https://www.scmp.com/rss/100/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP Economy\",\"government\",\"batch:50\"]",
     3600,
     "SCMP Economy — government feed (15 entries at probe)")

RSSX(intl_b50_scmp_com_13, "intl-b50-scmp-com-13",
     "SCMP Tech", "SCMP Tech",
     "government", "government",
     "https://www.scmp.com/rss/13/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP Tech\",\"government\",\"batch:50\"]",
     3600,
     "SCMP Tech — government feed (50 entries at probe)")

RSSX(intl_b50_scmp_com_318202, "intl-b50-scmp-com-318202",
     "SCMP China Economy", "SCMP China Economy",
     "government", "government",
     "https://www.scmp.com/rss/318202/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP China Economy\",\"government\",\"batch:50\"]",
     3600,
     "SCMP China Economy — government feed (50 entries at probe)")

RSSX(intl_b50_scmp_com_318421, "intl-b50-scmp-com-318421",
     "SCMP Economy China", "SCMP Economy China",
     "government", "government",
     "https://www.scmp.com/rss/318421/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP Economy China\",\"government\",\"batch:50\"]",
     3600,
     "SCMP Economy China — government feed (50 entries at probe)")

RSSX(intl_b50_scmp_com_518012, "intl-b50-scmp-com-518012",
     "SCMP Asia Economy", "SCMP Asia Economy",
     "government", "government",
     "https://www.scmp.com/rss/518012/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP Asia Economy\",\"government\",\"batch:50\"]",
     3600,
     "SCMP Asia Economy — government feed (50 entries at probe)")

RSSX(intl_b50_scmp_com_72, "intl-b50-scmp-com-72",
     "SCMP China Politics", "SCMP China Politics",
     "government", "government",
     "https://www.scmp.com/rss/72/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP China Politics\",\"government\",\"batch:50\"]",
     3600,
     "SCMP China Politics — government feed (50 entries at probe)")

RSSX(intl_b50_scmp_com_96, "intl-b50-scmp-com-96",
     "SCMP Property", "SCMP Property",
     "government", "government",
     "https://www.scmp.com/rss/96/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP Property\",\"government\",\"batch:50\"]",
     3600,
     "SCMP Property — government feed (50 entries at probe)")

/* ── news ────────────────────────────────────────────────────────────── */

RSSX(intl_b50_en_yna_co_kr_news, "intl-b50-en-yna-co-kr-news",
     "Yonhap News EN", "Yonhap News",
     "news", "media",
     "https://en.yna.co.kr/RSS/news.xml",
     "en", "[\"news\",\"country:intl\",\"publisher:Yonhap News EN\",\"media\",\"batch:50\"]",
     3600,
     "Yonhap News EN — news feed (102 entries at probe)")

RSSX(intl_b50_foreignpolicy_com, "intl-b50-foreignpolicy-com",
     "Foreign Policy", "Foreign Policy",
     "news", "media",
     "https://foreignpolicy.com/feed/",
     "en", "[\"news\",\"country:intl\",\"publisher:Foreign Policy\",\"media\",\"batch:50\"]",
     3600,
     "Foreign Policy — news feed (25 entries at probe)")

RSSX(intl_b50_lobste_rs, "intl-b50-lobste-rs",
     "Lobste.rs", "Lobste.rs",
     "news", "media",
     "https://lobste.rs/rss",
     "en", "[\"news\",\"country:intl\",\"publisher:Lobste.rs\",\"media\",\"batch:50\"]",
     3600,
     "Lobste.rs — news feed (25 entries at probe)")

RSSX(intl_b50_rss_dw_com_rss_en_all, "intl-b50-rss-dw-com-rss-en-all",
     "DW News", "DW News",
     "news", "media",
     "https://rss.dw.com/xml/rss-en-all",
     "en", "[\"news\",\"country:intl\",\"publisher:DW News\",\"media\",\"batch:50\"]",
     3600,
     "DW News — news feed (134 entries at probe)")

RSSX(intl_b50_rss_dw_com_rss_en_asia, "intl-b50-rss-dw-com-rss-en-asia",
     "DW Asia", "DW Asia",
     "news", "media",
     "https://rss.dw.com/xml/rss-en-asia",
     "en", "[\"news\",\"country:intl\",\"publisher:DW Asia\",\"media\",\"batch:50\"]",
     3600,
     "DW Asia — news feed (13 entries at probe)")

RSSX(intl_b50_rss_nytimes_com_services_nyt_business, "intl-b50-rss-nytimes-com-services-nyt-business",
     "NYT Business", "NYT Business",
     "news", "media",
     "https://rss.nytimes.com/services/xml/rss/nyt/Business.xml",
     "en", "[\"news\",\"country:intl\",\"publisher:NYT Business\",\"media\",\"batch:50\"]",
     3600,
     "NYT Business — news feed (49 entries at probe)")

RSSX(intl_b50_rss_nytimes_com_services_nyt_politics, "intl-b50-rss-nytimes-com-services-nyt-politics",
     "NYT Politics", "NYT Politics",
     "news", "media",
     "https://rss.nytimes.com/services/xml/rss/nyt/Politics.xml",
     "en", "[\"news\",\"country:intl\",\"publisher:NYT Politics\",\"media\",\"batch:50\"]",
     3600,
     "NYT Politics — news feed (20 entries at probe)")

RSSX(intl_b50_rss_nytimes_com_services_nyt_science, "intl-b50-rss-nytimes-com-services-nyt-science",
     "NYT Science", "NYT Science",
     "news", "media",
     "https://rss.nytimes.com/services/xml/rss/nyt/Science.xml",
     "en", "[\"news\",\"country:intl\",\"publisher:NYT Science\",\"media\",\"batch:50\"]",
     3600,
     "NYT Science — news feed (22 entries at probe)")

RSSX(intl_b50_rss_nytimes_com_services_nyt_technology, "intl-b50-rss-nytimes-com-services-nyt-technology",
     "NYT Technology", "NYT Technology",
     "news", "media",
     "https://rss.nytimes.com/services/xml/rss/nyt/Technology.xml",
     "en", "[\"news\",\"country:intl\",\"publisher:NYT Technology\",\"media\",\"batch:50\"]",
     3600,
     "NYT Technology — news feed (20 entries at probe)")

RSSX(intl_b50_rss_nytimes_com_services_nyt_us, "intl-b50-rss-nytimes-com-services-nyt-us",
     "NYT US", "NYT US",
     "news", "media",
     "https://rss.nytimes.com/services/xml/rss/nyt/US.xml",
     "en", "[\"news\",\"country:intl\",\"publisher:NYT US\",\"media\",\"batch:50\"]",
     3600,
     "NYT US — news feed (27 entries at probe)")

RSSX(intl_b50_techcrunch_com, "intl-b50-techcrunch-com",
     "TechCrunch", "TechCrunch",
     "news", "media",
     "https://techcrunch.com/feed/",
     "en", "[\"news\",\"country:intl\",\"publisher:TechCrunch\",\"media\",\"batch:50\"]",
     3600,
     "TechCrunch — news feed (20 entries at probe)")

RSSX(intl_b50_theguardian_com_uk_business, "intl-b50-theguardian-com-uk-business",
     "Guardian Business", "Guardian Business",
     "news", "media",
     "https://www.theguardian.com/uk/business/rss",
     "en", "[\"news\",\"country:intl\",\"publisher:Guardian Business\",\"media\",\"batch:50\"]",
     3600,
     "Guardian Business — news feed (40 entries at probe)")

RSSX(intl_b50_theguardian_com_uk_technology, "intl-b50-theguardian-com-uk-technology",
     "Guardian Tech", "Guardian Tech",
     "news", "media",
     "https://www.theguardian.com/uk/technology/rss",
     "en", "[\"news\",\"country:intl\",\"publisher:Guardian Tech\",\"media\",\"batch:50\"]",
     3600,
     "Guardian Tech — news feed (34 entries at probe)")

/* ── osint ───────────────────────────────────────────────────────────── */

RSSX(intl_b50_globalvoices_org, "intl-b50-globalvoices-org",
     "Global Voices", "Global Voices",
     "osint", "osint",
     "https://globalvoices.org/feed/",
     "en", "[\"gov\",\"country:intl\",\"publisher:Global Voices\",\"osint\",\"batch:50\"]",
     3600,
     "Global Voices — osint feed (15 entries at probe)")

RSSX(intl_b50_hrw_org, "intl-b50-hrw-org",
     "Human Rights Watch", "Human Rights Watch",
     "osint", "osint",
     "https://www.hrw.org/rss",
     "en", "[\"gov\",\"country:intl\",\"publisher:Human Rights Watch\",\"osint\",\"batch:50\"]",
     3600,
     "Human Rights Watch — osint feed (20 entries at probe)")

RSSX(intl_b50_opendemocracy_net_en, "intl-b50-opendemocracy-net-en",
     "OpenDemocracy", "OpenDemocracy",
     "osint", "osint",
     "https://www.opendemocracy.net/en/rss/",
     "en", "[\"gov\",\"country:intl\",\"publisher:OpenDemocracy\",\"osint\",\"batch:50\"]",
     3600,
     "OpenDemocracy — osint feed (15 entries at probe)")

/* ── science ─────────────────────────────────────────────────────────── */

RSSX(intl_b50_scmp_com_76, "intl-b50-scmp-com-76",
     "SCMP China Science", "SCMP China Science",
     "science", "science",
     "https://www.scmp.com/rss/76/feed",
     "en", "[\"gov\",\"country:intl\",\"publisher:SCMP China Science\",\"science\",\"batch:50\"]",
     10800,
     "SCMP China Science — science feed (50 entries at probe)")

RSSX(intl_b50_smithsonianmag_com_latest_articles, "intl-b50-smithsonianmag-com-latest-articles",
     "Smithsonian Magazine", "Smithsonian Magazine",
     "science", "science",
     "https://www.smithsonianmag.com/rss/latest_articles/",
     "en", "[\"gov\",\"country:intl\",\"publisher:Smithsonian Magazine\",\"science\",\"batch:50\"]",
     10800,
     "Smithsonian Magazine — science feed (10 entries at probe)")

RSSX(intl_b50_thelancet_com_rssfeed_lancet_current, "intl-b50-thelancet-com-rssfeed-lancet-current",
     "Lancet", "Lancet",
     "science", "science",
     "https://www.thelancet.com/rssfeed/lancet_current.xml",
     "en", "[\"gov\",\"country:intl\",\"publisher:Lancet\",\"science\",\"batch:50\"]",
     10800,
     "Lancet — science feed (34 entries at probe)")
