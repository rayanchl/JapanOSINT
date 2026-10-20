/* collectors/sources/jp_feeds_batch41.c
 * 50 new Japan-focused RSS/Atom feeds, all proven live (HTTP 200) and
 * verified absent from the existing tree on 2026-10-12.
 *
 * Categories covered: news, technology, gaming, lifestyle, government,
 * disaster, cybersecurity, finance, culture, and English-language Japan media.
 *
 * Every URL was tested with curl and returned a valid feed body.
 * Uses the RSSX macro from _source_macros.inc for concise registration. */
#include "_source_macros.inc"

/* ── News / Wire ─────────────────────────────────────────────────────── */

RSSX(jp_internet_watch, "jp-internet-watch",
     "INTERNET Watch", "INTERNET Watch",
     "news", "technology",
     "https://internet.watch.impress.co.jp/cda/rss/internet.rdf",
     "ja", "[\"news\",\"publisher:INTERNET Watch\",\"tech\",\"internet\"]",
     3600,
     "INTERNET Watch (Impress) — internet services, security and infrastructure")

RSSX(jp_news_on_japan, "jp-news-on-japan",
     "News on Japan", "ニュース・オン・ジャパン",
     "news", "media",
     "https://newsonjapan.com/rss/top.xml",
     "en", "[\"news\",\"publisher:News on Japan\"]",
     3600,
     "English-language Japan news aggregator — top stories")

RSSX(jp_soranews24, "jp-soranews24",
     "SoraNews24", "ソラニュース24",
     "news", "media",
     "https://soranews24.com/feed/",
     "en", "[\"news\",\"publisher:SoraNews24\",\"culture\"]",
     3600,
     "Japanese pop culture, travel and daily life in English")

RSSX(jp_unseen_japan, "jp-unseen-japan",
     "Unseen Japan", "アンシーン・ジャパン",
     "news", "media",
     "https://unseenjapan.com/feed/",
     "en", "[\"news\",\"publisher:Unseen Japan\",\"culture\"]",
     3600,
     "Deep-dive articles on Japanese society, history and culture")

RSSX(jp_tokyo_weekender, "jp-tokyo-weekender",
     "Tokyo Weekender", "東京ウィークエンダー",
     "news", "media",
     "https://www.tokyoweekender.com/feed/",
     "en", "[\"news\",\"publisher:Tokyo Weekender\",\"lifestyle\"]",
     3600,
     "English-language lifestyle and events magazine for Tokyo")

RSSX(jp_livedoor_top, "jp-livedoor-top",
     "Livedoor News (Top)", "ライブドアニュース（トップ）",
     "news", "media",
     "https://news.livedoor.com/topics/rss/top.xml",
     "ja", "[\"news\",\"publisher:Livedoor\",\"top\"]",
     1800,
     "Livedoor News — top stories aggregation")

RSSX(jp_livedoor_domestic, "jp-livedoor-domestic",
     "Livedoor News (Domestic)", "ライブドアニュース（国内）",
     "news", "media",
     "https://news.livedoor.com/topics/rss/dom.xml",
     "ja", "[\"news\",\"publisher:Livedoor\",\"domestic\"]",
     1800,
     "Livedoor News — domestic/national stories")

RSSX(jp_livedoor_intl, "jp-livedoor-intl",
     "Livedoor News (International)", "ライブドアニュース（国際）",
     "news", "media",
     "https://news.livedoor.com/topics/rss/int.xml",
     "ja", "[\"news\",\"publisher:Livedoor\",\"international\"]",
     1800,
     "Livedoor News — international stories")

RSSX(jp_livedoor_economy, "jp-livedoor-economy",
     "Livedoor News (Economy)", "ライブドアニュース（経済）",
     "news", "media",
     "https://news.livedoor.com/topics/rss/eco.xml",
     "ja", "[\"news\",\"publisher:Livedoor\",\"economy\"]",
     1800,
     "Livedoor News — economy and business stories")

