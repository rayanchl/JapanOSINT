/* collectors/sources/cn_feeds_batch49.c
 * Batch 49 — 39 China / Hong Kong government / institutional RSS-Atom feeds.
 *
 * Every row was fetched live on 2026-10-20: HTTP 200, an <rss>/<feed>/<rdf> root,
 * and at least one <item>/<entry>. URLs already registered anywhere in the tree
 * were dropped before probing.
 *
 * 3 of these were found by AUTODISCOVERY: the official homepage (from
 * Wikidata P856, or a host already in the tree) was fetched and its own
 * <link rel="alternate" type="application/rss+xml"> declarations were read —
 * the site was asked what it serves rather than probed at guessed paths.
 * Closed gov.uk bodies (gov.uk organisations API govuk_status=closed) poll weekly.
 * Per-row probe counts: docs/verified-sources-batch49.tsv. */
#include "_source_macros.inc"

/* ── government ──────────────────────────────────────────────────────── */

RSSX(cn_b49_shzuni_com, "cn-b49-shzuni-com",
     "Shihezi University", "石河子大学",
     "government", "government",
     "https://shzuni.com/feed/",
     "zh", "[\"gov\",\"country:cn\",\"publisher:Shihezi University\",\"government\",\"batch:49\"]",
     3600,
     "Shihezi University — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 1 entries at probe")

RSSX(cn_b49_um_edu_mo, "cn-b49-um-edu-mo",
     "University of Macau", "澳門大學",
     "government", "government",
     "https://www.um.edu.mo/feed/",
     "zh", "[\"gov\",\"country:cn\",\"publisher:University of Macau\",\"government\",\"batch:49\"]",
     3600,
     "University of Macau — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 12 entries at probe")

RSSX(cn_b49_xawl_org, "cn-b49-xawl-org",
     "Xi'an University", "西安文理学院",
     "government", "government",
     "https://www.xawl.org/feed/",
     "zh", "[\"gov\",\"country:cn\",\"publisher:Xi'an University\",\"government\",\"batch:49\"]",
     3600,
     "Xi'an University — RSS/Atom feed declared by the official website (Wikidata P856) of this government; 9 entries at probe")

/* ── news ────────────────────────────────────────────────────────────── */

RSSX(cn_b49_bbc_com_zhongwen_simp, "cn-b49-bbc-com-zhongwen-simp",
     "BBC Chinese", "BBC中文",
     "news", "media",
     "https://www.bbc.com/zhongwen/simp/index.xml",
     "zh", "[\"news\",\"country:cn\",\"publisher:BBC Chinese\",\"media\",\"batch:49\"]",
     3600,
     "BBC Chinese — news feed (38 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_business, "cn-b49-cgtn-com-subscribe-section-business",
     "CGTN Business", "CGTN Business",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/business.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN Business\",\"media\",\"batch:49\"]",
     3600,
     "CGTN Business — news feed (50 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_china, "cn-b49-cgtn-com-subscribe-section-china",
     "CGTN China", "CGTN China",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/china.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN China\",\"media\",\"batch:49\"]",
     3600,
     "CGTN China — news feed (50 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_culture, "cn-b49-cgtn-com-subscribe-section-culture",
     "CGTN Culture", "CGTN Culture",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/culture.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN Culture\",\"media\",\"batch:49\"]",
     3600,
     "CGTN Culture — news feed (50 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_nature, "cn-b49-cgtn-com-subscribe-section-nature",
     "CGTN Nature", "CGTN Nature",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/nature.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN Nature\",\"media\",\"batch:49\"]",
     3600,
     "CGTN Nature — news feed (50 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_opinion, "cn-b49-cgtn-com-subscribe-section-opinion",
     "CGTN Opinion", "CGTN Opinion",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/opinion.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN Opinion\",\"media\",\"batch:49\"]",
     3600,
     "CGTN Opinion — news feed (50 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_politics, "cn-b49-cgtn-com-subscribe-section-politics",
     "CGTN Politics", "CGTN Politics",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/politics.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN Politics\",\"media\",\"batch:49\"]",
     3600,
     "CGTN Politics — news feed (50 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_sports, "cn-b49-cgtn-com-subscribe-section-sports",
     "CGTN Sports", "CGTN Sports",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/sports.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN Sports\",\"media\",\"batch:49\"]",
     3600,
     "CGTN Sports — news feed (50 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_travel, "cn-b49-cgtn-com-subscribe-section-travel",
     "CGTN Travel", "CGTN Travel",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/travel.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN Travel\",\"media\",\"batch:49\"]",
     3600,
     "CGTN Travel — news feed (50 entries at probe)")

