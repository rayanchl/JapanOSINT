/* collectors/sources/jp_feeds_batch50.c
 * Batch 50 — 883 Japan government / institutional RSS-Atom feeds.
 *
 * Every row was fetched live on 2026-10-20: HTTP 200, an <rss>/<feed>/<rdf> root,
 * and at least one <item>/<entry>. URLs already registered anywhere in the tree
 * were dropped before probing.
 *
 * 856 of these were found by AUTODISCOVERY: the official homepage (from
 * Wikidata P856, or a host already in the tree) was fetched and its own
 * <link rel="alternate" type="application/rss+xml"> declarations were read —
 * the site was asked what it serves rather than probed at guessed paths.
 * Closed gov.uk bodies (gov.uk organisations API govuk_status=closed) poll weekly.
 * Per-row probe counts: docs/verified-sources-batch50.tsv. */
#include "_source_macros.inc"

/* ── jp_government ───────────────────────────────────────────────────── */

RSSX(jp_b50_adr_go_jp, "jp-b50-adr-go-jp",
     "かいけつサポート", "かいけつサポート",
     "jp_government", "government",
     "https://www.adr.go.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:かいけつサポート\",\"government\",\"batch:50\"]",
     3600,
     "かいけつサポート — RSS/Atom feed declared by the official website (Wikidata P856) of this national agency; 1 entries at probe")

RSSX(jp_b50_maff_go_jp_j_new, "jp-b50-maff-go-jp-j-new",
     "MAFF News", "農林水産省 新着情報",
     "jp_government", "government",
     "https://www.maff.go.jp/j/new/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:MAFF News\",\"government\",\"batch:50\"]",
     3600,
     "MAFF News — national agency feed (10 entries at probe)")

RSSX(jp_b50_mlit_go_jp, "jp-b50-mlit-go-jp",
     "MLIT News", "国土交通省 新着情報",
     "jp_government", "government",
     "https://www.mlit.go.jp/index.rdf",
     "ja", "[\"gov\",\"country:jp\",\"publisher:MLIT News\",\"government\",\"batch:50\"]",
     3600,
     "MLIT News — national agency feed (15 entries at probe)")

RSSX(jp_b50_mlit_go_jp_pressrelease, "jp-b50-mlit-go-jp-pressrelease",
     "MLIT Press Releases", "国土交通省 プレスリリース",
     "jp_government", "government",
     "https://www.mlit.go.jp/pressrelease.rdf",
     "ja", "[\"gov\",\"country:jp\",\"publisher:MLIT Press Releases\",\"government\",\"batch:50\"]",
     3600,
     "MLIT Press Releases — national agency feed (21 entries at probe)")

RSSX(jp_b50_npo_homepage_go_jp_npo_news, "jp-b50-npo-homepage-go-jp-npo-news",
     "NPO Info", "NPO法人ポータル",
     "jp_government", "government",
     "https://www.npo-homepage.go.jp/npo_news/feed/rdf",
     "ja", "[\"gov\",\"country:jp\",\"publisher:NPO Info\",\"government\",\"batch:50\"]",
     3600,
     "NPO Info — national agency feed (20 entries at probe)")

RSSX(jp_b50_rieti_go_jp_en, "jp-b50-rieti-go-jp-en",
     "Research Institute of Economy, Trade and Industry", "経済産業研究所",
     "jp_government", "government",
     "https://www.rieti.go.jp/en/rss/index.rdf",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Research Institute of Economy, Trade and Industry\",\"government\",\"batch:50\"]",
     3600,
     "Research Institute of Economy, Trade and Industry — RSS/Atom feed declared by the official website (Wikidata P856) of this national agency; 401 entries at probe")

RSSX(jp_b50_scienceportal_jst_go_jp_rsslatest, "jp-b50-scienceportal-jst-go-jp-rsslatest",
     "Science Portal", "Science Portal",
     "jp_government", "government",
     "https://scienceportal.jst.go.jp/rsslatest.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Science Portal\",\"government\",\"batch:50\"]",
     3600,
     "Science Portal — RSS/Atom feed declared by the official website (Wikidata P856) of this national agency; 50 entries at probe")

/* ── jp_local ────────────────────────────────────────────────────────── */

RSSX(jp_b50_city_imabari_ehime_jp_imabari, "jp-b50-city-imabari-ehime-jp-imabari",
     "Imabari — 今治市 RSS", "今治市 — 今治市 RSS",
     "jp_local", "government",
     "https://www.city.imabari.ehime.jp/imabari/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Imabari — 今治市 RSS\",\"government\",\"batch:50\"]",
     10800,
     "Imabari — 今治市 RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 8 entries at probe")

RSSX(jp_b50_city_kurayoshi_lg_jp_services_rss10_2844, "jp-b50-city-kurayoshi-lg-jp-services-rss10-2844",
     "Kurayoshi — (RSS)", "倉吉市 — (RSS)",
     "jp_local", "government",
     "https://www.city.kurayoshi.lg.jp/services/rdf/rss10/2844.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kurayoshi — (RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Kurayoshi — (RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 2 entries at probe")

RSSX(jp_b50_city_kyoto_lg_jp_ukyo, "jp-b50-city-kyoto-lg-jp-ukyo",
     "Ukyō Ward — 右京区役所", "右京区 — 右京区役所",
     "jp_local", "government",
     "https://www.city.kyoto.lg.jp/ukyo/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ukyō Ward — 右京区役所\",\"government\",\"batch:50\"]",
     10800,
     "Ukyō Ward — 右京区役所 — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 29 entries at probe")

RSSX(jp_b50_city_obanazawa_yamagata_jp, "jp-b50-city-obanazawa-yamagata-jp",
     "Obanazawa", "尾花沢市",
     "jp_local", "government",
     "https://www.city.obanazawa.yamagata.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Obanazawa\",\"government\",\"batch:50\"]",
     10800,
     "Obanazawa — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 1599 entries at probe")

RSSX(jp_b50_city_sapporo_jp_shiroishi_new_shiroishi_new, "jp-b50-city-sapporo-jp-shiroishi-new-shiroishi-new",
     "Shiroishi-ku — 札幌市白石区 新着・更新情報", "白石区 — 札幌市白石区 新着・更新情報",
     "jp_local", "government",
     "https://www.city.sapporo.jp/shiroishi/new/shiroishi_new.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shiroishi-ku — 札幌市白石区 新着・更新情報\",\"government\",\"batch:50\"]",
     10800,
     "Shiroishi-ku — 札幌市白石区 新着・更新情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 20 entries at probe")

RSSX(jp_b50_city_sapporo_jp_shiroishi_topics_shiroishi_attention, "jp-b50-city-sapporo-jp-shiroishi-topics-shiroishi-attention",
     "Shiroishi-ku — 札幌市白石区 注目情報", "白石区 — 札幌市白石区 注目情報",
     "jp_local", "government",
     "https://www.city.sapporo.jp/shiroishi/topics/shiroishi_attention.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shiroishi-ku — 札幌市白石区 注目情報\",\"government\",\"batch:50\"]",
     10800,
     "Shiroishi-ku — 札幌市白石区 注目情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 1 entries at probe")

RSSX(jp_b50_ifarc_metro_tokyo_lg_jp, "jp-b50-ifarc-metro-tokyo-lg-jp",
     "Tokyo Metropolitan Islands Area Research and Development Center for Agriculture, Forestry and Fisheries — Recent Entries", "東京都島しょ農林水産総合センター — Recent Entries",
     "jp_local", "government",
     "https://www.ifarc.metro.tokyo.lg.jp/atom.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tokyo Metropolitan Islands Area Research and Development Center for Agriculture, Forestry and Fisheries — Recent Entries\",\"government\",\"batch:50\"]",
     10800,
     "Tokyo Metropolitan Islands Area Research and Development Center for Agriculture, Forestry and Fisheries — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 15 entries at probe")

RSSX(jp_b50_keishicho_metro_tokyo_lg_jp_rss_news, "jp-b50-keishicho-metro-tokyo-lg-jp-rss-news",
     "警視庁ホームページ — 新着情報_警視庁ホームページ", "警視庁ホームページ — 新着情報_警視庁ホームページ",
     "jp_local", "government",
     "https://www.keishicho.metro.tokyo.lg.jp/rss_news.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:警視庁ホームページ — 新着情報_警視庁ホームページ\",\"government\",\"batch:50\"]",
     10800,
     "警視庁ホームページ — 新着情報_警視庁ホームページ — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 3 entries at probe")

RSSX(jp_b50_town_ando_nara_jp, "jp-b50-town-ando-nara-jp",
     "Ando — 安堵町役場", "安堵町 — 安堵町役場",
     "jp_local", "government",
     "https://www.town.ando.nara.jp/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ando — 安堵町役場\",\"government\",\"batch:50\"]",
     10800,
     "Ando — 安堵町役場 — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 30 entries at probe")

RSSX(jp_b50_town_harima_lg_jp_hotnews_hotnews, "jp-b50-town-harima-lg-jp-hotnews-hotnews",
     "Harima — 播磨町 ホットニュースのRSS", "播磨町 — 播磨町 ホットニュースのRSS",
     "jp_local", "government",
     "https://www.town.harima.lg.jp/hotnews/hotnews.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Harima — 播磨町 ホットニュースのRSS\",\"government\",\"batch:50\"]",
     10800,
     "Harima — 播磨町 ホットニュースのRSS — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 10 entries at probe")

RSSX(jp_b50_town_harima_lg_jp_shinchaku_shinchaku, "jp-b50-town-harima-lg-jp-shinchaku-shinchaku",
     "Harima — 播磨町 新着情報のRSS", "播磨町 — 播磨町 新着情報のRSS",
     "jp_local", "government",
     "https://www.town.harima.lg.jp/shinchaku/shinchaku.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Harima — 播磨町 新着情報のRSS\",\"government\",\"batch:50\"]",
     10800,
     "Harima — 播磨町 新着情報のRSS — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 10 entries at probe")

RSSX(jp_b50_town_hino_tottori_jp_services_rss10_1752, "jp-b50-town-hino-tottori-jp-services-rss10-1752",
     "Hino — 新着情報(RSS)", "日野町 — 新着情報(RSS)",
     "jp_local", "government",
     "https://www.town.hino.tottori.jp/services/rdf/rss10/1752.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hino — 新着情報(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Hino — 新着情報(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 15 entries at probe")

RSSX(jp_b50_town_hinode_tokyo_jp, "jp-b50-town-hinode-tokyo-jp",
     "Hinode — 日の出町ホームページ", "日の出町 — 日の出町ホームページ",
     "jp_local", "government",
     "https://www.town.hinode.tokyo.jp/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hinode — 日の出町ホームページ\",\"government\",\"batch:50\"]",
     10800,
     "Hinode — 日の出町ホームページ — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 30 entries at probe")

RSSX(jp_b50_town_horonobe_lg_jp_www4, "jp-b50-town-horonobe-lg-jp-www4",
     "Horonobe", "幌延町",
     "jp_local", "government",
     "https://www.town.horonobe.lg.jp/www4/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Horonobe\",\"government\",\"batch:50\"]",
     10800,
     "Horonobe — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 45 entries at probe")

RSSX(jp_b50_town_ibaraki_lg_jp_rss10_1, "jp-b50-town-ibaraki-lg-jp-rss10-1",
     "Ibaraki", "茨城町",
     "jp_local", "government",
     "https://www.town.ibaraki.lg.jp/rss/rss10-1.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ibaraki\",\"government\",\"batch:50\"]",
     10800,
     "Ibaraki — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 6 entries at probe")

RSSX(jp_b50_town_isen_kagoshima_jp_oshirase, "jp-b50-town-isen-kagoshima-jp-oshirase",
     "Isen — 新着情報", "伊仙町 — 新着情報",
     "jp_local", "government",
     "https://www.town.isen.kagoshima.jp/oshirase.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Isen — 新着情報\",\"government\",\"batch:50\"]",
     10800,
     "Isen — 新着情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 5 entries at probe")

RSSX(jp_b50_town_kamikawa_hyogo_jp, "jp-b50-town-kamikawa-hyogo-jp",
     "Kamikawa — 兵庫県神河町", "神河町 — 兵庫県神河町",
     "jp_local", "government",
     "https://www.town.kamikawa.hyogo.jp/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kamikawa — 兵庫県神河町\",\"government\",\"batch:50\"]",
     10800,
     "Kamikawa — 兵庫県神河町 — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 30 entries at probe")

RSSX(jp_b50_town_kaminokawa_lg_jp_rss10_1304011, "jp-b50-town-kaminokawa-lg-jp-rss10-1304011",
     "Kaminokawa", "上三川町",
     "jp_local", "government",
     "https://www.town.kaminokawa.lg.jp/rss/rss10-1304011.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kaminokawa\",\"government\",\"batch:50\"]",
     10800,
     "Kaminokawa — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 7 entries at probe")

RSSX(jp_b50_town_koryo_nara_jp, "jp-b50-town-koryo-nara-jp",
     "Kōryō-chō", "広陵町",
     "jp_local", "government",
     "https://www.town.koryo.nara.jp/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kōryō-chō\",\"government\",\"batch:50\"]",
     10800,
     "Kōryō-chō — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 30 entries at probe")

RSSX(jp_b50_town_oe_yamagata_jp_announce, "jp-b50-town-oe-yamagata-jp-announce",
     "Ōe-machi", "大江町",
     "jp_local", "government",
     "https://www.town.oe.yamagata.jp/feed/announce",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ōe-machi\",\"government\",\"batch:50\"]",
     10800,
     "Ōe-machi — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 10 entries at probe")

RSSX(jp_b50_town_oe_yamagata_jp_news, "jp-b50-town-oe-yamagata-jp-news",
     "Ōe-machi", "大江町",
     "jp_local", "government",
     "https://www.town.oe.yamagata.jp/feed/news",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ōe-machi\",\"government\",\"batch:50\"]",
     10800,
     "Ōe-machi — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 20 entries at probe")

RSSX(jp_b50_town_oharu_aichi_jp_services_rss10_1035, "jp-b50-town-oharu-aichi-jp-services-rss10-1035",
     "Ōharu-chō — お知らせ(RSS)", "大治町 — お知らせ(RSS)",
     "jp_local", "government",
     "https://www.town.oharu.aichi.jp/services/rdf/rss10/1035.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ōharu-chō — お知らせ(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Ōharu-chō — お知らせ(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 15 entries at probe")

RSSX(jp_b50_town_oharu_aichi_jp_services_rss10_1036, "jp-b50-town-oharu-aichi-jp-services-rss10-1036",
     "Ōharu-chō — イベント(RSS)", "大治町 — イベント(RSS)",
     "jp_local", "government",
     "https://www.town.oharu.aichi.jp/services/rdf/rss10/1036.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ōharu-chō — イベント(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Ōharu-chō — イベント(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 13 entries at probe")

RSSX(jp_b50_town_oharu_aichi_jp_services_rss10_1037, "jp-b50-town-oharu-aichi-jp-services-rss10-1037",
     "Ōharu-chō — 募集(RSS)", "大治町 — 募集(RSS)",
     "jp_local", "government",
     "https://www.town.oharu.aichi.jp/services/rdf/rss10/1037.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ōharu-chō — 募集(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Ōharu-chō — 募集(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 11 entries at probe")

RSSX(jp_b50_town_okoppe_lg_jp_cms, "jp-b50-town-okoppe-lg-jp-cms",
     "Okoppe", "興部町",
     "jp_local", "government",
     "https://www.town.okoppe.lg.jp/cms/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Okoppe\",\"government\",\"batch:50\"]",
     10800,
     "Okoppe — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 27 entries at probe")

RSSX(jp_b50_town_saka_lg_jp, "jp-b50-town-saka-lg-jp",
     "Saka", "坂町",
     "jp_local", "government",
     "https://www.town.saka.lg.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Saka\",\"government\",\"batch:50\"]",
     10800,
     "Saka — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 20 entries at probe")

RSSX(jp_b50_town_shimoichi_lg_jp, "jp-b50-town-shimoichi-lg-jp",
     "Shimoichi", "下市町",
     "jp_local", "government",
     "https://www.town.shimoichi.lg.jp/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shimoichi\",\"government\",\"batch:50\"]",
     10800,
     "Shimoichi — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 30 entries at probe")

RSSX(jp_b50_town_shirataka_lg_jp_services_rss10_1097, "jp-b50-town-shirataka-lg-jp-services-rss10-1097",
     "Shirataka — 新着情報(RSS)", "白鷹町 — 新着情報(RSS)",
     "jp_local", "government",
     "https://www.town.shirataka.lg.jp/services/rdf/rss10/1097.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shirataka — 新着情報(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Shirataka — 新着情報(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 15 entries at probe")

RSSX(jp_b50_town_shirataka_lg_jp_services_rss10_1099, "jp-b50-town-shirataka-lg-jp-services-rss10-1099",
     "Shirataka — 募集情報(RSS)", "白鷹町 — 募集情報(RSS)",
     "jp_local", "government",
     "https://www.town.shirataka.lg.jp/services/rdf/rss10/1099.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shirataka — 募集情報(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Shirataka — 募集情報(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 13 entries at probe")

RSSX(jp_b50_town_tara_lg_jp_topix, "jp-b50-town-tara-lg-jp-topix",
     "Tara", "太良町",
     "jp_local", "government",
     "https://www.town.tara.lg.jp/rss/topix.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tara\",\"government\",\"batch:50\"]",
     10800,
     "Tara — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 14 entries at probe")

RSSX(jp_b50_town_tatsugo_lg_jp_topics, "jp-b50-town-tatsugo-lg-jp-topics",
     "Tatsugō-chō — 鹿児島県龍郷町 トピックス", "龍郷町 — 鹿児島県龍郷町 トピックス",
     "jp_local", "government",
     "https://www.town.tatsugo.lg.jp/topics.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tatsugō-chō — 鹿児島県龍郷町 トピックス\",\"government\",\"batch:50\"]",
     10800,
     "Tatsugō-chō — 鹿児島県龍郷町 トピックス — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 10 entries at probe")

RSSX(jp_b50_town_toei_aichi_jp_services_rss10_1035, "jp-b50-town-toei-aichi-jp-services-rss10-1035",
     "Tōei-chō — 新着情報(RSS)", "東栄町 — 新着情報(RSS)",
     "jp_local", "government",
     "https://www.town.toei.aichi.jp/services/rdf/rss10/1035.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tōei-chō — 新着情報(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Tōei-chō — 新着情報(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 15 entries at probe")

RSSX(jp_b50_town_toei_aichi_jp_services_rss10_1036, "jp-b50-town-toei-aichi-jp-services-rss10-1036",
     "Tōei-chō — イベント情報(RSS)", "東栄町 — イベント情報(RSS)",
     "jp_local", "government",
     "https://www.town.toei.aichi.jp/services/rdf/rss10/1036.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tōei-chō — イベント情報(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Tōei-chō — イベント情報(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 12 entries at probe")

RSSX(jp_b50_town_toei_aichi_jp_services_rss10_1037, "jp-b50-town-toei-aichi-jp-services-rss10-1037",
     "Tōei-chō — 募集(RSS)", "東栄町 — 募集(RSS)",
     "jp_local", "government",
     "https://www.town.toei.aichi.jp/services/rdf/rss10/1037.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tōei-chō — 募集(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Tōei-chō — 募集(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 1 entries at probe")

RSSX(jp_b50_town_toei_aichi_jp_services_rss10_1039, "jp-b50-town-toei-aichi-jp-services-rss10-1039",
     "Tōei-chō — 緊急情報(RSS)", "東栄町 — 緊急情報(RSS)",
     "jp_local", "government",
     "https://www.town.toei.aichi.jp/services/rdf/rss10/1039.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tōei-chō — 緊急情報(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Tōei-chō — 緊急情報(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 2 entries at probe")

RSSX(jp_b50_town_tomamae_lg_jp, "jp-b50-town-tomamae-lg-jp",
     "Tomamae", "苫前町",
     "jp_local", "government",
     "http://www.town.tomamae.lg.jp/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tomamae\",\"government\",\"batch:50\"]",
     10800,
     "Tomamae — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 12 entries at probe")

RSSX(jp_b50_town_wakayama_inami_lg_jp, "jp-b50-town-wakayama-inami-lg-jp",
     "Inami", "印南町",
     "jp_local", "government",
     "https://www.town.wakayama-inami.lg.jp/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Inami\",\"government\",\"batch:50\"]",
     10800,
     "Inami — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 30 entries at probe")

RSSX(jp_b50_town_yoshida_shizuoka_jp_services_rss10_11095, "jp-b50-town-yoshida-shizuoka-jp-services-rss10-11095",
     "Yoshida — お知らせ(RSS)", "吉田町 — お知らせ(RSS)",
     "jp_local", "government",
     "https://www.town.yoshida.shizuoka.jp/services/rdf/rss10/11095.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Yoshida — お知らせ(RSS)\",\"government\",\"batch:50\"]",
     10800,
     "Yoshida — お知らせ(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 15 entries at probe")

RSSX(jp_b50_town_yoshitomi_lg_jp_user_feed_out_gyosei_topics, "jp-b50-town-yoshitomi-lg-jp-user-feed-out-gyosei-topics",
     "Yoshitomi — 福岡県築上郡 吉富町行政サイト", "吉富町 — 福岡県築上郡 吉富町行政サイト",
     "jp_local", "government",
     "https://www.town.yoshitomi.lg.jp/user/feed_out/gyosei_topics.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Yoshitomi — 福岡県築上郡 吉富町行政サイト\",\"government\",\"batch:50\"]",
     10800,
     "Yoshitomi — 福岡県築上郡 吉富町行政サイト — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 30 entries at probe")

RSSX(jp_b50_town_zao_miyagi_jp_rss_news, "jp-b50-town-zao-miyagi-jp-rss-news",
     "Zaō-machi — 新着情報_蔵王町トップページ", "蔵王町 — 新着情報_蔵王町トップページ",
     "jp_local", "government",
     "https://www.town.zao.miyagi.jp/rss_news.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Zaō-machi — 新着情報_蔵王町トップページ\",\"government\",\"batch:50\"]",
     10800,
     "Zaō-machi — 新着情報_蔵王町トップページ — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 19 entries at probe")

RSSX(jp_b50_vill_higashinaruse_lg_jp, "jp-b50-vill-higashinaruse-lg-jp",
     "Higashinaruse", "東成瀬村",
     "jp_local", "government",
     "https://vill.higashinaruse.lg.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Higashinaruse\",\"government\",\"batch:50\"]",
     10800,
     "Higashinaruse — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 10 entries at probe")

RSSX(jp_b50_vill_hinohara_tokyo_jp, "jp-b50-vill-hinohara-tokyo-jp",
     "Hinohara — 檜原村ホームページ", "檜原村 — 檜原村ホームページ",
     "jp_local", "government",
     "https://www.vill.hinohara.tokyo.jp/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hinohara — 檜原村ホームページ\",\"government\",\"batch:50\"]",
     10800,
     "Hinohara — 檜原村ホームページ — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 30 entries at probe")

RSSX(jp_b50_vill_nishimera_lg_jp_village, "jp-b50-vill-nishimera-lg-jp-village",
     "Nishimera", "西米良村",
     "jp_local", "government",
     "https://www.vill.nishimera.lg.jp/village/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Nishimera\",\"government\",\"batch:50\"]",
     10800,
     "Nishimera — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 10 entries at probe")

RSSX(jp_b50_vill_sakegawa_yamagata_jp_topics, "jp-b50-vill-sakegawa-yamagata-jp-topics",
     "Sakegawa", "鮭川村",
     "jp_local", "government",
     "https://www.vill.sakegawa.yamagata.jp/feed/topics",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sakegawa\",\"government\",\"batch:50\"]",
     10800,
     "Sakegawa — RSS/Atom feed declared by the official website (Wikidata P856) of this municipal government; 20 entries at probe")

/* ── jp_prefectural ──────────────────────────────────────────────────── */

RSSX(jp_b50_hidaka_pref_hokkaido_lg_jp_news, "jp-b50-hidaka-pref-hokkaido-lg-jp-news",
     "Hokkaido hidaka News", "北海道hidaka振興局",
     "jp_prefectural", "government",
     "https://www.hidaka.pref.hokkaido.lg.jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hokkaido hidaka News\",\"government\",\"batch:50\"]",
     3600,
     "Hokkaido hidaka News — prefectural government feed (50 entries at probe)")

RSSX(jp_b50_iburi_pref_hokkaido_lg_jp_news, "jp-b50-iburi-pref-hokkaido-lg-jp-news",
     "Hokkaido iburi News", "北海道iburi振興局",
     "jp_prefectural", "government",
     "https://www.iburi.pref.hokkaido.lg.jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hokkaido iburi News\",\"government\",\"batch:50\"]",
     3600,
     "Hokkaido iburi News — prefectural government feed (50 entries at probe)")

RSSX(jp_b50_kamikawa_pref_hokkaido_lg_jp_news, "jp-b50-kamikawa-pref-hokkaido-lg-jp-news",
     "Hokkaido kamikawa News", "北海道kamikawa振興局",
     "jp_prefectural", "government",
     "https://www.kamikawa.pref.hokkaido.lg.jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hokkaido kamikawa News\",\"government\",\"batch:50\"]",
     3600,
     "Hokkaido kamikawa News — prefectural government feed (50 entries at probe)")

RSSX(jp_b50_kushiro_pref_hokkaido_lg_jp_news, "jp-b50-kushiro-pref-hokkaido-lg-jp-news",
     "Hokkaido kushiro News", "北海道kushiro振興局",
     "jp_prefectural", "government",
     "https://www.kushiro.pref.hokkaido.lg.jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hokkaido kushiro News\",\"government\",\"batch:50\"]",
     3600,
     "Hokkaido kushiro News — prefectural government feed (50 entries at probe)")

RSSX(jp_b50_police_pref_nara_jp, "jp-b50-police-pref-nara-jp",
     "奈良県警察本部", "奈良県警察本部",
     "jp_prefectural", "government",
     "https://www.police.pref.nara.jp/rss/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:奈良県警察本部\",\"government\",\"batch:50\"]",
     3600,
     "奈良県警察本部 — RSS/Atom feed declared by the official website (Wikidata P856) of this prefectural government; 30 entries at probe")

RSSX(jp_b50_sorachi_pref_hokkaido_lg_jp_news, "jp-b50-sorachi-pref-hokkaido-lg-jp-news",
     "Hokkaido sorachi News", "北海道sorachi振興局",
     "jp_prefectural", "government",
     "https://www.sorachi.pref.hokkaido.lg.jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hokkaido sorachi News\",\"government\",\"batch:50\"]",
     3600,
     "Hokkaido sorachi News — prefectural government feed (50 entries at probe)")

RSSX(jp_b50_tokachi_pref_hokkaido_lg_jp_news, "jp-b50-tokachi-pref-hokkaido-lg-jp-news",
     "Hokkaido tokachi News", "北海道tokachi振興局",
     "jp_prefectural", "government",
     "https://www.tokachi.pref.hokkaido.lg.jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hokkaido tokachi News\",\"government\",\"batch:50\"]",
     3600,
     "Hokkaido tokachi News — prefectural government feed (50 entries at probe)")

/* ── jp_research ─────────────────────────────────────────────────────── */

RSSX(jp_b50_agr_kyushu_u_ac_jp_lab_iscec_wordpress, "jp-b50-agr-kyushu-u-ac-jp-lab-iscec-wordpress",
     "Insect Science and Creative Entomology Center", "昆虫科学・新産業創生研究センター",
     "jp_research", "science",
     "https://www.agr.kyushu-u.ac.jp/lab/iscec/wordpress/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Insect Science and Creative Entomology Center\",\"science\",\"batch:50\"]",
     10800,
     "Insect Science and Creative Entomology Center — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 2 entries at probe")

RSSX(jp_b50_agu_ac_jp_rss2, "jp-b50-agu-ac-jp-rss2",
     "Aichi Gakuin University", "愛知学院大学",
     "jp_research", "science",
     "http://www.agu.ac.jp/rss2.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Aichi Gakuin University\",\"science\",\"batch:50\"]",
     10800,
     "Aichi Gakuin University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_ai_u_tokyo_ac_jp_ja, "jp-b50-ai-u-tokyo-ac-jp-ja",
     "Next Generation Artificial Intelligence Research Center, The University of Tokyo", "東京大学次世代知能科学研究センター",
     "jp_research", "science",
     "https://www.ai.u-tokyo.ac.jp/ja/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Next Generation Artificial Intelligence Research Center, The University of Tokyo\",\"science\",\"batch:50\"]",
     10800,
     "Next Generation Artificial Intelligence Research Center, The University of Tokyo — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_aichi_u_ac_jp_rss_id_103580, "jp-b50-aichi-u-ac-jp-rss-id-103580",
     "Aichi University — 愛知大学 - 公開講座・講演会 / 愛知大学オープンカレッジ", "愛知大学 — 愛知大学 - 公開講座・講演会 / 愛知大学オープンカレッジ",
     "jp_research", "science",
     "https://www.aichi-u.ac.jp/rss.xml?id=103580",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Aichi University — 愛知大学 - 公開講座・講演会 / 愛知大学オープンカレッジ\",\"science\",\"batch:50\"]",
     10800,
     "Aichi University — 愛知大学 - 公開講座・講演会 / 愛知大学オープンカレッジ — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_aichi_u_ac_jp_rss_id_39875, "jp-b50-aichi-u-ac-jp-rss-id-39875",
     "Aichi University — 愛知大学 - NEWS", "愛知大学 — 愛知大学 - NEWS",
     "jp_research", "science",
     "https://www.aichi-u.ac.jp/rss.xml?id=39875",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Aichi University — 愛知大学 - NEWS\",\"science\",\"batch:50\"]",
     10800,
     "Aichi University — 愛知大学 - NEWS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_catherine_ac_jp, "jp-b50-catherine-ac-jp",
     "St. Catherine University", "聖カタリナ大学",
     "jp_research", "science",
     "https://www.catherine.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:St. Catherine University\",\"science\",\"batch:50\"]",
     10800,
     "St. Catherine University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(jp_b50_ceglos_u_shizuoka_ken_ac_jp, "jp-b50-ceglos-u-shizuoka-ken-ac-jp",
     "Center for Global Studies, Graduate School of International Relations, University of Shizuoka", "静岡県立大学大学院国際関係学研究科附属グローバル・スタディーズ研究センター",
     "jp_research", "science",
     "https://ceglos.u-shizuoka-ken.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Center for Global Studies, Graduate School of International Relations, University of Shizuoka\",\"science\",\"batch:50\"]",
     10800,
     "Center for Global Studies, Graduate School of International Relations, University of Shizuoka — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_coiu_ac_jp, "jp-b50-coiu-ac-jp",
     "Co-Innovation University", "コー・イノベーション大学",
     "jp_research", "science",
     "https://coiu.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Co-Innovation University\",\"science\",\"batch:50\"]",
     10800,
     "Co-Innovation University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(jp_b50_design_kyushu_u_ac_jp, "jp-b50-design-kyushu-u-ac-jp",
     "Kyushu Institute of Design", "九州芸術工科大学",
     "jp_research", "science",
     "https://www.design.kyushu-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kyushu Institute of Design\",\"science\",\"batch:50\"]",
     10800,
     "Kyushu Institute of Design — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_do_bunkyodai_ac_jp, "jp-b50-do-bunkyodai-ac-jp",
     "Hokkaido Bunkyo University", "北海道文教大学",
     "jp_research", "science",
     "https://www.do-bunkyodai.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hokkaido Bunkyo University\",\"science\",\"batch:50\"]",
     10800,
     "Hokkaido Bunkyo University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(jp_b50_english_otaru_uc_ac_jp, "jp-b50-english-otaru-uc-ac-jp",
     "Otaru University of Commerce", "小樽商科大学",
     "jp_research", "science",
     "https://english.otaru-uc.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Otaru University of Commerce\",\"science\",\"batch:50\"]",
     10800,
     "Otaru University of Commerce — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 6 entries at probe")

RSSX(jp_b50_es_hokudai_ac_jp, "jp-b50-es-hokudai-ac-jp",
     "Research Institute for Electronic Science, Hokkaido University", "北海道大学電子科学研究所",
     "jp_research", "science",
     "https://www.es.hokudai.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Research Institute for Electronic Science, Hokkaido University\",\"science\",\"batch:50\"]",
     10800,
     "Research Institute for Electronic Science, Hokkaido University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_fukuoka_u_ac_jp_news, "jp-b50-fukuoka-u-ac-jp-news",
     "Fukuoka University — 福岡大学ニュース", "福岡大学 — 福岡大学ニュース",
     "jp_research", "science",
     "https://www.fukuoka-u.ac.jp/rss/news.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Fukuoka University — 福岡大学ニュース\",\"science\",\"batch:50\"]",
     10800,
     "Fukuoka University — 福岡大学ニュース — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(jp_b50_fukushima_u_ac_jp, "jp-b50-fukushima-u-ac-jp",
     "Fukushima University", "福島大学",
     "jp_research", "science",
     "https://www.fukushima-u.ac.jp/atom.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Fukushima University\",\"science\",\"batch:50\"]",
     10800,
     "Fukushima University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_ganken_cri_kanazawa_u_ac_jp, "jp-b50-ganken-cri-kanazawa-u-ac-jp",
     "金沢大学がん進展制御研究所", "金沢大学がん進展制御研究所",
     "jp_research", "science",
     "https://ganken.cri.kanazawa-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:金沢大学がん進展制御研究所\",\"science\",\"batch:50\"]",
     10800,
     "金沢大学がん進展制御研究所 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_hcu_ac_jp, "jp-b50-hcu-ac-jp",
     "Hiroshima Cosmopolitan University", "広島都市学園大学",
     "jp_research", "science",
     "https://www.hcu.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hiroshima Cosmopolitan University\",\"science\",\"batch:50\"]",
     10800,
     "Hiroshima Cosmopolitan University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_hi_tech_ac_jp, "jp-b50-hi-tech-ac-jp",
     "Hachinohe Institute of Technology", "八戸工業大学",
     "jp_research", "science",
     "https://www.hi-tech.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hachinohe Institute of Technology\",\"science\",\"batch:50\"]",
     10800,
     "Hachinohe Institute of Technology — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_hirosaki_u_ac_jp, "jp-b50-hirosaki-u-ac-jp",
     "Hirosaki University", "弘前大学",
     "jp_research", "science",
     "https://www.hirosaki-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hirosaki University\",\"science\",\"batch:50\"]",
     10800,
     "Hirosaki University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_hokkai_ac_jp, "jp-b50-hokkai-ac-jp",
     "Hokkai School of Commerce", "北海商科大学",
     "jp_research", "science",
     "https://www.hokkai.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hokkai School of Commerce\",\"science\",\"batch:50\"]",
     10800,
     "Hokkai School of Commerce — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_hosen_ac_jp, "jp-b50-hosen-ac-jp",
     "Hosen College of Childhood Education", "こども教育宝仙大学",
     "jp_research", "science",
     "https://www.hosen.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Hosen College of Childhood Education\",\"science\",\"batch:50\"]",
     10800,
     "Hosen College of Childhood Education — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_iamas_ac_jp, "jp-b50-iamas-ac-jp",
     "Institute of Advanced Media Arts and Sciences — 情報科学芸術大学院大学 [IAMAS]", "情報科学芸術大学院大学 — 情報科学芸術大学院大学 [IAMAS]",
     "jp_research", "science",
     "https://www.iamas.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Institute of Advanced Media Arts and Sciences — 情報科学芸術大学院大学 [IAMAS]\",\"science\",\"batch:50\"]",
     10800,
     "Institute of Advanced Media Arts and Sciences — 情報科学芸術大学院大学 [IAMAS] — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_icl_keio_ac_jp, "jp-b50-icl-keio-ac-jp",
     "The Keio Institute of Cultural and Linguistic Studies — Recent Entries", "慶應義塾大学言語文化研究所 — Recent Entries",
     "jp_research", "science",
     "https://www.icl.keio.ac.jp/atom.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:The Keio Institute of Cultural and Linguistic Studies — Recent Entries\",\"science\",\"batch:50\"]",
     10800,
     "The Keio Institute of Cultural and Linguistic Studies — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_idac_tohoku_ac_jp_site_ja, "jp-b50-idac-tohoku-ac-jp-site-ja",
     "Institute of Development, Aging and Cancer (IDAC)", "東北大学加齢医学研究所",
     "jp_research", "science",
     "https://www.idac.tohoku.ac.jp/site_ja/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Institute of Development, Aging and Cancer (IDAC)\",\"science\",\"batch:50\"]",
     10800,
     "Institute of Development, Aging and Cancer (IDAC) — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_ipu_japan_ac_jp, "jp-b50-ipu-japan-ac-jp",
     "International Pacific University", "環太平洋大学",
     "jp_research", "science",
     "https://ipu-japan.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:International Pacific University\",\"science\",\"batch:50\"]",
     10800,
     "International Pacific University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 500 entries at probe")

RSSX(jp_b50_iqb_u_tokyo_ac_jp_en, "jp-b50-iqb-u-tokyo-ac-jp-en",
     "Institute for Quantitative Biosciences", "東京大学定量生命科学研究所",
     "jp_research", "science",
     "https://www.iqb.u-tokyo.ac.jp/en/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Institute for Quantitative Biosciences\",\"science\",\"batch:50\"]",
     10800,
     "Institute for Quantitative Biosciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_iqb_u_tokyo_ac_jp_en_top_2, "jp-b50-iqb-u-tokyo-ac-jp-en-top-2",
     "Institute for Quantitative Biosciences — IQB, The University of Tokyo » TOP Comments Feed", "東京大学定量生命科学研究所 — IQB, The University of Tokyo » TOP Comments Feed",
     "jp_research", "science",
     "https://www.iqb.u-tokyo.ac.jp/en/top-2/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Institute for Quantitative Biosciences — IQB, The University of Tokyo » TOP Comments Feed\",\"science\",\"batch:50\"]",
     10800,
     "Institute for Quantitative Biosciences — IQB, The University of Tokyo » TOP Comments Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(jp_b50_ishikawa_nu_ac_jp_eng, "jp-b50-ishikawa-nu-ac-jp-eng",
     "Ishikawa Prefectural Nursing University", "石川県立看護大学",
     "jp_research", "science",
     "https://www.ishikawa-nu.ac.jp/eng/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Ishikawa Prefectural Nursing University\",\"science\",\"batch:50\"]",
     10800,
     "Ishikawa Prefectural Nursing University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(jp_b50_iwate_pu_ac_jp_feed_page, "jp-b50-iwate-pu-ac-jp-feed-page",
     "Iwate Prefectural University — 公立大学法人 岩手県立大学 Feed", "岩手県立大学 — 公立大学法人 岩手県立大学 Feed",
     "jp_research", "science",
     "https://www.iwate-pu.ac.jp/feeds/feed-page.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Iwate Prefectural University — 公立大学法人 岩手県立大学 Feed\",\"science\",\"batch:50\"]",
     10800,
     "Iwate Prefectural University — 公立大学法人 岩手県立大学 Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 30 entries at probe")

RSSX(jp_b50_iwate_uhms_ac_jp, "jp-b50-iwate-uhms-ac-jp",
     "Iwate University of Health and Medical Sciences", "岩手保健医療大学",
     "jp_research", "science",
     "https://www.iwate-uhms.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Iwate University of Health and Medical Sciences\",\"science\",\"batch:50\"]",
     10800,
     "Iwate University of Health and Medical Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(jp_b50_jhsu_ac_jp_blog_feed, "jp-b50-jhsu-ac-jp-blog-feed",
     "Japan University of Health Science — 日本保健医療大学_HP - RSS", "日本保健医療大学 — 日本保健医療大学_HP - RSS",
     "jp_research", "science",
     "https://www.jhsu.ac.jp/blog-feed.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Japan University of Health Science — 日本保健医療大学_HP - RSS\",\"science\",\"batch:50\"]",
     10800,
     "Japan University of Health Science — 日本保健医療大学_HP - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 2 entries at probe")

RSSX(jp_b50_jikei_ac_jp, "jp-b50-jikei-ac-jp",
     "Jikei University School of Medicine", "東京慈恵会医科大学",
     "jp_research", "science",
     "https://www.jikei.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Jikei University School of Medicine\",\"science\",\"batch:50\"]",
     10800,
     "Jikei University School of Medicine — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_jrckicn_ac_jp, "jp-b50-jrckicn-ac-jp",
     "The Japanese Red Cross Kyushu International College of Nursing", "日本赤十字九州国際看護大学",
     "jp_research", "science",
     "https://www.jrckicn.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:The Japanese Red Cross Kyushu International College of Nursing\",\"science\",\"batch:50\"]",
     10800,
     "The Japanese Red Cross Kyushu International College of Nursing — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_jue_ac_jp, "jp-b50-jue-ac-jp",
     "Japan University of Economics", "日本経済大学",
     "jp_research", "science",
     "https://www.jue.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Japan University of Economics\",\"science\",\"batch:50\"]",
     10800,
     "Japan University of Economics — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kaichi_ac_jp, "jp-b50-kaichi-ac-jp",
     "Kaichi International University", "開智国際大学",
     "jp_research", "science",
     "https://www.kaichi.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kaichi International University\",\"science\",\"batch:50\"]",
     10800,
     "Kaichi International University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kcua_ac_jp, "jp-b50-kcua-ac-jp",
     "Kyoto City University of Arts", "京都市立芸術大学",
     "jp_research", "science",
     "https://www.kcua.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kyoto City University of Arts\",\"science\",\"batch:50\"]",
     10800,
     "Kyoto City University of Arts — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kitakyu_u_ac_jp, "jp-b50-kitakyu-u-ac-jp",
     "University of Kitakyushu — Recent Entries", "北九州市立大学 — Recent Entries",
     "jp_research", "science",
     "https://www.kitakyu-u.ac.jp/atom.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:University of Kitakyushu — Recent Entries\",\"science\",\"batch:50\"]",
     10800,
     "University of Kitakyushu — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_kiu_ac_jp, "jp-b50-kiu-ac-jp",
     "Kyushu International University", "九州国際大学",
     "jp_research", "science",
     "https://www.kiu.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kyushu International University\",\"science\",\"batch:50\"]",
     10800,
     "Kyushu International University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kki_ac_jp, "jp-b50-kki-ac-jp",
     "Kansai University of Nursing and Health Sciences", "関西看護医療大学",
     "jp_research", "science",
     "https://www.kki.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kansai University of Nursing and Health Sciences\",\"science\",\"batch:50\"]",
     10800,
     "Kansai University of Nursing and Health Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kochi_gu_ac_jp, "jp-b50-kochi-gu-ac-jp",
     "Kochi Gakuen University", "高知学園大学",
     "jp_research", "science",
     "https://kochi-gu.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kochi Gakuen University\",\"science\",\"batch:50\"]",
     10800,
     "Kochi Gakuen University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_koka_ac_jp, "jp-b50-koka-ac-jp",
     "Kyoto Koka Women's University", "京都光華女子大学",
     "jp_research", "science",
     "https://www.koka.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kyoto Koka Women's University\",\"science\",\"batch:50\"]",
     10800,
     "Kyoto Koka Women's University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(jp_b50_korea_u_ac_jp, "jp-b50-korea-u-ac-jp",
     "朝鮮大学校 |", "朝鮮大学校 |",
     "jp_research", "science",
     "https://www.korea-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:朝鮮大学校 |\",\"science\",\"batch:50\"]",
     10800,
     "朝鮮大学校 | — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kumamoto_u_ac_jp, "jp-b50-kumamoto-u-ac-jp",
     "Kumamoto University — サイト - RSS 1.0", "熊本大学 — サイト - RSS 1.0",
     "jp_research", "science",
     "https://www.kumamoto-u.ac.jp/RSS",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kumamoto University — サイト - RSS 1.0\",\"science\",\"batch:50\"]",
     10800,
     "Kumamoto University — サイト - RSS 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_kumamoto_u_ac_jp_2, "jp-b50-kumamoto-u-ac-jp-2",
     "Kumamoto University — サイト - RSS 2.0", "熊本大学 — サイト - RSS 2.0",
     "jp_research", "science",
     "https://www.kumamoto-u.ac.jp/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kumamoto University — サイト - RSS 2.0\",\"science\",\"batch:50\"]",
     10800,
     "Kumamoto University — サイト - RSS 2.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_kumamoto_u_ac_jp_3, "jp-b50-kumamoto-u-ac-jp-3",
     "Kumamoto University — サイト - Atom", "熊本大学 — サイト - Atom",
     "jp_research", "science",
     "https://www.kumamoto-u.ac.jp/atom.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kumamoto University — サイト - Atom\",\"science\",\"batch:50\"]",
     10800,
     "Kumamoto University — サイト - Atom — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_kunitachi_ac_jp_news, "jp-b50-kunitachi-ac-jp-news",
     "Kunitachi College of Music — ニュース - ［国立音楽大学 - くにたちおんがくだいがく］- RSS", "国立音楽大学 — ニュース - ［国立音楽大学 - くにたちおんがくだいがく］- RSS",
     "jp_research", "science",
     "https://www.kunitachi.ac.jp/NEWS/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kunitachi College of Music — ニュース - ［国立音楽大学 - くにたちおんがくだいがく］- RSS\",\"science\",\"batch:50\"]",
     10800,
     "Kunitachi College of Music — ニュース - ［国立音楽大学 - くにたちおんがくだいがく］- RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 29 entries at probe")

RSSX(jp_b50_kunitachi_ac_jp_news_rss_event, "jp-b50-kunitachi-ac-jp-news-rss-event",
     "Kunitachi College of Music — 演奏会・イベント - ［国立音楽大学 - くにたちおんがくだいがく］- RSS", "国立音楽大学 — 演奏会・イベント - ［国立音楽大学 - くにたちおんがくだいがく］- RSS",
     "jp_research", "science",
     "https://www.kunitachi.ac.jp/NEWS/rss_event.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kunitachi College of Music — 演奏会・イベント - ［国立音楽大学 - くにたちおんがくだいがく］- RSS\",\"science\",\"batch:50\"]",
     10800,
     "Kunitachi College of Music — 演奏会・イベント - ［国立音楽大学 - くにたちおんがくだいがく］- RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 29 entries at probe")

RSSX(jp_b50_kunitachi_ac_jp_news_rss_important, "jp-b50-kunitachi-ac-jp-news-rss-important",
     "Kunitachi College of Music — 重要なお知らせ - ［国立音楽大学 - くにたちおんがくだいがく］- RSS", "国立音楽大学 — 重要なお知らせ - ［国立音楽大学 - くにたちおんがくだいがく］- RSS",
     "jp_research", "science",
     "https://www.kunitachi.ac.jp/NEWS/rss_important.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kunitachi College of Music — 重要なお知らせ - ［国立音楽大学 - くにたちおんがくだいがく］- RSS\",\"science\",\"batch:50\"]",
     10800,
     "Kunitachi College of Music — 重要なお知らせ - ［国立音楽大学 - くにたちおんがくだいがく］- RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 30 entries at probe")

RSSX(jp_b50_kushiro_pu_ac_jp_news, "jp-b50-kushiro-pu-ac-jp-news",
     "Kushiro Public University of Economics", "釧路公立大学",
     "jp_research", "science",
     "https://www.kushiro-pu.ac.jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kushiro Public University of Economics\",\"science\",\"batch:50\"]",
     10800,
     "Kushiro Public University of Economics — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kusw_ac_jp, "jp-b50-kusw-ac-jp",
     "Kansai University of Social Welfare", "関西福祉大学",
     "jp_research", "science",
     "https://www.kusw.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kansai University of Social Welfare\",\"science\",\"batch:50\"]",
     10800,
     "Kansai University of Social Welfare — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kyorin_u_ac_jp_cn_html_kyorin_00003_top_00004, "jp-b50-kyorin-u-ac-jp-cn-html-kyorin-00003-top-00004",
     "Kyorin University — 杏林大学 重要なお知らせ", "杏林大学 — 杏林大学 重要なお知らせ",
     "jp_research", "science",
     "https://www.kyorin-u.ac.jp/cn/html/kyorin/00003/top_00004.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kyorin University — 杏林大学 重要なお知らせ\",\"science\",\"batch:50\"]",
     10800,
     "Kyorin University — 杏林大学 重要なお知らせ — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(jp_b50_kyorin_u_ac_jp_cn_html_kyorin_list_all, "jp-b50-kyorin-u-ac-jp-cn-html-kyorin-list-all",
     "Kyorin University — 杏林大学 全カテゴリ", "杏林大学 — 杏林大学 全カテゴリ",
     "jp_research", "science",
     "https://www.kyorin-u.ac.jp/cn/html/kyorin/list_all.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kyorin University — 杏林大学 全カテゴリ\",\"science\",\"batch:50\"]",
     10800,
     "Kyorin University — 杏林大学 全カテゴリ — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 200 entries at probe")

RSSX(jp_b50_kyoto_seika_ac_jp, "jp-b50-kyoto-seika-ac-jp",
     "Kyoto Seika University", "京都精華大学",
     "jp_research", "science",
     "https://www.kyoto-seika.ac.jp/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kyoto Seika University\",\"science\",\"batch:50\"]",
     10800,
     "Kyoto Seika University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kyotokango_ac_jp, "jp-b50-kyotokango-ac-jp",
     "学校法人 京都育英館 京都看護大学", "京都看護大学",
     "jp_research", "science",
     "https://www.kyotokango.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:学校法人 京都育英館 京都看護大学\",\"science\",\"batch:50\"]",
     10800,
     "学校法人 京都育英館 京都看護大学 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_kyutech_ac_jp_rss2, "jp-b50-kyutech-ac-jp-rss2",
     "Kyushu Institute of Technology", "九州工業大学",
     "jp_research", "science",
     "http://www.kyutech.ac.jp/rss2.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Kyushu Institute of Technology\",\"science\",\"batch:50\"]",
     10800,
     "Kyushu Institute of Technology — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_miyasankei_u_ac_jp, "jp-b50-miyasankei-u-ac-jp",
     "Miyazaki Sangyo-keiei University", "宮崎産業経営大学",
     "jp_research", "science",
     "https://www.miyasankei-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Miyazaki Sangyo-keiei University\",\"science\",\"batch:50\"]",
     10800,
     "Miyazaki Sangyo-keiei University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 50 entries at probe")

RSSX(jp_b50_miyazaki_u_ac_jp_newsrelease, "jp-b50-miyazaki-u-ac-jp-newsrelease",
     "University of Miyazaki", "宮崎大学",
     "jp_research", "science",
     "https://www.miyazaki-u.ac.jp/newsrelease/atom.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:University of Miyazaki\",\"science\",\"batch:50\"]",
     10800,
     "University of Miyazaki — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_nafu_ac_jp_rss2, "jp-b50-nafu-ac-jp-rss2",
     "Niigata Agro-Food University", "新潟食料農業大学",
     "jp_research", "science",
     "https://nafu.ac.jp/rss2.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Niigata Agro-Food University\",\"science\",\"batch:50\"]",
     10800,
     "Niigata Agro-Food University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_nafu_ac_jp_rss2_strengthened, "jp-b50-nafu-ac-jp-rss2-strengthened",
     "Niigata Agro-Food University", "新潟食料農業大学",
     "jp_research", "science",
     "https://nafu.ac.jp/rss2_strengthened.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Niigata Agro-Food University\",\"science\",\"batch:50\"]",
     10800,
     "Niigata Agro-Food University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_nagaokaut_ac_jp_news, "jp-b50-nagaokaut-ac-jp-news",
     "Nagaoka University of Technology — 長岡技術科学大学ニュースRSS", "長岡技術科学大学 — 長岡技術科学大学ニュースRSS",
     "jp_research", "science",
     "https://www.nagaokaut.ac.jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Nagaoka University of Technology — 長岡技術科学大学ニュースRSS\",\"science\",\"batch:50\"]",
     10800,
     "Nagaoka University of Technology — 長岡技術科学大学ニュースRSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_nagoya_su_ac_jp, "jp-b50-nagoya-su-ac-jp",
     "Nagoya Sangyo University", "名古屋産業大学",
     "jp_research", "science",
     "https://www.nagoya-su.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Nagoya Sangyo University\",\"science\",\"batch:50\"]",
     10800,
     "Nagoya Sangyo University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_nao_ac_jp_en, "jp-b50-nao-ac-jp-en",
     "National Astronomical Observatory of Japan — NAOJ | News", "国立天文台 — NAOJ | News",
     "jp_research", "science",
     "https://www.nao.ac.jp/en/atom.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:National Astronomical Observatory of Japan — NAOJ | News\",\"science\",\"batch:50\"]",
     10800,
     "National Astronomical Observatory of Japan — NAOJ | News — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(jp_b50_nayoro_ac_jp, "jp-b50-nayoro-ac-jp",
     "Nayoro City University", "名寄市立大学",
     "jp_research", "science",
     "http://www.nayoro.ac.jp/rss/feed.rss",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Nayoro City University\",\"science\",\"batch:50\"]",
     10800,
     "Nayoro City University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_nihonbunka_u_ac_jp, "jp-b50-nihonbunka-u-ac-jp",
     "Nihon Bunka University", "日本文化大学",
     "jp_research", "science",
     "https://nihonbunka-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Nihon Bunka University\",\"science\",\"batch:50\"]",
     10800,
     "Nihon Bunka University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_nucba_ac_jp_en_rss2, "jp-b50-nucba-ac-jp-en-rss2",
     "Nagoya University of Commerce & Business", "名古屋商科大学",
     "jp_research", "science",
     "https://www.nucba.ac.jp/en/rss2.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Nagoya University of Commerce & Business\",\"science\",\"batch:50\"]",
     10800,
     "Nagoya University of Commerce & Business — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_okayama_u_ac_jp_okadai_admission_news_ja, "jp-b50-okayama-u-ac-jp-okadai-admission-news-ja",
     "Okayama University — 国立大学法人 岡山大学 入試情報", "岡山大学 — 国立大学法人 岡山大学 入試情報",
     "jp_research", "science",
     "https://www.okayama-u.ac.jp/xml/okadai_admission_news_ja.rss",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Okayama University — 国立大学法人 岡山大学 入試情報\",\"science\",\"batch:50\"]",
     10800,
     "Okayama University — 国立大学法人 岡山大学 入試情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_okayama_u_ac_jp_okadai_event_2_ja, "jp-b50-okayama-u-ac-jp-okadai-event-2-ja",
     "Okayama University — 国立大学法人 岡山大学 イベント", "岡山大学 — 国立大学法人 岡山大学 イベント",
     "jp_research", "science",
     "https://www.okayama-u.ac.jp/xml/okadai_event_2_ja.rss",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Okayama University — 国立大学法人 岡山大学 イベント\",\"science\",\"batch:50\"]",
     10800,
     "Okayama University — 国立大学法人 岡山大学 イベント — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_okayama_u_ac_jp_okadai_news_2_ja, "jp-b50-okayama-u-ac-jp-okadai-news-2-ja",
     "Okayama University — 国立大学法人 岡山大学 ニュース", "岡山大学 — 国立大学法人 岡山大学 ニュース",
     "jp_research", "science",
     "https://www.okayama-u.ac.jp/xml/okadai_news_2_ja.rss",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Okayama University — 国立大学法人 岡山大学 ニュース\",\"science\",\"batch:50\"]",
     10800,
     "Okayama University — 国立大学法人 岡山大学 ニュース — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_okayama_u_ac_jp_okadai_release_ja, "jp-b50-okayama-u-ac-jp-okadai-release-ja",
     "Okayama University — 国立大学法人 岡山大学 プレスリリース", "岡山大学 — 国立大学法人 岡山大学 プレスリリース",
     "jp_research", "science",
     "https://www.okayama-u.ac.jp/xml/okadai_release_ja.rss",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Okayama University — 国立大学法人 岡山大学 プレスリリース\",\"science\",\"batch:50\"]",
     10800,
     "Okayama University — 国立大学法人 岡山大学 プレスリリース — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_okayama_u_ac_jp_okadai_topix_2_ja, "jp-b50-okayama-u-ac-jp-okadai-topix-2-ja",
     "Okayama University — 国立大学法人 岡山大学 トピックス", "岡山大学 — 国立大学法人 岡山大学 トピックス",
     "jp_research", "science",
     "https://www.okayama-u.ac.jp/xml/okadai_topix_2_ja.rss",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Okayama University — 国立大学法人 岡山大学 トピックス\",\"science\",\"batch:50\"]",
     10800,
     "Okayama University — 国立大学法人 岡山大学 トピックス — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_okigei_ac_jp, "jp-b50-okigei-ac-jp",
     "Okinawa Prefectural University of Arts", "沖縄県立芸術大学",
     "jp_research", "science",
     "https://www.okigei.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Okinawa Prefectural University of Arts\",\"science\",\"batch:50\"]",
     10800,
     "Okinawa Prefectural University of Arts — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_osu_ac_jp, "jp-b50-osu-ac-jp",
     "Okayama Shoka University", "岡山商科大学",
     "jp_research", "science",
     "https://www.osu.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Okayama Shoka University\",\"science\",\"batch:50\"]",
     10800,
     "Okayama Shoka University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(jp_b50_otaru_uc_ac_jp, "jp-b50-otaru-uc-ac-jp",
     "Otaru University of Commerce", "小樽商科大学",
     "jp_research", "science",
     "https://www.otaru-uc.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Otaru University of Commerce\",\"science\",\"batch:50\"]",
     10800,
     "Otaru University of Commerce — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_plaza_umin_ac_jp_jcts, "jp-b50-plaza-umin-ac-jp-jcts",
     "The Japan Council of Traffic Science", "一般社団法人日本交通科学学会",
     "jp_research", "science",
     "https://plaza.umin.ac.jp/jcts/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:The Japan Council of Traffic Science\",\"science\",\"batch:50\"]",
     10800,
     "The Japan Council of Traffic Science — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_riis_okayama_u_ac_jp_m, "jp-b50-riis-okayama-u-ac-jp-m",
     "Research Institute for Interdisciplinary Science — Research Institute for Interdisciplinary Science | Okayama U", "異分野基礎科学研究所 — Research Institute for Interdisciplinary Science | Okayama U",
     "jp_research", "science",
     "https://www.riis.okayama-u.ac.jp/m/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Research Institute for Interdisciplinary Science — Research Institute for Interdisciplinary Science | Okayama U\",\"science\",\"batch:50\"]",
     10800,
     "Research Institute for Interdisciplinary Science — Research Institute for Interdisciplinary Science | Okayama U — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_riis_okayama_u_ac_jp_m_research_highlights_feed_rss2, "jp-b50-riis-okayama-u-ac-jp-m-research-highlights-feed-rss2",
     "Research Institute for Interdisciplinary Science — Research Highlights of Okayama University", "異分野基礎科学研究所 — Research Highlights of Okayama University",
     "jp_research", "science",
     "https://www.riis.okayama-u.ac.jp/m/research_highlights/?feed=rss2",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Research Institute for Interdisciplinary Science — Research Highlights of Okayama University\",\"science\",\"batch:50\"]",
     10800,
     "Research Institute for Interdisciplinary Science — Research Highlights of Okayama University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(jp_b50_s_sph_ac_jp_rss2, "jp-b50-s-sph-ac-jp-rss2",
     "Shizuoka Graduate University of Public Health", "静岡社会健康医学大学院大学",
     "jp_research", "science",
     "https://s-sph.ac.jp/rss2.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shizuoka Graduate University of Public Health\",\"science\",\"batch:50\"]",
     10800,
     "Shizuoka Graduate University of Public Health — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_sbi_u_ac_jp_rss_id_35163, "jp-b50-sbi-u-ac-jp-rss-id-35163",
     "SBI Graduate School — 通信制（オンライン）ＭＢＡのSBI大学院大学 - 最新情報はこちら｜通信制（オンライン・日本国内）MBAのSBI大学院大", "SBI大学院大学 — 通信制（オンライン）ＭＢＡのSBI大学院大学 - 最新情報はこちら｜通信制（オンライン・日本国内）MBAのSBI大学院大",
     "jp_research", "science",
     "https://www.sbi-u.ac.jp/rss.xml?id=35163",
     "ja", "[\"gov\",\"country:jp\",\"publisher:SBI Graduate School — 通信制（オンライン）ＭＢＡのSBI大学院大学 - 最新情報はこちら｜通信制（オンライン・日本国内）MBAのSBI大学院大\",\"science\",\"batch:50\"]",
     10800,
     "SBI Graduate School — 通信制（オンライン）ＭＢＡのSBI大学院大学 - 最新情報はこちら｜通信制（オンライン・日本国内）MBAのSBI大学院大 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_sbi_u_ac_jp_rss_id_50463, "jp-b50-sbi-u-ac-jp-rss-id-50463",
     "SBI Graduate School — 通信制（オンライン）ＭＢＡのSBI大学院大学 - 教員の活動 アーカイブ｜通信制（オンライン・日本国内）MBAのSBI大", "SBI大学院大学 — 通信制（オンライン）ＭＢＡのSBI大学院大学 - 教員の活動 アーカイブ｜通信制（オンライン・日本国内）MBAのSBI大",
     "jp_research", "science",
     "https://www.sbi-u.ac.jp/rss.xml?id=50463",
     "ja", "[\"gov\",\"country:jp\",\"publisher:SBI Graduate School — 通信制（オンライン）ＭＢＡのSBI大学院大学 - 教員の活動 アーカイブ｜通信制（オンライン・日本国内）MBAのSBI大\",\"science\",\"batch:50\"]",
     10800,
     "SBI Graduate School — 通信制（オンライン）ＭＢＡのSBI大学院大学 - 教員の活動 アーカイブ｜通信制（オンライン・日本国内）MBAのSBI大 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_seisen_ac_jp_rss_id_99885, "jp-b50-seisen-ac-jp-rss-id-99885",
     "Seisen University — 聖泉大学 - NEWS & TOPICS", "聖泉大学 — 聖泉大学 - NEWS & TOPICS",
     "jp_research", "science",
     "https://www.seisen.ac.jp/rss.xml?id=99885",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Seisen University — 聖泉大学 - NEWS & TOPICS\",\"science\",\"batch:50\"]",
     10800,
     "Seisen University — 聖泉大学 - NEWS & TOPICS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_sgu_ac_jp, "jp-b50-sgu-ac-jp",
     "Sapporo Gakuin University", "札幌学院大学",
     "jp_research", "science",
     "https://www.sgu.ac.jp:443/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sapporo Gakuin University\",\"science\",\"batch:50\"]",
     10800,
     "Sapporo Gakuin University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_shizenkan_ac_jp_en, "jp-b50-shizenkan-ac-jp-en",
     "Shizenkan University", "大学院大学至善館",
     "jp_research", "science",
     "https://shizenkan.ac.jp/en/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shizenkan University\",\"science\",\"batch:50\"]",
     10800,
     "Shizenkan University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_shoin_u_ac_jp, "jp-b50-shoin-u-ac-jp",
     "Shoin University", "松蔭大学",
     "jp_research", "science",
     "https://www.shoin-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shoin University\",\"science\",\"batch:50\"]",
     10800,
     "Shoin University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_shubun_ac_jp, "jp-b50-shubun-ac-jp",
     "Shubun University", "修文大学",
     "jp_research", "science",
     "https://www.shubun.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shubun University\",\"science\",\"batch:50\"]",
     10800,
     "Shubun University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_shumei_u_ac_jp, "jp-b50-shumei-u-ac-jp",
     "Shumei University", "秀明大学",
     "jp_research", "science",
     "https://www.shumei-u.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shumei University\",\"science\",\"batch:50\"]",
     10800,
     "Shumei University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_sutoku_u_ac_jp, "jp-b50-sutoku-u-ac-jp",
     "Nagaoka Sutoku University — 長岡崇徳大学｜看護学部・看護学科 RSS Feed", "長岡崇徳大学 — 長岡崇徳大学｜看護学部・看護学科 RSS Feed",
     "jp_research", "science",
     "https://sutoku-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Nagaoka Sutoku University — 長岡崇徳大学｜看護学部・看護学科 RSS Feed\",\"science\",\"batch:50\"]",
     10800,
     "Nagaoka Sutoku University — 長岡崇徳大学｜看護学部・看護学科 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_suzuka_u_ac_jp, "jp-b50-suzuka-u-ac-jp",
     "Suzuka University of Medical Science", "鈴鹿医療科学大学",
     "jp_research", "science",
     "https://www.suzuka-u.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Suzuka University of Medical Science\",\"science\",\"batch:50\"]",
     10800,
     "Suzuka University of Medical Science — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_svo_dpri_kyoto_u_ac_jp_svo, "jp-b50-svo-dpri-kyoto-u-ac-jp-svo",
     "Sakurajima Volcano Research Center", "京都大学防災研究所附属火山活動研究センター",
     "jp_research", "science",
     "http://www.svo.dpri.kyoto-u.ac.jp/svo/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sakurajima Volcano Research Center\",\"science\",\"batch:50\"]",
     10800,
     "Sakurajima Volcano Research Center — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(jp_b50_tmd_ac_jp, "jp-b50-tmd-ac-jp",
     "Tokyo Medical and Dental University — Science Tokyo 旧・東京医科歯科大学", "東京医科歯科大学 — Science Tokyo 旧・東京医科歯科大学",
     "jp_research", "science",
     "https://www.tmd.ac.jp/RSS.rdf",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tokyo Medical and Dental University — Science Tokyo 旧・東京医科歯科大学\",\"science\",\"batch:50\"]",
     10800,
     "Tokyo Medical and Dental University — Science Tokyo 旧・東京医科歯科大学 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 50 entries at probe")

RSSX(jp_b50_tmu_ac_jp_extra_activity, "jp-b50-tmu-ac-jp-extra-activity",
     "Tokyo metropolitan university — HOT TOPICS", "東京都立大学 — HOT TOPICS",
     "jp_research", "science",
     "https://www.tmu.ac.jp/extra/rss/activity.html",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tokyo metropolitan university — HOT TOPICS\",\"science\",\"batch:50\"]",
     10800,
     "Tokyo metropolitan university — HOT TOPICS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(jp_b50_tmu_ac_jp_extra_entrance, "jp-b50-tmu-ac-jp-extra-entrance",
     "Tokyo metropolitan university — 入試情報", "東京都立大学 — 入試情報",
     "jp_research", "science",
     "https://www.tmu.ac.jp/extra/rss/entrance.html",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tokyo metropolitan university — 入試情報\",\"science\",\"batch:50\"]",
     10800,
     "Tokyo metropolitan university — 入試情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(jp_b50_tmu_ac_jp_extra_event, "jp-b50-tmu-ac-jp-extra-event",
     "Tokyo metropolitan university — イベント情報", "東京都立大学 — イベント情報",
     "jp_research", "science",
     "https://www.tmu.ac.jp/extra/rss/event.html",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tokyo metropolitan university — イベント情報\",\"science\",\"batch:50\"]",
     10800,
     "Tokyo metropolitan university — イベント情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(jp_b50_tmu_ac_jp_extra_topics, "jp-b50-tmu-ac-jp-extra-topics",
     "Tokyo metropolitan university — 最新情報", "東京都立大学 — 最新情報",
     "jp_research", "science",
     "https://www.tmu.ac.jp/extra/rss/topics.html",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tokyo metropolitan university — 最新情報\",\"science\",\"batch:50\"]",
     10800,
     "Tokyo metropolitan university — 最新情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(jp_b50_toho_music_ac_jp, "jp-b50-toho-music-ac-jp",
     "Toho College of Music", "東邦音楽大学",
     "jp_research", "science",
     "https://www.toho-music.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Toho College of Music\",\"science\",\"batch:50\"]",
     10800,
     "Toho College of Music — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_trc_u_shizuoka_ken_ac_jp, "jp-b50-trc-u-shizuoka-ken-ac-jp",
     "Tourism Research Center, Graduate School of Management and Information of Innovation, University of Shizuoka", "静岡県立大学大学院経営情報イノベーション研究科附属ツーリズム研究センター",
     "jp_research", "science",
     "http://trc.u-shizuoka-ken.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tourism Research Center, Graduate School of Management and Information of Innovation, University of Shizuoka\",\"science\",\"batch:50\"]",
     10800,
     "Tourism Research Center, Graduate School of Management and Information of Innovation, University of Shizuoka — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(jp_b50_twmu_ac_jp_hospital_iom, "jp-b50-twmu-ac-jp-hospital-iom",
     "東洋医学研究所", "東京女子医科大学東洋医学研究所",
     "jp_research", "science",
     "https://www.twmu.ac.jp/hospital/IOM/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:東洋医学研究所\",\"science\",\"batch:50\"]",
     10800,
     "東洋医学研究所 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_u_tokai_ac_jp, "jp-b50-u-tokai-ac-jp",
     "Tokai University", "東海大学",
     "jp_research", "science",
     "https://www.u-tokai.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Tokai University\",\"science\",\"batch:50\"]",
     10800,
     "Tokai University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_u_toyama_ac_jp_en, "jp-b50-u-toyama-ac-jp-en",
     "University of Toyama — 富山大学 » Englishページ フィード", "富山大学 — 富山大学 » Englishページ フィード",
     "jp_research", "science",
     "https://www.u-toyama.ac.jp/en/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:University of Toyama — 富山大学 » Englishページ フィード\",\"science\",\"batch:50\"]",
     10800,
     "University of Toyama — 富山大学 » Englishページ フィード — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 30 entries at probe")

RSSX(jp_b50_uekusa_ac_jp, "jp-b50-uekusa-ac-jp",
     "Uekusa University", "植草学園大学",
     "jp_research", "science",
     "https://www.uekusa.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Uekusa University\",\"science\",\"batch:50\"]",
     10800,
     "Uekusa University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_univ_shibata_ac_jp, "jp-b50-univ-shibata-ac-jp",
     "Shibata Gakuen University", "柴田学園大学",
     "jp_research", "science",
     "https://univ.shibata.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Shibata Gakuen University\",\"science\",\"batch:50\"]",
     10800,
     "Shibata Gakuen University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_web_sapmed_ac_jp_jp_news, "jp-b50-web-sapmed-ac-jp-jp-news",
     "Sapporo Medical University — RSS「新着情報」", "札幌医科大学 — RSS「新着情報」",
     "jp_research", "science",
     "https://web.sapmed.ac.jp/jp/news/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sapporo Medical University — RSS「新着情報」\",\"science\",\"batch:50\"]",
     10800,
     "Sapporo Medical University — RSS「新着情報」 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(jp_b50_web_sapmed_ac_jp_jp_news_event, "jp-b50-web-sapmed-ac-jp-jp-news-event",
     "Sapporo Medical University — RSS「イベント・公開講座」", "札幌医科大学 — RSS「イベント・公開講座」",
     "jp_research", "science",
     "https://web.sapmed.ac.jp/jp/news/event/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sapporo Medical University — RSS「イベント・公開講座」\",\"science\",\"batch:50\"]",
     10800,
     "Sapporo Medical University — RSS「イベント・公開講座」 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 13 entries at probe")

RSSX(jp_b50_web_sapmed_ac_jp_jp_news_photo, "jp-b50-web-sapmed-ac-jp-jp-news-photo",
     "Sapporo Medical University — RSS「写真ニュース」", "札幌医科大学 — RSS「写真ニュース」",
     "jp_research", "science",
     "https://web.sapmed.ac.jp/jp/news/photo/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sapporo Medical University — RSS「写真ニュース」\",\"science\",\"batch:50\"]",
     10800,
     "Sapporo Medical University — RSS「写真ニュース」 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 13 entries at probe")

RSSX(jp_b50_web_sapmed_ac_jp_jp_news_press, "jp-b50-web-sapmed-ac-jp-jp-news-press",
     "Sapporo Medical University — RSS「プレスリリース・メディア」", "札幌医科大学 — RSS「プレスリリース・メディア」",
     "jp_research", "science",
     "https://web.sapmed.ac.jp/jp/news/press/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sapporo Medical University — RSS「プレスリリース・メディア」\",\"science\",\"batch:50\"]",
     10800,
     "Sapporo Medical University — RSS「プレスリリース・メディア」 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 6 entries at probe")

RSSX(jp_b50_web_sapmed_ac_jp_jp_news_topics, "jp-b50-web-sapmed-ac-jp-jp-news-topics",
     "Sapporo Medical University — RSS「お知らせ」", "札幌医科大学 — RSS「お知らせ」",
     "jp_research", "science",
     "https://web.sapmed.ac.jp/jp/news/topics/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sapporo Medical University — RSS「お知らせ」\",\"science\",\"batch:50\"]",
     10800,
     "Sapporo Medical University — RSS「お知らせ」 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 8 entries at probe")

RSSX(jp_b50_web_sapmed_ac_jp_jp_public_job, "jp-b50-web-sapmed-ac-jp-jp-public-job",
     "Sapporo Medical University — RSS「採用情報」", "札幌医科大学 — RSS「採用情報」",
     "jp_research", "science",
     "https://web.sapmed.ac.jp/jp/public/job/rss.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Sapporo Medical University — RSS「採用情報」\",\"science\",\"batch:50\"]",
     10800,
     "Sapporo Medical University — RSS「採用情報」 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 6 entries at probe")

RSSX(jp_b50_yachts_ac_jp_rss2, "jp-b50-yachts-ac-jp-rss2",
     "Yamagata Prefectural University of Health Sciences", "山形県立保健医療大学",
     "jp_research", "science",
     "https://www.yachts.ac.jp/rss2.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Yamagata Prefectural University of Health Sciences\",\"science\",\"batch:50\"]",
     10800,
     "Yamagata Prefectural University of Health Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(jp_b50_yamanashi_ac_jp, "jp-b50-yamanashi-ac-jp",
     "University of Yamanashi — 山梨大学 RSSフィード", "山梨大学 — 山梨大学 RSSフィード",
     "jp_research", "science",
     "https://www.yamanashi.ac.jp/feed",
     "ja", "[\"gov\",\"country:jp\",\"publisher:University of Yamanashi — 山梨大学 RSSフィード\",\"science\",\"batch:50\"]",
     10800,
     "University of Yamanashi — 山梨大学 RSSフィード — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(jp_b50_yamanashi_eiwa_ac_jp, "jp-b50-yamanashi-eiwa-ac-jp",
     "Yamanashi Eiwa College", "山梨英和大学",
     "jp_research", "science",
     "https://www.yamanashi-eiwa.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Yamanashi Eiwa College\",\"science\",\"batch:50\"]",
     10800,
     "Yamanashi Eiwa College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(jp_b50_yamato_u_ac_jp, "jp-b50-yamato-u-ac-jp",
     "Yamato University", "大和大学",
     "jp_research", "science",
     "https://www.yamato-u.ac.jp/feed/",
     "ja", "[\"gov\",\"country:jp\",\"publisher:Yamato University\",\"science\",\"batch:50\"]",
     10800,
     "Yamato University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

/* ── news ────────────────────────────────────────────────────────────── */

RSSX(jp_b50_21cnbc_jp, "jp-b50-21cnbc-jp",
     "The 21st Century New Business Conference", "21世紀ニュービジネス協議会",
     "news", "media",
     "https://21cnbc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The 21st Century New Business Conference\",\"media\",\"batch:50\"]",
     3600,
     "The 21st Century New Business Conference — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_aacf_or_jp, "jp-b50-aacf-or-jp",
     "Asia-Africa Culture Foundation", "アジア・アフリカ文化財団",
     "news", "media",
     "https://aacf.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Asia-Africa Culture Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Asia-Africa Culture Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_aart_jp_feed_rss2, "jp-b50-aart-jp-feed-rss2",
     "The Akita Association of Radiological Technologists", "公益社団法人秋田県診療放射線技師会",
     "news", "media",
     "https://aart.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Akita Association of Radiological Technologists\",\"media\",\"batch:50\"]",
     3600,
     "The Akita Association of Radiological Technologists — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_abashiri_jc_com_feed_rss2, "jp-b50-abashiri-jc-com-feed-rss2",
     "Junior Chamber International Abashiri", "網走青年会議所",
     "news", "media",
     "https://abashiri-jc.com/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Junior Chamber International Abashiri\",\"media\",\"batch:50\"]",
     3600,
     "Junior Chamber International Abashiri — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_acc_arakawa_jp, "jp-b50-acc-arakawa-jp",
     "Arakawa City Art Culture Promotion Foundation", "荒川区芸術文化振興財団",
     "news", "media",
     "https://www.acc-arakawa.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Arakawa City Art Culture Promotion Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Arakawa City Art Culture Promotion Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_accs_or_jp, "jp-b50-accs-or-jp",
     "Academic Newtown Community Cable Service", "研究学園都市コミュニティケーブルサービス",
     "news", "media",
     "https://www.accs.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Academic Newtown Community Cable Service\",\"media\",\"batch:50\"]",
     3600,
     "Academic Newtown Community Cable Service — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_act_hitotomachidukuri_org, "jp-b50-act-hitotomachidukuri-org",
     "ACT Community Development", "ＡＣＴ・人とまちづくり",
     "news", "media",
     "https://act-hitotomachidukuri.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:ACT Community Development\",\"media\",\"batch:50\"]",
     3600,
     "ACT Community Development — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_adrajpn_org, "jp-b50-adrajpn-org",
     "ADRA Japan", "ADRA Japan",
     "news", "media",
     "https://www.adrajpn.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:ADRA Japan\",\"media\",\"batch:50\"]",
     3600,
     "ADRA Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_adtec_rf_co_jp, "jp-b50-adtec-rf-co-jp",
     "株式会社アドテックプラズマテクノロジー", "株式会社アドテックプラズマテクノロジー",
     "news", "media",
     "https://www.adtec-rf.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社アドテックプラズマテクノロジー\",\"media\",\"batch:50\"]",
     3600,
     "株式会社アドテックプラズマテクノロジー — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_aen_mekki_or_jp, "jp-b50-aen-mekki-or-jp",
     "Japan Galvanizers Association, Inc.", "一般社団法人日本溶融亜鉛鍍金協会",
     "news", "media",
     "https://www.aen-mekki.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Galvanizers Association, Inc.\",\"media\",\"batch:50\"]",
     3600,
     "Japan Galvanizers Association, Inc. — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ago_ac, "jp-b50-ago-ac",
     "The Academy of Gnathology and Occlusion", "日本顎咬合学会",
     "news", "media",
     "https://ago.ac/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Academy of Gnathology and Occlusion\",\"media\",\"batch:50\"]",
     3600,
     "The Academy of Gnathology and Occlusion — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_agrinews_co_jp, "jp-b50-agrinews-co-jp",
     "日本農業新聞 — 日本農業新聞公式ウェブサイト", "日本農業新聞 — 日本農業新聞公式ウェブサイト",
     "news", "media",
     "https://www.agrinews.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:日本農業新聞 — 日本農業新聞公式ウェブサイト\",\"media\",\"batch:50\"]",
     3600,
     "日本農業新聞 — 日本農業新聞公式ウェブサイト — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 185 entries at probe")

RSSX(jp_b50_ai_rakuten_com, "jp-b50-ai-rakuten-com",
     "Rakuten Institute of Technology", "楽天技術研究所",
     "news", "media",
     "https://ai.rakuten.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Rakuten Institute of Technology\",\"media\",\"batch:50\"]",
     3600,
     "Rakuten Institute of Technology — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 9 entries at probe")

RSSX(jp_b50_aiaj_or_jp, "jp-b50-aiaj-or-jp",
     "Aerosol Industry Association of Japan", "日本エアゾール協会",
     "news", "media",
     "https://www.aiaj.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Aerosol Industry Association of Japan\",\"media\",\"batch:50\"]",
     3600,
     "Aerosol Industry Association of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_aibsc_jp, "jp-b50-aibsc-jp",
     "Aichi Small Business Support Center", "あいち産業振興機構",
     "news", "media",
     "https://www.aibsc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Aichi Small Business Support Center\",\"media\",\"batch:50\"]",
     3600,
     "Aichi Small Business Support Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_aifcg_jp, "jp-b50-aifcg-jp",
     "AI フュージョンキャピタルグループ株式会社", "AI フュージョンキャピタルグループ株式会社",
     "news", "media",
     "https://aifcg.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:AI フュージョンキャピタルグループ株式会社\",\"media\",\"batch:50\"]",
     3600,
     "AI フュージョンキャピタルグループ株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_aigtokyo_or_jp_feed_rss2, "jp-b50-aigtokyo-or-jp-feed-rss2",
     "Japan-Italy Association", "日伊協会",
     "news", "media",
     "https://www.aigtokyo.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan-Italy Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan-Italy Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_aizoukyo_or_jp, "jp-b50-aizoukyo-or-jp",
     "Aichi Landscape Constructors", "愛知県造園建設業協会",
     "news", "media",
     "https://aizoukyo.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Aichi Landscape Constructors\",\"media\",\"batch:50\"]",
     3600,
     "Aichi Landscape Constructors — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_akari2006_or_jp, "jp-b50-akari2006-or-jp",
     "Akari Association", "あかり",
     "news", "media",
     "https://akari2006.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Akari Association\",\"media\",\"batch:50\"]",
     3600,
     "Akari Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_alianet_org, "jp-b50-alianet-org",
     "Association of Living Amenity", "リビングアメニティ協会",
     "news", "media",
     "https://www.alianet.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Association of Living Amenity\",\"media\",\"batch:50\"]",
     3600,
     "Association of Living Amenity — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_all_shizuoka_or_jp, "jp-b50-all-shizuoka-or-jp",
     "All Shizuoka Best Community", "オールしずおかベストコミュニティ",
     "news", "media",
     "https://www.all-shizuoka.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:All Shizuoka Best Community\",\"media\",\"batch:50\"]",
     3600,
     "All Shizuoka Best Community — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_aluminum_or_jp, "jp-b50-aluminum-or-jp",
     "Japan Aluminium Association", "日本アルミニウム協会",
     "news", "media",
     "https://www.aluminum.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Aluminium Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Aluminium Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_amda_minds_org, "jp-b50-amda-minds-org",
     "AMDA Social Development Organization", "AMDA社会開発機構",
     "news", "media",
     "https://amda-minds.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:AMDA Social Development Organization\",\"media\",\"batch:50\"]",
     3600,
     "AMDA Social Development Organization — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_amnesty_or_jp, "jp-b50-amnesty-or-jp",
     "Amnesty International Japan — アムネスティ日本", "アムネスティ・インターナショナル日本 — アムネスティ日本",
     "news", "media",
     "https://www.amnesty.or.jp/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Amnesty International Japan — アムネスティ日本\",\"media\",\"batch:50\"]",
     3600,
     "Amnesty International Japan — アムネスティ日本 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_ams_life_co_jp_ams, "jp-b50-ams-life-co-jp-ams",
     "トップページ", "トップページ",
     "news", "media",
     "https://www.ams-life.co.jp/ams/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:トップページ\",\"media\",\"batch:50\"]",
     3600,
     "トップページ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_animeanime_jp_rss20, "jp-b50-animeanime-jp-rss20",
     "アニメ！アニメ！", "アニメ！アニメ！",
     "news", "media",
     "https://animeanime.jp/rss20/index.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:アニメ！アニメ！\",\"media\",\"batch:50\"]",
     3600,
     "アニメ！アニメ！ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_antiphishing_jp, "jp-b50-antiphishing-jp",
     "フィッシング対策協議会 Council of Anti-Phishing Japan — Recent Entries", "フィッシング対策協議会 Council of Anti-Phishing Japan — Recent Entries",
     "news", "media",
     "https://www.antiphishing.jp/atom.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:フィッシング対策協議会 Council of Anti-Phishing Japan — Recent Entries\",\"media\",\"batch:50\"]",
     3600,
     "フィッシング対策協議会 Council of Anti-Phishing Japan — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_aoiito_com, "jp-b50-aoiito-com",
     "Aoi Ito (Blue Thread)", "あおい糸",
     "news", "media",
     "https://www.aoiito.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Aoi Ito (Blue Thread)\",\"media\",\"batch:50\"]",
     3600,
     "Aoi Ito (Blue Thread) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_aonsa_org, "jp-b50-aonsa-org",
     "Asia-Oceania Neutron Scattering Association", "AONSA",
     "news", "media",
     "https://www.aonsa.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Asia-Oceania Neutron Scattering Association\",\"media\",\"batch:50\"]",
     3600,
     "Asia-Oceania Neutron Scattering Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_apab_or_jp, "jp-b50-apab-or-jp",
     "The Association for Promotion of Advanced Broadcasting Services", "放送サービス高度化推進協会",
     "news", "media",
     "https://www.apab.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Association for Promotion of Advanced Broadcasting Services\",\"media\",\"batch:50\"]",
     3600,
     "The Association for Promotion of Advanced Broadcasting Services — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_appie_or_jp, "jp-b50-appie-or-jp",
     "The Association of Powder Process Industry and Engineering, JAPAN", "一般社団法人日本粉体工業技術協会",
     "news", "media",
     "https://appie.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Association of Powder Process Industry and Engineering, JAPAN\",\"media\",\"batch:50\"]",
     3600,
     "The Association of Powder Process Industry and Engineering, JAPAN — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_applic_or_jp, "jp-b50-applic-or-jp",
     "The Association for Promotion of Public Local Information and Communication", "一般財団法人全国地域情報化推進協会",
     "news", "media",
     "https://www.applic.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Association for Promotion of Public Local Information and Communication\",\"media\",\"batch:50\"]",
     3600,
     "The Association for Promotion of Public Local Information and Communication — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_apstj_jp, "jp-b50-apstj-jp",
     "The Academy of Pharmaceutical Science and Technology, Japan", "公益社団法人日本薬剤学会",
     "news", "media",
     "https://www.apstj.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Academy of Pharmaceutical Science and Technology, Japan\",\"media\",\"batch:50\"]",
     3600,
     "The Academy of Pharmaceutical Science and Technology, Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_aptec_or_jp, "jp-b50-aptec-or-jp",
     "Asia-Pacific Tourism Exchange Center", "アジア太平洋観光交流センター",
     "news", "media",
     "https://aptec.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Asia-Pacific Tourism Exchange Center\",\"media\",\"batch:50\"]",
     3600,
     "Asia-Pacific Tourism Exchange Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_argentina_jp, "jp-b50-argentina-jp",
     "Association Nippon Argentina", "日本アルゼンチン協会",
     "news", "media",
     "https://argentina.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Association Nippon Argentina\",\"media\",\"batch:50\"]",
     3600,
     "Association Nippon Argentina — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_arkcore_co_jp, "jp-b50-arkcore-co-jp",
     "TOP", "TOP",
     "news", "media",
     "https://arkcore.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:TOP\",\"media\",\"batch:50\"]",
     3600,
     "TOP — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_arsit_or_jp, "jp-b50-arsit-or-jp",
     "Association for Rainwater Storage and Infiltration Technology — 雨水貯留浸透技術協会 RSS Feed", "公益社団法人雨水貯留浸透技術協会 — 雨水貯留浸透技術協会 RSS Feed",
     "news", "media",
     "https://arsit.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Association for Rainwater Storage and Infiltration Technology — 雨水貯留浸透技術協会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Association for Rainwater Storage and Infiltration Technology — 雨水貯留浸透技術協会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_art_play_or_jp, "jp-b50-art-play-or-jp",
     "Art and Play Creation Association", "芸術と遊び創造協会",
     "news", "media",
     "https://art-play.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Art and Play Creation Association\",\"media\",\"batch:50\"]",
     3600,
     "Art and Play Creation Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_artgreen_co_jp, "jp-b50-artgreen-co-jp",
     "アートグリーン株式会社", "アートグリーン株式会社",
     "news", "media",
     "https://artgreen.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:アートグリーン株式会社\",\"media\",\"batch:50\"]",
     3600,
     "アートグリーン株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_asahikawa_park_or_jp_info, "jp-b50-asahikawa-park-or-jp-info",
     "Northern Wild Plants Garden — 旭川市公園緑地協会 - お知らせ（RSS）", "北邦野草園 — 旭川市公園緑地協会 - お知らせ（RSS）",
     "news", "media",
     "http://www.asahikawa-park.or.jp/info/rss.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Northern Wild Plants Garden — 旭川市公園緑地協会 - お知らせ（RSS）\",\"media\",\"batch:50\"]",
     3600,
     "Northern Wild Plants Garden — 旭川市公園緑地協会 - お知らせ（RSS） — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_asamed_jp, "jp-b50-asamed-jp",
     "Asahikawa Medical Association", "一般社団法人旭川市医師会",
     "news", "media",
     "https://asamed.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Asahikawa Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Asahikawa Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 7 entries at probe")

RSSX(jp_b50_aseed_hd_co_jp, "jp-b50-aseed-hd-co-jp",
     "アシードホールディングス", "アシードホールディングス",
     "news", "media",
     "https://www.aseed-hd.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:アシードホールディングス\",\"media\",\"batch:50\"]",
     3600,
     "アシードホールディングス — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_asios_org, "jp-b50-asios-org",
     "ASIOS", "ASIOS",
     "news", "media",
     "https://asios.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:ASIOS\",\"media\",\"batch:50\"]",
     3600,
     "ASIOS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_aska_co_jp, "jp-b50-aska-co-jp",
     "アスカ株式会社", "アスカ株式会社",
     "news", "media",
     "https://www.aska.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:アスカ株式会社\",\"media\",\"batch:50\"]",
     3600,
     "アスカ株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_asknet_org, "jp-b50-asknet-org",
     "Ask Net", "アスクネット",
     "news", "media",
     "https://asknet.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ask Net\",\"media\",\"batch:50\"]",
     3600,
     "Ask Net — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_asrc_idema_org, "jp-b50-asrc-idema-org",
     "Advanced Storage Research Consortium", "Home",
     "news", "media",
     "https://asrc.idema.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Advanced Storage Research Consortium\",\"media\",\"batch:50\"]",
     3600,
     "Advanced Storage Research Consortium — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_astf_or_jp_blog_feed, "jp-b50-astf-or-jp-blog-feed",
     "Aichi Science & Technology Foundation — 公益財団法人 科学技術交流財団 - RSS", "公益財団法人科学技術交流財団 — 公益財団法人 科学技術交流財団 - RSS",
     "news", "media",
     "https://www.astf.or.jp/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Aichi Science & Technology Foundation — 公益財団法人 科学技術交流財団 - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Aichi Science & Technology Foundation — 公益財団法人 科学技術交流財団 - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_atcaj_or_jp_feed_rss2, "jp-b50-atcaj-or-jp-feed-rss2",
     "Air Traffic Control Association, Japan", "航空交通管制協会",
     "news", "media",
     "https://atcaj.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Air Traffic Control Association, Japan\",\"media\",\"batch:50\"]",
     3600,
     "Air Traffic Control Association, Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_atsri_or_jp, "jp-b50-atsri-or-jp",
     "Air Traffic Service Research Institute", "航空保安研究センター",
     "news", "media",
     "https://www.atsri.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Air Traffic Service Research Institute\",\"media\",\"batch:50\"]",
     3600,
     "Air Traffic Service Research Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_attac_j_or_jp, "jp-b50-attac-j-or-jp",
     "All Japan Tohsu Technical Association in Civil Engineering", "全国トース技術研究組合",
     "news", "media",
     "https://attac-j.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:All Japan Tohsu Technical Association in Civil Engineering\",\"media\",\"batch:50\"]",
     3600,
     "All Japan Tohsu Technical Association in Civil Engineering — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_avcc_or_jp, "jp-b50-avcc-or-jp",
     "Advanced Visual Communication Center", "一般財団法人高度映像情報センター",
     "news", "media",
     "https://www.avcc.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Advanced Visual Communication Center\",\"media\",\"batch:50\"]",
     3600,
     "Advanced Visual Communication Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_axis_net_co_jp, "jp-b50-axis-net-co-jp",
     "株式会社アクシス", "株式会社アクシス",
     "news", "media",
     "https://www.axis-net.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社アクシス\",\"media\",\"batch:50\"]",
     3600,
     "株式会社アクシス — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_b_hatena_ne_jp_hotentry_entertainment, "jp-b50-b-hatena-ne-jp-hotentry-entertainment",
     "Hatena Bookmark Entertainment", "はてなブックマーク エンタメ",
     "news", "media",
     "https://b.hatena.ne.jp/hotentry/entertainment.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hatena Bookmark Entertainment\",\"media\",\"batch:50\"]",
     3600,
     "Hatena Bookmark Entertainment — news feed (30 entries at probe)")

RSSX(jp_b50_baa_or_jp, "jp-b50-baa-or-jp",
     "Beautiful Aging Association", "ビューティフルエージング協会",
     "news", "media",
     "https://baa.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Beautiful Aging Association\",\"media\",\"batch:50\"]",
     3600,
     "Beautiful Aging Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_barcos_jp, "jp-b50-barcos-jp",
     "BARCOS", "BARCOS",
     "news", "media",
     "https://barcos.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:BARCOS\",\"media\",\"batch:50\"]",
     3600,
     "BARCOS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_bdk_or_jp_english, "jp-b50-bdk-or-jp-english",
     "Bukkyō Dendō Kyōkai", "仏教伝道協会",
     "news", "media",
     "https://www.bdk.or.jp/english/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Bukkyō Dendō Kyōkai\",\"media\",\"batch:50\"]",
     3600,
     "Bukkyō Dendō Kyōkai — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_beans_fukushima_or_jp, "jp-b50-beans-fukushima-or-jp",
     "Beans Fukushima", "ビーンズふくしま",
     "news", "media",
     "https://beans-fukushima.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Beans Fukushima\",\"media\",\"batch:50\"]",
     3600,
     "Beans Fukushima — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_bergearth_co_jp, "jp-b50-bergearth-co-jp",
     "ベルグアース株式会社", "ベルグアース株式会社",
     "news", "media",
     "https://bergearth.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:ベルグアース株式会社\",\"media\",\"batch:50\"]",
     3600,
     "ベルグアース株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_bi_no_org, "jp-b50-bi-no-org",
     "Be-no-be-no", "びーのびーの",
     "news", "media",
     "https://bi-no.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Be-no-be-no\",\"media\",\"batch:50\"]",
     3600,
     "Be-no-be-no — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_bible_or_jp, "jp-b50-bible-or-jp",
     "Japan Bible Society", "日本聖書協会",
     "news", "media",
     "https://www.bible.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Bible Society\",\"media\",\"batch:50\"]",
     3600,
     "Japan Bible Society — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_bikazaidan_or_jp, "jp-b50-bikazaidan-or-jp",
     "Kanagawa Coastal Environmental Foundation", "かながわ海岸美化財団",
     "news", "media",
     "https://www.bikazaidan.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kanagawa Coastal Environmental Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Kanagawa Coastal Environmental Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_bird_research_jp_feed_rss2, "jp-b50-bird-research-jp-feed-rss2",
     "The Japan Bird Research Association", "認定NPO法人バードリサーチ",
     "news", "media",
     "https://www.bird-research.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japan Bird Research Association\",\"media\",\"batch:50\"]",
     3600,
     "The Japan Bird Research Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_blcj_or_jp, "jp-b50-blcj-or-jp",
     "Building information modeling Library Collaborative research association of Japan", "BIMライブラリ技術研究組合",
     "news", "media",
     "https://blcj.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Building information modeling Library Collaborative research association of Japan\",\"media\",\"batch:50\"]",
     3600,
     "Building information modeling Library Collaborative research association of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_blog_calil_jp, "jp-b50-blog-calil-jp",
     "カーリル — カーリルのブログ", "カーリル — カーリルのブログ",
     "news", "media",
     "https://blog.calil.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:カーリル — カーリルのブログ\",\"media\",\"batch:50\"]",
     3600,
     "カーリル — カーリルのブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_blog_nflabs_jp, "jp-b50-blog-nflabs-jp",
     "NFLabs. エンジニアブログ", "NFLabs. エンジニアブログ",
     "news", "media",
     "https://blog.nflabs.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:NFLabs. エンジニアブログ\",\"media\",\"batch:50\"]",
     3600,
     "NFLabs. エンジニアブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_blog_nicter_jp, "jp-b50-blog-nicter-jp",
     "blog.nicter.jp — NICTER Blog", "blog.nicter.jp — NICTER Blog",
     "news", "media",
     "https://blog.nicter.jp/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:blog.nicter.jp — NICTER Blog\",\"media\",\"batch:50\"]",
     3600,
     "blog.nicter.jp — NICTER Blog — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 69 entries at probe")

RSSX(jp_b50_blog_qiita_com, "jp-b50-blog-qiita-com",
     "Qiita Blog", "Qiitaブログ",
     "news", "media",
     "https://blog.qiita.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Qiita Blog\",\"media\",\"batch:50\"]",
     3600,
     "Qiita Blog — news feed (10 entries at probe)")

RSSX(jp_b50_blog_shibayan_jp, "jp-b50-blog-shibayan-jp",
     "しばやん雑記", "しばやん雑記",
     "news", "media",
     "https://blog.shibayan.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:しばやん雑記\",\"media\",\"batch:50\"]",
     3600,
     "しばやん雑記 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_blog_shibayan_jp_2, "jp-b50-blog-shibayan-jp-2",
     "しばやん雑記", "しばやん雑記",
     "news", "media",
     "https://blog.shibayan.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:しばやん雑記\",\"media\",\"batch:50\"]",
     3600,
     "しばやん雑記 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_blog_studysapuri_jp, "jp-b50-blog-studysapuri-jp",
     "スタディサプリ Product Team Blog", "スタディサプリ Product Team Blog",
     "news", "media",
     "https://blog.studysapuri.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:スタディサプリ Product Team Blog\",\"media\",\"batch:50\"]",
     3600,
     "スタディサプリ Product Team Blog — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_bmmc_or_jp, "jp-b50-bmmc-or-jp",
     "Building Maintenance & Management Center", "一般財団法人建築保全センター",
     "news", "media",
     "https://bmmc.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Building Maintenance & Management Center\",\"media\",\"batch:50\"]",
     3600,
     "Building Maintenance & Management Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_bondproject_jp, "jp-b50-bondproject-jp",
     "Bond Project", "BONDプロジェクト",
     "news", "media",
     "https://bondproject.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Bond Project\",\"media\",\"batch:50\"]",
     3600,
     "Bond Project — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_bplats_co_jp, "jp-b50-bplats-co-jp",
     "ビープラッツ", "ビープラッツ",
     "news", "media",
     "https://www.bplats.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:ビープラッツ\",\"media\",\"batch:50\"]",
     3600,
     "ビープラッツ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_brutus_jp_podcast_bizzarremineral, "jp-b50-brutus-jp-podcast-bizzarremineral",
     "ブルータス| BRUTUS.jp — ポッドキャストの RSS フィード", "ブルータス| BRUTUS.jp — ポッドキャストの RSS フィード",
     "news", "media",
     "https://brutus.jp/feed/podcast/bizzarremineral",
     "ja", "[\"news\",\"country:jp\",\"publisher:ブルータス| BRUTUS.jp — ポッドキャストの RSS フィード\",\"media\",\"batch:50\"]",
     3600,
     "ブルータス| BRUTUS.jp — ポッドキャストの RSS フィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_buzzap_jp_ftype_googlenews, "jp-b50-buzzap-jp-ftype-googlenews",
     "Buzzap！（バザップ！） — Buzzap！ RSS Feed", "Buzzap！（バザップ！） — Buzzap！ RSS Feed",
     "news", "media",
     "https://buzzap.jp/feed/?ftype=googlenews",
     "ja", "[\"news\",\"country:jp\",\"publisher:Buzzap！（バザップ！） — Buzzap！ RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Buzzap！（バザップ！） — Buzzap！ RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_byoujihoiku_take_uma_net, "jp-b50-byoujihoiku-take-uma-net",
     "Sick Child Care Creation Association", "病児保育を作る会",
     "news", "media",
     "https://byoujihoiku.take-uma.net/RSS/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Sick Child Care Creation Association\",\"media\",\"batch:50\"]",
     3600,
     "Sick Child Care Creation Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_byoujihoiku_take_uma_net_2, "jp-b50-byoujihoiku-take-uma-net-2",
     "Sick Child Care Creation Association", "病児保育を作る会",
     "news", "media",
     "https://byoujihoiku.take-uma.net/ATOM/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Sick Child Care Creation Association\",\"media\",\"batch:50\"]",
     3600,
     "Sick Child Care Creation Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_captain_or_jp_feed_rss2, "jp-b50-captain-or-jp-feed-rss2",
     "Japan Captains' Association — 一般社団法人日本船長協会 RSS Feed", "日本船長協会 — 一般社団法人日本船長協会 RSS Feed",
     "news", "media",
     "https://captain.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Captains' Association — 一般社団法人日本船長協会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Captains' Association — 一般社団法人日本船長協会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_cbc_co_jp, "jp-b50-cbc-co-jp",
     "CBC株式会社｜未来をカタチにする創造商社", "CBC株式会社｜未来をカタチにする創造商社",
     "news", "media",
     "https://www.cbc.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:CBC株式会社｜未来をカタチにする創造商社\",\"media\",\"batch:50\"]",
     3600,
     "CBC株式会社｜未来をカタチにする創造商社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_cbl_or_jp, "jp-b50-cbl-or-jp",
     "Center for Better Living", "一般財団法人ベターリビング",
     "news", "media",
     "https://www.cbl.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Center for Better Living\",\"media\",\"batch:50\"]",
     3600,
     "Center for Better Living — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ccaj_found_or_jp, "jp-b50-ccaj-found-or-jp",
     "Children's Cancer Association of Japan", "がんの子どもを守る会",
     "news", "media",
     "https://www.ccaj-found.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Children's Cancer Association of Japan\",\"media\",\"batch:50\"]",
     3600,
     "Children's Cancer Association of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ccis_tohoku_org, "jp-b50-ccis-tohoku-org",
     "Center for Collaborative Interdisciplinary Sciences", "特定非営利活動法人 科学協力学際センター",
     "news", "media",
     "https://ccis.tohoku.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Center for Collaborative Interdisciplinary Sciences\",\"media\",\"batch:50\"]",
     3600,
     "Center for Collaborative Interdisciplinary Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ccp_ngo_jp_news, "jp-b50-ccp-ngo-jp-news",
     "Campaign for Children of Palestine — お知らせ フィード", "パレスチナ子どものキャンペーン — お知らせ フィード",
     "news", "media",
     "https://ccp-ngo.jp/news/atom.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Campaign for Children of Palestine — お知らせ フィード\",\"media\",\"batch:50\"]",
     3600,
     "Campaign for Children of Palestine — お知らせ フィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_ccp_ngo_jp_report, "jp-b50-ccp-ngo-jp-report",
     "Campaign for Children of Palestine — 活動現場からの報告 フィード", "パレスチナ子どものキャンペーン — 活動現場からの報告 フィード",
     "news", "media",
     "https://ccp-ngo.jp/report/atom.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Campaign for Children of Palestine — 活動現場からの報告 フィード\",\"media\",\"batch:50\"]",
     3600,
     "Campaign for Children of Palestine — 活動現場からの報告 フィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_cdij_org_wiki_cmd_rss, "jp-b50-cdij-org-wiki-cmd-rss",
     "CDI", "シィー・ディー・アイ",
     "news", "media",
     "https://www.cdij.org:443/wiki/?cmd=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:CDI\",\"media\",\"batch:50\"]",
     3600,
     "CDI — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_centerye_org, "jp-b50-centerye-org",
     "Center for International Youth Exchange", "青少年国際交流推進センター",
     "news", "media",
     "https://centerye.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Center for International Youth Exchange\",\"media\",\"batch:50\"]",
     3600,
     "Center for International Youth Exchange — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_cgshd_co_jp, "jp-b50-cgshd-co-jp",
     "株式会社CGSホールディングス", "株式会社CGSホールディングス",
     "news", "media",
     "https://www.cgshd.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社CGSホールディングス\",\"media\",\"batch:50\"]",
     3600,
     "株式会社CGSホールディングス — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_chihiro_jp_tw_foundation, "jp-b50-chihiro-jp-tw-foundation",
     "Chihiro Iwasaki Memorial Foundation — 訂閱《知弘美術館》»「公益財団法人 いわさきちひろ記念事業団」的資訊提供", "いわさきちひろ記念事業団 — 訂閱《知弘美術館》»「公益財団法人 いわさきちひろ記念事業団」的資訊提供",
     "news", "media",
     "https://chihiro.jp/tw/foundation/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Chihiro Iwasaki Memorial Foundation — 訂閱《知弘美術館》»「公益財団法人 いわさきちひろ記念事業団」的資訊提供\",\"media\",\"batch:50\"]",
     3600,
     "Chihiro Iwasaki Memorial Foundation — 訂閱《知弘美術館》»「公益財団法人 いわさきちひろ記念事業団」的資訊提供 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_chiikinews_co_jp, "jp-b50-chiikinews-co-jp",
     "【公式】地域新聞社 ｜千葉・茨城のチラシ･ポスティングならフリーペーパー「ちいき新聞」", "【公式】地域新聞社 ｜千葉・茨城のチラシ･ポスティングならフリーペーパー「ちいき新聞」",
     "news", "media",
     "https://chiikinews.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:【公式】地域新聞社 ｜千葉・茨城のチラシ･ポスティングならフリーペーパー「ちいき新聞」\",\"media\",\"batch:50\"]",
     3600,
     "【公式】地域新聞社 ｜千葉・茨城のチラシ･ポスティングならフリーペーパー「ちいき新聞」 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_chikyumura_org, "jp-b50-chikyumura-org",
     "Network Earth Village — Recent Entries", "ネットワーク「地球村」 — Recent Entries",
     "news", "media",
     "https://chikyumura.org/atom.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Network Earth Village — Recent Entries\",\"media\",\"batch:50\"]",
     3600,
     "Network Earth Village — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_children_cc_org, "jp-b50-children-cc-org",
     "Children's Community Care", "こどもコミュニティケア",
     "news", "media",
     "https://children-cc.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Children's Community Care\",\"media\",\"batch:50\"]",
     3600,
     "Children's Community Care — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_chimonken_or_jp_blog, "jp-b50-chimonken-or-jp-blog",
     "Research Institute for Regional Planning and Development — ブログ", "一般社団法人地域問題研究所 — ブログ",
     "news", "media",
     "https://www.chimonken.or.jp/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:Research Institute for Regional Planning and Development — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "Research Institute for Regional Planning and Development — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 18 entries at probe")

RSSX(jp_b50_choken_or_jp, "jp-b50-choken-or-jp",
     "Nagano Construction Society", "長野県建設業協会",
     "news", "media",
     "https://www.choken.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nagano Construction Society\",\"media\",\"batch:50\"]",
     3600,
     "Nagano Construction Society — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 7 entries at probe")

RSSX(jp_b50_chuetsu_pulp_co_jp, "jp-b50-chuetsu-pulp-co-jp",
     "中越パルプ工業株式会社 — 中越パルプ工業株式会社 RSS Feed", "中越パルプ工業株式会社 — 中越パルプ工業株式会社 RSS Feed",
     "news", "media",
     "https://www.chuetsu-pulp.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:中越パルプ工業株式会社 — 中越パルプ工業株式会社 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "中越パルプ工業株式会社 — 中越パルプ工業株式会社 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_chukeikyo_net, "jp-b50-chukeikyo-net",
     "Kitakyushu Chukeikyo — 一般社団法人 北九州中小企業経営者協会(中経協) RSS Feed", "北九州中小企業経営者協会 — 一般社団法人 北九州中小企業経営者協会(中経協) RSS Feed",
     "news", "media",
     "https://www.chukeikyo.net/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kitakyushu Chukeikyo — 一般社団法人 北九州中小企業経営者協会(中経協) RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Kitakyushu Chukeikyo — 一般社団法人 北九州中小企業経営者協会(中経協) RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_churou_wp_sub_jp, "jp-b50-churou-wp-sub-jp",
     "Chugoku Occupational Health Association", "公益財団法人中国労働衛生協会",
     "news", "media",
     "https://churou-wp.sub.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Chugoku Occupational Health Association\",\"media\",\"batch:50\"]",
     3600,
     "Chugoku Occupational Health Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_cityscalekobe_jp, "jp-b50-cityscalekobe-jp",
     "Integrated Engineering System Research and Development Partnership", "都市丸ごとのシミュレーション技術研究組合",
     "news", "media",
     "https://cityscalekobe.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Integrated Engineering System Research and Development Partnership\",\"media\",\"batch:50\"]",
     3600,
     "Integrated Engineering System Research and Development Partnership — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_clack_ne_jp, "jp-b50-clack-ne-jp",
     "Certified NPO CLACK", "認定ＮＰＯ法人ＣＬＡＣＫ",
     "news", "media",
     "https://clack.ne.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Certified NPO CLACK\",\"media\",\"batch:50\"]",
     3600,
     "Certified NPO CLACK — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_classic_or_jp_posts_default, "jp-b50-classic-or-jp-posts-default",
     "Japan Association of Classical Music Presenters — 一般社団法人日本クラシック音楽事業協会 - Atom", "日本クラシック音楽事業協会 — 一般社団法人日本クラシック音楽事業協会 - Atom",
     "news", "media",
     "https://www.classic.or.jp/feeds/posts/default",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Association of Classical Music Presenters — 一般社団法人日本クラシック音楽事業協会 - Atom\",\"media\",\"batch:50\"]",
     3600,
     "Japan Association of Classical Music Presenters — 一般社団法人日本クラシック音楽事業協会 - Atom — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 25 entries at probe")

RSSX(jp_b50_classic_or_jp_posts_default_alt_rss, "jp-b50-classic-or-jp-posts-default-alt-rss",
     "Japan Association of Classical Music Presenters — 一般社団法人日本クラシック音楽事業協会 - RSS", "日本クラシック音楽事業協会 — 一般社団法人日本クラシック音楽事業協会 - RSS",
     "news", "media",
     "https://www.classic.or.jp/feeds/posts/default?alt=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Association of Classical Music Presenters — 一般社団法人日本クラシック音楽事業協会 - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Japan Association of Classical Music Presenters — 一般社団法人日本クラシック音楽事業協会 - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 25 entries at probe")

RSSX(jp_b50_cmtwork_net, "jp-b50-cmtwork-net",
     "Community Work Research and Practice Center", "コミュニティワーク研究実践センター",
     "news", "media",
     "https://cmtwork.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Community Work Research and Practice Center\",\"media\",\"batch:50\"]",
     3600,
     "Community Work Research and Practice Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_codezine_jp_new, "jp-b50-codezine-jp-new",
     "CodeZine（コードジン）- 開発者のための情報メディア — RSS new article", "CodeZine（コードジン）- 開発者のための情報メディア — RSS new article",
     "news", "media",
     "https://codezine.jp/rss/new/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:CodeZine（コードジン）- 開発者のための情報メディア — RSS new article\",\"media\",\"batch:50\"]",
     3600,
     "CodeZine（コードジン）- 開発者のための情報メディア — RSS new article — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_communitylink_jp, "jp-b50-communitylink-jp",
     "Community Link", "コミュニティリンク",
     "news", "media",
     "https://communitylink.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Community Link\",\"media\",\"batch:50\"]",
     3600,
     "Community Link — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_conet_or_jp, "jp-b50-conet-or-jp",
     "Institute of Community Association Network", "一般社団法人コミュニティネットワーク協会",
     "news", "media",
     "https://conet.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Institute of Community Association Network\",\"media\",\"batch:50\"]",
     3600,
     "Institute of Community Association Network — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_creator_economy_jp, "jp-b50-creator-economy-jp",
     "Creater Economy Association", "クリエイターエコノミー協会",
     "news", "media",
     "https://creator-economy.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Creater Economy Association\",\"media\",\"batch:50\"]",
     3600,
     "Creater Economy Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_cties_jp, "jp-b50-cties-jp",
     "Cyber Campus Consortium TIES", "TIES",
     "news", "media",
     "https://www.cties.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Cyber Campus Consortium TIES\",\"media\",\"batch:50\"]",
     3600,
     "Cyber Campus Consortium TIES — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_cus4_zwtk_or_jp, "jp-b50-cus4-zwtk-or-jp",
     "Wagyu Registry Association", "公益社団法人全国和牛登録協会",
     "news", "media",
     "http://cus4.zwtk.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Wagyu Registry Association\",\"media\",\"batch:50\"]",
     3600,
     "Wagyu Registry Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_cwaj_org, "jp-b50-cwaj-org",
     "College Women's Association of Japan", "大学婦人協会",
     "news", "media",
     "https://cwaj.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:College Women's Association of Japan\",\"media\",\"batch:50\"]",
     3600,
     "College Women's Association of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_cwsjapan_org, "jp-b50-cwsjapan-org",
     "CWS Japan", "ＣＷＳ Ｊａｐａｎ",
     "news", "media",
     "https://www.cwsjapan.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:CWS Japan\",\"media\",\"batch:50\"]",
     3600,
     "CWS Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_cynd_co_jp, "jp-b50-cynd-co-jp",
     "株式会社サインド", "株式会社サインド",
     "news", "media",
     "https://cynd.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社サインド\",\"media\",\"batch:50\"]",
     3600,
     "株式会社サインド — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_daito_jc_jp_feed_rss2, "jp-b50-daito-jc-jp-feed-rss2",
     "Junior Chamber International Daitō", "一般社団法人大東青年会議所",
     "news", "media",
     "https://daito-jc.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Junior Chamber International Daitō\",\"media\",\"batch:50\"]",
     3600,
     "Junior Chamber International Daitō — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_das_or_jp_feed_rss2, "jp-b50-das-or-jp-feed-rss2",
     "DAS Designers Association", "一般社団法人総合デザイナー協会",
     "news", "media",
     "https://das.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:DAS Designers Association\",\"media\",\"batch:50\"]",
     3600,
     "DAS Designers Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_dear_bae_com, "jp-b50-dear-bae-com",
     "bae", "bae",
     "news", "media",
     "https://www.dear-bae.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:bae\",\"media\",\"batch:50\"]",
     3600,
     "bae — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_densetsuko_co_jp, "jp-b50-densetsuko-co-jp",
     "HOME", "HOME",
     "news", "media",
     "https://www.densetsuko.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:HOME\",\"media\",\"batch:50\"]",
     3600,
     "HOME — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_developers_freee_co_jp, "jp-b50-developers-freee-co-jp",
     "freee Developers Hub", "freee Developers Hub",
     "news", "media",
     "https://developers.freee.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:freee Developers Hub\",\"media\",\"batch:50\"]",
     3600,
     "freee Developers Hub — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_developers_prtimes_com, "jp-b50-developers-prtimes-com",
     "PR TIMES 開発者ブログ", "PR TIMES 開発者ブログ",
     "news", "media",
     "https://developers.prtimes.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:PR TIMES 開発者ブログ\",\"media\",\"batch:50\"]",
     3600,
     "PR TIMES 開発者ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_devnetinternational_org, "jp-b50-devnetinternational-org",
     "DEVNET", "DEVNET",
     "news", "media",
     "https://devnetinternational.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:DEVNET\",\"media\",\"batch:50\"]",
     3600,
     "DEVNET — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_diaelec_hd_co_jp, "jp-b50-diaelec-hd-co-jp",
     "ダイヤモンドエレクトリックホールディングス株式会社| DIAMOND ELECTRIC HOLDINGS", "ダイヤモンドエレクトリックホールディングス株式会社| DIAMOND ELECTRIC HOLDINGS",
     "news", "media",
     "https://www.diaelec-hd.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:ダイヤモンドエレクトリックホールディングス株式会社| DIAMOND ELECTRIC HOLDINGS\",\"media\",\"batch:50\"]",
     3600,
     "ダイヤモンドエレクトリックホールディングス株式会社| DIAMOND ELECTRIC HOLDINGS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_dits_center_jp, "jp-b50-dits-center-jp",
     "Decarbonized Industrial Thermo-System Center", "脱炭素産業熱システム技術研究組合",
     "news", "media",
     "https://dits-center.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Decarbonized Industrial Thermo-System Center\",\"media\",\"batch:50\"]",
     3600,
     "Decarbonized Industrial Thermo-System Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 4 entries at probe")

RSSX(jp_b50_diversitykobo_org_blog_feed, "jp-b50-diversitykobo-org-blog-feed",
     "Diversity Workshop — ダイバーシティ工房 - RSS", "ダイバーシティ工房 — ダイバーシティ工房 - RSS",
     "news", "media",
     "https://www.diversitykobo.org/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Diversity Workshop — ダイバーシティ工房 - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Diversity Workshop — ダイバーシティ工房 - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_doronko_club_or_jp, "jp-b50-doronko-club-or-jp",
     "Children's Support Doronko Club", "子どもサポートどろんこクラブ",
     "news", "media",
     "https://doronko-club.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Children's Support Doronko Club\",\"media\",\"batch:50\"]",
     3600,
     "Children's Support Doronko Club — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_doshisha_co_jp, "jp-b50-doshisha-co-jp",
     "株式会社ドウシシャ", "株式会社ドウシシャ",
     "news", "media",
     "https://www.doshisha.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社ドウシシャ\",\"media\",\"batch:50\"]",
     3600,
     "株式会社ドウシシャ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_earc_or_jp, "jp-b50-earc-or-jp",
     "Employment Advance Research Center", "雇用開発センター",
     "news", "media",
     "https://earc.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Employment Advance Research Center\",\"media\",\"batch:50\"]",
     3600,
     "Employment Advance Research Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_eazima_co_jp, "jp-b50-eazima-co-jp",
     "暁飯島工業(株) -", "暁飯島工業(株) -",
     "news", "media",
     "https://www.eazima.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:暁飯島工業(株) -\",\"media\",\"batch:50\"]",
     3600,
     "暁飯島工業(株) - — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_eco_hyogo_jp_tools_blocks_page_list_rss_bid_15753_cid_1_arhandle_main, "jp-b50-eco-hyogo-jp-tools-blocks-page-list-rss-bid-15753-cid-1-arhandle-main",
     "Hyogo Environmental Advancement Association — 新着情報", "公益財団法人ひょうご環境創造協会 — 新着情報",
     "news", "media",
     "https://www.eco-hyogo.jp/index.php/tools/blocks/page_list/rss?bID=15753&cID=1&arHandle=Main",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hyogo Environmental Advancement Association — 新着情報\",\"media\",\"batch:50\"]",
     3600,
     "Hyogo Environmental Advancement Association — 新着情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_ecpr_or_jp, "jp-b50-ecpr-or-jp",
     "Ehime Center for Regional Vitality Creation", "えひめ地域活力創造センター",
     "news", "media",
     "https://www.ecpr.or.jp/feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ehime Center for Regional Vitality Creation\",\"media\",\"batch:50\"]",
     3600,
     "Ehime Center for Regional Vitality Creation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_ehsc_jp, "jp-b50-ehsc-jp",
     "East Hokkaido Sports Commission", "東北海道スポーツコミッション",
     "news", "media",
     "https://ehsc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:East Hokkaido Sports Commission\",\"media\",\"batch:50\"]",
     3600,
     "East Hokkaido Sports Commission — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_eiseikyokai_or_jp, "jp-b50-eiseikyokai-or-jp",
     "Miyagi Public Health Association", "宮城県公衆衛生協会",
     "news", "media",
     "https://www.eiseikyokai.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Miyagi Public Health Association\",\"media\",\"batch:50\"]",
     3600,
     "Miyagi Public Health Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 9 entries at probe")

RSSX(jp_b50_ejcm_or_jp, "jp-b50-ejcm-or-jp",
     "Japan Federation of Construction Managing Engineers Associations", "全国土木施工管理技士会連合会",
     "news", "media",
     "https://www.ejcm.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Federation of Construction Managing Engineers Associations\",\"media\",\"batch:50\"]",
     3600,
     "Japan Federation of Construction Managing Engineers Associations — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ejp_or_jp, "jp-b50-ejp-or-jp",
     "Association of Plastic Molders, East Japan", "東日本プラスチック製品工業協会",
     "news", "media",
     "https://ejp.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Association of Plastic Molders, East Japan\",\"media\",\"batch:50\"]",
     3600,
     "Association of Plastic Molders, East Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_en_yanagida_flower_jp, "jp-b50-en-yanagida-flower-jp",
     "Yanagida Botanical Park", "能登町柳田植物公園",
     "news", "media",
     "https://en.yanagida-flower.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yanagida Botanical Park\",\"media\",\"batch:50\"]",
     3600,
     "Yanagida Botanical Park — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_enaa_or_jp, "jp-b50-enaa-or-jp",
     "Engineering Advancement Association of Japan", "般財団法人エンジニアリング協会",
     "news", "media",
     "https://www.enaa.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Engineering Advancement Association of Japan\",\"media\",\"batch:50\"]",
     3600,
     "Engineering Advancement Association of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_engineer_crowdworks_jp, "jp-b50-engineer-crowdworks-jp",
     "クラウドワークス エンジニアブログ", "クラウドワークス エンジニアブログ",
     "news", "media",
     "https://engineer.crowdworks.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:クラウドワークス エンジニアブログ\",\"media\",\"batch:50\"]",
     3600,
     "クラウドワークス エンジニアブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_engineers_ffri_jp, "jp-b50-engineers-ffri-jp",
     "FFRIエンジニアブログ", "FFRIエンジニアブログ",
     "news", "media",
     "https://engineers.ffri.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:FFRIエンジニアブログ\",\"media\",\"batch:50\"]",
     3600,
     "FFRIエンジニアブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_enterprisezine_jp_new, "jp-b50-enterprisezine-jp-new",
     "EnterpriseZine（エンタープライズジン） — RSS new article", "EnterpriseZine（エンタープライズジン） — RSS new article",
     "news", "media",
     "https://enterprisezine.jp/rss/new/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:EnterpriseZine（エンタープライズジン） — RSS new article\",\"media\",\"batch:50\"]",
     3600,
     "EnterpriseZine（エンタープライズジン） — RSS new article — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_env_r_com, "jp-b50-env-r-com",
     "Environmental Relations Institute", "特定非営利活動法人環境リレーションズ研究所",
     "news", "media",
     "https://env-r.com/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Environmental Relations Institute\",\"media\",\"batch:50\"]",
     3600,
     "Environmental Relations Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_esports_world_jp, "jp-b50-esports-world-jp",
     "eSports World|eスポーツを戦う人、観る人、携わる人のための専門サイト", "eSports World|eスポーツを戦う人、観る人、携わる人のための専門サイト",
     "news", "media",
     "https://esports-world.jp/rss/",
     "ja", "[\"news\",\"country:jp\",\"publisher:eSports World|eスポーツを戦う人、観る人、携わる人のための専門サイト\",\"media\",\"batch:50\"]",
     3600,
     "eSports World|eスポーツを戦う人、観る人、携わる人のための専門サイト — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_estelle_co_jp, "jp-b50-estelle-co-jp",
     "ESTELLE HOLDINGS CO.,LTD.", "ESTELLE HOLDINGS CO.,LTD.",
     "news", "media",
     "https://www.estelle.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:ESTELLE HOLDINGS CO.,LTD.\",\"media\",\"batch:50\"]",
     3600,
     "ESTELLE HOLDINGS CO.,LTD. — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_f_jast_or_jp, "jp-b50-f-jast-or-jp",
     "Japan AeroSpace Technology Foundation", "一般財団法人航空宇宙技術振興財団",
     "news", "media",
     "https://www.f-jast.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan AeroSpace Technology Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Japan AeroSpace Technology Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_fabercompany_co_jp, "jp-b50-fabercompany-co-jp",
     "株式会社Faber Company (ファベルカンパニー)", "株式会社Faber Company (ファベルカンパニー)",
     "news", "media",
     "https://www.fabercompany.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社Faber Company (ファベルカンパニー)\",\"media\",\"batch:50\"]",
     3600,
     "株式会社Faber Company (ファベルカンパニー) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_face_or_jp, "jp-b50-face-or-jp",
     "Factory Assisting 3C Emergence", "創造支援工房フェイス",
     "news", "media",
     "https://www.face.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Factory Assisting 3C Emergence\",\"media\",\"batch:50\"]",
     3600,
     "Factory Assisting 3C Emergence — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_factcheckcenter_jp, "jp-b50-factcheckcenter-jp",
     "Japan Fact-check Center — 日本ファクトチェックセンター (JFC)", "日本ファクトチェックセンター — 日本ファクトチェックセンター (JFC)",
     "news", "media",
     "https://www.factcheckcenter.jp/rss/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Fact-check Center — 日本ファクトチェックセンター (JFC)\",\"media\",\"batch:50\"]",
     3600,
     "Japan Fact-check Center — 日本ファクトチェックセンター (JFC) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_feeds_feedburner_com_hatena_b_hotentry, "jp-b50-feeds-feedburner-com-hatena-b-hotentry",
     "Hatena Hot Entry", "はてなブックマーク人気",
     "news", "media",
     "https://feeds.feedburner.com/hatena/b/hotentry",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hatena Hot Entry\",\"media\",\"batch:50\"]",
     3600,
     "Hatena Hot Entry — news feed (40 entries at probe)")

RSSX(jp_b50_fengming_jp, "jp-b50-fengming-jp",
     "植物園", "愛知豊明花の文化園「とびだす植物園」",
     "news", "media",
     "https://fengming.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:植物園\",\"media\",\"batch:50\"]",
     3600,
     "植物園 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_fenwal_co_jp, "jp-b50-fenwal-co-jp",
     "日本フェンオール株式会社", "日本フェンオール株式会社",
     "news", "media",
     "https://www.fenwal.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:日本フェンオール株式会社\",\"media\",\"batch:50\"]",
     3600,
     "日本フェンオール株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_fief_or_jp, "jp-b50-fief-or-jp",
     "Fukuoka International Exchange Foundation", "福岡県国際交流センター",
     "news", "media",
     "https://fief.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Fukuoka International Exchange Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Fukuoka International Exchange Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_flanders_jp, "jp-b50-flanders-jp",
     "Arts Flanders Japan", "公益財団法人フランダースセンター",
     "news", "media",
     "http://www.flanders.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Arts Flanders Japan\",\"media\",\"batch:50\"]",
     3600,
     "Arts Flanders Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 9 entries at probe")

RSSX(jp_b50_flowercenter_jp, "jp-b50-flowercenter-jp",
     "Hyogo Prefectural Flower Center", "兵庫県立フラワーセンター",
     "news", "media",
     "https://flowercenter.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hyogo Prefectural Flower Center\",\"media\",\"batch:50\"]",
     3600,
     "Hyogo Prefectural Flower Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_fm_tango_jp_feed_rss2, "jp-b50-fm-tango-jp-feed-rss2",
     "FMたんご 79.4", "京丹後コミュニティ放送",
     "news", "media",
     "https://fm-tango.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:FMたんご 79.4\",\"media\",\"batch:50\"]",
     3600,
     "FMたんご 79.4 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_foejapan_org, "jp-b50-foejapan-org",
     "Friends of the Earth Japan — 国際環境NGO FoE Japan RSS Feed", "国際環境NGO FoE Japan — 国際環境NGO FoE Japan RSS Feed",
     "news", "media",
     "https://foejapan.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Friends of the Earth Japan — 国際環境NGO FoE Japan RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Friends of the Earth Japan — 国際環境NGO FoE Japan RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_fooma_or_jp, "jp-b50-fooma-or-jp",
     "The Japan Food Machinery Manufacturers' Association", "日本食品機械工業会",
     "news", "media",
     "https://www.fooma.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japan Food Machinery Manufacturers' Association\",\"media\",\"batch:50\"]",
     3600,
     "The Japan Food Machinery Manufacturers' Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_fruitpark_org_rss2, "jp-b50-fruitpark-org-rss2",
     "Togokusan Fruits Park", "東谷山フルーツパーク",
     "news", "media",
     "https://www.fruitpark.org/rss2.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Togokusan Fruits Park\",\"media\",\"batch:50\"]",
     3600,
     "Togokusan Fruits Park — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ftech_co_jp, "jp-b50-ftech-co-jp",
     "株式会社エフテック -", "株式会社エフテック -",
     "news", "media",
     "https://www.ftech.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社エフテック -\",\"media\",\"batch:50\"]",
     3600,
     "株式会社エフテック - — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_fuluhashi_co_jp, "jp-b50-fuluhashi-co-jp",
     "フルハシEPO株式会社 -環境で未来をクリエイトする-", "フルハシEPO株式会社 -環境で未来をクリエイトする-",
     "news", "media",
     "https://www.fuluhashi.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:フルハシEPO株式会社 -環境で未来をクリエイトする-\",\"media\",\"batch:50\"]",
     3600,
     "フルハシEPO株式会社 -環境で未来をクリエイトする- — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_fureai_nagoya_jp, "jp-b50-fureai-nagoya-jp",
     "Mai Yume", "まいゆめ",
     "news", "media",
     "https://fureai-nagoya.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Mai Yume\",\"media\",\"batch:50\"]",
     3600,
     "Mai Yume — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_fureaizaidan_or_jp, "jp-b50-fureaizaidan-or-jp",
     "Hokkaido Community Activities Promotion Association", "北海道地域活動振興協会",
     "news", "media",
     "https://www.fureaizaidan.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hokkaido Community Activities Promotion Association\",\"media\",\"batch:50\"]",
     3600,
     "Hokkaido Community Activities Promotion Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_gamepres_org_en, "jp-b50-gamepres-org-en",
     "Game Preservation Society", "ゲーム保存協会",
     "news", "media",
     "https://www.gamepres.org/en/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Game Preservation Society\",\"media\",\"batch:50\"]",
     3600,
     "Game Preservation Society — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_gamespark_jp_rss20, "jp-b50-gamespark-jp-rss20",
     "Game*Spark", "Game*Spark",
     "news", "media",
     "https://www.gamespark.jp/rss20/index.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Game*Spark\",\"media\",\"batch:50\"]",
     3600,
     "Game*Spark — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_gardengroup_co_jp, "jp-b50-gardengroup-co-jp",
     "株式会社ガーデン", "株式会社ガーデン",
     "news", "media",
     "https://gardengroup.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社ガーデン\",\"media\",\"batch:50\"]",
     3600,
     "株式会社ガーデン — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_gd_inc_co_jp, "jp-b50-gd-inc-co-jp",
     "GRAPHITE DESIGN", "GRAPHITE DESIGN",
     "news", "media",
     "https://www.gd-inc.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:GRAPHITE DESIGN\",\"media\",\"batch:50\"]",
     3600,
     "GRAPHITE DESIGN — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_gec_jp, "jp-b50-gec-jp",
     "Global Environment Centre Foundation", "公益財団法人地球環境センター",
     "news", "media",
     "https://gec.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Global Environment Centre Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Global Environment Centre Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_gif_or_jp, "jp-b50-gif-or-jp",
     "Global Infrastructure Fund Research Foundation Japan", "日本グローバル・インフラストラクチャー研究財団",
     "news", "media",
     "https://gif.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Global Infrastructure Fund Research Foundation Japan\",\"media\",\"batch:50\"]",
     3600,
     "Global Infrastructure Fund Research Foundation Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_gifu_med_or_jp, "jp-b50-gifu-med-or-jp",
     "一般社団法人岐阜県医師会", "一般社団法人岐阜県医師会",
     "news", "media",
     "https://www.gifu.med.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:一般社団法人岐阜県医師会\",\"media\",\"batch:50\"]",
     3600,
     "一般社団法人岐阜県医師会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_gihyo_jp_rss1, "jp-b50-gihyo-jp-rss1",
     "gihyo.jp — gihyo.jp RSS1.0", "gihyo.jp — gihyo.jp RSS1.0",
     "news", "media",
     "https://gihyo.jp/feed/rss1",
     "ja", "[\"news\",\"country:jp\",\"publisher:gihyo.jp — gihyo.jp RSS1.0\",\"media\",\"batch:50\"]",
     3600,
     "gihyo.jp — gihyo.jp RSS1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 878 entries at probe")

RSSX(jp_b50_ginreikai_net_blog, "jp-b50-ginreikai-net-blog",
     "Ginreikai — ブログ", "銀鈴会 — ブログ",
     "news", "media",
     "https://www.ginreikai.net/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ginreikai — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "Ginreikai — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 18 entries at probe")

RSSX(jp_b50_gizmodo_jp, "jp-b50-gizmodo-jp",
     "Gizmodo Japan", "ギズモード",
     "news", "media",
     "https://www.gizmodo.jp/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Gizmodo Japan\",\"media\",\"batch:50\"]",
     3600,
     "Gizmodo Japan — news feed (20 entries at probe)")

RSSX(jp_b50_glider_jp, "jp-b50-glider-jp",
     "Japan Soaring Club", "日本グライダークラブ",
     "news", "media",
     "https://www.glider.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Soaring Club\",\"media\",\"batch:50\"]",
     3600,
     "Japan Soaring Club — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_globalecolabelling_net, "jp-b50-globalecolabelling-net",
     "Global Ecolabelling Network", "国際エコラベル・ネットワーク",
     "news", "media",
     "https://globalecolabelling.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Global Ecolabelling Network\",\"media\",\"batch:50\"]",
     3600,
     "Global Ecolabelling Network — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_gqjapan_jp, "jp-b50-gqjapan-jp",
     "メンズファッション、時計、高級車、最新ライフスタイル情報", "メンズファッション、時計、高級車、最新ライフスタイル情報",
     "news", "media",
     "https://www.gqjapan.jp/feed/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:メンズファッション、時計、高級車、最新ライフスタイル情報\",\"media\",\"batch:50\"]",
     3600,
     "メンズファッション、時計、高級車、最新ライフスタイル情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_groundwork_or_jp_feed_rss2, "jp-b50-groundwork-or-jp-feed-rss2",
     "Japan Groundwork Association", "一般財団法人日本グラウンドワーク協会",
     "news", "media",
     "https://groundwork.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Groundwork Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Groundwork Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_group_momiji_com, "jp-b50-group-momiji-com",
     "Group Momiji", "グループもみじ",
     "news", "media",
     "https://www.group-momiji.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Group Momiji\",\"media\",\"batch:50\"]",
     3600,
     "Group Momiji — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_gyoda_med_org, "jp-b50-gyoda-med-org",
     "Gyoda Medical Association", "行田市医師会",
     "news", "media",
     "https://gyoda-med.org/index.php/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Gyoda Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Gyoda Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_h_guidedog_org, "jp-b50-h-guidedog-org",
     "Hokkaido Guide Dog Association", "北海道盲導犬協会",
     "news", "media",
     "https://www.h-guidedog.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hokkaido Guide Dog Association\",\"media\",\"batch:50\"]",
     3600,
     "Hokkaido Guide Dog Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hachinohe_ip_co_jp, "jp-b50-hachinohe-ip-co-jp",
     "Hachinohe Regional Advanced Technology Promotion Center Foundation", "八戸地域高度技術振興センター",
     "news", "media",
     "https://www.hachinohe-ip.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hachinohe Regional Advanced Technology Promotion Center Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Hachinohe Regional Advanced Technology Promotion Center Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hakuyohin_or_jp, "jp-b50-hakuyohin-or-jp",
     "Nippon Hakuyohin Kentei Kyokai", "日本舶用品検定協会",
     "news", "media",
     "https://www.hakuyohin.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nippon Hakuyohin Kentei Kyokai\",\"media\",\"batch:50\"]",
     3600,
     "Nippon Hakuyohin Kentei Kyokai — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hanakokusai_wordpress_com, "jp-b50-hanakokusai-wordpress-com",
     "Hanamaki International Exchange Association", "花巻国際交流協会",
     "news", "media",
     "https://hanakokusai.wordpress.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hanamaki International Exchange Association\",\"media\",\"batch:50\"]",
     3600,
     "Hanamaki International Exchange Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hanatoizumi_com, "jp-b50-hanatoizumi-com",
     "Hanaizumi Flower World — 花と泉の公園 RSS Feed", "花と泉の公園 — 花と泉の公園 RSS Feed",
     "news", "media",
     "https://hanatoizumi.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hanaizumi Flower World — 花と泉の公園 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Hanaizumi Flower World — 花と泉の公園 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hands_or_jp, "jp-b50-hands-or-jp",
     "HANDS", "ＨＡＮＤＳ",
     "news", "media",
     "https://www.hands.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:HANDS\",\"media\",\"batch:50\"]",
     3600,
     "HANDS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_haot_jp, "jp-b50-haot-jp",
     "Hokkaido Association of Occupational Therapists", "北海道作業療法士会",
     "news", "media",
     "https://www.haot.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hokkaido Association of Occupational Therapists\",\"media\",\"batch:50\"]",
     3600,
     "Hokkaido Association of Occupational Therapists — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hataraku_shiga_net, "jp-b50-hataraku-shiga-net",
     "Shiga Prefecture Social Employment Business Promotion Center", "滋賀県社会就労事業振興センター",
     "news", "media",
     "https://hataraku-shiga.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Shiga Prefecture Social Employment Business Promotion Center\",\"media\",\"batch:50\"]",
     3600,
     "Shiga Prefecture Social Employment Business Promotion Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_hepco_co_jp_info_infevt_rss, "jp-b50-hepco-co-jp-info-infevt-rss",
     "北海道電力 — 北海道電力 ほくでんからのお知らせ", "北海道電力 — 北海道電力 ほくでんからのお知らせ",
     "news", "media",
     "https://www.hepco.co.jp/info/rss/infevt_rss.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:北海道電力 — 北海道電力 ほくでんからのお知らせ\",\"media\",\"batch:50\"]",
     3600,
     "北海道電力 — 北海道電力 ほくでんからのお知らせ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_hiezu_jp, "jp-b50-hiezu-jp",
     "Hiezu", "日吉津村",
     "news", "media",
     "https://www.hiezu.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hiezu\",\"media\",\"batch:50\"]",
     3600,
     "Hiezu — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hirose_isf_or_jp, "jp-b50-hirose-isf-or-jp",
     "Hirose Foundation", "公益財団法人ヒロセ財団",
     "news", "media",
     "https://hirose-isf.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hirose Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Hirose Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_hiroshima_catholic_jp, "jp-b50-hiroshima-catholic-jp",
     "Roman Catholic Diocese of Hiroshima", "カトリック広島司教区",
     "news", "media",
     "https://hiroshima.catholic.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Roman Catholic Diocese of Hiroshima\",\"media\",\"batch:50\"]",
     3600,
     "Roman Catholic Diocese of Hiroshima — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hisaichi5518_hatenablog_jp, "jp-b50-hisaichi5518-hatenablog-jp",
     "パルカワ2", "パルカワ2",
     "news", "media",
     "https://hisaichi5518.hatenablog.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:パルカワ2\",\"media\",\"batch:50\"]",
     3600,
     "パルカワ2 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_hna_or_jp_rss2, "jp-b50-hna-or-jp-rss2",
     "Hyogo Nursing Association", "兵庫県看護協会",
     "news", "media",
     "https://www.hna.or.jp/rss2.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hyogo Nursing Association\",\"media\",\"batch:50\"]",
     3600,
     "Hyogo Nursing Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hobea_or_jp, "jp-b50-hobea-or-jp",
     "Hokkaido Building Engineering Association", "一般社団法人北海道建築技術協会",
     "news", "media",
     "https://hobea.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hokkaido Building Engineering Association\",\"media\",\"batch:50\"]",
     3600,
     "Hokkaido Building Engineering Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hofg_org, "jp-b50-hofg-org",
     "Heart of Gold", "ハート・オブ・ゴールド",
     "news", "media",
     "https://hofg.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Heart of Gold\",\"media\",\"batch:50\"]",
     3600,
     "Heart of Gold — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_hokkaido_taigan_jp, "jp-b50-hokkaido-taigan-jp",
     "Hokkaido Cancer Society — 公益財団法人 北海道対がん協会のフィード", "公益財団法人北海道対がん協会 — 公益財団法人 北海道対がん協会のフィード",
     "news", "media",
     "https://hokkaido-taigan.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hokkaido Cancer Society — 公益財団法人 北海道対がん協会のフィード\",\"media\",\"batch:50\"]",
     3600,
     "Hokkaido Cancer Society — 公益財団法人 北海道対がん協会のフィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hokurouren_jp, "jp-b50-hokurouren-jp",
     "Hokkaido Federation of the Deaf", "北海道ろうあ連盟",
     "news", "media",
     "https://hokurouren.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hokkaido Federation of the Deaf\",\"media\",\"batch:50\"]",
     3600,
     "Hokkaido Federation of the Deaf — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_hoso_jigyo_or_jp, "jp-b50-hoso-jigyo-or-jp",
     "Association of Hokkaido Pavement Constructors", "北海道舗装事業協会",
     "news", "media",
     "https://hoso-jigyo.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Association of Hokkaido Pavement Constructors\",\"media\",\"batch:50\"]",
     3600,
     "Association of Hokkaido Pavement Constructors — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hotel_barmen_hba_or_jp_blog_feed, "jp-b50-hotel-barmen-hba-or-jp-blog-feed",
     "Hotel Barmen's Association, Japan — hotel-barmen - RSS", "日本ホテルバーメンズ協会 — hotel-barmen - RSS",
     "news", "media",
     "https://www.hotel-barmen-hba.or.jp/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hotel Barmen's Association, Japan — hotel-barmen - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Hotel Barmen's Association, Japan — hotel-barmen - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_hotspacenakahara_org, "jp-b50-hotspacenakahara-org",
     "Hot Space Nakahara", "ホッとスペース中原",
     "news", "media",
     "https://hotspacenakahara.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hot Space Nakahara\",\"media\",\"batch:50\"]",
     3600,
     "Hot Space Nakahara — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_hurusatonokai_jp_blog, "jp-b50-hurusatonokai-jp-blog",
     "Self-Reliance Support Center Furusato (Hometown) Association — ブログ", "自立支援センターふるさとの会 — ブログ",
     "news", "media",
     "https://www.hurusatonokai.jp/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:Self-Reliance Support Center Furusato (Hometown) Association — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "Self-Reliance Support Center Furusato (Hometown) Association — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_hws_kyokai_or_jp_format_feed_type_atom, "jp-b50-hws-kyokai-or-jp-format-feed-type-atom",
     "Health, Labour and Welfare Statistics Association — 一般財団法人厚生労働統計協会｜国民衛生の動向、厚生労働統計情報を提供", "一般財団法人厚生労働統計協会 — 一般財団法人厚生労働統計協会｜国民衛生の動向、厚生労働統計情報を提供",
     "news", "media",
     "https://www.hws-kyokai.or.jp/?format=feed&type=atom",
     "ja", "[\"news\",\"country:jp\",\"publisher:Health, Labour and Welfare Statistics Association — 一般財団法人厚生労働統計協会｜国民衛生の動向、厚生労働統計情報を提供\",\"media\",\"batch:50\"]",
     3600,
     "Health, Labour and Welfare Statistics Association — 一般財団法人厚生労働統計協会｜国民衛生の動向、厚生労働統計情報を提供 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_hws_kyokai_or_jp_format_feed_type_rss, "jp-b50-hws-kyokai-or-jp-format-feed-type-rss",
     "Health, Labour and Welfare Statistics Association — 一般財団法人厚生労働統計協会｜国民衛生の動向、厚生労働統計情報を提供", "一般財団法人厚生労働統計協会 — 一般財団法人厚生労働統計協会｜国民衛生の動向、厚生労働統計情報を提供",
     "news", "media",
     "https://www.hws-kyokai.or.jp/?format=feed&type=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Health, Labour and Welfare Statistics Association — 一般財団法人厚生労働統計協会｜国民衛生の動向、厚生労働統計情報を提供\",\"media\",\"batch:50\"]",
     3600,
     "Health, Labour and Welfare Statistics Association — 一般財団法人厚生労働統計協会｜国民衛生の動向、厚生労働統計情報を提供 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_hyogodeaf_com, "jp-b50-hyogodeaf-com",
     "Hyogo Association of the Deaf — 公益社団法人兵庫県聴覚障害者協会 RSS Feed", "兵庫県聴覚障害者協会 — 公益社団法人兵庫県聴覚障害者協会 RSS Feed",
     "news", "media",
     "https://hyogodeaf.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hyogo Association of the Deaf — 公益社団法人兵庫県聴覚障害者協会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Hyogo Association of the Deaf — 公益社団法人兵庫県聴覚障害者協会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_hyogokenshin_co_jp, "jp-b50-hyogokenshin-co-jp",
     "けんしん 兵庫県信用組合", "けんしん 兵庫県信用組合",
     "news", "media",
     "https://www.hyogokenshin.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:けんしん 兵庫県信用組合\",\"media\",\"batch:50\"]",
     3600,
     "けんしん 兵庫県信用組合 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_i_sec_net, "jp-b50-i-sec-net",
     "Ibaraki New Business Conference", "茨城ニュービジネス協議会",
     "news", "media",
     "https://i-sec.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ibaraki New Business Conference\",\"media\",\"batch:50\"]",
     3600,
     "Ibaraki New Business Conference — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_i_step_org_news, "jp-b50-i-step-org-news",
     "Science and Technology Promotion Foundation of Ibaraki", "一般財団法人茨城県科学技術振興財団",
     "news", "media",
     "https://www.i-step.org/news.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Science and Technology Promotion Foundation of Ibaraki\",\"media\",\"batch:50\"]",
     3600,
     "Science and Technology Promotion Foundation of Ibaraki — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 16 entries at probe")

RSSX(jp_b50_iamu_edu_org, "jp-b50-iamu-edu-org",
     "International Association of Maritime Universities", "国際海事大学連合",
     "news", "media",
     "https://iamu-edu.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:International Association of Maritime Universities\",\"media\",\"batch:50\"]",
     3600,
     "International Association of Maritime Universities — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ibagen_or_jp, "jp-b50-ibagen-or-jp",
     "Ibaraki Atomic Energy Council", "茨城原子力協議会",
     "news", "media",
     "http://ibagen.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ibaraki Atomic Energy Council\",\"media\",\"batch:50\"]",
     3600,
     "Ibaraki Atomic Energy Council — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_ibaken_or_jp, "jp-b50-ibaken-or-jp",
     "Ibaraki Association of Construction Industry", "茨城県建設業協会",
     "news", "media",
     "http://ibaken.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ibaraki Association of Construction Industry\",\"media\",\"batch:50\"]",
     3600,
     "Ibaraki Association of Construction Industry — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_iccc_jp, "jp-b50-iccc-jp",
     "ICCC", "ICCC",
     "news", "media",
     "https://iccc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:ICCC\",\"media\",\"batch:50\"]",
     3600,
     "ICCC — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ichinomiya_jc_or_jp, "jp-b50-ichinomiya-jc-or-jp",
     "Junior Chamber International Ichinomiya", "一宮青年会議所",
     "news", "media",
     "http://www.ichinomiya-jc.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Junior Chamber International Ichinomiya\",\"media\",\"batch:50\"]",
     3600,
     "Junior Chamber International Ichinomiya — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_ih_osaka_or_jp, "jp-b50-ih-osaka-or-jp",
     "Osaka International House Foundation", "大阪国際交流センター",
     "news", "media",
     "https://www.ih-osaka.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Osaka International House Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Osaka International House Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ihep_jp, "jp-b50-ihep-jp",
     "Institute for Health Economics and Policy", "医療経済研究機構",
     "news", "media",
     "https://www.ihep.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Institute for Health Economics and Policy\",\"media\",\"batch:50\"]",
     3600,
     "Institute for Health Economics and Policy — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_iif_jp, "jp-b50-iif-jp",
     "株式会社インターネットインフィニティー", "株式会社インターネットインフィニティー",
     "news", "media",
     "https://iif.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社インターネットインフィニティー\",\"media\",\"batch:50\"]",
     3600,
     "株式会社インターネットインフィニティー — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_iij_ad_jp_rss_temp_seminar, "jp-b50-iij-ad-jp-rss-temp-seminar",
     "トップ — イベント・セミナーRSS 2.0", "トップ — イベント・セミナーRSS 2.0",
     "news", "media",
     "https://www.iij.ad.jp/rss-temp/seminar/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:トップ — イベント・セミナーRSS 2.0\",\"media\",\"batch:50\"]",
     3600,
     "トップ — イベント・セミナーRSS 2.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 244 entries at probe")

RSSX(jp_b50_ikk_or_jp, "jp-b50-ikk-or-jp",
     "Ibaraki Employers' Association", "茨城県経営者協会",
     "news", "media",
     "https://ikk.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ibaraki Employers' Association\",\"media\",\"batch:50\"]",
     3600,
     "Ibaraki Employers' Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 18 entries at probe")

RSSX(jp_b50_ikuji_land_jp, "jp-b50-ikuji-land-jp",
     "Yamagata Child-Rearing Circle Land", "やまがた育児サークルランド",
     "news", "media",
     "http://ikuji-land.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yamagata Child-Rearing Circle Land\",\"media\",\"batch:50\"]",
     3600,
     "Yamagata Child-Rearing Circle Land — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_imomusi_org, "jp-b50-imomusi-org",
     "Imomushi Association", "いもむし",
     "news", "media",
     "https://imomusi.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Imomushi Association\",\"media\",\"batch:50\"]",
     3600,
     "Imomushi Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ingsinc_co_jp, "jp-b50-ingsinc-co-jp",
     "株式会社INGS", "株式会社INGS",
     "news", "media",
     "https://ingsinc.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社INGS\",\"media\",\"batch:50\"]",
     3600,
     "株式会社INGS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_innovation_co_jp, "jp-b50-innovation-co-jp",
     "株式会社イノベーション — 株式会社イノベーション - INNOVATION", "株式会社イノベーション — 株式会社イノベーション - INNOVATION",
     "news", "media",
     "https://www.innovation.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社イノベーション — 株式会社イノベーション - INNOVATION\",\"media\",\"batch:50\"]",
     3600,
     "株式会社イノベーション — 株式会社イノベーション - INNOVATION — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_inside_games_jp_rss20, "jp-b50-inside-games-jp-rss20",
     "インサイド", "インサイド",
     "news", "media",
     "https://www.inside-games.jp/rss20/index.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:インサイド\",\"media\",\"batch:50\"]",
     3600,
     "インサイド — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_io_cyberdefense_jp, "jp-b50-io-cyberdefense-jp",
     "DARK MATTER", "DARK MATTER",
     "news", "media",
     "https://io.cyberdefense.jp/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:DARK MATTER\",\"media\",\"batch:50\"]",
     3600,
     "DARK MATTER — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 153 entries at probe")

RSSX(jp_b50_ishishin_co_jp, "jp-b50-ishishin-co-jp",
     "神奈川県医師信用組合", "神奈川県医師信用組合",
     "news", "media",
     "https://www.ishishin.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:神奈川県医師信用組合\",\"media\",\"batch:50\"]",
     3600,
     "神奈川県医師信用組合 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_isit_or_jp, "jp-b50-isit-or-jp",
     "Institute of Systems, Information Technologies and Nanotechnologies", "九州先端科学技術研究所",
     "news", "media",
     "https://www.isit.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Institute of Systems, Information Technologies and Nanotechnologies\",\"media\",\"batch:50\"]",
     3600,
     "Institute of Systems, Information Technologies and Nanotechnologies — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_istdjapan_org_blog, "jp-b50-istdjapan-org-blog",
     "Institute on Social Theory and Dynamics — ブログ", "社会理論・動態研究所 — ブログ",
     "news", "media",
     "https://www.istdjapan.org/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:Institute on Social Theory and Dynamics — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "Institute on Social Theory and Dynamics — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_iti_japan_or_jp, "jp-b50-iti-japan-or-jp",
     "Japanese Centre of International Theatre Institute", "国際演劇協会日本センター",
     "news", "media",
     "https://iti-japan.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Centre of International Theatre Institute\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Centre of International Theatre Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ituaj_jp_feed_rss2, "jp-b50-ituaj-jp-feed-rss2",
     "The ITU Association of Japan", "一般財団法人日本ITU協会",
     "news", "media",
     "https://ituaj.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:The ITU Association of Japan\",\"media\",\"batch:50\"]",
     3600,
     "The ITU Association of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_iwami_gr_jp_services_rss10_4902, "jp-b50-iwami-gr-jp-services-rss10-4902",
     "Iwami-chō — 大切なお知らせ(RSS)", "岩美町 — 大切なお知らせ(RSS)",
     "news", "media",
     "https://www.iwami.gr.jp/services/rdf/rss10/4902.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Iwami-chō — 大切なお知らせ(RSS)\",\"media\",\"batch:50\"]",
     3600,
     "Iwami-chō — 大切なお知らせ(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_iwami_gr_jp_services_rss10_4903, "jp-b50-iwami-gr-jp-services-rss10-4903",
     "Iwami-chō — 新着情報(RSS)", "岩美町 — 新着情報(RSS)",
     "news", "media",
     "https://www.iwami.gr.jp/services/rdf/rss10/4903.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Iwami-chō — 新着情報(RSS)\",\"media\",\"batch:50\"]",
     3600,
     "Iwami-chō — 新着情報(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_iwami_gr_jp_services_rss10_4904, "jp-b50-iwami-gr-jp-services-rss10-4904",
     "Iwami-chō — 行事レポート(RSS)", "岩美町 — 行事レポート(RSS)",
     "news", "media",
     "https://www.iwami.gr.jp/services/rdf/rss10/4904.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Iwami-chō — 行事レポート(RSS)\",\"media\",\"batch:50\"]",
     3600,
     "Iwami-chō — 行事レポート(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 13 entries at probe")

RSSX(jp_b50_iwami_gr_jp_services_rss10_4905, "jp-b50-iwami-gr-jp-services-rss10-4905",
     "Iwami-chō — 募集(RSS)", "岩美町 — 募集(RSS)",
     "news", "media",
     "https://www.iwami.gr.jp/services/rdf/rss10/4905.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Iwami-chō — 募集(RSS)\",\"media\",\"batch:50\"]",
     3600,
     "Iwami-chō — 募集(RSS) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 13 entries at probe")

RSSX(jp_b50_iwylg_jp_com, "jp-b50-iwylg-jp-com",
     "国際婦人年連絡会", "国際婦人年連絡会",
     "news", "media",
     "https://iwylg-jp.com/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:国際婦人年連絡会\",\"media\",\"batch:50\"]",
     3600,
     "国際婦人年連絡会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_izu_kamori_jp, "jp-b50-izu-kamori-jp",
     "Izu Animal Kingdom", "伊豆アニマルキングダム",
     "news", "media",
     "https://www.izu-kamori.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Izu Animal Kingdom\",\"media\",\"batch:50\"]",
     3600,
     "Izu Animal Kingdom — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_j_fma_or_jp, "jp-b50-j-fma-or-jp",
     "Japan Forming Machinery Association", "一般社団法人日本鍛圧機械工業会",
     "news", "media",
     "https://j-fma.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Forming Machinery Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Forming Machinery Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_j_focus_or_jp_rss2, "jp-b50-j-focus-or-jp-rss2",
     "Foundation for Computational Science", "計算科学振興財団",
     "news", "media",
     "https://www.j-focus.or.jp/rss2.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Foundation for Computational Science\",\"media\",\"batch:50\"]",
     3600,
     "Foundation for Computational Science — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_jabg_or_jp, "jp-b50-jabg-or-jp",
     "Japan Association of Botanical Gardens", "日本植物園協会",
     "news", "media",
     "https://jabg.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Association of Botanical Gardens\",\"media\",\"batch:50\"]",
     3600,
     "Japan Association of Botanical Gardens — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jabmee_or_jp, "jp-b50-jabmee-or-jp",
     "Japanese Association of Building Mechanical and Electrical Engineers", "建築設備技術者協会",
     "news", "media",
     "https://www.jabmee.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Association of Building Mechanical and Electrical Engineers\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Association of Building Mechanical and Electrical Engineers — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_jaca_1963_or_jp, "jp-b50-jaca-1963-or-jp",
     "Japan Air Cleaning Association", "公益社団法人日本空気清浄協会",
     "news", "media",
     "https://www.jaca-1963.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Air Cleaning Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Air Cleaning Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_jadc_jp, "jp-b50-jadc-jp",
     "Japan Aircraft Development Corporation — 一般財団法人 日本航空機開発協会", "一般財団法人日本航空機開発協会 — 一般財団法人 日本航空機開発協会",
     "news", "media",
     "http://www.jadc.jp/RSS.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Aircraft Development Corporation — 一般財団法人 日本航空機開発協会\",\"media\",\"batch:50\"]",
     3600,
     "Japan Aircraft Development Corporation — 一般財団法人 日本航空機開発協会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 9 entries at probe")

RSSX(jp_b50_jadc_jp_files_blog_rss_blog_all, "jp-b50-jadc-jp-files-blog-rss-blog-all",
     "Japan Aircraft Development Corporation — 一般財団法人 日本航空機開発協会", "一般財団法人日本航空機開発協会 — 一般財団法人 日本航空機開発協会",
     "news", "media",
     "http://www.jadc.jp/files/blog/rss/RSS_BLOG_ALL.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Aircraft Development Corporation — 一般財団法人 日本航空機開発協会\",\"media\",\"batch:50\"]",
     3600,
     "Japan Aircraft Development Corporation — 一般財団法人 日本航空機開発協会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_jadec_or_jp, "jp-b50-jadec-or-jp",
     "Jadec", "一般財団法人能力開発工学センター",
     "news", "media",
     "https://jadec.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Jadec\",\"media\",\"batch:50\"]",
     3600,
     "Jadec — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jaeic_or_jp, "jp-b50-jaeic-or-jp",
     "The Japan Architectural Education and Information Center — Recent Entries", "公益財団法人建築技術教育普及センター — Recent Entries",
     "news", "media",
     "https://www.jaeic.or.jp/atom.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japan Architectural Education and Information Center — Recent Entries\",\"media\",\"batch:50\"]",
     3600,
     "The Japan Architectural Education and Information Center — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_jagat_or_jp, "jp-b50-jagat-or-jp",
     "Japan Association of Graphic Arts Technology", "公益社団法人日本印刷技術協会",
     "news", "media",
     "https://www.jagat.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Association of Graphic Arts Technology\",\"media\",\"batch:50\"]",
     3600,
     "Japan Association of Graphic Arts Technology — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jagra_or_jp, "jp-b50-jagra-or-jp",
     "The Japan Graphic Services Industry Association", "日本グラフィックサービス工業会",
     "news", "media",
     "https://www.jagra.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japan Graphic Services Industry Association\",\"media\",\"batch:50\"]",
     3600,
     "The Japan Graphic Services Industry Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jahfa_jp, "jp-b50-jahfa-jp",
     "Japan Automotive Hall of Fame", "日本自動車殿堂",
     "news", "media",
     "https://www.jahfa.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Automotive Hall of Fame\",\"media\",\"batch:50\"]",
     3600,
     "Japan Automotive Hall of Fame — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jaiti_org, "jp-b50-jaiti-org",
     "Japanese Agricultural In-service Training Institute — JAITI RSS Feed", "日本農業研修場協力団 — JAITI RSS Feed",
     "news", "media",
     "http://www.jaiti.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Agricultural In-service Training Institute — JAITI RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Agricultural In-service Training Institute — JAITI RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jankenpon_jp, "jp-b50-jankenpon-jp",
     "Jankenpon Association", "じゃんけんぽん",
     "news", "media",
     "https://www.jankenpon.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Jankenpon Association\",\"media\",\"batch:50\"]",
     3600,
     "Jankenpon Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_japan_israel_friendship_or_jp, "jp-b50-japan-israel-friendship-or-jp",
     "Japan Israel Friendship Association", "公益社団法人日本イスラエル親善協会",
     "news", "media",
     "https://japan-israel-friendship.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Israel Friendship Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Israel Friendship Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_japanese_gardens_jp, "jp-b50-japanese-gardens-jp",
     "Nihon teien bunka kokusai kenkyūkai", "日本庭園文化国際研究会",
     "news", "media",
     "https://japanese-gardens.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nihon teien bunka kokusai kenkyūkai\",\"media\",\"batch:50\"]",
     3600,
     "Nihon teien bunka kokusai kenkyūkai — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_japanheart_org, "jp-b50-japanheart-org",
     "International Medical Foundation of Japan", "財団法人日本国際医療団",
     "news", "media",
     "https://www.japanheart.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:International Medical Foundation of Japan\",\"media\",\"batch:50\"]",
     3600,
     "International Medical Foundation of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_japanmyanmar_org, "jp-b50-japanmyanmar-org",
     "Japan Myanmar Association", "日本ミャンマー協会",
     "news", "media",
     "https://japanmyanmar.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Myanmar Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Myanmar Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_japet_or_jp, "jp-b50-japet-or-jp",
     "Japan Association for Promotion of Educational Technology", "一般社団法人日本教育情報化振興会",
     "news", "media",
     "https://www.japet.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Association for Promotion of Educational Technology\",\"media\",\"batch:50\"]",
     3600,
     "Japan Association for Promotion of Educational Technology — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_japia_or_jp, "jp-b50-japia-or-jp",
     "一般社団法人 日本自動車部品工業会", "一般社団法人 日本自動車部品工業会",
     "news", "media",
     "https://www.japia.or.jp/RSS.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:一般社団法人 日本自動車部品工業会\",\"media\",\"batch:50\"]",
     3600,
     "一般社団法人 日本自動車部品工業会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_japr_or_jp, "jp-b50-japr-or-jp",
     "Japan Association For Advancement Of PHYTO-REGULATORS", "公益財団法人日本植物調節剤研究協会",
     "news", "media",
     "https://japr.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Association For Advancement Of PHYTO-REGULATORS\",\"media\",\"batch:50\"]",
     3600,
     "Japan Association For Advancement Of PHYTO-REGULATORS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 7 entries at probe")

RSSX(jp_b50_jasa_web_jp_post_type_post, "jp-b50-jasa-web-jp-post-type-post",
     "Japanese Association for Strabismus and Amblyopia — お知らせ", "日本弱視斜視学会 — お知らせ",
     "news", "media",
     "https://www.jasa-web.jp/feed/?post_type=post",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Association for Strabismus and Amblyopia — お知らせ\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Association for Strabismus and Amblyopia — お知らせ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jasa_web_jp_post_type_topics_post, "jp-b50-jasa-web-jp-post-type-topics-post",
     "Japanese Association for Strabismus and Amblyopia — topics", "日本弱視斜視学会 — topics",
     "news", "media",
     "https://www.jasa-web.jp/feed/?post_type=topics_post",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Association for Strabismus and Amblyopia — topics\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Association for Strabismus and Amblyopia — topics — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jasca_org, "jp-b50-jasca-org",
     "The Japanese Society of Cultural Anthropology — 日本文化人類学会- The Japanese Society of Cultural Anthropology » フィ", "日本文化人類学会 — 日本文化人類学会- The Japanese Society of Cultural Anthropology » フィ",
     "news", "media",
     "https://jasca.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japanese Society of Cultural Anthropology — 日本文化人類学会- The Japanese Society of Cultural Anthropology » フィ\",\"media\",\"batch:50\"]",
     3600,
     "The Japanese Society of Cultural Anthropology — 日本文化人類学会- The Japanese Society of Cultural Anthropology » フィ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jatahq_org, "jp-b50-jatahq-org",
     "Japan Anti-Tuberculosis Association", "結核予防会",
     "news", "media",
     "https://www.jatahq.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Anti-Tuberculosis Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Anti-Tuberculosis Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_jbfa_or_jp, "jp-b50-jbfa-or-jp",
     "Japan-Brunei Friendship Association — 公益社団法人 日本ブルネイ友好協会公式サイト RSS Feed", "日本ブルネイ友好協会 — 公益社団法人 日本ブルネイ友好協会公式サイト RSS Feed",
     "news", "media",
     "http://www.jbfa.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan-Brunei Friendship Association — 公益社団法人 日本ブルネイ友好協会公式サイト RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan-Brunei Friendship Association — 公益社団法人 日本ブルネイ友好協会公式サイト RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jbi_or_jp, "jp-b50-jbi-or-jp",
     "Japan Bearing Inspection Institute", "一般財団法人日本軸受検査協会",
     "news", "media",
     "https://jbi.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Bearing Inspection Institute\",\"media\",\"batch:50\"]",
     3600,
     "Japan Bearing Inspection Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_jbpi_or_jp, "jp-b50-jbpi-or-jp",
     "Japan Bicycle Promotion Institute", "一般財団法人自転車産業振興協会",
     "news", "media",
     "https://jbpi.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Bicycle Promotion Institute\",\"media\",\"batch:50\"]",
     3600,
     "Japan Bicycle Promotion Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jca_apc_org, "jp-b50-jca-apc-org",
     "Japan Computer Access Network", "JCA-NET",
     "news", "media",
     "https://jca.apc.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Computer Access Network\",\"media\",\"batch:50\"]",
     3600,
     "Japan Computer Access Network — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jca_home_jp, "jp-b50-jca-home-jp",
     "Japan Consumers' Association — 一般財団法人 日本消費者協会 | JCA RSS Feed", "日本消費者協会 — 一般財団法人 日本消費者協会 | JCA RSS Feed",
     "news", "media",
     "https://jca-home.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Consumers' Association — 一般財団法人 日本消費者協会 | JCA RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Consumers' Association — 一般財団法人 日本消費者協会 | JCA RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jcca_net_or_jp, "jp-b50-jcca-net-or-jp",
     "Japan Compensation Consultant Association — JCC 社団法人 日本補償コンサルタント協会 RSS Feed", "日本補償コンサルタント協会 — JCC 社団法人 日本補償コンサルタント協会 RSS Feed",
     "news", "media",
     "https://www.jcca-net.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Compensation Consultant Association — JCC 社団法人 日本補償コンサルタント協会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Compensation Consultant Association — JCC 社団法人 日本補償コンサルタント協会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jclu_org, "jp-b50-jclu-org",
     "Japan Civil Liberties Union", "自由人権協会",
     "news", "media",
     "https://jclu.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Civil Liberties Union\",\"media\",\"batch:50\"]",
     3600,
     "Japan Civil Liberties Union — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jcma2_jp_rss1_0, "jp-b50-jcma2-jp-rss1-0",
     "The Japanese Electric Wire & Cable Makers' Association", "日本電線工業会",
     "news", "media",
     "https://www.jcma2.jp/feed/rss1.0.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japanese Electric Wire & Cable Makers' Association\",\"media\",\"batch:50\"]",
     3600,
     "The Japanese Electric Wire & Cable Makers' Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_jcma2_jp_rss2_0, "jp-b50-jcma2-jp-rss2-0",
     "The Japanese Electric Wire & Cable Makers' Association", "日本電線工業会",
     "news", "media",
     "https://www.jcma2.jp/feed/rss2.0.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japanese Electric Wire & Cable Makers' Association\",\"media\",\"batch:50\"]",
     3600,
     "The Japanese Electric Wire & Cable Makers' Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_jdfma_jp, "jp-b50-jdfma-jp",
     "Japan Defense Foundation for Mutual Aid", "防衛弘済会",
     "news", "media",
     "https://jdfma.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Defense Foundation for Mutual Aid\",\"media\",\"batch:50\"]",
     3600,
     "Japan Defense Foundation for Mutual Aid — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_jdg_or_jp, "jp-b50-jdg-or-jp",
     "Japanisch-Deutsche Gesellschaft Tokyo", "日独協会 (東京)",
     "news", "media",
     "https://www.jdg.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanisch-Deutsche Gesellschaft Tokyo\",\"media\",\"batch:50\"]",
     3600,
     "Japanisch-Deutsche Gesellschaft Tokyo — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_jdg_osaka_com, "jp-b50-jdg-osaka-com",
     "Japanisch-Deutsche Gesellschaft Osaka", "大阪日独協会",
     "news", "media",
     "https://jdg-osaka.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanisch-Deutsche Gesellschaft Osaka\",\"media\",\"batch:50\"]",
     3600,
     "Japanisch-Deutsche Gesellschaft Osaka — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jeas_org, "jp-b50-jeas-org",
     "Japan Association of Environment Assessment — JEAS RSS Feed", "一般社団法人日本環境アセスメント協会 — JEAS RSS Feed",
     "news", "media",
     "https://jeas.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Association of Environment Assessment — JEAS RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Association of Environment Assessment — JEAS RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jeef_or_jp_rsslatest, "jp-b50-jeef-or-jp-rsslatest",
     "Japan Environmental Education Forum", "公益社団法人日本環境教育フォーラム",
     "news", "media",
     "https://www.jeef.or.jp/rsslatest.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Environmental Education Forum\",\"media\",\"batch:50\"]",
     3600,
     "Japan Environmental Education Forum — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_jei_or_jp, "jp-b50-jei-or-jp",
     "Japanese Esperanto Institute — 一般財団法人日本エスペラント協会 RSS Feed", "日本エスペラント協会 — 一般財団法人日本エスペラント協会 RSS Feed",
     "news", "media",
     "https://www.jei.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Esperanto Institute — 一般財団法人日本エスペラント協会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Esperanto Institute — 一般財団法人日本エスペラント協会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jemca_jp, "jp-b50-jemca-jp",
     "Japan Earth Moving Constructors Association", "日本機械土工協会",
     "news", "media",
     "https://www.jemca.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Earth Moving Constructors Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Earth Moving Constructors Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jemca_or_jp, "jp-b50-jemca-or-jp",
     "JAPAN ENVIRONMENTAL MEASUREMENT and CHEMICAL ANALYSIS ASSOCIATION — 一般社団法人日本環境測定分析協会 RSS Feed", "日本環境測定分析協会 — 一般社団法人日本環境測定分析協会 RSS Feed",
     "news", "media",
     "https://www.jemca.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:JAPAN ENVIRONMENTAL MEASUREMENT and CHEMICAL ANALYSIS ASSOCIATION — 一般社団法人日本環境測定分析協会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "JAPAN ENVIRONMENTAL MEASUREMENT and CHEMICAL ANALYSIS ASSOCIATION — 一般社団法人日本環境測定分析協会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jf_nagisa_or_jp, "jp-b50-jf-nagisa-or-jp",
     "なぎさ信用漁業協同組合連合会｜なぎさ信漁連", "なぎさ信用漁業協同組合連合会｜なぎさ信漁連",
     "news", "media",
     "https://www.jf-nagisa.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:なぎさ信用漁業協同組合連合会｜なぎさ信漁連\",\"media\",\"batch:50\"]",
     3600,
     "なぎさ信用漁業協同組合連合会｜なぎさ信漁連 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jfa_kagu_jp, "jp-b50-jfa-kagu-jp",
     "Japan Furniture Industry Development Association", "日本家具産業振興会",
     "news", "media",
     "https://jfa-kagu.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Furniture Industry Development Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Furniture Industry Development Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 7 entries at probe")

RSSX(jp_b50_jfa1953_org, "jp-b50-jfa1953-org",
     "Japan Flying Association", "一般社団法人日本飛行連盟",
     "news", "media",
     "https://jfa1953.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Flying Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Flying Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jfda_or_jp, "jp-b50-jfda-or-jp",
     "Japan Flying Disc Association", "日本フライングディスク協会",
     "news", "media",
     "https://www.jfda.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Flying Disc Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Flying Disc Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jflc_or_jp_xml_rss2, "jp-b50-jflc-or-jp-xml-rss2",
     "Japan federation of landscape contractors", "一般社団法人日本造園組合連合会",
     "news", "media",
     "https://www.jflc.or.jp/xml-rss2.php",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan federation of landscape contractors\",\"media\",\"batch:50\"]",
     3600,
     "Japan federation of landscape contractors — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jfnet_or_jp, "jp-b50-jfnet-or-jp",
     "Japan Foodservice Association", "日本フードサービス協会",
     "news", "media",
     "https://www.jfnet.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Foodservice Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Foodservice Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_jgha_com, "jp-b50-jgha-com",
     "Japan Greenhouse Horticulture Association — 一般社団法人日本施設園芸協会 RSS Feed", "一般社団法人日本施設園芸協会 — 一般社団法人日本施設園芸協会 RSS Feed",
     "news", "media",
     "https://jgha.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Greenhouse Horticulture Association — 一般社団法人日本施設園芸協会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Greenhouse Horticulture Association — 一般社団法人日本施設園芸協会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jgia_gr_jp, "jp-b50-jgia-gr-jp",
     "Japan Gas Meter Industry Association.", "日本ガスメーター工業会",
     "news", "media",
     "https://www.jgia.gr.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Gas Meter Industry Association.\",\"media\",\"batch:50\"]",
     3600,
     "Japan Gas Meter Industry Association. — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_jgra_or_jp, "jp-b50-jgra-or-jp",
     "Japan Golf Range Association", "全日本ゴルフ練習場連盟",
     "news", "media",
     "https://www.jgra.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Golf Range Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Golf Range Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jida_or_jp_ccm_calendar_1, "jp-b50-jida-or-jp-ccm-calendar-1",
     "Japan Industrial Designers' Association", "公益社団法人日本インダストリアルデザイナー協会",
     "news", "media",
     "https://www.jida.or.jp/ccm/calendar/feed/1",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Industrial Designers' Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Industrial Designers' Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_jiffa_or_jp_rss2, "jp-b50-jiffa-or-jp-rss2",
     "Japan International Freight Forwarders Association", "国際フレイトフォワーダーズ協会",
     "news", "media",
     "http://www.jiffa.or.jp/rss2.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan International Freight Forwarders Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan International Freight Forwarders Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jiima_or_jp, "jp-b50-jiima-or-jp",
     "Japan Image and Information Management Association", "日本文書情報マネジメント協会",
     "news", "media",
     "https://www.jiima.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Image and Information Management Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Image and Information Management Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jim_net_org, "jp-b50-jim-net-org",
     "JIM-NET", "ＪＩＭ－ＮＥＴ",
     "news", "media",
     "https://www.jim-net.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:JIM-NET\",\"media\",\"batch:50\"]",
     3600,
     "JIM-NET — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jipa_or_jp, "jp-b50-jipa-or-jp",
     "Japan Intellectual Property Association", "日本知的財産協会",
     "news", "media",
     "https://www.jipa.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Intellectual Property Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Intellectual Property Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_jipea_or_jp_blog, "jp-b50-jipea-or-jp-blog",
     "Japan Interlocking Block Pavement Engineering Association — ブログ", "インターロッキングブロック舗装技術協会 — ブログ",
     "news", "media",
     "https://www.jipea.or.jp/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Interlocking Block Pavement Engineering Association — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "Japan Interlocking Block Pavement Engineering Association — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 18 entries at probe")

RSSX(jp_b50_jistec_or_jp, "jp-b50-jistec-or-jp",
     "Japan International Science and Technology Exchange Center", "社団法人科学技術国際交流センター",
     "news", "media",
     "https://www.jistec.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan International Science and Technology Exchange Center\",\"media\",\"batch:50\"]",
     3600,
     "Japan International Science and Technology Exchange Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jlta_jp_test, "jp-b50-jlta-jp-test",
     "Japan Livestock Technology Association", "公益社団法人畜産技術協会",
     "news", "media",
     "https://jlta.jp/test/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Livestock Technology Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Livestock Technology Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jma_sangaku_or_jp, "jp-b50-jma-sangaku-or-jp",
     "Japan Mountaineering and Sport Climbing Association", "日本山岳・スポーツクライミング協会",
     "news", "media",
     "https://www.jma-sangaku.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Mountaineering and Sport Climbing Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Mountaineering and Sport Climbing Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_jma_wawasan_com, "jp-b50-jma-wawasan-com",
     "Japan-Malaysia Association", "公益社団法人日本マレーシア協会",
     "news", "media",
     "https://jma-wawasan.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan-Malaysia Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan-Malaysia Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jmas_ngo_jp, "jp-b50-jmas-ngo-jp",
     "Japan Mine Action Service", "日本地雷処理を支援する会",
     "news", "media",
     "https://jmas-ngo.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Mine Action Service\",\"media\",\"batch:50\"]",
     3600,
     "Japan Mine Action Service — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jmfa_or_jp, "jp-b50-jmfa-or-jp",
     "Japan Myanmar Friendship Association", "日本ミャンマー友好協会",
     "news", "media",
     "https://jmfa.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Myanmar Friendship Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Myanmar Friendship Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jmmaa_org, "jp-b50-jmmaa-org",
     "一般社団法人日本マーダーミステリー作家協会", "日本マーダーミステリー作家協会",
     "news", "media",
     "https://jmmaa.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:一般社団法人日本マーダーミステリー作家協会\",\"media\",\"batch:50\"]",
     3600,
     "一般社団法人日本マーダーミステリー作家協会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jmsa1964_com, "jp-b50-jmsa1964-com",
     "Japan Metal Stamping Association", "日本金属プレス工業協会",
     "news", "media",
     "https://jmsa1964.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Metal Stamping Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Metal Stamping Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jmwa_or_jp_blog_feed, "jp-b50-jmwa-or-jp-blog-feed",
     "Japan Medical Women's Association — 日本女医会 - RSS", "日本女医会 — 日本女医会 - RSS",
     "news", "media",
     "https://www.jmwa.or.jp/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Medical Women's Association — 日本女医会 - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Japan Medical Women's Association — 日本女医会 - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_jnpoc_ne_jp, "jp-b50-jnpoc-ne-jp",
     "Japan NPO Center", "特定非営利活動法人 日本NPOセンター",
     "news", "media",
     "https://www.jnpoc.ne.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan NPO Center\",\"media\",\"batch:50\"]",
     3600,
     "Japan NPO Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_jocs_jp, "jp-b50-jocs-jp",
     "Japan Oil Chemists' Society", "公益社団法人日本油化学会",
     "news", "media",
     "https://jocs.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Oil Chemists' Society\",\"media\",\"batch:50\"]",
     3600,
     "Japan Oil Chemists' Society — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jopa_or_jp_feed_rss2, "jp-b50-jopa-or-jp-feed-rss2",
     "Japan Oceangoing Passenger Ship Association", "日本外航客船協会",
     "news", "media",
     "https://jopa.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Oceangoing Passenger Ship Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Oceangoing Passenger Ship Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_joycom_or_jp, "jp-b50-joycom-or-jp",
     "Joy Com", "ジョイ・コム",
     "news", "media",
     "https://joycom.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Joy Com\",\"media\",\"batch:50\"]",
     3600,
     "Joy Com — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 4 entries at probe")

RSSX(jp_b50_jp_pearl_com, "jp-b50-jp-pearl-com",
     "Japan Pearl Promotion Society", "日本真珠振興会",
     "news", "media",
     "https://jp-pearl.com/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Pearl Promotion Society\",\"media\",\"batch:50\"]",
     3600,
     "Japan Pearl Promotion Society — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jpbpa_net, "jp-b50-jpbpa-net",
     "Japan Professional Baseball Players Association", "日本プロ野球選手会",
     "news", "media",
     "https://jpbpa.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Professional Baseball Players Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Professional Baseball Players Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jpca_jp, "jp-b50-jpca-jp",
     "Japan Electronics Packaging and Circuits Association", "一般社団法人日本電子回路工業会",
     "news", "media",
     "https://jpca.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Electronics Packaging and Circuits Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Electronics Packaging and Circuits Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jpfa_or_jp, "jp-b50-jpfa-or-jp",
     "Japan Park Facilities Association", "一般社団法人日本公園施設業協会",
     "news", "media",
     "https://www.jpfa.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Park Facilities Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Park Facilities Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jpg_tokyo_com, "jp-b50-jpg-tokyo-com",
     "JAPAN PRODUCTION GUILD", "日本プロダクション協会",
     "news", "media",
     "https://jpg-tokyo.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:JAPAN PRODUCTION GUILD\",\"media\",\"batch:50\"]",
     3600,
     "JAPAN PRODUCTION GUILD — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jpgardenart_com, "jp-b50-jpgardenart-com",
     "Nihon teien geijutsu kyōkai", "日本庭園芸術協会",
     "news", "media",
     "https://jpgardenart.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nihon teien geijutsu kyōkai\",\"media\",\"batch:50\"]",
     3600,
     "Nihon teien geijutsu kyōkai — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jpna_jp, "jp-b50-jpna-jp",
     "Japanese Psychiatric Nurses Association", "日本精神科看護協会",
     "news", "media",
     "https://jpna.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Psychiatric Nurses Association\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Psychiatric Nurses Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jppa_biz, "jp-b50-jppa-biz",
     "Japan Pork Producers Association", "一般社団法人日本養豚協会",
     "news", "media",
     "https://jppa.biz/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Pork Producers Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Pork Producers Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jprs_co_jp, "jp-b50-jprs-co-jp",
     "株式会社日本レジストリサービス（JPRS）", "株式会社日本レジストリサービス（JPRS）",
     "news", "media",
     "https://jprs.co.jp/rss.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社日本レジストリサービス（JPRS）\",\"media\",\"batch:50\"]",
     3600,
     "株式会社日本レジストリサービス（JPRS） — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_jps_gr_jp, "jp-b50-jps-gr-jp",
     "Japan Professional Photographers Society", "日本写真家協会",
     "news", "media",
     "https://www.jps.gr.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Professional Photographers Society\",\"media\",\"batch:50\"]",
     3600,
     "Japan Professional Photographers Society — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jpta_or_jp, "jp-b50-jpta-or-jp",
     "Japan Professional Tennis Association", "日本プロテニス協会",
     "news", "media",
     "https://jpta.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Professional Tennis Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Professional Tennis Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jri_or_jp, "jp-b50-jri-or-jp",
     "Japan Research Institute", "日本総合研究所",
     "news", "media",
     "https://www.jri.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Research Institute\",\"media\",\"batch:50\"]",
     3600,
     "Japan Research Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jrps_or_jp, "jp-b50-jrps-or-jp",
     "The Japan Reinforced Plastics Society", "一般社団法人強化プラスチック協会",
     "news", "media",
     "https://jrps.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japan Reinforced Plastics Society\",\"media\",\"batch:50\"]",
     3600,
     "The Japan Reinforced Plastics Society — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jsee_or_jp, "jp-b50-jsee-or-jp",
     "Japanese Society for Engineering Education", "公益社団法人日本工学教育協会",
     "news", "media",
     "https://www.jsee.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Society for Engineering Education\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Society for Engineering Education — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jsia_or_jp, "jp-b50-jsia-or-jp",
     "Japan Switchboard & control system Industries Association", "日本配電制御システム工業会",
     "news", "media",
     "https://jsia.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Switchboard & control system Industries Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Switchboard & control system Industries Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_jspf_org_en, "jp-b50-jspf-org-en",
     "Japan Science Promotion Inc. Foundation", "一般財団法人日本科学振興財団",
     "news", "media",
     "https://jspf.org/en/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Science Promotion Inc. Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Japan Science Promotion Inc. Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jspm_ne_jp_rss1_0, "jp-b50-jspm-ne-jp-rss1-0",
     "Japanese Society for Palliative Medicine", "日本緩和医療学会",
     "news", "media",
     "https://www.jspm.ne.jp/feed/rss1.0.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Society for Palliative Medicine\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Society for Palliative Medicine — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_jspm_ne_jp_rss2_0, "jp-b50-jspm-ne-jp-rss2-0",
     "Japanese Society for Palliative Medicine", "日本緩和医療学会",
     "news", "media",
     "https://www.jspm.ne.jp/feed/rss2.0.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Society for Palliative Medicine\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Society for Palliative Medicine — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_jssi_or_jp, "jp-b50-jssi-or-jp",
     "The Japan Society of Seismic Isolation", "一般社団法人日本免震構造協会",
     "news", "media",
     "https://www.jssi.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Japan Society of Seismic Isolation\",\"media\",\"batch:50\"]",
     3600,
     "The Japan Society of Seismic Isolation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jste_or_jp, "jp-b50-jste-or-jp",
     "Japan Society of Traffic Engineers", "交通工学研究会",
     "news", "media",
     "https://www.jste.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Society of Traffic Engineers\",\"media\",\"batch:50\"]",
     3600,
     "Japan Society of Traffic Engineers — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jtua_or_jp_rss_ict_column, "jp-b50-jtua-or-jp-rss-ict-column",
     "Japan Telecom Users Association — JTUA ICTコラム", "日本ICTテレコムユーザー協会 — JTUA ICTコラム",
     "news", "media",
     "https://www.jtua.or.jp/rss_ict_column/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Telecom Users Association — JTUA ICTコラム\",\"media\",\"batch:50\"]",
     3600,
     "Japan Telecom Users Association — JTUA ICTコラム — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jtua_or_jp_rss_ict_solution, "jp-b50-jtua-or-jp-rss-ict-solution",
     "Japan Telecom Users Association — JTUA ICTソリューション紹介", "日本ICTテレコムユーザー協会 — JTUA ICTソリューション紹介",
     "news", "media",
     "https://www.jtua.or.jp/rss_ict_solution/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Telecom Users Association — JTUA ICTソリューション紹介\",\"media\",\"batch:50\"]",
     3600,
     "Japan Telecom Users Association — JTUA ICTソリューション紹介 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jtua_or_jp_rss_ict_topic, "jp-b50-jtua-or-jp-rss-ict-topic",
     "Japan Telecom Users Association — JTUA 企業ICT導入事例", "日本ICTテレコムユーザー協会 — JTUA 企業ICT導入事例",
     "news", "media",
     "https://www.jtua.or.jp/rss_ict_topic/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Telecom Users Association — JTUA 企業ICT導入事例\",\"media\",\"batch:50\"]",
     3600,
     "Japan Telecom Users Association — JTUA 企業ICT導入事例 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jtua_or_jp_rss_jtua_news, "jp-b50-jtua-or-jp-rss-jtua-news",
     "Japan Telecom Users Association — JTUA お知らせ", "日本ICTテレコムユーザー協会 — JTUA お知らせ",
     "news", "media",
     "https://www.jtua.or.jp/rss_jtua_news/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Telecom Users Association — JTUA お知らせ\",\"media\",\"batch:50\"]",
     3600,
     "Japan Telecom Users Association — JTUA お知らせ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jtua_or_jp_rss_tel_column, "jp-b50-jtua-or-jp-rss-tel-column",
     "Japan Telecom Users Association — JTUA 電話応対でCS向上コラム", "日本ICTテレコムユーザー協会 — JTUA 電話応対でCS向上コラム",
     "news", "media",
     "https://www.jtua.or.jp/rss_tel_column/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Telecom Users Association — JTUA 電話応対でCS向上コラム\",\"media\",\"batch:50\"]",
     3600,
     "Japan Telecom Users Association — JTUA 電話応対でCS向上コラム — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jtua_or_jp_rss_tel_topic, "jp-b50-jtua-or-jp-rss-tel-topic",
     "Japan Telecom Users Association — JTUA 電話応対でCS向上事例", "日本ICTテレコムユーザー協会 — JTUA 電話応対でCS向上事例",
     "news", "media",
     "https://www.jtua.or.jp/rss_tel_topic/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Telecom Users Association — JTUA 電話応対でCS向上事例\",\"media\",\"batch:50\"]",
     3600,
     "Japan Telecom Users Association — JTUA 電話応対でCS向上事例 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jtua_or_jp_rssevent, "jp-b50-jtua-or-jp-rssevent",
     "Japan Telecom Users Association — JTUA イベントセミナー", "日本ICTテレコムユーザー協会 — JTUA イベントセミナー",
     "news", "media",
     "https://www.jtua.or.jp/rssevent/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Telecom Users Association — JTUA イベントセミナー\",\"media\",\"batch:50\"]",
     3600,
     "Japan Telecom Users Association — JTUA イベントセミナー — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_jutem_co_jp, "jp-b50-jutem-co-jp",
     "JUTEM", "超高温材料研究所",
     "news", "media",
     "https://www.jutem.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:JUTEM\",\"media\",\"batch:50\"]",
     3600,
     "JUTEM — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_jvn_jp_jvnjp, "jp-b50-jvn-jp-jvnjp",
     "Japan Vulnerability Notes — JVN - JP 情報", "Japan Vulnerability Notes — JVN - JP 情報",
     "news", "media",
     "https://jvn.jp/rss/jvnJP.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Vulnerability Notes — JVN - JP 情報\",\"media\",\"batch:50\"]",
     3600,
     "Japan Vulnerability Notes — JVN - JP 情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 181 entries at probe")

RSSX(jp_b50_jvn_jp_jvnta, "jp-b50-jvn-jp-jvnta",
     "Japan Vulnerability Notes — JVN - TA 情報", "Japan Vulnerability Notes — JVN - TA 情報",
     "news", "media",
     "https://jvn.jp/rss/jvnTA.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Vulnerability Notes — JVN - TA 情報\",\"media\",\"batch:50\"]",
     3600,
     "Japan Vulnerability Notes — JVN - TA 情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_jvn_jp_jvnvu, "jp-b50-jvn-jp-jvnvu",
     "Japan Vulnerability Notes — JVN - VU 情報", "Japan Vulnerability Notes — JVN - VU 情報",
     "news", "media",
     "https://jvn.jp/rss/jvnVU.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Vulnerability Notes — JVN - VU 情報\",\"media\",\"batch:50\"]",
     3600,
     "Japan Vulnerability Notes — JVN - VU 情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 389 entries at probe")

RSSX(jp_b50_jvndb_jvn_jp_en_jvndb_new, "jp-b50-jvndb-jvn-jp-en-jvndb-new",
     "JVN iPedia — JVN iPedia New", "JVN iPedia — JVN iPedia New",
     "news", "media",
     "https://jvndb.jvn.jp/en/rss/jvndb_new.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:JVN iPedia — JVN iPedia New\",\"media\",\"batch:50\"]",
     3600,
     "JVN iPedia — JVN iPedia New — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 500 entries at probe")

RSSX(jp_b50_jvpa_jp_jvpa_feed_rss2_cat_14, "jp-b50-jvpa-jp-jvpa-feed-rss2-cat-14",
     "Japan Veterinary Products Association — 公益社団法人日本動物用医薬品協会 latest posts", "公益社団法人日本動物用医薬品協会 — 公益社団法人日本動物用医薬品協会 latest posts",
     "news", "media",
     "https://jvpa.jp/jvpa/?feed=rss2&cat=14",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Veterinary Products Association — 公益社団法人日本動物用医薬品協会 latest posts\",\"media\",\"batch:50\"]",
     3600,
     "Japan Veterinary Products Association — 公益社団法人日本動物用医薬品協会 latest posts — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_jwaa_or_jp, "jp-b50-jwaa-or-jp",
     "Japan Women's Association of Aviation", "日本女性航空協会",
     "news", "media",
     "https://www.jwaa.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Women's Association of Aviation\",\"media\",\"batch:50\"]",
     3600,
     "Japan Women's Association of Aviation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_jwia_or_jp, "jp-b50-jwia-or-jp",
     "Japan Women Inventors Association", "婦人発明家協会",
     "news", "media",
     "https://jwia.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Women Inventors Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Women Inventors Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_jwif_org, "jp-b50-jwif-org",
     "KE'KEN Textile Testing & Certification Center", "一般財団法人ケケン試験認証センター",
     "news", "media",
     "https://www.jwif.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:KE'KEN Textile Testing & Certification Center\",\"media\",\"batch:50\"]",
     3600,
     "KE'KEN Textile Testing & Certification Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_kahanrin_kusanosk_co_jp, "jp-b50-kahanrin-kusanosk-co-jp",
     "Kusano River Side Woods Bank Trust Foundation — 公益財団法人草野河畔林トラスト財団のフィード", "草野河畔林トラスト財団 — 公益財団法人草野河畔林トラスト財団のフィード",
     "news", "media",
     "https://kahanrin.kusanosk.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kusano River Side Woods Bank Trust Foundation — 公益財団法人草野河畔林トラスト財団のフィード\",\"media\",\"batch:50\"]",
     3600,
     "Kusano River Side Woods Bank Trust Foundation — 公益財団法人草野河畔林トラスト財団のフィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kaigai_bunka_org_wp1, "jp-b50-kaigai-bunka-org-wp1",
     "International Culture Appreciation and Interchange Society", "海外と文化を交流する会",
     "news", "media",
     "https://kaigai-bunka.org/wp1/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:International Culture Appreciation and Interchange Society\",\"media\",\"batch:50\"]",
     3600,
     "International Culture Appreciation and Interchange Society — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_kak_shien_c_jp, "jp-b50-kak-shien-c-jp",
     "Nagano Traffic Safety Education Support Center", "長野県交通安全教育支援センター",
     "news", "media",
     "https://www.kak-shien-c.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nagano Traffic Safety Education Support Center\",\"media\",\"batch:50\"]",
     3600,
     "Nagano Traffic Safety Education Support Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kakogawa_kia_or_jp, "jp-b50-kakogawa-kia-or-jp",
     "Kakogawa International Association", "加古川市国際交流協会",
     "news", "media",
     "https://kakogawa-kia.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kakogawa International Association\",\"media\",\"batch:50\"]",
     3600,
     "Kakogawa International Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kamoltd_co_jp, "jp-b50-kamoltd-co-jp",
     "Kamoso Kachoen", "加茂荘花鳥園",
     "news", "media",
     "https://kamoltd.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kamoso Kachoen\",\"media\",\"batch:50\"]",
     3600,
     "Kamoso Kachoen — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kanbi_org, "jp-b50-kanbi-org",
     "Kansai Art Academy", "関西美術院",
     "news", "media",
     "https://kanbi.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kansai Art Academy\",\"media\",\"batch:50\"]",
     3600,
     "Kansai Art Academy — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kanda_web_co_jp, "jp-b50-kanda-web-co-jp",
     "カンダホールディングス株式会社", "カンダホールディングス株式会社",
     "news", "media",
     "https://www.kanda-web.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:カンダホールディングス株式会社\",\"media\",\"batch:50\"]",
     3600,
     "カンダホールディングス株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kansaikodomo_com, "jp-b50-kansaikodomo-com",
     "Kansai Children's Culture Association", "関西こども文化協会",
     "news", "media",
     "https://kansaikodomo.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kansai Children's Culture Association\",\"media\",\"batch:50\"]",
     3600,
     "Kansai Children's Culture Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kasumigasekikai_jp, "jp-b50-kasumigasekikai-jp",
     "Kasumigaseki Foreign Service Association", "霞関会",
     "news", "media",
     "https://www.kasumigasekikai.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kasumigaseki Foreign Service Association\",\"media\",\"batch:50\"]",
     3600,
     "Kasumigaseki Foreign Service Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kasumigasekikai_or_jp, "jp-b50-kasumigasekikai-or-jp",
     "Kasumigaseki Foreign Service Association", "霞関会",
     "news", "media",
     "https://www.kasumigasekikai.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kasumigaseki Foreign Service Association\",\"media\",\"batch:50\"]",
     3600,
     "Kasumigaseki Foreign Service Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kasumigaura_com, "jp-b50-kasumigaura-com",
     "Kasumigaura Citizens' Association", "霞ヶ浦市民協会",
     "news", "media",
     "http://www.kasumigaura.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kasumigaura Citizens' Association\",\"media\",\"batch:50\"]",
     3600,
     "Kasumigaura Citizens' Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kasyukyo_or_jp, "jp-b50-kasyukyo-or-jp",
     "Japan Fruit Tree Seedling & Clonal Association", "一般社団法人日本果樹種苗協会",
     "news", "media",
     "https://www.kasyukyo.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Fruit Tree Seedling & Clonal Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Fruit Tree Seedling & Clonal Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_katazaidan_or_jp, "jp-b50-katazaidan-or-jp",
     "The Die and Mould Technology Promotion Foundation", "公益財団法人金型技術振興財団",
     "news", "media",
     "https://katazaidan.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Die and Mould Technology Promotion Foundation\",\"media\",\"batch:50\"]",
     3600,
     "The Die and Mould Technology Promotion Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kawabe_co_jp, "jp-b50-kawabe-co-jp",
     "INTERMODE KAWABE│川辺株式会社 -ハンカチ・スカーフ・マフラー・バッグ・フレグランス・香水", "INTERMODE KAWABE│川辺株式会社 -ハンカチ・スカーフ・マフラー・バッグ・フレグランス・香水",
     "news", "media",
     "https://www.kawabe.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:INTERMODE KAWABE│川辺株式会社 -ハンカチ・スカーフ・マフラー・バッグ・フレグランス・香水\",\"media\",\"batch:50\"]",
     3600,
     "INTERMODE KAWABE│川辺株式会社 -ハンカチ・スカーフ・マフラー・バッグ・フレグランス・香水 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kawaguchi_med_or_jp, "jp-b50-kawaguchi-med-or-jp",
     "Kawaguchi Medical Association", "一般社団法人川口医師会",
     "news", "media",
     "https://kawaguchi-med.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kawaguchi Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Kawaguchi Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kawasaki_corp_co_jp, "jp-b50-kawasaki-corp-co-jp",
     "株式会社カワサキ", "株式会社カワサキ",
     "news", "media",
     "https://kawasaki-corp.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社カワサキ\",\"media\",\"batch:50\"]",
     3600,
     "株式会社カワサキ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kben_jp, "jp-b50-kben-jp",
     "鹿児島県弁護士会", "鹿児島県弁護士会",
     "news", "media",
     "https://www.kben.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:鹿児島県弁護士会\",\"media\",\"batch:50\"]",
     3600,
     "鹿児島県弁護士会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kenkoigaku_or_jp, "jp-b50-kenkoigaku-or-jp",
     "Kenkoigaku Associaton", "一般財団法人健康医学協会",
     "news", "media",
     "https://kenkoigaku.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kenkoigaku Associaton\",\"media\",\"batch:50\"]",
     3600,
     "Kenkoigaku Associaton — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ketsuatsu_net, "jp-b50-ketsuatsu-net",
     "Japanese Society of Hypertension", "日本高血圧協会",
     "news", "media",
     "https://www.ketsuatsu.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Society of Hypertension\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Society of Hypertension — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_kfo_or_jp, "jp-b50-kfo-or-jp",
     "Kobe Fashion Organization", "神戸ファッション協会",
     "news", "media",
     "https://kfo.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kobe Fashion Organization\",\"media\",\"batch:50\"]",
     3600,
     "Kobe Fashion Organization — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kgu_greenken_or_jp_rss2, "jp-b50-kgu-greenken-or-jp-rss2",
     "Kansai Golf Union - Green Section Research Center", "一般財団法人関西グリーン研究所",
     "news", "media",
     "https://www.kgu-greenken.or.jp/rss2.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kansai Golf Union - Green Section Research Center\",\"media\",\"batch:50\"]",
     3600,
     "Kansai Golf Union - Green Section Research Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_khf_or_jp, "jp-b50-khf-or-jp",
     "Kanagawa Health Foundation", "公益財団法人かながわ健康財団",
     "news", "media",
     "https://www.khf.or.jp/index.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kanagawa Health Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Kanagawa Health Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_kian_or_jp_kianfeed, "jp-b50-kian-or-jp-kianfeed",
     "Kawasaki International Association", "公益財団法人川崎市国際交流協会",
     "news", "media",
     "https://www.kian.or.jp/rss/kianfeed.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kawasaki International Association\",\"media\",\"batch:50\"]",
     3600,
     "Kawasaki International Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 464 entries at probe")

RSSX(jp_b50_kihoren_com, "jp-b50-kihoren-com",
     "Japan Christian Federation on Early Childhood Care and Education", "キリスト教保育連盟",
     "news", "media",
     "https://kihoren.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Christian Federation on Early Childhood Care and Education\",\"media\",\"batch:50\"]",
     3600,
     "Japan Christian Federation on Early Childhood Care and Education — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kikaireefs_org, "jp-b50-kikaireefs-org",
     "KIKAI institute for Coral Reef Sciences", "喜界島サンゴ礁科学研究所",
     "news", "media",
     "https://kikaireefs.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:KIKAI institute for Coral Reef Sciences\",\"media\",\"batch:50\"]",
     3600,
     "KIKAI institute for Coral Reef Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kinari_kids_com, "jp-b50-kinari-kids-com",
     "Kinari Kids", "きなり",
     "news", "media",
     "https://kinari-kids.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kinari Kids\",\"media\",\"batch:50\"]",
     3600,
     "Kinari Kids — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kishiwada_med_jp_feed_atom, "jp-b50-kishiwada-med-jp-feed-atom",
     "Kishiwada Medical Association — Atom 0.3", "一般社団法人岸和田市医師会 — Atom 0.3",
     "news", "media",
     "http://www.kishiwada-med.jp/?feed=atom",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kishiwada Medical Association — Atom 0.3\",\"media\",\"batch:50\"]",
     3600,
     "Kishiwada Medical Association — Atom 0.3 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kishiwada_med_jp_feed_rss2, "jp-b50-kishiwada-med-jp-feed-rss2",
     "Kishiwada Medical Association", "一般社団法人岸和田市医師会",
     "news", "media",
     "http://www.kishiwada-med.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kishiwada Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Kishiwada Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kitaku_med_or_jp, "jp-b50-kitaku-med-or-jp",
     "Tokyo Kitaku Medical Association", "東京都北区医師会",
     "news", "media",
     "http://www.kitaku-med.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Kitaku Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Kitaku Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kitalive_co_jp, "jp-b50-kitalive-co-jp",
     "株式会社キットアライブ — キットアライブ", "株式会社キットアライブ — キットアライブ",
     "news", "media",
     "https://www.kitalive.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社キットアライブ — キットアライブ\",\"media\",\"batch:50\"]",
     3600,
     "株式会社キットアライブ — キットアライブ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_kitamoto_usagi_com_blog_feed, "jp-b50-kitamoto-usagi-com-blog-feed",
     "Kitamoto After-School Care Association Usagikko Club — 埼玉県北本市 学童保育 うさぎっ子クラブ - RSS", "北本学童保育の会うさぎっ子クラブ — 埼玉県北本市 学童保育 うさぎっ子クラブ - RSS",
     "news", "media",
     "https://www.kitamoto-usagi.com/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kitamoto After-School Care Association Usagikko Club — 埼玉県北本市 学童保育 うさぎっ子クラブ - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Kitamoto After-School Care Association Usagikko Club — 埼玉県北本市 学童保育 うさぎっ子クラブ - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_kitashiba_org, "jp-b50-kitashiba-org",
     "Community Building Network Kitashiba", "暮らしづくりネットワーク北芝",
     "news", "media",
     "https://www.kitashiba.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Community Building Network Kitashiba\",\"media\",\"batch:50\"]",
     3600,
     "Community Building Network Kitashiba — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kmes_kanagawa_or_jp, "jp-b50-kmes-kanagawa-or-jp",
     "Kanagawa Water-Energy Service", "かながわ水・エネルギーサービス",
     "news", "media",
     "https://www.kmes-kanagawa.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kanagawa Water-Energy Service\",\"media\",\"batch:50\"]",
     3600,
     "Kanagawa Water-Energy Service — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kmt_ti_or_jp, "jp-b50-kmt-ti-or-jp",
     "Kumamoto Industry Support Foundation — 公益財団法人くまもと産業支援財団 RSS Feed", "公益財団法人くまもと産業支援財団 — 公益財団法人くまもと産業支援財団 RSS Feed",
     "news", "media",
     "https://www.kmt-ti.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kumamoto Industry Support Foundation — 公益財団法人くまもと産業支援財団 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Kumamoto Industry Support Foundation — 公益財団法人くまもと産業支援財団 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_knsk_osaka_jp_info_article13_category_event, "jp-b50-knsk-osaka-jp-info-article13-category-event",
     "Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — イベント情報", "大阪府立環境農林水産総合研究所 — イベント情報",
     "news", "media",
     "https://www.knsk-osaka.jp/info/article13/category/event/index.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — イベント情報\",\"media\",\"batch:50\"]",
     3600,
     "Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — イベント情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_knsk_osaka_jp_info_article13_category_press, "jp-b50-knsk-osaka-jp-info-article13-category-press",
     "Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — 報道発表", "大阪府立環境農林水産総合研究所 — 報道発表",
     "news", "media",
     "https://www.knsk-osaka.jp/info/article13/category/press/index.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — 報道発表\",\"media\",\"batch:50\"]",
     3600,
     "Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — 報道発表 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_knsk_osaka_jp_nourin_news, "jp-b50-knsk-osaka-jp-nourin-news",
     "Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — 新着情報", "大阪府立環境農林水産総合研究所 — 新着情報",
     "news", "media",
     "https://www.knsk-osaka.jp/nourin/news/index.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — 新着情報\",\"media\",\"batch:50\"]",
     3600,
     "Research Institute of Environment, Agriculture and Fisheries, Osaka Prefecture — 新着情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kobe_dmo_jp, "jp-b50-kobe-dmo-jp",
     "KOBE Tourism Bureau", "神戸観光局",
     "news", "media",
     "https://kobe-dmo.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:KOBE Tourism Bureau\",\"media\",\"batch:50\"]",
     3600,
     "KOBE Tourism Bureau — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_kobe_fta_or_jp, "jp-b50-kobe-fta-or-jp",
     "Kobe Foreign Trade Association", "神戸貿易協会",
     "news", "media",
     "https://kobe-fta.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kobe Foreign Trade Association\",\"media\",\"batch:50\"]",
     3600,
     "Kobe Foreign Trade Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kobeherb_com, "jp-b50-kobeherb-com",
     "Nunobiki Herb Garden", "布引ハーブ園",
     "news", "media",
     "https://www.kobeherb.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nunobiki Herb Garden\",\"media\",\"batch:50\"]",
     3600,
     "Nunobiki Herb Garden — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kobenichifutsu_com, "jp-b50-kobenichifutsu-com",
     "Société Franco-Japonaise de Kobe", "神戸日仏協会",
     "news", "media",
     "https://kobenichifutsu.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Société Franco-Japonaise de Kobe\",\"media\",\"batch:50\"]",
     3600,
     "Société Franco-Japonaise de Kobe — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kodomo_npo_or_jp, "jp-b50-kodomo-npo-or-jp",
     "Kodomo NPO", "こどもＮＰＯ",
     "news", "media",
     "https://kodomo-npo.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kodomo NPO\",\"media\",\"batch:50\"]",
     3600,
     "Kodomo NPO — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kogyokai_com, "jp-b50-kogyokai-com",
     "Japan Mechanical Design Industries Association", "日本機械設計工業会",
     "news", "media",
     "https://kogyokai.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Mechanical Design Industries Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Mechanical Design Industries Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_koukogakuzaidan_or_jp, "jp-b50-koukogakuzaidan-or-jp",
     "Kanagawa Archeology Foundation", "かながわ考古学財団",
     "news", "media",
     "https://koukogakuzaidan.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kanagawa Archeology Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Kanagawa Archeology Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kousotsu_jp, "jp-b50-kousotsu-jp",
     "Kousotsu Support Association", "認定NPO法人 高卒支援会",
     "news", "media",
     "https://kousotsu.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kousotsu Support Association\",\"media\",\"batch:50\"]",
     3600,
     "Kousotsu Support Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_krk_osaka_or_jp, "jp-b50-krk-osaka-or-jp",
     "Kansai Occupational Health Association", "一般財団法人関西労働保健協会",
     "news", "media",
     "https://www.krk-osaka.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kansai Occupational Health Association\",\"media\",\"batch:50\"]",
     3600,
     "Kansai Occupational Health Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kskk_or_jp, "jp-b50-kskk-or-jp",
     "International Youth Association of Japan", "国際青少年研修協会",
     "news", "media",
     "https://kskk.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:International Youth Association of Japan\",\"media\",\"batch:50\"]",
     3600,
     "International Youth Association of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 9 entries at probe")

RSSX(jp_b50_ksyc_jp, "jp-b50-ksyc-jp",
     "KOBE Student Youth Center", "公益財団法人神戸学生青年センター",
     "news", "media",
     "https://ksyc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:KOBE Student Youth Center\",\"media\",\"batch:50\"]",
     3600,
     "KOBE Student Youth Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kuragaikepark_com, "jp-b50-kuragaikepark-com",
     "Kuragaike Park", "鞍ケ池公園",
     "news", "media",
     "https://kuragaikepark.com/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kuragaike Park\",\"media\",\"batch:50\"]",
     3600,
     "Kuragaike Park — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kuramae_ne_jp, "jp-b50-kuramae-ne-jp",
     "Tokyo Tech Alumni Association", "蔵前工業会",
     "news", "media",
     "https://www.kuramae.ne.jp/RSS.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Tech Alumni Association\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Tech Alumni Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_kuramae_ne_jp_files_blog_rss_blog_all, "jp-b50-kuramae-ne-jp-files-blog-rss-blog-all",
     "Tokyo Tech Alumni Association", "蔵前工業会",
     "news", "media",
     "https://www.kuramae.ne.jp/files/blog/rss/RSS_BLOG_ALL.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Tech Alumni Association\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Tech Alumni Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_kuramoto_co_jp, "jp-b50-kuramoto-co-jp",
     "半歩先行く技術と商品力で未来を創る株式会社倉元製作所", "半歩先行く技術と商品力で未来を創る株式会社倉元製作所",
     "news", "media",
     "https://www.kuramoto.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:半歩先行く技術と商品力で未来を創る株式会社倉元製作所\",\"media\",\"batch:50\"]",
     3600,
     "半歩先行く技術と商品力で未来を創る株式会社倉元製作所 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kuroshio_or_jp, "jp-b50-kuroshio-or-jp",
     "Kuroshio Biology Research Institute", "公益財団法人黒潮生物研究所",
     "news", "media",
     "https://kuroshio.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kuroshio Biology Research Institute\",\"media\",\"batch:50\"]",
     3600,
     "Kuroshio Biology Research Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kusamura_org, "jp-b50-kusamura-org",
     "Tama Kusamura Association", "多摩草むらの会",
     "news", "media",
     "https://kusamura.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tama Kusamura Association\",\"media\",\"batch:50\"]",
     3600,
     "Tama Kusamura Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kvsc_kyoto_jp, "jp-b50-kvsc-kyoto-jp",
     "Kyoto Victim Support Center — 京都犯罪被害者支援センター RSS Feed", "公益社団法人京都犯罪被害者支援センター — 京都犯罪被害者支援センター RSS Feed",
     "news", "media",
     "https://kvsc.kyoto.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kyoto Victim Support Center — 京都犯罪被害者支援センター RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Kyoto Victim Support Center — 京都犯罪被害者支援センター RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_kyotoymca_or_jp_feed_rss2, "jp-b50-kyotoymca-or-jp-feed-rss2",
     "Kyoto YMCA", "公益財団法人京都YMCA",
     "news", "media",
     "https://kyotoymca.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kyoto YMCA\",\"media\",\"batch:50\"]",
     3600,
     "Kyoto YMCA — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_lac_co_jp_news, "jp-b50-lac-co-jp-news",
     "株式会社ラック — 株式会社ラック ニュースリリース", "株式会社ラック — 株式会社ラック ニュースリリース",
     "news", "media",
     "https://www.lac.co.jp/news/feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社ラック — 株式会社ラック ニュースリリース\",\"media\",\"batch:50\"]",
     3600,
     "株式会社ラック — 株式会社ラック ニュースリリース — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_lamick_co_jp, "jp-b50-lamick-co-jp",
     "大成ラミック株式会社", "大成ラミック株式会社",
     "news", "media",
     "https://www.lamick.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:大成ラミック株式会社\",\"media\",\"batch:50\"]",
     3600,
     "大成ラミック株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_lasalle_org, "jp-b50-lasalle-org",
     "Institute of the Brothers of the Christian Schools", "ラ・サール会",
     "news", "media",
     "https://www.lasalle.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Institute of the Brothers of the Christian Schools\",\"media\",\"batch:50\"]",
     3600,
     "Institute of the Brothers of the Christian Schools — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 16 entries at probe")

RSSX(jp_b50_legika_jp, "jp-b50-legika-jp",
     "LEGIKA", "ＬＥＧＩＫＡ",
     "news", "media",
     "https://www.legika.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:LEGIKA\",\"media\",\"batch:50\"]",
     3600,
     "LEGIKA — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_liberas_net_index_cmd_rss, "jp-b50-liberas-net-index-cmd-rss",
     "LIBERAS", "ＬＩＢＥＲＡＳ",
     "news", "media",
     "https://liberas.net/index.php?cmd=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:LIBERAS\",\"media\",\"batch:50\"]",
     3600,
     "LIBERAS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_libra_or_jp, "jp-b50-libra-or-jp",
     "THE Tokai Foundation For Gender Studies — 公益財団法人 東海ジェンダー研究所 RSS Feed", "東海ジェンダー研究所 — 公益財団法人 東海ジェンダー研究所 RSS Feed",
     "news", "media",
     "https://libra.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:THE Tokai Foundation For Gender Studies — 公益財団法人 東海ジェンダー研究所 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "THE Tokai Foundation For Gender Studies — 公益財団法人 東海ジェンダー研究所 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_lifelink_or_jp, "jp-b50-lifelink-or-jp",
     "Suicide Prevention Support Center LifeLink", "自殺対策支援センター ライフリンク",
     "news", "media",
     "https://lifelink.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Suicide Prevention Support Center LifeLink\",\"media\",\"batch:50\"]",
     3600,
     "Suicide Prevention Support Center LifeLink — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_lnews_jp, "jp-b50-lnews-jp",
     "物流ニュースのＬＮＥＷＳ ─ 物流・ロジスティクス・ＳＣＭに関する情報を提供", "物流ニュースのＬＮＥＷＳ ─ 物流・ロジスティクス・ＳＣＭに関する情報を提供",
     "news", "media",
     "https://www.lnews.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:物流ニュースのＬＮＥＷＳ ─ 物流・ロジスティクス・ＳＣＭに関する情報を提供\",\"media\",\"batch:50\"]",
     3600,
     "物流ニュースのＬＮＥＷＳ ─ 物流・ロジスティクス・ＳＣＭに関する情報を提供 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_lpc_or_jp, "jp-b50-lpc-or-jp",
     "The Life Planning Center Foundation", "一般財団法人ライフ・プランニング・センター",
     "news", "media",
     "https://lpc.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Life Planning Center Foundation\",\"media\",\"batch:50\"]",
     3600,
     "The Life Planning Center Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_lsf_or_jp, "jp-b50-lsf-or-jp",
     "Life Sports Promotion Foundation", "ライフスポーツ財団",
     "news", "media",
     "https://www.lsf.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Life Sports Promotion Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Life Sports Promotion Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_lutra_jp, "jp-b50-lutra-jp",
     "Shikoku Institute of Natural History", "四国自然史科学研究センター",
     "news", "media",
     "https://lutra.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Shikoku Institute of Natural History\",\"media\",\"batch:50\"]",
     3600,
     "Shikoku Institute of Natural History — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_m_isc_jp, "jp-b50-m-isc-jp",
     "Matsumoto Industrial Innovation Center", "まつもと産業イノベーションセンター",
     "news", "media",
     "https://www.m-isc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Matsumoto Industrial Innovation Center\",\"media\",\"batch:50\"]",
     3600,
     "Matsumoto Industrial Innovation Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_made_livesense_co_jp, "jp-b50-made-livesense-co-jp",
     "LIVESENSE ENGINEER BLOG", "LIVESENSE ENGINEER BLOG",
     "news", "media",
     "https://made.livesense.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:LIVESENSE ENGINEER BLOG\",\"media\",\"batch:50\"]",
     3600,
     "LIVESENSE ENGINEER BLOG — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_magnetics_jp, "jp-b50-magnetics-jp",
     "The Magnetics Society of Japan", "公益社団法人日本磁気学会",
     "news", "media",
     "https://www.magnetics.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Magnetics Society of Japan\",\"media\",\"batch:50\"]",
     3600,
     "The Magnetics Society of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_maia_or_jp, "jp-b50-maia-or-jp",
     "Marine Accidents Inquiry Association", "海難審判・船舶事故調査協会",
     "news", "media",
     "https://www.maia.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Marine Accidents Inquiry Association\",\"media\",\"batch:50\"]",
     3600,
     "Marine Accidents Inquiry Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_matsuo_acad_or_jp, "jp-b50-matsuo-acad-or-jp",
     "Matsuo Foundation", "公益財団法人松尾学術振興財団",
     "news", "media",
     "https://matsuo-acad.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Matsuo Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Matsuo Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_mcae_jp, "jp-b50-mcae-jp",
     "Muroran Constructors Association", "室蘭建設業協会",
     "news", "media",
     "https://www.mcae.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Muroran Constructors Association\",\"media\",\"batch:50\"]",
     3600,
     "Muroran Constructors Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_me_rise_com, "jp-b50-me-rise-com",
     "Mi Raise 2", "み・らいず２",
     "news", "media",
     "https://me-rise.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Mi Raise 2\",\"media\",\"batch:50\"]",
     3600,
     "Mi Raise 2 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_megri_or_jp_blog_feed, "jp-b50-megri-or-jp-blog-feed",
     "Medical Governance Research Institute — 医療ガバナンス研究所 - RSS", "医療ガバナンス研究所 — 医療ガバナンス研究所 - RSS",
     "news", "media",
     "https://www.megri.or.jp/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Medical Governance Research Institute — 医療ガバナンス研究所 - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Medical Governance Research Institute — 医療ガバナンス研究所 - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_mf21_or_jp, "jp-b50-mf21-or-jp",
     "MARINO-FORUM 21", "一般社団法人マリノフォーラム21",
     "news", "media",
     "https://www.mf21.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:MARINO-FORUM 21\",\"media\",\"batch:50\"]",
     3600,
     "MARINO-FORUM 21 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_mie_kenchikushikai_or_jp, "jp-b50-mie-kenchikushikai-or-jp",
     "Mie Association of Architects and Building Engineers", "三重県建築士会",
     "news", "media",
     "https://mie-kenchikushikai.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Mie Association of Architects and Building Engineers\",\"media\",\"batch:50\"]",
     3600,
     "Mie Association of Architects and Building Engineers — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_mifa_jp, "jp-b50-mifa-jp",
     "Meguro International Friendship Association — 目黒区国際交流協会(MIFA) Meguro International Friendship Association", "目黒区国際交流協会 — 目黒区国際交流協会(MIFA) Meguro International Friendship Association",
     "news", "media",
     "https://mifa.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Meguro International Friendship Association — 目黒区国際交流協会(MIFA) Meguro International Friendship Association\",\"media\",\"batch:50\"]",
     3600,
     "Meguro International Friendship Association — 目黒区国際交流協会(MIFA) Meguro International Friendship Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_minka_or_jp, "jp-b50-minka-or-jp",
     "Japan Minka Revival Association", "日本民家再生協会",
     "news", "media",
     "https://minka.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Minka Revival Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Minka Revival Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_minsei_or_jp, "jp-b50-minsei-or-jp",
     "Public Welfare Institute of Scientific Research Foundation", "一般財団法人民生科学協会",
     "news", "media",
     "https://www.minsei.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Public Welfare Institute of Scientific Research Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Public Welfare Institute of Scientific Research Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_mint_care_com, "jp-b50-mint-care-com",
     "Mint Care", "みんとけあ",
     "news", "media",
     "https://www.mint-care.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Mint Care\",\"media\",\"batch:50\"]",
     3600,
     "Mint Care — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_mitsubishi_ufj_foundation_jp, "jp-b50-mitsubishi-ufj-foundation-jp",
     "Mitsubishi UFJ Foundation", "三菱UFJ国際財団",
     "news", "media",
     "https://www.mitsubishi-ufj-foundation.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Mitsubishi UFJ Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Mitsubishi UFJ Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_miyagi_kodomo_net, "jp-b50-miyagi-kodomo-net",
     "MIYAGI Children's Network", "ＭＩＹＡＧＩ子どもネットワーク",
     "news", "media",
     "https://www.miyagi-kodomo.net/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:MIYAGI Children's Network\",\"media\",\"batch:50\"]",
     3600,
     "MIYAGI Children's Network — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_miyasoku_or_jp_feed_rss2, "jp-b50-miyasoku-or-jp-feed-rss2",
     "Miyagi Association of Planners and Surveyors", "宮城県測量設計業協会",
     "news", "media",
     "https://miyasoku.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Miyagi Association of Planners and Surveyors\",\"media\",\"batch:50\"]",
     3600,
     "Miyagi Association of Planners and Surveyors — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_mmij_or_jp, "jp-b50-mmij-or-jp",
     "The Mining and Materials Processing Institute of Japan", "資源・素材学会",
     "news", "media",
     "https://www.mmij.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Mining and Materials Processing Institute of Japan\",\"media\",\"batch:50\"]",
     3600,
     "The Mining and Materials Processing Institute of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_moanakids_org, "jp-b50-moanakids-org",
     "Moana Kids Nature School", "もあなキッズ自然楽校",
     "news", "media",
     "https://moanakids.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Moana Kids Nature School\",\"media\",\"batch:50\"]",
     3600,
     "Moana Kids Nature School — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_moneyforward_dev_jp, "jp-b50-moneyforward-dev-jp",
     "Money Forward Developers Blog", "Money Forward Developers Blog",
     "news", "media",
     "https://moneyforward-dev.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Money Forward Developers Blog\",\"media\",\"batch:50\"]",
     3600,
     "Money Forward Developers Blog — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_morinoyouchien_org, "jp-b50-morinoyouchien-org",
     "NPO法人森のようちえん全国ネットワーク連盟", "森のようちえん全国ネットワーク連盟",
     "news", "media",
     "https://morinoyouchien.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:NPO法人森のようちえん全国ネットワーク連盟\",\"media\",\"batch:50\"]",
     3600,
     "NPO法人森のようちえん全国ネットワーク連盟 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_mrc_or_jp_rss_moduleid_634_portalid_0, "jp-b50-mrc-or-jp-rss-moduleid-634-portalid-0",
     "The MRC Foundation — 移動無線センターお知らせ", "移動無線センター — 移動無線センターお知らせ",
     "news", "media",
     "https://www.mrc.or.jp/rss.aspx?moduleid=634&portalid=0",
     "ja", "[\"news\",\"country:jp\",\"publisher:The MRC Foundation — 移動無線センターお知らせ\",\"media\",\"batch:50\"]",
     3600,
     "The MRC Foundation — 移動無線センターお知らせ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_mssf_or_jp, "jp-b50-mssf-or-jp",
     "The Mechanical Social Systems Foundation", "一般財団法人機械システム振興協会",
     "news", "media",
     "https://www.mssf.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Mechanical Social Systems Foundation\",\"media\",\"batch:50\"]",
     3600,
     "The Mechanical Social Systems Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_muro_med_jp, "jp-b50-muro-med-jp",
     "Muroran Medical Association", "公益社団法人室蘭市医師会",
     "news", "media",
     "https://muro-med.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Muroran Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Muroran Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_murotech_or_jp, "jp-b50-murotech-or-jp",
     "Muroran Advancement Center of Industrial Technology and Management", "室蘭テクノセンター",
     "news", "media",
     "https://www.murotech.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Muroran Advancement Center of Industrial Technology and Management\",\"media\",\"batch:50\"]",
     3600,
     "Muroran Advancement Center of Industrial Technology and Management — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_muryoso_okoshi_yasu_com, "jp-b50-muryoso-okoshi-yasu-com",
     "Fukushukai (Welfare Gathering Association)", "福聚会",
     "news", "media",
     "http://muryoso.okoshi-yasu.com/rss.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Fukushukai (Welfare Gathering Association)\",\"media\",\"batch:50\"]",
     3600,
     "Fukushukai (Welfare Gathering Association) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_muslim_or_jp, "jp-b50-muslim-or-jp",
     "Japan Muslim Association", "日本ムスリム協会",
     "news", "media",
     "https://www.muslim.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Muslim Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Muslim Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_my_way_jp, "jp-b50-my-way-jp",
     "My Way", "マイＷａｙ",
     "news", "media",
     "https://my-way.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:My Way\",\"media\",\"batch:50\"]",
     3600,
     "My Way — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 4 entries at probe")

RSSX(jp_b50_nagano_rt_org, "jp-b50-nagano-rt-org",
     "Nagano Association of Radiological Technologists — （一社）長野県診療放射線技師会 RSS Feed", "長野県診療放射線技師会 — （一社）長野県診療放射線技師会 RSS Feed",
     "news", "media",
     "https://nagano-rt.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nagano Association of Radiological Technologists — （一社）長野県診療放射線技師会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Nagano Association of Radiological Technologists — （一社）長野県診療放射線技師会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nagasaki_jk_net, "jp-b50-nagasaki-jk-net",
     "Nagasaki Association of Architectural Firms", "長崎県建築士事務所協会",
     "news", "media",
     "https://www.nagasaki-jk.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nagasaki Association of Architectural Firms\",\"media\",\"batch:50\"]",
     3600,
     "Nagasaki Association of Architectural Firms — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_naha_catholic_jp, "jp-b50-naha-catholic-jp",
     "Roman Catholic Diocese of Naha", "カトリック那覇教区",
     "news", "media",
     "https://www.naha.catholic.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Roman Catholic Diocese of Naha\",\"media\",\"batch:50\"]",
     3600,
     "Roman Catholic Diocese of Naha — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nakano_sato_org_news, "jp-b50-nakano-sato-org-news",
     "Nakano Sato Weaving Association — ニュースリリース RSS 2.0", "なかの里を紡ぐ会 — ニュースリリース RSS 2.0",
     "news", "media",
     "https://www.nakano-sato.org/news/index.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nakano Sato Weaving Association — ニュースリリース RSS 2.0\",\"media\",\"batch:50\"]",
     3600,
     "Nakano Sato Weaving Association — ニュースリリース RSS 2.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nakayamashoten_jp_wordpress_zaidan, "jp-b50-nakayamashoten-jp-wordpress-zaidan",
     "Nakayama Foundation for Human Science", "公益財団法人 中山人間科学振興財団",
     "news", "media",
     "https://nakayamashoten.jp/wordpress/zaidan/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nakayama Foundation for Human Science\",\"media\",\"batch:50\"]",
     3600,
     "Nakayama Foundation for Human Science — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nalc_jp, "jp-b50-nalc-jp",
     "Nippon Active Life Club", "ニッポン・アクティブライフ・クラブ",
     "news", "media",
     "https://nalc.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nippon Active Life Club\",\"media\",\"batch:50\"]",
     3600,
     "Nippon Active Life Club — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_namt_jp, "jp-b50-namt-jp",
     "Nagano Association of Medical Technologists — 長野県臨床検査技師会 RSS Feed", "一般社団法人長野県臨床検査技師会 — 長野県臨床検査技師会 RSS Feed",
     "news", "media",
     "https://www.namt.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nagano Association of Medical Technologists — 長野県臨床検査技師会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Nagano Association of Medical Technologists — 長野県臨床検査技師会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nanbu_okinawa_med_or_jp, "jp-b50-nanbu-okinawa-med-or-jp",
     "Okinawa Nanbu Medical Association", "一般社団法人南部地区医師会",
     "news", "media",
     "https://www.nanbu.okinawa.med.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Okinawa Nanbu Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Okinawa Nanbu Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_nanbyou_jp, "jp-b50-nanbyou-jp",
     "Japan Intractable Diseases Research Foundation", "公益財団法人難病医学研究財団",
     "news", "media",
     "https://www.nanbyou.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Intractable Diseases Research Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Japan Intractable Diseases Research Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nanyo_co_jp, "jp-b50-nanyo-co-jp",
     "株式会社 南陽", "株式会社 南陽",
     "news", "media",
     "https://www.nanyo.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社 南陽\",\"media\",\"batch:50\"]",
     3600,
     "株式会社 南陽 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nasudenki_co_jp, "jp-b50-nasudenki-co-jp",
     "那須電機鉄工株式会社", "那須電機鉄工株式会社",
     "news", "media",
     "https://www.nasudenki.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:那須電機鉄工株式会社\",\"media\",\"batch:50\"]",
     3600,
     "那須電機鉄工株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nestra_space_feed_rss2, "jp-b50-nestra-space-feed-rss2",
     "Next generation Space system Technology Research Association", "次世代宇宙システム技術研究組合",
     "news", "media",
     "https://nestra.space/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Next generation Space system Technology Research Association\",\"media\",\"batch:50\"]",
     3600,
     "Next generation Space system Technology Research Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_news_yahoo_co_jp_topics_entertainment, "jp-b50-news-yahoo-co-jp-topics-entertainment",
     "Yahoo Japan entertainment", "Yahoo!ニュース entertainment",
     "news", "media",
     "https://news.yahoo.co.jp/rss/topics/entertainment.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yahoo Japan entertainment\",\"media\",\"batch:50\"]",
     3600,
     "Yahoo Japan entertainment — news feed (8 entries at probe)")

RSSX(jp_b50_news_yahoo_co_jp_topics_science, "jp-b50-news-yahoo-co-jp-topics-science",
     "Yahoo Japan science", "Yahoo!ニュース science",
     "news", "media",
     "https://news.yahoo.co.jp/rss/topics/science.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yahoo Japan science\",\"media\",\"batch:50\"]",
     3600,
     "Yahoo Japan science — news feed (8 entries at probe)")

RSSX(jp_b50_news_yahoo_co_jp_topics_sports, "jp-b50-news-yahoo-co-jp-topics-sports",
     "Yahoo Japan sports", "Yahoo!ニュース sports",
     "news", "media",
     "https://news.yahoo.co.jp/rss/topics/sports.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yahoo Japan sports\",\"media\",\"batch:50\"]",
     3600,
     "Yahoo Japan sports — news feed (8 entries at probe)")

RSSX(jp_b50_nico_or_jp, "jp-b50-nico-or-jp",
     "Niigata Industrial Creation Organization", "にいがた産業創造機構",
     "news", "media",
     "https://www.nico.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Niigata Industrial Creation Organization\",\"media\",\"batch:50\"]",
     3600,
     "Niigata Industrial Creation Organization — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_nihon_s_co_jp, "jp-b50-nihon-s-co-jp",
     "日本精密へようこそ！日本精密株式会社へのご訪問を心より歓迎申し上げます。", "日本精密へようこそ！日本精密株式会社へのご訪問を心より歓迎申し上げます。",
     "news", "media",
     "https://www.nihon-s.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:日本精密へようこそ！日本精密株式会社へのご訪問を心より歓迎申し上げます。\",\"media\",\"batch:50\"]",
     3600,
     "日本精密へようこそ！日本精密株式会社へのご訪問を心より歓迎申し上げます。 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nihondaikyo_or_jp, "jp-b50-nihondaikyo-or-jp",
     "Independent Insurance Agents Of Japan, Inc.", "一般社団法人日本損害保険代理業協会",
     "news", "media",
     "https://www.nihondaikyo.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Independent Insurance Agents Of Japan, Inc.\",\"media\",\"batch:50\"]",
     3600,
     "Independent Insurance Agents Of Japan, Inc. — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 4 entries at probe")

RSSX(jp_b50_nihonkeieishikai_com_topcms, "jp-b50-nihonkeieishikai-com-topcms",
     "Association of Management Consultants in Japan", "一般社団法人日本経営士会",
     "news", "media",
     "https://nihonkeieishikai.com/topcms/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Association of Management Consultants in Japan\",\"media\",\"batch:50\"]",
     3600,
     "Association of Management Consultants in Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_niijima_com, "jp-b50-niijima-com",
     "Niijima", "新島村",
     "news", "media",
     "https://www.niijima.com/rss/feed.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Niijima\",\"media\",\"batch:50\"]",
     3600,
     "Niijima — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_niki_kai_or_jp, "jp-b50-niki-kai-or-jp",
     "Niki-kai", "二紀会",
     "news", "media",
     "https://niki-kai.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Niki-kai\",\"media\",\"batch:50\"]",
     3600,
     "Niki-kai — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nishitama_med_or_jp, "jp-b50-nishitama-med-or-jp",
     "Nishitama Medical Association", "西多摩医師会",
     "news", "media",
     "https://nishitama-med.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nishitama Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Nishitama Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nishobi_org_blog_feed, "jp-b50-nishobi-org-blog-feed",
     "Japan Calligraphy Art Academy — nihonsyodou2018test - RSS", "日本書道美術院 — nihonsyodou2018test - RSS",
     "news", "media",
     "https://www.nishobi.org/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Calligraphy Art Academy — nihonsyodou2018test - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Japan Calligraphy Art Academy — nihonsyodou2018test - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nissan_zaidan_or_jp, "jp-b50-nissan-zaidan-or-jp",
     "Nissan Global Foundation", "公益財団法人日産財団",
     "news", "media",
     "https://www.nissan-zaidan.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nissan Global Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Nissan Global Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nissay_seirei_org_rss_moduleid_876_portalid_0, "jp-b50-nissay-seirei-org-rss-moduleid-876-portalid-0",
     "Nissay-Seirei Health and Welfare Foundation — 奈良ニッセイエデンの園", "ニッセイ聖隷健康福祉財団 — 奈良ニッセイエデンの園",
     "news", "media",
     "https://www.nissay-seirei.org/rss.aspx?moduleid=876&portalid=0",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nissay-Seirei Health and Welfare Foundation — 奈良ニッセイエデンの園\",\"media\",\"batch:50\"]",
     3600,
     "Nissay-Seirei Health and Welfare Foundation — 奈良ニッセイエデンの園 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 100 entries at probe")

RSSX(jp_b50_nissay_seirei_org_rss_moduleid_886_portalid_0, "jp-b50-nissay-seirei-org-rss-moduleid-886-portalid-0",
     "Nissay-Seirei Health and Welfare Foundation — 松戸ニッセイエデンの園", "ニッセイ聖隷健康福祉財団 — 松戸ニッセイエデンの園",
     "news", "media",
     "https://www.nissay-seirei.org/rss.aspx?moduleid=886&portalid=0",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nissay-Seirei Health and Welfare Foundation — 松戸ニッセイエデンの園\",\"media\",\"batch:50\"]",
     3600,
     "Nissay-Seirei Health and Welfare Foundation — 松戸ニッセイエデンの園 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_nittoso_or_jp, "jp-b50-nittoso-or-jp",
     "Japan Painting Contractors Association", "一般社団法人日本塗装工業会",
     "news", "media",
     "https://www.nittoso.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Painting Contractors Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Painting Contractors Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nkclc_info_blog, "jp-b50-nkclc-info-blog",
     "NPO Community Life Center — ブログ", "ＮＰＯ法人地域生活センター — ブログ",
     "news", "media",
     "https://www.nkclc.info/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:NPO Community Life Center — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "NPO Community Life Center — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 18 entries at probe")

RSSX(jp_b50_nmda_or_jp, "jp-b50-nmda-or-jp",
     "New Media Development Association", "ニューメディア開発協会",
     "news", "media",
     "https://www.nmda.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:New Media Development Association\",\"media\",\"batch:50\"]",
     3600,
     "New Media Development Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npo_cfa_com, "jp-b50-npo-cfa-com",
     "Career Facilitator Association", "キャリア・ファシリテーター協会",
     "news", "media",
     "https://npo-cfa.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Career Facilitator Association\",\"media\",\"batch:50\"]",
     3600,
     "Career Facilitator Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npo_entree_org_blog, "jp-b50-npo-entree-org-blog",
     "Entry — ブログ", "エンツリー — ブログ",
     "news", "media",
     "https://www.npo-entree.org/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:Entry — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "Entry — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_npo_fuji_com, "jp-b50-npo-fuji-com",
     "Group Fuji", "ぐるーぷ藤",
     "news", "media",
     "https://npo-fuji.com/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Group Fuji\",\"media\",\"batch:50\"]",
     3600,
     "Group Fuji — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_npo_hanauta_net, "jp-b50-npo-hanauta-net",
     "Hanauta", "はなうた",
     "news", "media",
     "https://npo-hanauta.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hanauta\",\"media\",\"batch:50\"]",
     3600,
     "Hanauta — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npo_hit_or_jp, "jp-b50-npo-hit-or-jp",
     "Mental Health Support Association Hit", "精神障害者支援の会ヒット",
     "news", "media",
     "https://npo-hit.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Mental Health Support Association Hit\",\"media\",\"batch:50\"]",
     3600,
     "Mental Health Support Association Hit — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_npo_icds_jp, "jp-b50-npo-icds-jp",
     "NPO ICDS", "ＮＰＯ法人ＩＣＤＳ",
     "news", "media",
     "https://npo.icds.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:NPO ICDS\",\"media\",\"batch:50\"]",
     3600,
     "NPO ICDS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 9 entries at probe")

RSSX(jp_b50_npo_pippi_net, "jp-b50-npo-pippi-net",
     "Pippi Parent-Child Support Net", "ピッピ・親子サポートネット",
     "news", "media",
     "https://npo-pippi.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Pippi Parent-Child Support Net\",\"media\",\"batch:50\"]",
     3600,
     "Pippi Parent-Child Support Net — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npo_sanzenkai_com_new2, "jp-b50-npo-sanzenkai-com-new2",
     "Sanzenkai Association", "燦然会",
     "news", "media",
     "https://npo-sanzenkai.com/NEW2/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Sanzenkai Association\",\"media\",\"batch:50\"]",
     3600,
     "Sanzenkai Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npo_ysc_jp, "jp-b50-npo-ysc-jp",
     "Youth Self-Reliance Support Center", "青少年自立援助センター",
     "news", "media",
     "https://npo-ysc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Youth Self-Reliance Support Center\",\"media\",\"batch:50\"]",
     3600,
     "Youth Self-Reliance Support Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npobunka_net, "jp-b50-npobunka-net",
     "Cultural Learning Cooperative Network", "文化学習協同ネットワーク",
     "news", "media",
     "https://npobunka.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Cultural Learning Cooperative Network\",\"media\",\"batch:50\"]",
     3600,
     "Cultural Learning Cooperative Network — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npocc_org, "jp-b50-npocc-org",
     "Challenged Community", "チャレンジド・コミュニティ",
     "news", "media",
     "https://npocc.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Challenged Community\",\"media\",\"batch:50\"]",
     3600,
     "Challenged Community — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npocommons_org, "jp-b50-npocommons-org",
     "Ibaraki NPO Center Commons", "茨城エヌ・ピー・オー・センター・コモンズ",
     "news", "media",
     "https://www.npocommons.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ibaraki NPO Center Commons\",\"media\",\"batch:50\"]",
     3600,
     "Ibaraki NPO Center Commons — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_npokokoro_com_blog_feed, "jp-b50-npokokoro-com-blog-feed",
     "Tokyo Holocaust Education Resource Center — main - RSS", "ホロコースト教育資料センター — main - RSS",
     "news", "media",
     "https://www.npokokoro.com/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Holocaust Education Resource Center — main - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Holocaust Education Resource Center — main - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_nponobel_jp, "jp-b50-nponobel-jp",
     "Certified NPO Nobel", "認定ＮＰＯ法人ノーベル",
     "news", "media",
     "https://nponobel.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Certified NPO Nobel\",\"media\",\"batch:50\"]",
     3600,
     "Certified NPO Nobel — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_nukada_jimdoweb_com_blog, "jp-b50-nukada-jimdoweb-com-blog",
     "The Nukada Institute for Medical and Biological Research — ブログ", "額田医学生物学研究所 — ブログ",
     "news", "media",
     "https://nukada.jimdoweb.com/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Nukada Institute for Medical and Biological Research — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "The Nukada Institute for Medical and Biological Research — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_nwsnet_or_jp, "jp-b50-nwsnet-or-jp",
     "National Women's Shelter Net", "全国女性シェルターネット",
     "news", "media",
     "https://nwsnet.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:National Women's Shelter Net\",\"media\",\"batch:50\"]",
     3600,
     "National Women's Shelter Net — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_oag_jp, "jp-b50-oag-jp",
     "German East Asiatic Society — OAG – Deutsche Gesellschaft für Natur- und Völkerkunde Ostas", "ドイツ東洋文化研究協会 — OAG – Deutsche Gesellschaft für Natur- und Völkerkunde Ostas",
     "news", "media",
     "https://oag.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:German East Asiatic Society — OAG – Deutsche Gesellschaft für Natur- und Völkerkunde Ostas\",\"media\",\"batch:50\"]",
     3600,
     "German East Asiatic Society — OAG – Deutsche Gesellschaft für Natur- und Völkerkunde Ostas — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 22 entries at probe")

RSSX(jp_b50_obda_or_jp, "jp-b50-obda-or-jp",
     "Osaka Business Development Agency", "公益財団法人 大阪産業局",
     "news", "media",
     "https://www.obda.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Osaka Business Development Agency\",\"media\",\"batch:50\"]",
     3600,
     "Osaka Business Development Agency — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_objapan_org, "jp-b50-objapan-org",
     "Operation Blessing Japan", "オペレーション・ブレッシング・ジャパン",
     "news", "media",
     "https://objapan.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Operation Blessing Japan\",\"media\",\"batch:50\"]",
     3600,
     "Operation Blessing Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_obma_or_jp, "jp-b50-obma-or-jp",
     "Okinawa Building Maintenance Association", "沖縄県ビルメンテナンス協会",
     "news", "media",
     "https://obma.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Okinawa Building Maintenance Association\",\"media\",\"batch:50\"]",
     3600,
     "Okinawa Building Maintenance Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_odha_or_jp, "jp-b50-odha-or-jp",
     "Osaka Dental Hygienists' Association", "公益社団法人大阪府歯科衛生士会",
     "news", "media",
     "https://www.odha.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Osaka Dental Hygienists' Association\",\"media\",\"batch:50\"]",
     3600,
     "Osaka Dental Hygienists' Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_oea_or_jp, "jp-b50-oea-or-jp",
     "Osaka Equipment Association", "一般社団法人大阪府設備設計事務所協会",
     "news", "media",
     "https://oea.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Osaka Equipment Association\",\"media\",\"batch:50\"]",
     3600,
     "Osaka Equipment Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ofix_or_jp, "jp-b50-ofix-or-jp",
     "Osaka Foundation of International Exchange — 公益財団法人 大阪府国際交流財団 Osaka Foundation of International Exchange", "大阪府国際交流財団 — 公益財団法人 大阪府国際交流財団 Osaka Foundation of International Exchange",
     "news", "media",
     "https://ofix.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Osaka Foundation of International Exchange — 公益財団法人 大阪府国際交流財団 Osaka Foundation of International Exchange\",\"media\",\"batch:50\"]",
     3600,
     "Osaka Foundation of International Exchange — 公益財団法人 大阪府国際交流財団 Osaka Foundation of International Exchange — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_oguraclutch_co_jp, "jp-b50-oguraclutch-co-jp",
     "小倉クラッチ株式会社", "小倉クラッチ株式会社",
     "news", "media",
     "https://www.oguraclutch.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:小倉クラッチ株式会社\",\"media\",\"batch:50\"]",
     3600,
     "小倉クラッチ株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ohmikai_com_index_cmd_rss, "jp-b50-ohmikai-com-index-cmd-rss",
     "Oujitsukai (Cherry Blossom Association)", "桜実会",
     "news", "media",
     "https://www.ohmikai.com/index.php?cmd=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Oujitsukai (Cherry Blossom Association)\",\"media\",\"batch:50\"]",
     3600,
     "Oujitsukai (Cherry Blossom Association) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_oiea_jp, "jp-b50-oiea-jp",
     "Okaya International Center", "公益財団法人おかや文化振興事業団国際交流センター",
     "news", "media",
     "https://oiea.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Okaya International Center\",\"media\",\"batch:50\"]",
     3600,
     "Okaya International Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_oiga_jp, "jp-b50-oiga-jp",
     "Otsu International Goodwill Association", "大津市国際親善協会",
     "news", "media",
     "https://oiga.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Otsu International Goodwill Association\",\"media\",\"batch:50\"]",
     3600,
     "Otsu International Goodwill Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_oisca_international_org, "jp-b50-oisca-international-org",
     "OISCA", "オイスカ",
     "news", "media",
     "https://oisca-international.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:OISCA\",\"media\",\"batch:50\"]",
     3600,
     "OISCA — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_okaben_or_jp, "jp-b50-okaben-or-jp",
     "岡山弁護士会 — 岡山弁護士会 RSS Feed", "岡山弁護士会 — 岡山弁護士会 RSS Feed",
     "news", "media",
     "https://www.okaben.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:岡山弁護士会 — 岡山弁護士会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "岡山弁護士会 — 岡山弁護士会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_okayama_jaces_org_blog_feed, "jp-b50-okayama-jaces-org-blog-feed",
     "Okayama Japan-America Cultural Exchange Society — OJACES - RSS", "岡山日米文化協会 — OJACES - RSS",
     "news", "media",
     "https://www.okayama-jaces.org/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Okayama Japan-America Cultural Exchange Society — OJACES - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Okayama Japan-America Cultural Exchange Society — OJACES - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_okazaki_lita_com, "jp-b50-okazaki-lita-com",
     "Okazaki Community Development Center Rita", "岡崎まち育てセンター・りた",
     "news", "media",
     "https://www.okazaki-lita.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Okazaki Community Development Center Rita\",\"media\",\"batch:50\"]",
     3600,
     "Okazaki Community Development Center Rita — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_oki_pt_or_jp, "jp-b50-oki-pt-or-jp",
     "Okinawa Physical Therapy Association", "沖縄県理学療法士協会",
     "news", "media",
     "https://oki-pt.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Okinawa Physical Therapy Association\",\"media\",\"batch:50\"]",
     3600,
     "Okinawa Physical Therapy Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_olime21_org_index_cmd_rss, "jp-b50-olime21-org-index-cmd-rss",
     "Olimu 21", "おりーむ21",
     "news", "media",
     "https://olime21.org/index.php?cmd=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Olimu 21\",\"media\",\"batch:50\"]",
     3600,
     "Olimu 21 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_omocoro_jp, "jp-b50-omocoro-jp",
     "オモコロ あたまゆるゆるインターネット — オモコロ あたまゆるゆるインターネット » トップページ のコメントのフィード", "オモコロ あたまゆるゆるインターネット — オモコロ あたまゆるゆるインターネット » トップページ のコメントのフィード",
     "news", "media",
     "https://omocoro.jp/index/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:オモコロ あたまゆるゆるインターネット — オモコロ あたまゆるゆるインターネット » トップページ のコメントのフィード\",\"media\",\"batch:50\"]",
     3600,
     "オモコロ あたまゆるゆるインターネット — オモコロ あたまゆるゆるインターネット » トップページ のコメントのフィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_omu_moe, "jp-b50-omu-moe",
     "Botanical Gardens, Osaka Metropolitan University — 公立大学法人大阪 臨時お知らせサイト", "大阪公立大学附属植物園 — 公立大学法人大阪 臨時お知らせサイト",
     "news", "media",
     "https://www.omu.moe/feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Botanical Gardens, Osaka Metropolitan University — 公立大学法人大阪 臨時お知らせサイト\",\"media\",\"batch:50\"]",
     3600,
     "Botanical Gardens, Osaka Metropolitan University — 公立大学法人大阪 臨時お知らせサイト — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 166 entries at probe")

RSSX(jp_b50_ora_or_jp, "jp-b50-ora-or-jp",
     "Osaka Restaurant Management Association", "大阪外食産業協会",
     "news", "media",
     "https://www.ora.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Osaka Restaurant Management Association\",\"media\",\"batch:50\"]",
     3600,
     "Osaka Restaurant Management Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_orangestar_hatenadiary_jp, "jp-b50-orangestar-hatenadiary-jp",
     "orangestarの雑記", "orangestarの雑記",
     "news", "media",
     "https://orangestar.hatenadiary.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:orangestarの雑記\",\"media\",\"batch:50\"]",
     3600,
     "orangestarの雑記 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_orea_or_jp, "jp-b50-orea-or-jp",
     "Nioi Kaori Kankyō Kyōkai", "におい・かおり環境協会",
     "news", "media",
     "https://orea.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nioi Kaori Kankyō Kyōkai\",\"media\",\"batch:50\"]",
     3600,
     "Nioi Kaori Kankyō Kyōkai — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ori_bu_net_blog_feed, "jp-b50-ori-bu-net-blog-feed",
     "Olive Garden — ori-bu.net - RSS", "オリーブの園 — ori-bu.net - RSS",
     "news", "media",
     "https://www.ori-bu.net/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Olive Garden — ori-bu.net - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Olive Garden — ori-bu.net - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_osaka_community_or_jp, "jp-b50-osaka-community-or-jp",
     "The Osaka Community Foundation — 公益財団法人 大阪コミュニティ財団", "公益財団法人大阪コミュニティ財団 — 公益財団法人 大阪コミュニティ財団",
     "news", "media",
     "https://osaka-community.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Osaka Community Foundation — 公益財団法人 大阪コミュニティ財団\",\"media\",\"batch:50\"]",
     3600,
     "The Osaka Community Foundation — 公益財団法人 大阪コミュニティ財団 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_our_think_or_jp, "jp-b50-our-think-or-jp",
     "Tokushima Industrial Promotion Organization", "とくしま産業振興機構",
     "news", "media",
     "https://www.our-think.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokushima Industrial Promotion Organization\",\"media\",\"batch:50\"]",
     3600,
     "Tokushima Industrial Promotion Organization — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ova_japan_org_feed_rss2, "jp-b50-ova-japan-org-feed-rss2",
     "OVA", "ＯＶＡ",
     "news", "media",
     "https://ova-japan.org/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:OVA\",\"media\",\"batch:50\"]",
     3600,
     "OVA — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ozawa_academy_com_en, "jp-b50-ozawa-academy-com-en",
     "Ozawa International Chamber Music Academy Okushiga", "小澤国際室内楽アカデミー奥志賀",
     "news", "media",
     "https://ozawa-academy.com/en/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Ozawa International Chamber Music Academy Okushiga\",\"media\",\"batch:50\"]",
     3600,
     "Ozawa International Chamber Music Academy Okushiga — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_oze_fnd_or_jp, "jp-b50-oze-fnd-or-jp",
     "Oze Preservation Foundation", "公益財団法人尾瀬保護財団",
     "news", "media",
     "https://oze-fnd.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Oze Preservation Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Oze Preservation Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_pancan1_org_index_format_feed_type_atom, "jp-b50-pancan1-org-index-format-feed-type-atom",
     "ビデオ講座：膵がん早期発見ネットワーク（尾道方式） — Atom 1.0", "パンキャンジャパン — Atom 1.0",
     "news", "media",
     "https://pancan1.org/index.php?format=feed&type=atom",
     "ja", "[\"news\",\"country:jp\",\"publisher:ビデオ講座：膵がん早期発見ネットワーク（尾道方式） — Atom 1.0\",\"media\",\"batch:50\"]",
     3600,
     "ビデオ講座：膵がん早期発見ネットワーク（尾道方式） — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_pancan1_org_index_format_feed_type_rss, "jp-b50-pancan1-org-index-format-feed-type-rss",
     "ビデオ講座：膵がん早期発見ネットワーク（尾道方式）", "パンキャンジャパン",
     "news", "media",
     "https://pancan1.org/index.php?format=feed&type=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:ビデオ講座：膵がん早期発見ネットワーク（尾道方式）\",\"media\",\"batch:50\"]",
     3600,
     "ビデオ講座：膵がん早期発見ネットワーク（尾道方式） — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_paraca_co_jp, "jp-b50-paraca-co-jp",
     "パラカ株式会社", "パラカ株式会社",
     "news", "media",
     "https://paraca.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:パラカ株式会社\",\"media\",\"batch:50\"]",
     3600,
     "パラカ株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_park_kagishippo_com_posts_default, "jp-b50-park-kagishippo-com-posts-default",
     "Parc — NPO法人パルク - Atom", "パルク — NPO法人パルク - Atom",
     "news", "media",
     "https://park.kagishippo.com/feeds/posts/default",
     "ja", "[\"news\",\"country:jp\",\"publisher:Parc — NPO法人パルク - Atom\",\"media\",\"batch:50\"]",
     3600,
     "Parc — NPO法人パルク - Atom — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 25 entries at probe")

RSSX(jp_b50_park_kagishippo_com_posts_default_alt_rss, "jp-b50-park-kagishippo-com-posts-default-alt-rss",
     "Parc — NPO法人パルク - RSS", "パルク — NPO法人パルク - RSS",
     "news", "media",
     "https://park.kagishippo.com/feeds/posts/default?alt=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Parc — NPO法人パルク - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Parc — NPO法人パルク - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 25 entries at probe")

RSSX(jp_b50_pbv_or_jp, "jp-b50-pbv-or-jp",
     "Peace Boat Disaster Relief", "ピースボート災害支援センター",
     "news", "media",
     "https://pbv.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Peace Boat Disaster Relief\",\"media\",\"batch:50\"]",
     3600,
     "Peace Boat Disaster Relief — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_pdd_gf_jp, "jp-b50-pdd-gf-jp",
     "PDD Support Center Green Forest", "ＰＤＤサポートセンターグリーンフォーレスト",
     "news", "media",
     "https://www.pdd-gf.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:PDD Support Center Green Forest\",\"media\",\"batch:50\"]",
     3600,
     "PDD Support Center Green Forest — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_peace_wing_n_or_jp, "jp-b50-peace-wing-n-or-jp",
     "Nagasaki Foundation for the Promotion of Peace", "長崎平和推進協会",
     "news", "media",
     "https://www.peace-wing-n.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nagasaki Foundation for the Promotion of Peace\",\"media\",\"batch:50\"]",
     3600,
     "Nagasaki Foundation for the Promotion of Peace — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_peerminato_com, "jp-b50-peerminato-com",
     "Disability Welfare Promotion Association Minato", "障害者の福祉を進める会みなと",
     "news", "media",
     "https://peerminato.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Disability Welfare Promotion Association Minato\",\"media\",\"batch:50\"]",
     3600,
     "Disability Welfare Promotion Association Minato — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_phd_kobe_org, "jp-b50-phd-kobe-org",
     "Peace, Health and Human Development", "PHD協会",
     "news", "media",
     "https://www.phd-kobe.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Peace, Health and Human Development\",\"media\",\"batch:50\"]",
     3600,
     "Peace, Health and Human Development — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_piif_jmtc_or_jp, "jp-b50-piif-jmtc-or-jp",
     "Japan Meat Trading Center — 公益財団法人日本食肉流通センター RSS Feed", "日本食肉流通センター — 公益財団法人日本食肉流通センター RSS Feed",
     "news", "media",
     "https://www.piif.jmtc.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Meat Trading Center — 公益財団法人日本食肉流通センター RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Meat Trading Center — 公益財団法人日本食肉流通センター RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_piyolog_hatenadiary_jp, "jp-b50-piyolog-hatenadiary-jp",
     "piyolog", "piyolog",
     "news", "media",
     "https://piyolog.hatenadiary.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:piyolog\",\"media\",\"batch:50\"]",
     3600,
     "piyolog — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_planetarium_jp, "jp-b50-planetarium-jp",
     "Japan Planetarium Association", "日本プラネタリウム協議会",
     "news", "media",
     "https://planetarium.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Planetarium Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Planetarium Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_po0po0_org, "jp-b50-po0po0-org",
     "Community Support Center Polepole — 地域支援センターポレポレ / RSS", "地域支援センターポレポレ — 地域支援センターポレポレ / RSS",
     "news", "media",
     "https://po0po0.org/feed.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Community Support Center Polepole — 地域支援センターポレポレ / RSS\",\"media\",\"batch:50\"]",
     3600,
     "Community Support Center Polepole — 地域支援センターポレポレ / RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_popalpha_co_jp, "jp-b50-popalpha-co-jp",
     "株式会社アルファ — 株式会社アルファ RSS Feed", "株式会社アルファ — 株式会社アルファ RSS Feed",
     "news", "media",
     "https://www.popalpha.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社アルファ — 株式会社アルファ RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "株式会社アルファ — 株式会社アルファ RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_pptc_co_jp, "jp-b50-pptc-co-jp",
     "【公式】株式会社property technologies — 【公式】株式会社property technologies | property technologies Inc. »", "【公式】株式会社property technologies — 【公式】株式会社property technologies | property technologies Inc. »",
     "news", "media",
     "https://pptc.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:【公式】株式会社property technologies — 【公式】株式会社property technologies | property technologies Inc. »\",\"media\",\"batch:50\"]",
     3600,
     "【公式】株式会社property technologies — 【公式】株式会社property technologies | property technologies Inc. » — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_prsj_or_jp, "jp-b50-prsj-or-jp",
     "Public Relations Society Of Japan", "日本パブリック・リレーションズ協会",
     "news", "media",
     "https://prsj.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Public Relations Society Of Japan\",\"media\",\"batch:50\"]",
     3600,
     "Public Relations Society Of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_pure_higashiosaka_com, "jp-b50-pure-higashiosaka-com",
     "Pure", "ピュア",
     "news", "media",
     "https://pure-higashiosaka.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Pure\",\"media\",\"batch:50\"]",
     3600,
     "Pure — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_qiita_com_popular_items, "jp-b50-qiita-com-popular-items",
     "Qiita Popular", "Qiita 人気記事",
     "news", "media",
     "https://qiita.com/popular-items/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Qiita Popular\",\"media\",\"batch:50\"]",
     3600,
     "Qiita Popular — news feed (30 entries at probe)")

RSSX(jp_b50_qqzaidan_jp, "jp-b50-qqzaidan-jp",
     "Japan Foundation For Emergency Medicine", "日本救急医療財団",
     "news", "media",
     "https://qqzaidan.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Foundation For Emergency Medicine\",\"media\",\"batch:50\"]",
     3600,
     "Japan Foundation For Emergency Medicine — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_rada_or_jp, "jp-b50-rada-or-jp",
     "Quantum Radiation Application Development Association", "般財団法人 量子放射線利用振興協会",
     "news", "media",
     "https://rada.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Quantum Radiation Application Development Association\",\"media\",\"batch:50\"]",
     3600,
     "Quantum Radiation Application Development Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_radtech_miyagi_or_jp, "jp-b50-radtech-miyagi-or-jp",
     "Miyagi Association of Radiological Technologists — 公益社団法人 宮城県放射線技師会 RSS Feed", "宮城県放射線技師会 — 公益社団法人 宮城県放射線技師会 RSS Feed",
     "news", "media",
     "https://www.radtech-miyagi.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Miyagi Association of Radiological Technologists — 公益社団法人 宮城県放射線技師会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Miyagi Association of Radiological Technologists — 公益社団法人 宮城県放射線技師会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_rescuesystem_org, "jp-b50-rescuesystem-org",
     "International Rescue System", "国際レスキューシステム",
     "news", "media",
     "http://www.rescuesystem.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:International Rescue System\",\"media\",\"batch:50\"]",
     3600,
     "International Rescue System — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 7 entries at probe")

RSSX(jp_b50_resemom_jp_rss20, "jp-b50-resemom-jp-rss20",
     "受験や進路・進学情報の教育ニュースは「リセマム」", "受験や進路・進学情報の教育ニュースは「リセマム」",
     "news", "media",
     "https://resemom.jp/rss20/index.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:受験や進路・進学情報の教育ニュースは「リセマム」\",\"media\",\"batch:50\"]",
     3600,
     "受験や進路・進学情報の教育ニュースは「リセマム」 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_retpc_jp, "jp-b50-retpc-jp",
     "The Real Estate Transaction Promotion Center", "公益財団法人不動産流通推進センター",
     "news", "media",
     "https://www.retpc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Real Estate Transaction Promotion Center\",\"media\",\"batch:50\"]",
     3600,
     "The Real Estate Transaction Promotion Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_rfa_or_jp, "jp-b50-rfa-or-jp",
     "Railway Freight Association", "鉄道貨物協会",
     "news", "media",
     "https://rfa.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Railway Freight Association\",\"media\",\"batch:50\"]",
     3600,
     "Railway Freight Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ric_shizuoka_or_jp_news, "jp-b50-ric-shizuoka-or-jp-news",
     "Shizuoka Prefectural Industrial Promotion Foundation", "公益財団法人静岡県産業振興財団",
     "news", "media",
     "https://www.ric-shizuoka.or.jp/news.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Shizuoka Prefectural Industrial Promotion Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Shizuoka Prefectural Industrial Promotion Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 137 entries at probe")

RSSX(jp_b50_rice_cook_com, "jp-b50-rice-cook-com",
     "Japan Cooked-rice Association", "日本炊飯協会",
     "news", "media",
     "https://www.rice-cook.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Cooked-rice Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Cooked-rice Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ring_or_jp, "jp-b50-ring-or-jp",
     "Research Association of Refinery Integration for Group-Operation", "石油コンビナート高度統合運営技術研究組合",
     "news", "media",
     "https://ring.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Research Association of Refinery Integration for Group-Operation\",\"media\",\"batch:50\"]",
     3600,
     "Research Association of Refinery Integration for Group-Operation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_rinri_or_jp, "jp-b50-rinri-or-jp",
     "The Uehiro Foundation on Ethics and Education", "上廣倫理財団",
     "news", "media",
     "https://www.rinri.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Uehiro Foundation on Ethics and Education\",\"media\",\"batch:50\"]",
     3600,
     "The Uehiro Foundation on Ethics and Education — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_rite_or_jp_en, "jp-b50-rite-or-jp-en",
     "Research Institute of Innovative Technology for the Earth — Recent Entries", "地球環境産業技術研究機構 — Recent Entries",
     "news", "media",
     "https://www.rite.or.jp/en/atom.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Research Institute of Innovative Technology for the Earth — Recent Entries\",\"media\",\"batch:50\"]",
     3600,
     "Research Institute of Innovative Technology for the Earth — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_river_or_jp, "jp-b50-river-or-jp",
     "Foundation of River&Basin Integrated Communications — Recent Entries", "河川情報センター — Recent Entries",
     "news", "media",
     "https://www.river.or.jp/atom.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Foundation of River&Basin Integrated Communications — Recent Entries\",\"media\",\"batch:50\"]",
     3600,
     "Foundation of River&Basin Integrated Communications — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_robocip_or_jp, "jp-b50-robocip-or-jp",
     "Robot Industrial Basic Technology Collaborative Innovation Partnership", "産業用ロボット次世代基礎技術研究機構",
     "news", "media",
     "https://www.robocip.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Robot Industrial Basic Technology Collaborative Innovation Partnership\",\"media\",\"batch:50\"]",
     3600,
     "Robot Industrial Basic Technology Collaborative Innovation Partnership — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 9 entries at probe")

RSSX(jp_b50_rotary_yoneyama_or_jp, "jp-b50-rotary-yoneyama-or-jp",
     "Rotary Yoneyama Memorial Foundation", "公益財団法人ロータリー米山記念奨学会",
     "news", "media",
     "https://www.rotary-yoneyama.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Rotary Yoneyama Memorial Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Rotary Yoneyama Memorial Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_rss_itmedia_co_jp_0_91_ait, "jp-b50-rss-itmedia-co-jp-0-91-ait",
     "ITエキスパートのための問題解決メディア — ＠IT RSS 0.91", "ITエキスパートのための問題解決メディア — ＠IT RSS 0.91",
     "news", "media",
     "https://rss.itmedia.co.jp/rss/0.91/ait.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:ITエキスパートのための問題解決メディア — ＠IT RSS 0.91\",\"media\",\"batch:50\"]",
     3600,
     "ITエキスパートのための問題解決メディア — ＠IT RSS 0.91 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_rss_itmedia_co_jp_1_0_ait, "jp-b50-rss-itmedia-co-jp-1-0-ait",
     "ITエキスパートのための問題解決メディア — ＠IT RSS 1.0", "ITエキスパートのための問題解決メディア — ＠IT RSS 1.0",
     "news", "media",
     "https://rss.itmedia.co.jp/rss/1.0/ait.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:ITエキスパートのための問題解決メディア — ＠IT RSS 1.0\",\"media\",\"batch:50\"]",
     3600,
     "ITエキスパートのための問題解決メディア — ＠IT RSS 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_rssj_or_jp, "jp-b50-rssj-or-jp",
     "The Remote Sensing Society of Japan", "一般社団法人日本リモートセンシング学会",
     "news", "media",
     "https://www.rssj.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Remote Sensing Society of Japan\",\"media\",\"batch:50\"]",
     3600,
     "The Remote Sensing Society of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ryokuiku_net, "jp-b50-ryokuiku-net",
     "Chausuyama Natural Botanical Gardens", "茶臼山自然植物園",
     "news", "media",
     "https://ryokuiku.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Chausuyama Natural Botanical Gardens\",\"media\",\"batch:50\"]",
     3600,
     "Chausuyama Natural Botanical Gardens — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sagamiharashi_machimidori_or_jp, "jp-b50-sagamiharashi-machimidori-or-jp",
     "Sagamihara City Town and Greenery Public Corporation", "相模原市まち・みどり公社",
     "news", "media",
     "https://www.sagamiharashi-machimidori.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Sagamihara City Town and Greenery Public Corporation\",\"media\",\"batch:50\"]",
     3600,
     "Sagamihara City Town and Greenery Public Corporation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_salvationarmy_or_jp_home, "jp-b50-salvationarmy-or-jp-home",
     "Salvation Army Japan — 救世軍 » トップページ のコメントのフィード", "救世軍 — 救世軍 » トップページ のコメントのフィード",
     "news", "media",
     "https://www.salvationarmy.or.jp/home/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Salvation Army Japan — 救世軍 » トップページ のコメントのフィード\",\"media\",\"batch:50\"]",
     3600,
     "Salvation Army Japan — 救世軍 » トップページ のコメントのフィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_samugaku_com, "jp-b50-samugaku-com",
     "Samurai Gakuen Scuola Imajin", "侍学園スクオーラ・今人",
     "news", "media",
     "https://samugaku.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Samurai Gakuen Scuola Imajin\",\"media\",\"batch:50\"]",
     3600,
     "Samurai Gakuen Scuola Imajin — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sanei_or_jp_rss1_0, "jp-b50-sanei-or-jp-rss1-0",
     "Japan Society for Occupational Health", "日本産業衛生学会",
     "news", "media",
     "https://www.sanei.or.jp/feed/rss1.0.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Society for Occupational Health\",\"media\",\"batch:50\"]",
     3600,
     "Japan Society for Occupational Health — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_sanei_or_jp_rss2_0, "jp-b50-sanei-or-jp-rss2-0",
     "Japan Society for Occupational Health", "日本産業衛生学会",
     "news", "media",
     "https://www.sanei.or.jp/feed/rss2.0.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Society for Occupational Health\",\"media\",\"batch:50\"]",
     3600,
     "Japan Society for Occupational Health — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_sanko_techno_co_jp, "jp-b50-sanko-techno-co-jp",
     "Home", "Home",
     "news", "media",
     "https://sanko-techno.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Home\",\"media\",\"batch:50\"]",
     3600,
     "Home — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sapporo_deaf_jp, "jp-b50-sapporo-deaf-jp",
     "Sapporo Association of the Deaf", "札幌聴覚障害者協会",
     "news", "media",
     "https://www.sapporo-deaf.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Sapporo Association of the Deaf\",\"media\",\"batch:50\"]",
     3600,
     "Sapporo Association of the Deaf — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_satsukikai_org, "jp-b50-satsukikai-org",
     "NPO Komae Satsuki (Azalea) Association", "ＮＰＯ狛江さつき会",
     "news", "media",
     "https://satsukikai.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:NPO Komae Satsuki (Azalea) Association\",\"media\",\"batch:50\"]",
     3600,
     "NPO Komae Satsuki (Azalea) Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sbgf_jp, "jp-b50-sbgf-jp",
     "Shibuya City Botanical Garden Fureai", "渋谷区ふれあい植物センター",
     "news", "media",
     "https://sbgf.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Shibuya City Botanical Garden Fureai\",\"media\",\"batch:50\"]",
     3600,
     "Shibuya City Botanical Garden Fureai — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 17 entries at probe")

RSSX(jp_b50_scan_netsecurity_ne_jp_rss20, "jp-b50-scan-netsecurity-ne-jp-rss20",
     "ScanNetSecurity", "ScanNetSecurity",
     "news", "media",
     "https://scan.netsecurity.ne.jp/rss20/index.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:ScanNetSecurity\",\"media\",\"batch:50\"]",
     3600,
     "ScanNetSecurity — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_scil_jp, "jp-b50-scil-jp",
     "Himawari Business Association", "ひまわり事業団",
     "news", "media",
     "https://www.scil.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Himawari Business Association\",\"media\",\"batch:50\"]",
     3600,
     "Himawari Business Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_scorez_org, "jp-b50-scorez-org",
     "SCORE", "ＳＣＯＲＥ",
     "news", "media",
     "https://scorez.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:SCORE\",\"media\",\"batch:50\"]",
     3600,
     "SCORE — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sdemo_net, "jp-b50-sdemo-net",
     "Roman Catholic Diocese of Kagoshima", "カトリック鹿児島司教区",
     "news", "media",
     "https://sdemo.net/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Roman Catholic Diocese of Kagoshima\",\"media\",\"batch:50\"]",
     3600,
     "Roman Catholic Diocese of Kagoshima — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_seiko_denki_co_jp, "jp-b50-seiko-denki-co-jp",
     "株式会社 正興電機製作所", "株式会社 正興電機製作所",
     "news", "media",
     "https://www.seiko-denki.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社 正興電機製作所\",\"media\",\"batch:50\"]",
     3600,
     "株式会社 正興電機製作所 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_seishonen_or_jp_honbu, "jp-b50-seishonen-or-jp-honbu",
     "Hyogo Youth Services Administration", "兵庫県青少年本部",
     "news", "media",
     "https://seishonen.or.jp/honbu/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hyogo Youth Services Administration\",\"media\",\"batch:50\"]",
     3600,
     "Hyogo Youth Services Administration — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_senkon_co_jp, "jp-b50-senkon-co-jp",
     "センコン物流株式会社", "センコン物流株式会社",
     "news", "media",
     "https://www.senkon.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:センコン物流株式会社\",\"media\",\"batch:50\"]",
     3600,
     "センコン物流株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_setagaya_ia_or_jp, "jp-b50-setagaya-ia-or-jp",
     "The Setagaya Industrial Association", "世田谷工業振興協会",
     "news", "media",
     "https://setagaya-ia.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Setagaya Industrial Association\",\"media\",\"batch:50\"]",
     3600,
     "The Setagaya Industrial Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sfwj_jp, "jp-b50-sfwj-jp",
     "Science Fiction and Fantasy Writers of Japan", "日本SF作家クラブ",
     "news", "media",
     "https://sfwj.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Science Fiction and Fantasy Writers of Japan\",\"media\",\"batch:50\"]",
     3600,
     "Science Fiction and Fantasy Writers of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_shajoukyo_ciao_jp_index_cmd_rss, "jp-b50-shajoukyo-ciao-jp-index-cmd-rss",
     "All Japan Shooting Range Association", "全日本指定射撃場協会",
     "news", "media",
     "https://shajoukyo.ciao.jp/index.php?cmd=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:All Japan Shooting Range Association\",\"media\",\"batch:50\"]",
     3600,
     "All Japan Shooting Range Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_sherlock_jp, "jp-b50-sherlock-jp",
     "Sherlock Homes Association", "シャーロックホームズ",
     "news", "media",
     "https://sherlock.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Sherlock Homes Association\",\"media\",\"batch:50\"]",
     3600,
     "Sherlock Homes Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_shinbunka_co_jp, "jp-b50-shinbunka-co-jp",
     "新文化オンライン", "新文化オンライン",
     "news", "media",
     "https://www.shinbunka.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:新文化オンライン\",\"media\",\"batch:50\"]",
     3600,
     "新文化オンライン — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_shintaku_kyokai_or_jp_rss2, "jp-b50-shintaku-kyokai-or-jp-rss2",
     "信託協会", "信託協会",
     "news", "media",
     "https://www.shintaku-kyokai.or.jp/rss2.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:信託協会\",\"media\",\"batch:50\"]",
     3600,
     "信託協会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_shintohd_co_jp, "jp-b50-shintohd-co-jp",
     "ニュース", "ニュース",
     "news", "media",
     "https://www.shintohd.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:ニュース\",\"media\",\"batch:50\"]",
     3600,
     "ニュース — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_shiso_med_jp, "jp-b50-shiso-med-jp",
     "Shiso City Medical Association", "宍粟市医師会",
     "news", "media",
     "https://shiso-med.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Shiso City Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Shiso City Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_shokuhineisei_or_jp, "jp-b50-shokuhineisei-or-jp",
     "Japanese Society for Food Hygiene and Safety", "公益社団法人日本食品衛生学会",
     "news", "media",
     "https://shokuhineisei.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Society for Food Hygiene and Safety\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Society for Food Hygiene and Safety — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_showa_gakuen_net, "jp-b50-showa-gakuen-net",
     "Showa Gakuen", "翔和学園",
     "news", "media",
     "https://showa-gakuen.net/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Showa Gakuen\",\"media\",\"batch:50\"]",
     3600,
     "Showa Gakuen — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_shure_or_jp, "jp-b50-shure-or-jp",
     "Tokyo Shure", "東京シューレ",
     "news", "media",
     "https://www.shure.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Shure\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Shure — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sign_jp_org, "jp-b50-sign-jp-org",
     "Japan Sign Association", "公益社団法人日本サイン協会",
     "news", "media",
     "https://sign-jp.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Sign Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Sign Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sign_or_jp, "jp-b50-sign-or-jp",
     "Japan Sign Design Association", "日本サインデザイン協会",
     "news", "media",
     "https://www.sign.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Sign Design Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Sign Design Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_single_mama_com, "jp-b50-single-mama-com",
     "Single Mothers Forum", "しんぐるまざあず・ふぉーらむ",
     "news", "media",
     "https://www.single-mama.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Single Mothers Forum\",\"media\",\"batch:50\"]",
     3600,
     "Single Mothers Forum — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sitejama_jp_feed_rss2, "jp-b50-sitejama-jp-feed-rss2",
     "Japanese Association of Management Accounting", "日本管理会計学会",
     "news", "media",
     "https://sitejama.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Association of Management Accounting\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Association of Management Accounting — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_sjdf_org, "jp-b50-sjdf-org",
     "Société Japonaise de Didactique du Français — SJDF RSSフィード", "日本フランス語教育学会 — SJDF RSSフィード",
     "news", "media",
     "https://sjdf.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Société Japonaise de Didactique du Français — SJDF RSSフィード\",\"media\",\"batch:50\"]",
     3600,
     "Société Japonaise de Didactique du Français — SJDF RSSフィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_sjve_org, "jp-b50-sjve-org",
     "Society of Japanese Value Engineering", "日本バリュー・エンジニアリング協会",
     "news", "media",
     "https://www.sjve.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Society of Japanese Value Engineering\",\"media\",\"batch:50\"]",
     3600,
     "Society of Japanese Value Engineering — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_snf_jp, "jp-b50-snf-jp",
     "トップページ — 新日本海フェリー｜舞鶴・敦賀・新潟・秋田と北海道を結ぶフェリー航路。 RSS Feed", "トップページ — 新日本海フェリー｜舞鶴・敦賀・新潟・秋田と北海道を結ぶフェリー航路。 RSS Feed",
     "news", "media",
     "https://www.snf.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:トップページ — 新日本海フェリー｜舞鶴・敦賀・新潟・秋田と北海道を結ぶフェリー航路。 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "トップページ — 新日本海フェリー｜舞鶴・敦賀・新潟・秋田と北海道を結ぶフェリー航路。 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_solarcells_jp, "jp-b50-solarcells-jp",
     "Research Association for Technological Innovation of Organic Photovoltaics — 有機系太陽電池技術研究組合 RSS Feed", "有機系太陽電池技術研究組合 — 有機系太陽電池技術研究組合 RSS Feed",
     "news", "media",
     "https://solarcells.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Research Association for Technological Innovation of Organic Photovoltaics — 有機系太陽電池技術研究組合 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Research Association for Technological Innovation of Organic Photovoltaics — 有機系太陽電池技術研究組合 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_solways_or_jp, "jp-b50-solways-or-jp",
     "Solways", "ソルウェイズ",
     "news", "media",
     "https://solways.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Solways\",\"media\",\"batch:50\"]",
     3600,
     "Solways — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_soredoko_jp, "jp-b50-soredoko-jp",
     "ソレドコ", "ソレドコ",
     "news", "media",
     "https://soredoko.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:ソレドコ\",\"media\",\"batch:50\"]",
     3600,
     "ソレドコ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_soredoko_jp_2, "jp-b50-soredoko-jp-2",
     "ソレドコ", "ソレドコ",
     "news", "media",
     "https://soredoko.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:ソレドコ\",\"media\",\"batch:50\"]",
     3600,
     "ソレドコ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_spirit_tokai_sakura_ne_jp_spirit, "jp-b50-spirit-tokai-sakura-ne-jp-spirit",
     "Strategic Peace and International Affairs Research Institute, Tokai University", "東海大学平和戦略国際研究所",
     "news", "media",
     "https://spirit-tokai.sakura.ne.jp/spirit/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Strategic Peace and International Affairs Research Institute, Tokai University\",\"media\",\"batch:50\"]",
     3600,
     "Strategic Peace and International Affairs Research Institute, Tokai University — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_spjd_or_jp_wpspjd_feed_rss2, "jp-b50-spjd-or-jp-wpspjd-feed-rss2",
     "The Society for Promotion of Japanese Diplomacy", "日本外交協会",
     "news", "media",
     "https://www.spjd.or.jp/wpspjd/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Society for Promotion of Japanese Diplomacy\",\"media\",\"batch:50\"]",
     3600,
     "The Society for Promotion of Japanese Diplomacy — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ssda_or_jp, "jp-b50-ssda-or-jp",
     "Japan Solar System Development Association", "一般社団法人ソーラーシステム振興協会",
     "news", "media",
     "https://ssda.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Solar System Development Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Solar System Development Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ssndcentralpacific_org, "jp-b50-ssndcentralpacific-org",
     "School Sisters of Notre Dame Central Pacific Province", "Home",
     "news", "media",
     "https://www.ssndcentralpacific.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:School Sisters of Notre Dame Central Pacific Province\",\"media\",\"batch:50\"]",
     3600,
     "School Sisters of Notre Dame Central Pacific Province — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_starseeds_co_jp, "jp-b50-starseeds-co-jp",
     "スターシーズ株式会社 — RSS2.0 Feed", "スターシーズ株式会社 — RSS2.0 Feed",
     "news", "media",
     "https://starseeds.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:スターシーズ株式会社 — RSS2.0 Feed\",\"media\",\"batch:50\"]",
     3600,
     "スターシーズ株式会社 — RSS2.0 Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_starts_pub_jp, "jp-b50-starts-pub-jp",
     "スターツ出版", "スターツ出版",
     "news", "media",
     "https://starts-pub.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:スターツ出版\",\"media\",\"batch:50\"]",
     3600,
     "スターツ出版 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_step_edogawa_com, "jp-b50-step-edogawa-com",
     "Independent Living Center STEP Edogawa", "自立生活センターSTEPえどがわ",
     "news", "media",
     "https://www.step-edogawa.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Independent Living Center STEP Edogawa\",\"media\",\"batch:50\"]",
     3600,
     "Independent Living Center STEP Edogawa — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_step_hamamatsu_jp, "jp-b50-step-hamamatsu-jp",
     "Hamamatsu Foundation for Science and Technology Promotion", "公益財団法人浜松科学技術研究振興会",
     "news", "media",
     "https://step-hamamatsu.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Hamamatsu Foundation for Science and Technology Promotion\",\"media\",\"batch:50\"]",
     3600,
     "Hamamatsu Foundation for Science and Technology Promotion — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_suisankai_or_jp, "jp-b50-suisankai-or-jp",
     "Japan Fisheries Association — 大日本水産会 RSS Feed", "大日本水産会 — 大日本水産会 RSS Feed",
     "news", "media",
     "https://suisankai.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Fisheries Association — 大日本水産会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Fisheries Association — 大日本水産会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_sukui_jp, "jp-b50-sukui-jp",
     "Sukui no Hikari Kyōdan", "救いの光教団",
     "news", "media",
     "https://sukui.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Sukui no Hikari Kyōdan\",\"media\",\"batch:50\"]",
     3600,
     "Sukui no Hikari Kyōdan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_sundrug_co_jp, "jp-b50-sundrug-co-jp",
     "サンドラッグ", "サンドラッグ",
     "news", "media",
     "https://www.sundrug.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:サンドラッグ\",\"media\",\"batch:50\"]",
     3600,
     "サンドラッグ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_super_or_jp_feed_rss2, "jp-b50-super-or-jp-feed-rss2",
     "National Supermarket Association of Japan", "全国スーパーマーケット協会",
     "news", "media",
     "https://www.super.or.jp/?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:National Supermarket Association of Japan\",\"media\",\"batch:50\"]",
     3600,
     "National Supermarket Association of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_support_hikari_net, "jp-b50-support-hikari-net",
     "Kagayaki HIKARI", "輝ＨＩＫＡＲＩ",
     "news", "media",
     "https://support-hikari.net/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kagayaki HIKARI\",\"media\",\"batch:50\"]",
     3600,
     "Kagayaki HIKARI — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_surimi_org, "jp-b50-surimi-org",
     "National Surimi Manufacturers Association", "一般社団法人全国すり身協会",
     "news", "media",
     "https://surimi.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:National Surimi Manufacturers Association\",\"media\",\"batch:50\"]",
     3600,
     "National Surimi Manufacturers Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_suwaken_jc_jp, "jp-b50-suwaken-jc-jp",
     "Junior Chamber International Suwaken — 公益社団法人 諏訪圏青年会議所（JCI諏訪圏）", "諏訪圏青年会議所 — 公益社団法人 諏訪圏青年会議所（JCI諏訪圏）",
     "news", "media",
     "https://www.suwaken-jc.jp/RSS.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Junior Chamber International Suwaken — 公益社団法人 諏訪圏青年会議所（JCI諏訪圏）\",\"media\",\"batch:50\"]",
     3600,
     "Junior Chamber International Suwaken — 公益社団法人 諏訪圏青年会議所（JCI諏訪圏） — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_swedishcenter_or_jp_en, "jp-b50-swedishcenter-or-jp-en",
     "Swedish Center Foundation", "一般財団法人スウェーデン交流センター",
     "news", "media",
     "http://swedishcenter.or.jp/en/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Swedish Center Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Swedish Center Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_t_mbp_com, "jp-b50-t-mbp-com",
     "Tohoku Institute for Management of Blood Pressure", "一般社団法人東北血圧管理協会",
     "news", "media",
     "https://t-mbp.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tohoku Institute for Management of Blood Pressure\",\"media\",\"batch:50\"]",
     3600,
     "Tohoku Institute for Management of Blood Pressure — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_t_wada_hatenablog_jp, "jp-b50-t-wada-hatenablog-jp",
     "t-wadaのブログ", "t-wadaのブログ",
     "news", "media",
     "https://t-wada.hatenablog.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:t-wadaのブログ\",\"media\",\"batch:50\"]",
     3600,
     "t-wadaのブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 19 entries at probe")

RSSX(jp_b50_tachikawa_chiikibunka_or_jp, "jp-b50-tachikawa-chiikibunka-or-jp",
     "Tachikawa Community & Culture Foundation", "立川市地域文化振興財団",
     "news", "media",
     "https://www.tachikawa-chiikibunka.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tachikawa Community & Culture Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Tachikawa Community & Culture Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tacy_co_jp, "jp-b50-tacy-co-jp",
     "高見沢サイバネティックス — RSS 2.0 Feed", "高見沢サイバネティックス — RSS 2.0 Feed",
     "news", "media",
     "https://www.tacy.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:高見沢サイバネティックス — RSS 2.0 Feed\",\"media\",\"batch:50\"]",
     3600,
     "高見沢サイバネティックス — RSS 2.0 Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tadsa_or_jp, "jp-b50-tadsa-or-jp",
     "Tokyo Authorized Driving School Association — 一般社団法人東京指定自動車教習所協会", "東京指定自動車教習所協会 — 一般社団法人東京指定自動車教習所協会",
     "news", "media",
     "https://www.tadsa.or.jp/RSS.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Authorized Driving School Association — 一般社団法人東京指定自動車教習所協会\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Authorized Driving School Association — 一般社団法人東京指定自動車教習所協会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 22 entries at probe")

RSSX(jp_b50_tadsa_or_jp_files_blog_rss_blog_all, "jp-b50-tadsa-or-jp-files-blog-rss-blog-all",
     "Tokyo Authorized Driving School Association — 一般社団法人東京指定自動車教習所協会", "東京指定自動車教習所協会 — 一般社団法人東京指定自動車教習所協会",
     "news", "media",
     "https://www.tadsa.or.jp/files/blog/rss/RSS_BLOG_ALL.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Authorized Driving School Association — 一般社団法人東京指定自動車教習所協会\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Authorized Driving School Association — 一般社団法人東京指定自動車教習所協会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_tagajo_sc_jp, "jp-b50-tagajo-sc-jp",
     "Tagajo Citizens Sports Club", "多賀城市民スポーツクラブ",
     "news", "media",
     "https://tagajo-sc.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tagajo Citizens Sports Club\",\"media\",\"batch:50\"]",
     3600,
     "Tagajo Citizens Sports Club — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 9 entries at probe")

RSSX(jp_b50_tajima_garden_jp, "jp-b50-tajima-garden-jp",
     "Tajima Plateau Botanical Gardens", "但馬高原植物園",
     "news", "media",
     "https://tajima-garden.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tajima Plateau Botanical Gardens\",\"media\",\"batch:50\"]",
     3600,
     "Tajima Plateau Botanical Gardens — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_takaoka_bunka_com, "jp-b50-takaoka-bunka-com",
     "Takaoka Cultural Foundation", "高岡市民文化振興事業団",
     "news", "media",
     "https://www.takaoka-bunka.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Takaoka Cultural Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Takaoka Cultural Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_takarazuka_c_jp_rss2, "jp-b50-takarazuka-c-jp-rss2",
     "Takarazuka Foundation for Culture Promotion", "宝塚市文化財団",
     "news", "media",
     "https://takarazuka-c.jp/rss2.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Takarazuka Foundation for Culture Promotion\",\"media\",\"batch:50\"]",
     3600,
     "Takarazuka Foundation for Culture Promotion — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_takarazuka_gc_or_jp, "jp-b50-takarazuka-gc-or-jp",
     "Takarazuka Golf Club", "一般社団法人宝塚ゴルフ倶楽部",
     "news", "media",
     "https://www.takarazuka-gc.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Takarazuka Golf Club\",\"media\",\"batch:50\"]",
     3600,
     "Takarazuka Golf Club — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_tamaweb_or_jp, "jp-b50-tamaweb-or-jp",
     "Greater Tokyo Initiative", "一般社団法人首都圏産業活性化協会",
     "news", "media",
     "https://www.tamaweb.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Greater Tokyo Initiative\",\"media\",\"batch:50\"]",
     3600,
     "Greater Tokyo Initiative — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_tanagocolo_org, "jp-b50-tanagocolo-org",
     "KENKO Investment for Health — 吹田市・茨木市のケアプラン・デイサービス・訪問看護は【たなごころ】へ / RSS", "掌 — 吹田市・茨木市のケアプラン・デイサービス・訪問看護は【たなごころ】へ / RSS",
     "news", "media",
     "https://tanagocolo.org/feed.rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:KENKO Investment for Health — 吹田市・茨木市のケアプラン・デイサービス・訪問看護は【たなごころ】へ / RSS\",\"media\",\"batch:50\"]",
     3600,
     "KENKO Investment for Health — 吹田市・茨木市のケアプラン・デイサービス・訪問看護は【たなごころ】へ / RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tanuki_pon_com, "jp-b50-tanuki-pon-com",
     "Heart Setagaya", "はぁと世田谷",
     "news", "media",
     "https://tanuki-pon.com/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Heart Setagaya\",\"media\",\"batch:50\"]",
     3600,
     "Heart Setagaya — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 12 entries at probe")

RSSX(jp_b50_tasukeai_kizuna_jp, "jp-b50-tasukeai-kizuna-jp",
     "Kizuna", "絆",
     "news", "media",
     "https://tasukeai-kizuna.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kizuna\",\"media\",\"batch:50\"]",
     3600,
     "Kizuna — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tatsuno_ibo_or_jp, "jp-b50-tatsuno-ibo-or-jp",
     "Tatsuno City and Ibo District Medical Association", "たつの市・揖保郡医師会",
     "news", "media",
     "https://tatsuno-ibo.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tatsuno City and Ibo District Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Tatsuno City and Ibo District Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tcer_or_jp, "jp-b50-tcer-or-jp",
     "Tokyo Center for Economic Research", "公益財団法人 東京経済研究センター",
     "news", "media",
     "https://www.tcer.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Center for Economic Research\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Center for Economic Research — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tcf_or_jp_exhibition, "jp-b50-tcf-or-jp-exhibition",
     "Tsukuba Cultural Foundation — イベント情報", "つくば文化振興財団 — イベント情報",
     "news", "media",
     "https://www.tcf.or.jp/exhibition/feed/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tsukuba Cultural Foundation — イベント情報\",\"media\",\"batch:50\"]",
     3600,
     "Tsukuba Cultural Foundation — イベント情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tcf_or_jp_news, "jp-b50-tcf-or-jp-news",
     "Tsukuba Cultural Foundation — 最新情報", "つくば文化振興財団 — 最新情報",
     "news", "media",
     "https://www.tcf.or.jp/news/feed/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tsukuba Cultural Foundation — 最新情報\",\"media\",\"batch:50\"]",
     3600,
     "Tsukuba Cultural Foundation — 最新情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tcog_jp, "jp-b50-tcog-jp",
     "Tokyo Cooperative Oncology Group", "東京がん化学療法研究会",
     "news", "media",
     "https://tcog.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Cooperative Oncology Group\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Cooperative Oncology Group — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tech_andpad_co_jp, "jp-b50-tech-andpad-co-jp",
     "ANDPAD Tech Blog", "ANDPAD Tech Blog",
     "news", "media",
     "https://tech.andpad.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:ANDPAD Tech Blog\",\"media\",\"batch:50\"]",
     3600,
     "ANDPAD Tech Blog — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tech_blog_optim_co_jp, "jp-b50-tech-blog-optim-co-jp",
     "OPTiM TECH BLOG", "OPTiM TECH BLOG",
     "news", "media",
     "https://tech-blog.optim.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:OPTiM TECH BLOG\",\"media\",\"batch:50\"]",
     3600,
     "OPTiM TECH BLOG — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tech_layerx_co_jp, "jp-b50-tech-layerx-co-jp",
     "LayerX エンジニアブログ", "LayerX エンジニアブログ",
     "news", "media",
     "https://tech.layerx.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:LayerX エンジニアブログ\",\"media\",\"batch:50\"]",
     3600,
     "LayerX エンジニアブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tech_medpeer_co_jp, "jp-b50-tech-medpeer-co-jp",
     "メドピア開発者ブログ", "メドピア開発者ブログ",
     "news", "media",
     "https://tech.medpeer.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:メドピア開発者ブログ\",\"media\",\"batch:50\"]",
     3600,
     "メドピア開発者ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tech_smartcamp_co_jp, "jp-b50-tech-smartcamp-co-jp",
     "SMARTCAMP Engineer Blog", "SMARTCAMP Engineer Blog",
     "news", "media",
     "https://tech.smartcamp.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:SMARTCAMP Engineer Blog\",\"media\",\"batch:50\"]",
     3600,
     "SMARTCAMP Engineer Blog — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tech_smarthr_jp, "jp-b50-tech-smarthr-jp",
     "SmartHR Tech Blog", "SmartHR Tech Blog",
     "news", "media",
     "https://tech.smarthr.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:SmartHR Tech Blog\",\"media\",\"batch:50\"]",
     3600,
     "SmartHR Tech Blog — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tech_speee_jp, "jp-b50-tech-speee-jp",
     "Speee DEVELOPER BLOG", "Speee DEVELOPER BLOG",
     "news", "media",
     "https://tech.speee.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Speee DEVELOPER BLOG\",\"media\",\"batch:50\"]",
     3600,
     "Speee DEVELOPER BLOG — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tech_timee_co_jp, "jp-b50-tech-timee-co-jp",
     "Timee Product Team Blog", "Timee Product Team Blog",
     "news", "media",
     "https://tech.timee.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:Timee Product Team Blog\",\"media\",\"batch:50\"]",
     3600,
     "Timee Product Team Blog — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_tech_willgate_co_jp, "jp-b50-tech-willgate-co-jp",
     "WILLGATE TECH BLOG", "WILLGATE TECH BLOG",
     "news", "media",
     "https://tech.willgate.co.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:WILLGATE TECH BLOG\",\"media\",\"batch:50\"]",
     3600,
     "WILLGATE TECH BLOG — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_technologies_group_co_jp, "jp-b50-technologies-group-co-jp",
     "株式会社テクノロジーズ", "株式会社テクノロジーズ",
     "news", "media",
     "https://technologies-group.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社テクノロジーズ\",\"media\",\"batch:50\"]",
     3600,
     "株式会社テクノロジーズ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_teri_or_jp, "jp-b50-teri-or-jp",
     "Tokushima Economic Research Institute", "徳島経済研究所",
     "news", "media",
     "https://www.teri.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokushima Economic Research Institute\",\"media\",\"batch:50\"]",
     3600,
     "Tokushima Economic Research Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tesec_co_jp, "jp-b50-tesec-co-jp",
     "株式会社テセック", "株式会社テセック",
     "news", "media",
     "https://www.tesec.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社テセック\",\"media\",\"batch:50\"]",
     3600,
     "株式会社テセック — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 4 entries at probe")

RSSX(jp_b50_tfd_deaf_tokyo, "jp-b50-tfd-deaf-tokyo",
     "Tokyo Federation of Deaf", "公益社団法人東京都聴覚障害者連盟",
     "news", "media",
     "https://www.tfd.deaf.tokyo/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Federation of Deaf\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Federation of Deaf — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_tft_or_jp, "jp-b50-tft-or-jp",
     "Toukai Foundation for Technology", "東海産業技術振興財団",
     "news", "media",
     "https://tft.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Toukai Foundation for Technology\",\"media\",\"batch:50\"]",
     3600,
     "Toukai Foundation for Technology — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_thinkit_co_jp, "jp-b50-thinkit-co-jp",
     "Think IT", "Think IT",
     "news", "media",
     "https://thinkit.co.jp/rss.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Think IT\",\"media\",\"batch:50\"]",
     3600,
     "Think IT — news feed (30 entries at probe)")

RSSX(jp_b50_tiis_or_jp, "jp-b50-tiis-or-jp",
     "Technology Institution of Industrial Safety", "公益社団法人産業安全技術協会",
     "news", "media",
     "https://www.tiis.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Technology Institution of Industrial Safety\",\"media\",\"batch:50\"]",
     3600,
     "Technology Institution of Industrial Safety — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tmsa_or_jp, "jp-b50-tmsa-or-jp",
     "Tokyo Metal Stamping Association", "東京都金属プレス工業会",
     "news", "media",
     "https://www.tmsa.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Metal Stamping Association\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Metal Stamping Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tobira_tokyo, "jp-b50-tobira-tokyo",
     "Tokyo Biomarker Innovation Research Association", "東京バイオマーカー・イノベーション技術研究組合",
     "news", "media",
     "https://tobira.tokyo/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Biomarker Innovation Research Association\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Biomarker Innovation Research Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tokiwayama_org_en, "jp-b50-tokiwayama-org-en",
     "Tokiwayama Bunko — 常盤山文庫 RSS Feed", "常盤山文庫 — 常盤山文庫 RSS Feed",
     "news", "media",
     "https://tokiwayama.org/en/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokiwayama Bunko — 常盤山文庫 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "Tokiwayama Bunko — 常盤山文庫 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_tokugawa_gr_jp, "jp-b50-tokugawa-gr-jp",
     "The Tokugawa Museum Foundation", "公益財団法人徳川ミュージアム",
     "news", "media",
     "http://www.tokugawa.gr.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Tokugawa Museum Foundation\",\"media\",\"batch:50\"]",
     3600,
     "The Tokugawa Museum Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tokugawa_ne_jp, "jp-b50-tokugawa-ne-jp",
     "Tokugawa Memorial Foundation", "徳川記念財団",
     "news", "media",
     "https://www.tokugawa.ne.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokugawa Memorial Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Tokugawa Memorial Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tokunoshima_town_org_chose_koho_photonews_photo, "jp-b50-tokunoshima-town-org-chose-koho-photonews-photo",
     "Tokunoshima — 徳之島町 撮っておき！フォトニュース", "徳之島町 — 徳之島町 撮っておき！フォトニュース",
     "news", "media",
     "https://www.tokunoshima-town.org/chose/koho/photonews/photo.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokunoshima — 徳之島町 撮っておき！フォトニュース\",\"media\",\"batch:50\"]",
     3600,
     "Tokunoshima — 徳之島町 撮っておき！フォトニュース — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_tokunoshima_town_org_gyoji_gyoji, "jp-b50-tokunoshima-town-org-gyoji-gyoji",
     "Tokunoshima — 徳之島町 行事予定", "徳之島町 — 徳之島町 行事予定",
     "news", "media",
     "https://www.tokunoshima-town.org/gyoji/gyoji.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokunoshima — 徳之島町 行事予定\",\"media\",\"batch:50\"]",
     3600,
     "Tokunoshima — 徳之島町 行事予定 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_tokunoshima_town_org_oshirase_oshirase, "jp-b50-tokunoshima-town-org-oshirase-oshirase",
     "Tokunoshima — 徳之島町 お知らせ", "徳之島町 — 徳之島町 お知らせ",
     "news", "media",
     "https://www.tokunoshima-town.org/oshirase/oshirase.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokunoshima — 徳之島町 お知らせ\",\"media\",\"batch:50\"]",
     3600,
     "Tokunoshima — 徳之島町 お知らせ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_tokunoshima_town_org_shinchaku_shinchaku, "jp-b50-tokunoshima-town-org-shinchaku-shinchaku",
     "Tokunoshima — 徳之島町 新着情報", "徳之島町 — 徳之島町 新着情報",
     "news", "media",
     "https://www.tokunoshima-town.org/shinchaku/shinchaku.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokunoshima — 徳之島町 新着情報\",\"media\",\"batch:50\"]",
     3600,
     "Tokunoshima — 徳之島町 新着情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 50 entries at probe")

RSSX(jp_b50_tokushima_portugal_org, "jp-b50-tokushima-portugal-org",
     "Tokushima Japan-Portugal Association", "徳島日本ポルトガル協会",
     "news", "media",
     "https://tokushima-portugal.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokushima Japan-Portugal Association\",\"media\",\"batch:50\"]",
     3600,
     "Tokushima Japan-Portugal Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tokyo_ichiban_foods_co_jp, "jp-b50-tokyo-ichiban-foods-co-jp",
     "株式会社 東京一番フーズ｜コーポレートサイト", "株式会社 東京一番フーズ｜コーポレートサイト",
     "news", "media",
     "https://www.tokyo-ichiban-foods.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社 東京一番フーズ｜コーポレートサイト\",\"media\",\"batch:50\"]",
     3600,
     "株式会社 東京一番フーズ｜コーポレートサイト — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tokyo_orange_jp, "jp-b50-tokyo-orange-jp",
     "Inclusion Center Tokyo Orange", "インクルージョンセンター東京オレンヂ",
     "news", "media",
     "https://tokyo-orange.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Inclusion Center Tokyo Orange\",\"media\",\"batch:50\"]",
     3600,
     "Inclusion Center Tokyo Orange — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_tokyo_yogashi_com, "jp-b50-tokyo-yogashi-com",
     "Tokyo Confectionery Association", "東京都洋菓子協会",
     "news", "media",
     "https://tokyo-yogashi.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Confectionery Association\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Confectionery Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tokyobase_co_jp, "jp-b50-tokyobase-co-jp",
     "TOKYO BASE", "TOKYO BASE",
     "news", "media",
     "https://tokyobase.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:TOKYO BASE\",\"media\",\"batch:50\"]",
     3600,
     "TOKYO BASE — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_tokyokenji_steiner_jp, "jp-b50-tokyokenji-steiner-jp",
     "Tokyo Kenji School", "東京賢治の学校",
     "news", "media",
     "https://tokyokenji-steiner.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tokyo Kenji School\",\"media\",\"batch:50\"]",
     3600,
     "Tokyo Kenji School — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_toshiseibi_or_jp, "jp-b50-toshiseibi-or-jp",
     "Kanagawa Construction Technology Center", "公益財団法人神奈川県都市整備技術センター",
     "news", "media",
     "https://toshiseibi.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kanagawa Construction Technology Center\",\"media\",\"batch:50\"]",
     3600,
     "Kanagawa Construction Technology Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 4 entries at probe")

RSSX(jp_b50_touseki_ikai_or_jp, "jp-b50-touseki-ikai-or-jp",
     "Japanese Association of Dialysis Physicians", "公益社団法人日本透析医会",
     "news", "media",
     "https://touseki-ikai.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japanese Association of Dialysis Physicians\",\"media\",\"batch:50\"]",
     3600,
     "Japanese Association of Dialysis Physicians — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_toyo_logistics_co_jp, "jp-b50-toyo-logistics-co-jp",
     "東陽倉庫株式会社", "東陽倉庫株式会社",
     "news", "media",
     "https://www.toyo-logistics.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:東陽倉庫株式会社\",\"media\",\"batch:50\"]",
     3600,
     "東陽倉庫株式会社 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tri_step_or_jp, "jp-b50-tri-step-or-jp",
     "Shikoku Industry & Technology Promotion Center", "一般財団法人四国産業・技術振興センター",
     "news", "media",
     "https://www.tri-step.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Shikoku Industry & Technology Promotion Center\",\"media\",\"batch:50\"]",
     3600,
     "Shikoku Industry & Technology Promotion Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tsuchiya_co_jp, "jp-b50-tsuchiya-co-jp",
     "新築住宅の土屋ホームからリフォーム、不動産・賃貸事業まで｜土屋グループ", "新築住宅の土屋ホームからリフォーム、不動産・賃貸事業まで｜土屋グループ",
     "news", "media",
     "https://www.tsuchiya.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:新築住宅の土屋ホームからリフォーム、不動産・賃貸事業まで｜土屋グループ\",\"media\",\"batch:50\"]",
     3600,
     "新築住宅の土屋ホームからリフォーム、不動産・賃貸事業まで｜土屋グループ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tsukuba_fc_com, "jp-b50-tsukuba-fc-com",
     "Tsukuba Football Club — 最新情報", "つくばフットボールクラブ — 最新情報",
     "news", "media",
     "https://www.tsukuba-fc.com/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tsukuba Football Club — 最新情報\",\"media\",\"batch:50\"]",
     3600,
     "Tsukuba Football Club — 最新情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tsukuba_fc_com_game, "jp-b50-tsukuba-fc-com-game",
     "Tsukuba Football Club — 試合日程・結果", "つくばフットボールクラブ — 試合日程・結果",
     "news", "media",
     "https://www.tsukuba-fc.com/game/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tsukuba Football Club — 試合日程・結果\",\"media\",\"batch:50\"]",
     3600,
     "Tsukuba Football Club — 試合日程・結果 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tsukuba_fc_com_info, "jp-b50-tsukuba-fc-com-info",
     "Tsukuba Football Club — お知らせ", "つくばフットボールクラブ — お知らせ",
     "news", "media",
     "https://www.tsukuba-fc.com/info/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tsukuba Football Club — お知らせ\",\"media\",\"batch:50\"]",
     3600,
     "Tsukuba Football Club — お知らせ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tsukuba_fc_com_joyfulhonda_game, "jp-b50-tsukuba-fc-com-joyfulhonda-game",
     "Tsukuba Football Club — 試合日程・結果（ジョイフル本田つくばFC）", "つくばフットボールクラブ — 試合日程・結果（ジョイフル本田つくばFC）",
     "news", "media",
     "https://www.tsukuba-fc.com/joyfulhonda/game/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tsukuba Football Club — 試合日程・結果（ジョイフル本田つくばFC）\",\"media\",\"batch:50\"]",
     3600,
     "Tsukuba Football Club — 試合日程・結果（ジョイフル本田つくばFC） — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tsukuba_fc_com_ladies_game, "jp-b50-tsukuba-fc-com-ladies-game",
     "Tsukuba Football Club — 試合日程・結果（つくばFCレディース）", "つくばフットボールクラブ — 試合日程・結果（つくばFCレディース）",
     "news", "media",
     "https://www.tsukuba-fc.com/ladies/game/index.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tsukuba Football Club — 試合日程・結果（つくばFCレディース）\",\"media\",\"batch:50\"]",
     3600,
     "Tsukuba Football Club — 試合日程・結果（つくばFCレディース） — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tsukui_jc_com, "jp-b50-tsukui-jc-com",
     "Junior Chamber International Tsukui", "津久井青年会議所",
     "news", "media",
     "https://tsukui-jc.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Junior Chamber International Tsukui\",\"media\",\"batch:50\"]",
     3600,
     "Junior Chamber International Tsukui — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 7 entries at probe")

RSSX(jp_b50_tsuruoka_sk_jp, "jp-b50-tsuruoka-sk-jp",
     "鶴岡信用金庫", "鶴岡信用金庫",
     "news", "media",
     "https://www.tsuruoka-sk.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:鶴岡信用金庫\",\"media\",\"batch:50\"]",
     3600,
     "鶴岡信用金庫 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_tukusi_org, "jp-b50-tukusi-org",
     "Tsukushi Association", "つくし",
     "news", "media",
     "https://www.tukusi.org/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tsukushi Association\",\"media\",\"batch:50\"]",
     3600,
     "Tsukushi Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_tulipfair_or_jp_en, "jp-b50-tulipfair-or-jp-en",
     "Tonami Tulip Gallery — チューリップ四季彩館 » English のコメントのフィード", "チューリップ四季彩館 — チューリップ四季彩館 » English のコメントのフィード",
     "news", "media",
     "https://www.tulipfair.or.jp/en/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Tonami Tulip Gallery — チューリップ四季彩館 » English のコメントのフィード\",\"media\",\"batch:50\"]",
     3600,
     "Tonami Tulip Gallery — チューリップ四季彩館 » English のコメントのフィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 2 entries at probe")

RSSX(jp_b50_txbiz_tv_tokyo_co_jp_news_media_sitemap, "jp-b50-txbiz-tv-tokyo-co-jp-news-media-sitemap",
     "テレ東BIZ", "テレ東BIZ",
     "news", "media",
     "https://txbiz.tv-tokyo.co.jp/news_media/sitemap/rss.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:テレ東BIZ\",\"media\",\"batch:50\"]",
     3600,
     "テレ東BIZ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_ueki_or_jp_xml_rss2, "jp-b50-ueki-or-jp-xml-rss2",
     "Japan Nurserymen's Association", "日本植木協会",
     "news", "media",
     "https://www.ueki.or.jp/xml-rss2.php",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Nurserymen's Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Nurserymen's Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_uitech_jp, "jp-b50-uitech-jp",
     "Urban Infrastructure Technology Center Foundation", "都市技術センター",
     "news", "media",
     "https://uitech.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Urban Infrastructure Technology Center Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Urban Infrastructure Technology Center Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_uken_net_kouhoutoukei_chumoku, "jp-b50-uken-net-kouhoutoukei-chumoku",
     "Uken — 宇検村 注目情報", "宇検村 — 宇検村 注目情報",
     "news", "media",
     "https://www.uken.net/kouhoutoukei/chumoku.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Uken — 宇検村 注目情報\",\"media\",\"batch:50\"]",
     3600,
     "Uken — 宇検村 注目情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_uken_net_kouhoutoukei_shinchaku, "jp-b50-uken-net-kouhoutoukei-shinchaku",
     "Uken — 宇検村 新着情報", "宇検村 — 宇検村 新着情報",
     "news", "media",
     "https://www.uken.net/kouhoutoukei/shinchaku.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Uken — 宇検村 新着情報\",\"media\",\"batch:50\"]",
     3600,
     "Uken — 宇検村 新着情報 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_unit42_paloaltonetworks_com_ja, "jp-b50-unit42-paloaltonetworks-com-ja",
     "Unit 42", "Unit 42",
     "news", "media",
     "https://unit42.paloaltonetworks.com/ja/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Unit 42\",\"media\",\"batch:50\"]",
     3600,
     "Unit 42 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_univers_or_jp_index_cmd_rss, "jp-b50-univers-or-jp-index-cmd-rss",
     "The Univers Foundation", "公益財団法人ユニベール財団",
     "news", "media",
     "https://www.univers.or.jp/index.php?cmd=rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:The Univers Foundation\",\"media\",\"batch:50\"]",
     3600,
     "The Univers Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_untendaikoukyoukai_or_jp, "jp-b50-untendaikoukyoukai-or-jp",
     "Japan Driving service Association — RDF Feed", "全国運転代行協会 — RDF Feed",
     "news", "media",
     "https://www.untendaikoukyoukai.or.jp/feed/rdf/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Driving service Association — RDF Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Driving service Association — RDF Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_untendaikoukyoukai_or_jp_2, "jp-b50-untendaikoukyoukai-or-jp-2",
     "Japan Driving service Association", "全国運転代行協会",
     "news", "media",
     "https://www.untendaikoukyoukai.or.jp/feed/rss/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Driving service Association\",\"media\",\"batch:50\"]",
     3600,
     "Japan Driving service Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_untendaikoukyoukai_or_jp_3, "jp-b50-untendaikoukyoukai-or-jp-3",
     "Japan Driving service Association — RSS2.0 Feed", "全国運転代行協会 — RSS2.0 Feed",
     "news", "media",
     "https://www.untendaikoukyoukai.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Japan Driving service Association — RSS2.0 Feed\",\"media\",\"batch:50\"]",
     3600,
     "Japan Driving service Association — RSS2.0 Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_uoriki_co_jp, "jp-b50-uoriki-co-jp",
     "株式会社魚力", "株式会社魚力",
     "news", "media",
     "https://uoriki.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:株式会社魚力\",\"media\",\"batch:50\"]",
     3600,
     "株式会社魚力 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 3 entries at probe")

RSSX(jp_b50_urawa_med_or_jp, "jp-b50-urawa-med-or-jp",
     "Urawa Medical Association", "浦和医師会",
     "news", "media",
     "https://www.urawa-med.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Urawa Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Urawa Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_usedoor_jp_feed_rss2, "jp-b50-usedoor-jp-feed-rss2",
     "様々なモノ/サービスの使い方・方法やおトク情報、ニュース配信を行うウェブメディア「usedoor」", "様々なモノ/サービスの使い方・方法やおトク情報、ニュース配信を行うウェブメディア「usedoor」",
     "news", "media",
     "https://usedoor.jp?feed=rss2",
     "ja", "[\"news\",\"country:jp\",\"publisher:様々なモノ/サービスの使い方・方法やおトク情報、ニュース配信を行うウェブメディア「usedoor」\",\"media\",\"batch:50\"]",
     3600,
     "様々なモノ/サービスの使い方・方法やおトク情報、ニュース配信を行うウェブメディア「usedoor」 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_uyemura_co_jp, "jp-b50-uyemura-co-jp",
     "TOP", "TOP",
     "news", "media",
     "https://www.uyemura.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:TOP\",\"media\",\"batch:50\"]",
     3600,
     "TOP — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_vipo_or_jp_feed_rss2_cat_4_5_6, "jp-b50-vipo-or-jp-feed-rss2-cat-4-5-6",
     "Visual Industry Promotion Organization — 【VIPO】映像産業振興機構", "映像産業振興機構 — 【VIPO】映像産業振興機構",
     "news", "media",
     "https://www.vipo.or.jp/?feed=rss2&cat=4,5,6",
     "ja", "[\"news\",\"country:jp\",\"publisher:Visual Industry Promotion Organization — 【VIPO】映像産業振興機構\",\"media\",\"batch:50\"]",
     3600,
     "Visual Industry Promotion Organization — 【VIPO】映像産業振興機構 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_w_tokyo_com, "jp-b50-w-tokyo-com",
     "West Tokyo Self-Reliance Support Center", "西東京自立支援センター",
     "news", "media",
     "https://w-tokyo.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:West Tokyo Self-Reliance Support Center\",\"media\",\"batch:50\"]",
     3600,
     "West Tokyo Self-Reliance Support Center — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_walive_org, "jp-b50-walive-org",
     "NPO法人 関西演芸推進協議会", "関西演芸推進協議会",
     "news", "media",
     "https://walive.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:NPO法人 関西演芸推進協議会\",\"media\",\"batch:50\"]",
     3600,
     "NPO法人 関西演芸推進協議会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_waunet_org_iuaes, "jp-b50-waunet-org-iuaes",
     "International Union of Anthropological and Ethnological Sciences", "Home",
     "news", "media",
     "https://waunet.org/iuaes/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:International Union of Anthropological and Ethnological Sciences\",\"media\",\"batch:50\"]",
     3600,
     "International Union of Anthropological and Ethnological Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 8 entries at probe")

RSSX(jp_b50_webapi_yanoshin_jp_webapi_tdnet_list_recent, "jp-b50-webapi-yanoshin-jp-webapi-tdnet-list-recent",
     "東証TDnet WEB-API by やのしん — 東証Tdnet適時開示情報 RSS Feed", "東証TDnet WEB-API by やのしん — 東証Tdnet適時開示情報 RSS Feed",
     "news", "media",
     "https://webapi.yanoshin.jp/webapi/tdnet/list/recent.atom",
     "ja", "[\"news\",\"country:jp\",\"publisher:東証TDnet WEB-API by やのしん — 東証Tdnet適時開示情報 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "東証TDnet WEB-API by やのしん — 東証Tdnet適時開示情報 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 300 entries at probe")

RSSX(jp_b50_wels_jp, "jp-b50-wels-jp",
     "WEL'S — 特定非営利活動法人WEL'S フィード", "ＷＥＬ’Ｓ — 特定非営利活動法人WEL'S フィード",
     "news", "media",
     "https://www.wels.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:WEL'S — 特定非営利活動法人WEL'S フィード\",\"media\",\"batch:50\"]",
     3600,
     "WEL'S — 特定非営利活動法人WEL'S フィード — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_wens_gr_jp, "jp-b50-wens-gr-jp",
     "Whole Earth Nature School", "ホールアース自然学校",
     "news", "media",
     "https://wens.gr.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Whole Earth Nature School\",\"media\",\"batch:50\"]",
     3600,
     "Whole Earth Nature School — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_wsmarch_org_blog_feed, "jp-b50-wsmarch-org-blog-feed",
     "Domani — Wsmarchi - RSS", "ドマーニ — Wsmarchi - RSS",
     "news", "media",
     "https://www.wsmarch.org/blog-feed.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Domani — Wsmarchi - RSS\",\"media\",\"batch:50\"]",
     3600,
     "Domani — Wsmarchi - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 16 entries at probe")

RSSX(jp_b50_www3_nhk_or_jp_news_cat1, "jp-b50-www3-nhk-or-jp-news-cat1",
     "NHK Social News", "NHK 社会ニュース",
     "news", "media",
     "https://www3.nhk.or.jp/rss/news/cat1.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:NHK Social News\",\"media\",\"batch:50\"]",
     3600,
     "NHK Social News — news feed (264 entries at probe)")

RSSX(jp_b50_www3_nhk_or_jp_news_cat3, "jp-b50-www3-nhk-or-jp-news-cat3",
     "NHK Science Culture", "NHK 科学・文化",
     "news", "media",
     "https://www3.nhk.or.jp/rss/news/cat3.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:NHK Science Culture\",\"media\",\"batch:50\"]",
     3600,
     "NHK Science Culture — news feed (12 entries at probe)")

RSSX(jp_b50_www3_nhk_or_jp_news_cat4, "jp-b50-www3-nhk-or-jp-news-cat4",
     "NHK Politics", "NHK 政治",
     "news", "media",
     "https://www3.nhk.or.jp/rss/news/cat4.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:NHK Politics\",\"media\",\"batch:50\"]",
     3600,
     "NHK Politics — news feed (72 entries at probe)")

RSSX(jp_b50_www3_nhk_or_jp_news_cat5, "jp-b50-www3-nhk-or-jp-news-cat5",
     "NHK Economy", "NHK 経済",
     "news", "media",
     "https://www3.nhk.or.jp/rss/news/cat5.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:NHK Economy\",\"media\",\"batch:50\"]",
     3600,
     "NHK Economy — news feed (86 entries at probe)")

RSSX(jp_b50_www3_nhk_or_jp_news_cat6, "jp-b50-www3-nhk-or-jp-news-cat6",
     "NHK International", "NHK 国際",
     "news", "media",
     "https://www3.nhk.or.jp/rss/news/cat6.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:NHK International\",\"media\",\"batch:50\"]",
     3600,
     "NHK International — news feed (98 entries at probe)")

RSSX(jp_b50_www3_nhk_or_jp_news_cat7, "jp-b50-www3-nhk-or-jp-news-cat7",
     "NHK Sports", "NHK スポーツ",
     "news", "media",
     "https://www3.nhk.or.jp/rss/news/cat7.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:NHK Sports\",\"media\",\"batch:50\"]",
     3600,
     "NHK Sports — news feed (88 entries at probe)")

RSSX(jp_b50_xtech_nikkei_com, "jp-b50-xtech-nikkei-com",
     "Nikkei xTECH", "日経クロステック",
     "news", "media",
     "https://xtech.nikkei.com/rss/index.rdf",
     "ja", "[\"news\",\"country:jp\",\"publisher:Nikkei xTECH\",\"media\",\"batch:50\"]",
     3600,
     "Nikkei xTECH — news feed (100 entries at probe)")

RSSX(jp_b50_yakuji_co_jp, "jp-b50-yakuji-co-jp",
     "薬事日報ウェブサイト — 薬事日報ウェブサイト RSS Feed", "薬事日報ウェブサイト — 薬事日報ウェブサイト RSS Feed",
     "news", "media",
     "https://www.yakuji.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:薬事日報ウェブサイト — 薬事日報ウェブサイト RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "薬事日報ウェブサイト — 薬事日報ウェブサイト RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 25 entries at probe")

RSSX(jp_b50_yama_ben_jp, "jp-b50-yama-ben-jp",
     "山梨県弁護士会 || 法律相談と弁護士情報 — 山梨県弁護士会 RDF Feed", "山梨県弁護士会 || 法律相談と弁護士情報 — 山梨県弁護士会 RDF Feed",
     "news", "media",
     "https://yama-ben.jp/feed/rdf/",
     "ja", "[\"news\",\"country:jp\",\"publisher:山梨県弁護士会 || 法律相談と弁護士情報 — 山梨県弁護士会 RDF Feed\",\"media\",\"batch:50\"]",
     3600,
     "山梨県弁護士会 || 法律相談と弁護士情報 — 山梨県弁護士会 RDF Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yama_ben_jp_2, "jp-b50-yama-ben-jp-2",
     "山梨県弁護士会 || 法律相談と弁護士情報 — 山梨県弁護士会 RSS Feed", "山梨県弁護士会 || 法律相談と弁護士情報 — 山梨県弁護士会 RSS Feed",
     "news", "media",
     "https://yama-ben.jp/feed/rss/",
     "ja", "[\"news\",\"country:jp\",\"publisher:山梨県弁護士会 || 法律相談と弁護士情報 — 山梨県弁護士会 RSS Feed\",\"media\",\"batch:50\"]",
     3600,
     "山梨県弁護士会 || 法律相談と弁護士情報 — 山梨県弁護士会 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yamatointr_co_jp, "jp-b50-yamatointr-co-jp",
     "ヤマト インターナショナル株式会社│Yamato International Inc.", "ヤマト インターナショナル株式会社│Yamato International Inc.",
     "news", "media",
     "https://www.yamatointr.co.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:ヤマト インターナショナル株式会社│Yamato International Inc.\",\"media\",\"batch:50\"]",
     3600,
     "ヤマト インターナショナル株式会社│Yamato International Inc. — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yano_co_jp_announce, "jp-b50-yano-co-jp-announce",
     "Yano Research Institute — 矢野経済研究所 お知らせ (RSS 2.0)", "矢野経済研究所 — 矢野経済研究所 お知らせ (RSS 2.0)",
     "news", "media",
     "https://www.yano.co.jp/announce/rss.php",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yano Research Institute — 矢野経済研究所 お知らせ (RSS 2.0)\",\"media\",\"batch:50\"]",
     3600,
     "Yano Research Institute — 矢野経済研究所 お知らせ (RSS 2.0) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yano_co_jp_market_reports, "jp-b50-yano-co-jp-market-reports",
     "Yano Research Institute — 矢野経済研究所 マーケットレポート (RSS 2.0)", "矢野経済研究所 — 矢野経済研究所 マーケットレポート (RSS 2.0)",
     "news", "media",
     "https://www.yano.co.jp/market_reports/rss.php",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yano Research Institute — 矢野経済研究所 マーケットレポート (RSS 2.0)\",\"media\",\"batch:50\"]",
     3600,
     "Yano Research Institute — 矢野経済研究所 マーケットレポート (RSS 2.0) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yano_co_jp_press, "jp-b50-yano-co-jp-press",
     "Yano Research Institute — 矢野経済研究所 プレスリリース～市場調査結果サマリー～ (RSS 2.0)", "矢野経済研究所 — 矢野経済研究所 プレスリリース～市場調査結果サマリー～ (RSS 2.0)",
     "news", "media",
     "https://www.yano.co.jp/press/rss.php",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yano Research Institute — 矢野経済研究所 プレスリリース～市場調査結果サマリー～ (RSS 2.0)\",\"media\",\"batch:50\"]",
     3600,
     "Yano Research Institute — 矢野経済研究所 プレスリリース～市場調査結果サマリー～ (RSS 2.0) — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yasouen_jp, "jp-b50-yasouen-jp",
     "Yamagata Wild Flower Park", "山形市野草園",
     "news", "media",
     "https://www.yasouen.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yamagata Wild Flower Park\",\"media\",\"batch:50\"]",
     3600,
     "Yamagata Wild Flower Park — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yh_kyoto_or_jp_utano_yh, "jp-b50-yh-kyoto-or-jp-utano-yh",
     "Kyoto Youth Hostel Association", "一般財団法人京都ユースホステル協会",
     "news", "media",
     "https://www.yh-kyoto.or.jp/utano_yh/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Kyoto Youth Hostel Association\",\"media\",\"batch:50\"]",
     3600,
     "Kyoto Youth Hostel Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ymm21_jp, "jp-b50-ymm21-jp",
     "Yokohama Minatomirai", "一般社団法人横浜みなとみらい21",
     "news", "media",
     "https://ymm21.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yokohama Minatomirai\",\"media\",\"batch:50\"]",
     3600,
     "Yokohama Minatomirai — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_ynet_or_jp, "jp-b50-ynet-or-jp",
     "Yamagata Promotional Organization for Industrial Technology — Recent Entries", "公益財団法人山形県産業技術振興機構 — Recent Entries",
     "news", "media",
     "https://www.ynet.or.jp/atom.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yamagata Promotional Organization for Industrial Technology — Recent Entries\",\"media\",\"batch:50\"]",
     3600,
     "Yamagata Promotional Organization for Industrial Technology — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_yokeweb_com, "jp-b50-yokeweb-com",
     "Yokohama Association for International Communications and Exchanges", "横浜市国際交流協会",
     "news", "media",
     "https://www.yokeweb.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yokohama Association for International Communications and Exchanges\",\"media\",\"batch:50\"]",
     3600,
     "Yokohama Association for International Communications and Exchanges — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yokohama_history_org, "jp-b50-yokohama-history-org",
     "Yokohama City Furusato History Foundation", "横浜市ふるさと歴史財団",
     "news", "media",
     "https://www.yokohama-history.org/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yokohama City Furusato History Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Yokohama City Furusato History Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yokohama_ywca_jp_elderly, "jp-b50-yokohama-ywca-jp-elderly",
     "Yokohama YWCA — Recent Entries", "横浜YWCA — Recent Entries",
     "news", "media",
     "http://yokohama-ywca.jp/elderly.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yokohama YWCA — Recent Entries\",\"media\",\"batch:50\"]",
     3600,
     "Yokohama YWCA — Recent Entries — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 15 entries at probe")

RSSX(jp_b50_yokohamaport_co_jp, "jp-b50-yokohamaport-co-jp",
     "横浜港埠頭株式会社 公式サイト", "横浜港埠頭株式会社 公式サイト",
     "news", "media",
     "https://yokohamaport.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:横浜港埠頭株式会社 公式サイト\",\"media\",\"batch:50\"]",
     3600,
     "横浜港埠頭株式会社 公式サイト — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yokoyama_fdn_or_jp, "jp-b50-yokoyama-fdn-or-jp",
     "YOKOYAMA Foundation for Clinical Pharmacology", "一般財団法人横山臨床薬理研究助成基金",
     "news", "media",
     "https://yokoyama-fdn.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:YOKOYAMA Foundation for Clinical Pharmacology\",\"media\",\"batch:50\"]",
     3600,
     "YOKOYAMA Foundation for Clinical Pharmacology — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 1 entries at probe")

RSSX(jp_b50_yoshiokabunko_or_jp, "jp-b50-yoshiokabunko-or-jp",
     "一般財団法人 吉岡文庫育英会", "吉岡文庫育英会",
     "news", "media",
     "https://yoshiokabunko.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:一般財団法人 吉岡文庫育英会\",\"media\",\"batch:50\"]",
     3600,
     "一般財団法人 吉岡文庫育英会 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 24 entries at probe")

RSSX(jp_b50_youthleader_or_jp_blog, "jp-b50-youthleader-or-jp-blog",
     "Development Association for Youthleaders — ブログ", "日本ユースリーダー協会 — ブログ",
     "news", "media",
     "https://www.youthleader.or.jp/rss/blog",
     "ja", "[\"news\",\"country:jp\",\"publisher:Development Association for Youthleaders — ブログ\",\"media\",\"batch:50\"]",
     3600,
     "Development Association for Youthleaders — ブログ — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 6 entries at probe")

RSSX(jp_b50_ys_holdings_co_jp, "jp-b50-ys-holdings-co-jp",
     "焼肉坂井ホールディングス", "焼肉坂井ホールディングス",
     "news", "media",
     "https://ys-holdings.co.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:焼肉坂井ホールディングス\",\"media\",\"batch:50\"]",
     3600,
     "焼肉坂井ホールディングス — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yu_cho_f_jp, "jp-b50-yu-cho-f-jp",
     "Yu-cho Foundation", "一般財団法人ゆうちょ財団",
     "news", "media",
     "https://www.yu-cho-f.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yu-cho Foundation\",\"media\",\"batch:50\"]",
     3600,
     "Yu-cho Foundation — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yubujima_com, "jp-b50-yubujima-com",
     "Yubu Island", "由布島",
     "news", "media",
     "https://yubujima.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yubu Island\",\"media\",\"batch:50\"]",
     3600,
     "Yubu Island — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_yugawara_cc_com, "jp-b50-yugawara-cc-com",
     "Yugawara Country Club", "湯河原カンツリー倶楽部",
     "news", "media",
     "https://yugawara-cc.com/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Yugawara Country Club\",\"media\",\"batch:50\"]",
     3600,
     "Yugawara Country Club — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 5 entries at probe")

RSSX(jp_b50_yulily100_hatenablog_jp, "jp-b50-yulily100-hatenablog-jp",
     "ゆるりカナダ日記", "ゆるりカナダ日記",
     "news", "media",
     "https://yulily100.hatenablog.jp/rss",
     "ja", "[\"news\",\"country:jp\",\"publisher:ゆるりカナダ日記\",\"media\",\"batch:50\"]",
     3600,
     "ゆるりカナダ日記 — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 30 entries at probe")

RSSX(jp_b50_zaikei_co_jp_main, "jp-b50-zaikei-co-jp-main",
     "財経新聞：経済・株式の最新ニュース — Main News", "財経新聞：経済・株式の最新ニュース — Main News",
     "news", "media",
     "https://www.zaikei.co.jp/rss/main.xml",
     "ja", "[\"news\",\"country:jp\",\"publisher:財経新聞：経済・株式の最新ニュース — Main News\",\"media\",\"batch:50\"]",
     3600,
     "財経新聞：経済・株式の最新ニュース — Main News — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_zenkyukyo_or_jp, "jp-b50-zenkyukyo-or-jp",
     "Association of Job Information of Japan", "全国求人情報協会",
     "news", "media",
     "https://www.zenkyukyo.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Association of Job Information of Japan\",\"media\",\"batch:50\"]",
     3600,
     "Association of Job Information of Japan — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 20 entries at probe")

RSSX(jp_b50_zenn_dev, "jp-b50-zenn-dev",
     "Zenn Feed", "Zenn フィード",
     "news", "media",
     "https://zenn.dev/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:Zenn Feed\",\"media\",\"batch:50\"]",
     3600,
     "Zenn Feed — news feed (20 entries at probe)")

RSSX(jp_b50_zennenren_or_jp, "jp-b50-zennenren-or-jp",
     "National Federation of Employees' Pensioners' Associations", "全国年金受給者団体連合会",
     "news", "media",
     "https://www.zennenren.or.jp/feed",
     "ja", "[\"news\",\"country:jp\",\"publisher:National Federation of Employees' Pensioners' Associations\",\"media\",\"batch:50\"]",
     3600,
     "National Federation of Employees' Pensioners' Associations — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

RSSX(jp_b50_zuyo_med_jp, "jp-b50-zuyo-med-jp",
     "Zuyo Medical Association", "逗葉医師会",
     "news", "media",
     "https://zuyo-med.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:Zuyo Medical Association\",\"media\",\"batch:50\"]",
     3600,
     "Zuyo Medical Association — RSS/Atom feed declared by the official website (Wikidata P856) of this organisation; 10 entries at probe")

/* ── browser-profile hosts (restored 2026-10-21) ─────────────────────
 * Rejected at authoring for refusing every self-identifying agent; fetched
 * now with the per-host browser profile in core/host_agents.def, HA_BROWSER. */

RSSX(jp_b50_hkd_meti_go_jp_indexatom, "jp-b50-hkd-meti-go-jp-indexatom",
     "経済産業省北海道経済産業局 — 新着情報 (Atom)", "経済産業省北海道経済産業局 — 新着情報 (Atom)",
     "jp_government", "government",
     "https://www.hkd.meti.go.jp/rss/indexatom.xml",
     "ja", "[\"gov\",\"country:jp\",\"publisher:経済産業省北海道経済産業局 — 新着情報 (Atom)\",\"government\",\"batch:50\"]",
     3600,
     "経済産業省北海道経済産業局 — 新着情報 (Atom) — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(jp_b50_bungeika_or_jp, "jp-b50-bungeika-or-jp",
     "日本文藝家協会 (Japan Writers’ Association)", "日本文藝家協会 (Japan Writers’ Association)",
     "news", "media",
     "https://www.bungeika.or.jp/feed/",
     "ja", "[\"news\",\"country:jp\",\"publisher:日本文藝家協会 (Japan Writers’ Association)\",\"media\",\"batch:50\"]",
     3600,
     "日本文藝家協会 (Japan Writers’ Association) — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")