RSSX(jp_livedoor_entertainment, "jp-livedoor-entertainment",
     "Livedoor News (Entertainment)", "ライブドアニュース（エンタメ）",
     "news", "media",
     "https://news.livedoor.com/topics/rss/ent.xml",
     "ja", "[\"news\",\"publisher:Livedoor\",\"entertainment\"]",
     1800,
     "Livedoor News — entertainment and celebrity stories")

RSSX(jp_livedoor_sports, "jp-livedoor-sports",
     "Livedoor News (Sports)", "ライブドアニュース（スポーツ）",
     "news", "media",
     "https://news.livedoor.com/topics/rss/spo.xml",
     "ja", "[\"news\",\"publisher:Livedoor\",\"sports\"]",
     1800,
     "Livedoor News — sports stories")

RSSX(jp_biglobe_domestic, "jp-biglobe-domestic",
     "BIGLOBE News (Domestic)", "BIGLOBEニュース（国内）",
     "news", "media",
     "https://news.biglobe.ne.jp/rss/smart_domestic.xml",
     "ja", "[\"news\",\"publisher:BIGLOBE\",\"domestic\"]",
     1800,
     "BIGLOBE News — domestic stories aggregation")

/* ── Technology / IT ─────────────────────────────────────────────────── */

RSSX(jp_codezine, "jp-codezine",
     "CodeZine", "コードジン",
     "news", "technology",
     "https://codezine.jp/rss/new/20/index.xml",
     "ja", "[\"news\",\"publisher:CodeZine\",\"tech\",\"development\"]",
     3600,
     "CodeZine — developer-focused articles and tutorials")

RSSX(jp_atmarkit, "jp-atmarkit",
     "@IT", "＠IT",
     "news", "technology",
     "https://atmarkit.itmedia.co.jp/rss/rss2dc.xml",
     "ja", "[\"news\",\"publisher:@IT\",\"tech\",\"enterprise\"]",
     3600,
     "@IT — enterprise IT, cloud, security and development news")

RSSX(jp_itmedia_news, "jp-itmedia-news",
     "ITmedia NEWS", "ITmedia NEWS",
     "news", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/itmedia_news.xml",
     "ja", "[\"news\",\"publisher:ITmedia NEWS\",\"tech\"]",
     1800,
     "ITmedia NEWS — Japanese technology and business news")

RSSX(jp_itmedia_enterprise, "jp-itmedia-enterprise",
     "ITmedia Enterprise", "ITmedia エンタープライズ",
     "news", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/itmedia_enterprise.xml",
     "ja", "[\"news\",\"publisher:ITmedia Enterprise\",\"tech\",\"enterprise\"]",
     3600,
     "ITmedia — enterprise IT, security and cloud computing")

RSSX(jp_itmedia_ai, "jp-itmedia-ai",
     "ITmedia AI+", "ITmedia AI+",
     "news", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/itmedia_aiplus.xml",
     "ja", "[\"news\",\"publisher:ITmedia AI+\",\"tech\",\"ai\"]",
     3600,
     "ITmedia AI+ — artificial intelligence and machine learning news")

RSSX(jp_itmedia_business, "jp-itmedia-business",
     "ITmedia Business", "ITmedia ビジネスオンライン",
     "news", "media",
     "https://rss.itmedia.co.jp/rss/2.0/itmedia_business.xml",
     "ja", "[\"news\",\"publisher:ITmedia Business\",\"business\"]",
     3600,
     "ITmedia — business trends, management and economy")

RSSX(jp_itmedia_healthcare, "jp-itmedia-healthcare",
     "ITmedia Healthcare", "ITmedia ヘルスケア",
     "news", "media",
     "https://rss.itmedia.co.jp/rss/2.0/itmedia_healthcare.xml",
     "ja", "[\"news\",\"publisher:ITmedia Healthcare\",\"health\"]",
     3600,
     "ITmedia — healthcare, medical technology and wellness")

RSSX(jp_itmedia_mobile, "jp-itmedia-mobile",
     "ITmedia Mobile", "ITmedia Mobile",
     "news", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/itmedia_mobile.xml",
     "ja", "[\"news\",\"publisher:ITmedia Mobile\",\"tech\",\"mobile\"]",
     3600,
     "ITmedia Mobile — smartphones, carriers and mobile tech")