RSSX(cn_b49_cgtn_com_subscribe_section_world, "cn-b49-cgtn-com-subscribe-section-world",
     "CGTN World", "CGTN World",
     "news", "media",
     "https://www.cgtn.com/subscribe/rss/section/world.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:CGTN World\",\"media\",\"batch:49\"]",
     3600,
     "CGTN World — news feed (50 entries at probe)")

RSSX(cn_b49_ecns_cn, "cn-b49-ecns-cn",
     "ECNS China News", "ECNS China News",
     "news", "media",
     "https://www.ecns.cn/rss/rss.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:ECNS China News\",\"media\",\"batch:49\"]",
     3600,
     "ECNS China News — news feed (100 entries at probe)")

RSSX(cn_b49_macaudailytimes_com_mo, "cn-b49-macaudailytimes-com-mo",
     "Macau Daily Times", "Macau Daily Times",
     "news", "media",
     "https://macaudailytimes.com.mo/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:Macau Daily Times\",\"media\",\"batch:49\"]",
     3600,
     "Macau Daily Times — news feed (10 entries at probe)")

RSSX(cn_b49_scmp_com_11, "cn-b49-scmp-com-11",
     "SCMP Comment", "SCMP Comment",
     "news", "media",
     "https://www.scmp.com/rss/11/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Comment\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Comment — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_12, "cn-b49-scmp-com-12",
     "SCMP Lifestyle", "SCMP Lifestyle",
     "news", "media",
     "https://www.scmp.com/rss/12/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Lifestyle\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Lifestyle — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_17, "cn-b49-scmp-com-17",
     "SCMP Sport", "SCMP Sport",
     "news", "media",
     "https://www.scmp.com/rss/17/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Sport\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Sport — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_19, "cn-b49-scmp-com-19",
     "SCMP Health", "SCMP Health",
     "news", "media",
     "https://www.scmp.com/rss/19/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Health\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Health — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_2, "cn-b49-scmp-com-2",
     "SCMP Hong Kong", "SCMP Hong Kong",
     "news", "media",
     "https://www.scmp.com/rss/2/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Hong Kong\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Hong Kong — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_20, "cn-b49-scmp-com-20",
     "SCMP People & Culture", "SCMP People & Culture",
     "news", "media",
     "https://www.scmp.com/rss/20/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP People & Culture\",\"media\",\"batch:49\"]",
     3600,
     "SCMP People & Culture — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_21074, "cn-b49-scmp-com-21074",
     "SCMP Xinhua", "SCMP Xinhua",
     "news", "media",
     "https://www.scmp.com/rss/21074/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Xinhua\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Xinhua — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_270238, "cn-b49-scmp-com-270238",
     "SCMP Climate", "SCMP Climate",
     "news", "media",
     "https://www.scmp.com/rss/270238/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Climate\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Climate — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_3, "cn-b49-scmp-com-3",
     "SCMP China", "SCMP China",
     "news", "media",
     "https://www.scmp.com/rss/3/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP China\",\"media\",\"batch:49\"]",
     3600,
     "SCMP China — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_36, "cn-b49-scmp-com-36",
     "SCMP Economy", "SCMP Economy",
     "news", "media",
     "https://www.scmp.com/rss/36/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Economy\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Economy — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_37, "cn-b49-scmp-com-37",
     "SCMP Multimedia", "SCMP Multimedia",
     "news", "media",
     "https://www.scmp.com/rss/37/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Multimedia\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Multimedia — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_5, "cn-b49-scmp-com-5",
     "SCMP World", "SCMP World",
     "news", "media",
     "https://www.scmp.com/rss/5/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP World\",\"media\",\"batch:49\"]",
     3600,
     "SCMP World — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_6, "cn-b49-scmp-com-6",
     "SCMP Business", "SCMP Business",
     "news", "media",
     "https://www.scmp.com/rss/6/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Business\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Business — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_71, "cn-b49-scmp-com-71",
     "SCMP China Diplomacy", "SCMP China Diplomacy",
     "news", "media",
     "https://www.scmp.com/rss/71/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP China Diplomacy\",\"media\",\"batch:49\"]",
     3600,
     "SCMP China Diplomacy — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_73, "cn-b49-scmp-com-73",
     "SCMP China Society", "SCMP China Society",
     "news", "media",
     "https://www.scmp.com/rss/73/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP China Society\",\"media\",\"batch:49\"]",
     3600,
     "SCMP China Society — news feed (50 entries at probe)")