RSSX(jp_itmedia_pcuser, "jp-itmedia-pcuser",
     "ITmedia PC USER", "ITmedia PC USER",
     "news", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/pcuser.xml",
     "ja", "[\"news\",\"publisher:ITmedia PC USER\",\"tech\",\"hardware\"]",
     3600,
     "ITmedia PC USER — hardware, peripherals and OS reviews")

RSSX(jp_xtech_it, "jp-xtech-it",
     "Nikkei xTECH (IT)", "日経クロステック（IT）",
     "news", "technology",
     "https://xtech.nikkei.com/rss/xtech-it.rdf",
     "ja", "[\"news\",\"publisher:Nikkei xTECH\",\"tech\",\"enterprise\"]",
     3600,
     "Nikkei xTECH — enterprise IT, digital transformation news")

RSSX(jp_xtech_dm, "jp-xtech-dm",
     "Nikkei xTECH (DM)", "日経クロステック（ものづくり）",
     "news", "technology",
     "https://xtech.nikkei.com/rss/xtech-dm.rdf",
     "ja", "[\"news\",\"publisher:Nikkei xTECH\",\"tech\",\"manufacturing\"]",
     3600,
     "Nikkei xTECH — manufacturing, design and engineering news")

RSSX(jp_weekly_ascii, "jp-weekly-ascii",
     "Weekly ASCII", "週刊アスキー",
     "news", "technology",
     "https://weekly.ascii.jp/rss.xml",
     "ja", "[\"news\",\"publisher:Weekly ASCII\",\"tech\",\"gadgets\"]",
     3600,
     "Weekly ASCII — gadgets, apps and consumer technology")

RSSX(jp_thebridge, "jp-thebridge",
     "THE BRIDGE", "THE BRIDGE",
     "news", "technology",
     "https://thebridge.jp/feed",
     "ja", "[\"news\",\"publisher:THE BRIDGE\",\"tech\",\"startup\"]",
     3600,
     "THE BRIDGE — Japanese startup, VC and tech industry news")

RSSX(jp_appbank, "jp-appbank",
     "AppBank", "AppBank",
     "news", "technology",
     "https://www.appbank.net/feed",
     "ja", "[\"news\",\"publisher:AppBank\",\"tech\",\"mobile\",\"apps\"]",
     3600,
     "AppBank — mobile apps, iPhone/Android reviews and tips")

/* ── Impress Watch Network ───────────────────────────────────────────── */

RSSX(jp_cloud_watch, "jp-cloud-watch",
     "CLOUD Watch", "CLOUD Watch",
     "news", "technology",
     "https://cloud.watch.impress.co.jp/data/rss/1.0/clw/feed.rdf",
     "ja", "[\"news\",\"publisher:CLOUD Watch\",\"tech\",\"cloud\"]",
     3600,
     "CLOUD Watch (Impress) — cloud computing and enterprise IT")

RSSX(jp_dc_watch, "jp-dc-watch",
     "DC Watch", "デジカメ Watch",
     "news", "technology",
     "https://dc.watch.impress.co.jp/data/rss/1.0/dcw/feed.rdf",
     "ja", "[\"news\",\"publisher:DC Watch\",\"tech\",\"camera\"]",
     3600,
     "DC Watch (Impress) — digital cameras and photography gear")

RSSX(jp_av_watch, "jp-av-watch",
     "AV Watch", "AV Watch",
     "news", "technology",
     "https://av.watch.impress.co.jp/data/rss/1.0/avw/feed.rdf",
     "ja", "[\"news\",\"publisher:AV Watch\",\"tech\",\"audio-video\"]",
     3600,
     "AV Watch (Impress) — audio, video and home theater equipment")

RSSX(jp_game_watch, "jp-game-watch",
     "GAME Watch", "GAME Watch",
     "news", "technology",
     "https://game.watch.impress.co.jp/data/rss/1.0/gmw/feed.rdf",
     "ja", "[\"news\",\"publisher:GAME Watch\",\"tech\",\"gaming\"]",
     3600,
     "GAME Watch (Impress) — video games, consoles and esports")

RSSX(jp_travel_watch, "jp-travel-watch",
     "Travel Watch", "トラベル Watch",
     "news", "media",
     "https://travel.watch.impress.co.jp/data/rss/1.0/trw/feed.rdf",
     "ja", "[\"news\",\"publisher:Travel Watch\",\"travel\",\"japan\"]",
     3600,
     "Travel Watch (Impress) — travel, airlines and tourism in Japan")

RSSX(jp_kaden_watch, "jp-kaden-watch",
     "Kaden Watch", "家電 Watch",
     "news", "technology",
     "https://kaden.watch.impress.co.jp/data/rss/1.0/kdw/feed.rdf",
     "ja", "[\"news\",\"publisher:Kaden Watch\",\"tech\",\"appliances\"]",
     3600,
     "Kaden Watch (Impress) — home appliances, smart home and IoT")

RSSX(jp_akiba_pc, "jp-akiba-pc",
     "AKIBA PC Hotline!", "AKIBA PC Hotline!",
     "news", "technology",
     "https://akiba-pc.watch.impress.co.jp/data/rss/1.0/ah/feed.rdf",
     "ja", "[\"news\",\"publisher:AKIBA PC Hotline\",\"tech\",\"hardware\"]",
     3600,
     "AKIBA PC Hotline (Impress) — Akihabara PC parts and pricing")

/* ── Gaming ──────────────────────────────────────────────────────────── */

RSSX(jp_automaton, "jp-automaton",
     "AUTOMATON", "AUTOMATON",
     "news", "media",
     "https://automaton-media.com/feed/",
     "ja", "[\"news\",\"publisher:AUTOMATON\",\"gaming\"]",
     3600,
     "AUTOMATON — Japanese and global video game industry news")

RSSX(jp_4gamer, "jp-4gamer",
     "4Gamer.net", "4Gamer.net",
     "news", "media",
     "https://www.4gamer.net/rss/index.xml",
     "ja", "[\"news\",\"publisher:4Gamer\",\"gaming\"]",
     3600,
     "4Gamer.net — comprehensive Japanese gaming news and reviews")

/* ── Social / Bookmarking ────────────────────────────────────────────── */

RSSX(jp_hatena_game, "jp-hatena-game",
     "Hatena Bookmark (Game)", "はてなブックマーク（ゲーム）",
     "news", "media",
     "https://b.hatena.ne.jp/hotentry/game.rss",
     "ja", "[\"hatena\",\"trending\",\"game\"]",
     1800,
     "Hatena Bookmark — trending game and entertainment bookmarks")

/* ── Government / Policy ─────────────────────────────────────────────── */

RSSX(jp_maff_press, "jp-maff-press",
     "MAFF Press Releases", "農林水産省 報道発表",
     "news", "politics",
     "https://www.maff.go.jp/j/press/rss.xml",
     "ja", "[\"news\",\"publisher:MAFF\",\"government\",\"agriculture\"]",
     3600,
     "Ministry of Agriculture, Forestry and Fisheries — press releases")

/* ── Disaster / River ────────────────────────────────────────────────── */

RSSX(jp_river_rain, "jp-river-rain",
     "River.go.jp Rain Alert", "川の防災情報 雨量",
     "news", "disaster",
     "https://www.river.go.jp/rss/rain_top.xml",
     "ja", "[\"disaster\",\"rain\",\"flood\",\"japan\"]",
     1800,
     "MLIT river disaster information — rainfall alerts and river levels")

/* ── Cybersecurity ───────────────────────────────────────────────────── */

RSSX(jp_jvndb, "jp-jvndb",
     "JVN iPedia (DB)", "JVN iPedia（脆弱性データベース）",
     "news", "cyber",
     "https://jvndb.jvn.jp/rss/jvndb.rdf",
     "ja", "[\"security\",\"publisher:JVN iPedia\",\"vulnerability\",\"database\"]",
     7200,
     "JVN iPedia — Japanese vulnerability database (NVD counterpart)")

/* ── Science / Space ─────────────────────────────────────────────────── */