RSSX(cn_b49_scmp_com_95, "cn-b49-scmp-com-95",
     "SCMP Sport", "SCMP Sport",
     "news", "media",
     "https://www.scmp.com/rss/95/feed",
     "en", "[\"news\",\"country:cn\",\"publisher:SCMP Sport\",\"media\",\"batch:49\"]",
     3600,
     "SCMP Sport — news feed (50 entries at probe)")

RSSX(cn_b49_sixthtone_com, "cn-b49-sixthtone-com",
     "Sixth Tone", "Sixth Tone",
     "news", "media",
     "https://www.sixthtone.com/rss",
     "en", "[\"news\",\"country:cn\",\"publisher:Sixth Tone\",\"media\",\"batch:49\"]",
     3600,
     "Sixth Tone — news feed (51 entries at probe)")

RSSX(cn_b49_xinhuanet_com_english_businessrss, "cn-b49-xinhuanet-com-english-businessrss",
     "Xinhua Business", "Xinhua Business",
     "news", "media",
     "http://www.xinhuanet.com/english/rss/businessrss.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:Xinhua Business\",\"media\",\"batch:49\"]",
     3600,
     "Xinhua Business — news feed (20 entries at probe)")

RSSX(cn_b49_xinhuanet_com_english_chinarss, "cn-b49-xinhuanet-com-english-chinarss",
     "Xinhua China", "Xinhua China",
     "news", "media",
     "http://www.xinhuanet.com/english/rss/chinarss.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:Xinhua China\",\"media\",\"batch:49\"]",
     3600,
     "Xinhua China — news feed (20 entries at probe)")

RSSX(cn_b49_xinhuanet_com_english_entertainmentrss, "cn-b49-xinhuanet-com-english-entertainmentrss",
     "Xinhua Entertainment", "Xinhua Entertainment",
     "news", "media",
     "http://www.xinhuanet.com/english/rss/entertainmentrss.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:Xinhua Entertainment\",\"media\",\"batch:49\"]",
     3600,
     "Xinhua Entertainment — news feed (20 entries at probe)")

RSSX(cn_b49_xinhuanet_com_english_healthrss, "cn-b49-xinhuanet-com-english-healthrss",
     "Xinhua Health", "Xinhua Health",
     "news", "media",
     "http://www.xinhuanet.com/english/rss/healthrss.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:Xinhua Health\",\"media\",\"batch:49\"]",
     3600,
     "Xinhua Health — news feed (20 entries at probe)")

RSSX(cn_b49_xinhuanet_com_english_sportsrss, "cn-b49-xinhuanet-com-english-sportsrss",
     "Xinhua Sports", "Xinhua Sports",
     "news", "media",
     "http://www.xinhuanet.com/english/rss/sportsrss.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:Xinhua Sports\",\"media\",\"batch:49\"]",
     3600,
     "Xinhua Sports — news feed (20 entries at probe)")

RSSX(cn_b49_xinhuanet_com_english_videorss, "cn-b49-xinhuanet-com-english-videorss",
     "Xinhua Video", "Xinhua Video",
     "news", "media",
     "http://www.xinhuanet.com/english/rss/videorss.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:Xinhua Video\",\"media\",\"batch:49\"]",
     3600,
     "Xinhua Video — news feed (20 entries at probe)")

RSSX(cn_b49_xinhuanet_com_english_worldrss, "cn-b49-xinhuanet-com-english-worldrss",
     "Xinhua World", "Xinhua World",
     "news", "media",
     "http://www.xinhuanet.com/english/rss/worldrss.xml",
     "en", "[\"news\",\"country:cn\",\"publisher:Xinhua World\",\"media\",\"batch:49\"]",
     3600,
     "Xinhua World — news feed (20 entries at probe)")