RSSX(jp_isas_jaxa, "jp-isas-jaxa",
     "ISAS/JAXA Topics", "ISAS/JAXA トピックス",
     "news", "research",
     "https://www.isas.jaxa.jp/topics/rss.xml",
     "ja", "[\"news\",\"publisher:ISAS/JAXA\",\"space\",\"science\"]",
     7200,
     "ISAS/JAXA — space science research institute news and discoveries")

RSSX(jp_nisc_en, "jp-nisc-en",
     "NISC Cybersecurity (EN)", "NISC サイバーセキュリティ（英語）",
     "news", "cyber",
     "https://www.nisc.go.jp/eng/rss/index.xml",
     "en", "[\"security\",\"publisher:NISC\",\"government\",\"cybersecurity\"]",
     7200,
     "National center of Incident readiness and Strategy for Cybersecurity — English")

RSSX(jp_nta_press, "jp-nta-press",
     "NTA Releases", "国税庁 新着情報",
     "news", "politics",
     "https://www.nta.go.jp/information/release/rss.xml",
     "ja", "[\"news\",\"publisher:NTA\",\"government\",\"tax\"]",
     7200,
     "National Tax Agency — press releases and tax policy updates")

/* ── Entertainment / Culture ────────────────────────────────────────── */

RSSX(jp_rocketnews24, "jp-rocketnews24",
     "RocketNews24", "ロケットニュース24",
     "news", "media",
     "https://rocketnews24.com/feed/",
     "ja", "[\"news\",\"publisher:RocketNews24\",\"entertainment\",\"viral\"]",
     1800,
     "RocketNews24 — Japanese viral news, food and entertainment")

RSSX(jp_natalie_music, "jp-natalie-music",
     "Natalie Music", "音楽ナタリー",
     "news", "media",
     "https://natalie.mu/music/feed/news",
     "ja", "[\"news\",\"publisher:Natalie\",\"music\",\"entertainment\"]",
     3600,
     "Natalie — Japanese music industry news, releases and artist updates")

RSSX(jp_natalie_comic, "jp-natalie-comic",
     "Natalie Comic", "コミックナタリー",
     "news", "media",
     "https://natalie.mu/comic/feed/news",
     "ja", "[\"news\",\"publisher:Natalie\",\"manga\",\"comic\"]",
     3600,
     "Natalie — manga, anime and comic news")

/* ── English-language Japan media ───────────────────────────────────── */

RSSX(jp_japantoday, "jp-japantoday",
     "Japan Today", "ジャパントゥデイ",
     "news", "media",
     "https://japantoday.com/feed",
     "en", "[\"news\",\"publisher:Japan Today\",\"japan\"]",
     1800,
     "Japan Today — English-language Japan news aggregator")

RSSX(jp_tokyo_reporter, "jp-tokyo-reporter",
     "Tokyo Reporter", "東京レポーター",
     "news", "media",
     "https://www.tokyoreporter.com/feed/",
     "en", "[\"news\",\"publisher:Tokyo Reporter\",\"crime\",\"japan\"]",
     3600,
     "Tokyo Reporter — English crime and tabloid journalism from Japan")

RSSX(jp_japan_forward, "jp-japan-forward",
     "Japan Forward", "ジャパンフォワード",
     "news", "media",
     "https://japan-forward.com/feed/",
     "en", "[\"news\",\"publisher:Japan Forward\",\"policy\",\"japan\"]",
     3600,
     "Japan Forward — English Japan news, opinion and analysis")

RSSX(jp_metropolis, "jp-metropolis",
     "Metropolis Japan", "メトロポリス・ジャパン",
     "news", "media",
     "https://metropolisjapan.com/feed/",
     "en", "[\"news\",\"publisher:Metropolis\",\"lifestyle\",\"tokyo\"]",
     3600,
     "Metropolis Japan — English Tokyo lifestyle, events and dining")

RSSX(jp_savvy_tokyo, "jp-savvy-tokyo",
     "Savvy Tokyo", "サヴィー東京",
     "news", "media",
     "https://www.savvytokyo.com/feed/",
     "en", "[\"news\",\"publisher:Savvy Tokyo\",\"lifestyle\",\"expat\"]",
     3600,
     "Savvy Tokyo — English-language expat living and events in Tokyo")
