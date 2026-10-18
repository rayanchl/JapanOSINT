/* collectors/sources/jp_feeds_batch44.c
 * 200 new Japan-focused RSS/Atom feeds, all proven live (HTTP 200) and
 * verified absent from the existing tree on 2026-10-18.
 *
 * Deep/niche coverage: industry verticals, traditional culture,
 * regional communities, specialist hobbies, English-language Japan.
 * Every URL was tested with curl and returned a valid RSS/Atom body.
 * Uses the RSSX macro from _source_macros.inc for concise registration. */
#include "_source_macros.inc"

/* ── Technology / IT ───────────────────────────────────────────── */

RSSX(jp_b44_blog_hatenablog_com, "jp-b44-blog-hatenablog-com",
     "Hatena Blog Staff", "はてなブログ開発",
     "social", "technology",
     "https://staff.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:Hatena Blog Staff\",\"tech\"]",
     3600,
     "Hatena Blog Staff (はてなブログ開発) — tech")

RSSX(jp_b44_hatenanews_com, "jp-b44-hatenanews-com",
     "Hatena News", "はてなニュース",
     "social", "technology",
     "https://hatenanews.com/feed",
     "ja", "[\"news\",\"publisher:Hatena News\",\"tech\",\"social\"]",
     3600,
     "Hatena News (はてなニュース) — tech social")

RSSX(jp_b44_jxpress_net, "jp-b44-jxpress-net",
     "JX Press", "JX通信社",
     "social", "technology",
     "https://jxpress.net/feed/",
     "ja", "[\"news\",\"publisher:JX Press\",\"tech\",\"news\"]",
     3600,
     "JX Press (JX通信社) — news tech")

RSSX(jp_b44_magazine_rubyist_net, "jp-b44-magazine-rubyist-net",
     "Rubyist Magazine", "るびま",
     "social", "technology",
     "https://magazine.rubyist.net/feed.xml",
     "ja", "[\"news\",\"publisher:Rubyist Magazine\",\"tech\"]",
     3600,
     "Rubyist Magazine (るびま) — tech")

RSSX(jp_b44_postd_cc, "jp-b44-postd-cc",
     "POSTD", "POSTD",
     "social", "technology",
     "https://postd.cc/feed/",
     "ja", "[\"news\",\"publisher:POSTD\",\"tech\"]",
     3600,
     "POSTD — tech")

RSSX(jp_b44_pyconjp_blogspot_com, "jp-b44-pyconjp-blogspot-com",
     "PyCon JP Blog", "PyCon JP",
     "social", "technology",
     "https://pyconjp.blogspot.com/feeds/posts/default",
     "ja", "[\"news\",\"publisher:PyCon JP Blog\",\"tech\",\"opensource\"]",
     3600,
     "PyCon JP Blog (PyCon JP) — opensource")

/* ── Tech Blogs ────────────────────────────────────────────────── */

RSSX(jp_b44_anemone_dodgson_org, "jp-b44-anemone-dodgson-org",
     "anemone", "anemone",
     "social", "technology",
     "https://anemone.dodgson.org/feed",
     "ja", "[\"news\",\"publisher:anemone\",\"tech\",\"blog\"]",
     3600,
     "anemone — tech blog")

RSSX(jp_b44_blog_64p_org, "jp-b44-blog-64p-org",
     "tokuhirom", "tokuhirom",
     "social", "technology",
     "https://blog.64p.org/feed",
     "ja", "[\"news\",\"publisher:tokuhirom\",\"tech\",\"blog\"]",
     3600,
     "tokuhirom — tech blog")

RSSX(jp_b44_blog_jnito_com, "jp-b44-blog-jnito-com",
     "Junichi Ito Blog", "伊藤淳一ブログ",
     "social", "technology",
     "https://blog.jnito.com/feed",
     "ja", "[\"news\",\"publisher:Junichi Ito Blog\",\"tech\",\"blog\"]",
     3600,
     "Junichi Ito Blog (伊藤淳一ブログ) — tech blog")

RSSX(jp_b44_blog_shibayu36_org, "jp-b44-blog-shibayu36-org",
     "shibayu36", "shibayu36",
     "social", "technology",
     "https://blog.shibayu36.org/feed",
     "ja", "[\"news\",\"publisher:shibayu36\",\"tech\",\"blog\"]",
     3600,
     "shibayu36 — tech blog")

RSSX(jp_b44_blog_studysapuri_jp, "jp-b44-blog-studysapuri-jp",
     "StudySapuri", "スタディサプリ Product Team",
     "social", "technology",
     "https://blog.studysapuri.jp/feed",
     "ja", "[\"news\",\"publisher:StudySapuri\",\"tech\",\"blog\"]",
     3600,
     "StudySapuri (スタディサプリ Product Team) — tech blog")

RSSX(jp_b44_cockscomb_hatenablog_com, "jp-b44-cockscomb-hatenablog-com",
     "cockscomb", "cockscomb",
     "social", "technology",
     "https://cockscomb.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:cockscomb\",\"tech\",\"blog\"]",
     3600,
     "cockscomb — tech blog")

RSSX(jp_b44_creators_note_chatwork_com, "jp-b44-creators-note-chatwork-com",
     "Chatwork Creators", "Chatwork クリエーターズノート",
     "social", "technology",
     "https://creators-note.chatwork.com/feed",
     "ja", "[\"news\",\"publisher:Chatwork Creators\",\"tech\",\"blog\"]",
     3600,
     "Chatwork Creators (Chatwork クリエーターズノート) — techblog")

RSSX(jp_b44_developers_prtimes_jp, "jp-b44-developers-prtimes-jp",
     "PR TIMES Developers", "PR TIMES 開発者ブログ",
     "social", "technology",
     "https://developers.prtimes.jp/feed/",
     "ja", "[\"news\",\"publisher:PR TIMES Developers\",\"tech\",\"blog\"]",
     3600,
     "PR TIMES Developers (PR TIMES 開発者ブログ) — techblog")

RSSX(jp_b44_efcl_info, "jp-b44-efcl-info",
     "efcl", "efcl.info",
     "social", "technology",
     "https://efcl.info/feed/",
     "ja", "[\"news\",\"publisher:efcl\",\"tech\",\"blog\"]",
     3600,
     "efcl (efcl.info) — tech blog")

RSSX(jp_b44_engineer_crowdworks_jp, "jp-b44-engineer-crowdworks-jp",
     "CrowdWorks Eng", "クラウドワークス エンジニアブログ",
     "social", "technology",
     "https://engineer.crowdworks.jp/feed",
     "ja", "[\"news\",\"publisher:CrowdWorks Eng\",\"tech\",\"blog\"]",
     3600,
     "CrowdWorks Eng (クラウドワークス エンジニアブログ) — tech blog")

RSSX(jp_b44_engineering_meetsmore_com, "jp-b44-engineering-meetsmore-com",
     "MeetsMore", "MeetsMore",
     "social", "technology",
     "https://engineering.meetsmore.com/feed",
     "ja", "[\"news\",\"publisher:MeetsMore\",\"tech\",\"blog\"]",
     3600,
     "MeetsMore — tech blog")

RSSX(jp_b44_hail2u_net, "jp-b44-hail2u-net",
     "hail2u", "hail2u",
     "social", "technology",
     "https://hail2u.net/feed",
     "ja", "[\"news\",\"publisher:hail2u\",\"tech\",\"blog\"]",
     3600,
     "hail2u — tech blog")

RSSX(jp_b44_hisaichi5518_hatenablog_jp, "jp-b44-hisaichi5518-hatenablog-jp",
     "hisaichi5518", "hisaichi5518",
     "social", "technology",
     "https://hisaichi5518.hatenablog.jp/feed",
     "ja", "[\"news\",\"publisher:hisaichi5518\",\"tech\",\"blog\"]",
     3600,
     "hisaichi5518 — tech blog")

RSSX(jp_b44_inside_pixiv_blog, "jp-b44-inside-pixiv-blog",
     "pixiv inside", "pixiv inside",
     "social", "technology",
     "https://inside.pixiv.blog/feed",
     "ja", "[\"news\",\"publisher:pixiv inside\",\"tech\",\"blog\"]",
     3600,
     "pixiv inside — techblog")

RSSX(jp_b44_k0kubun_hatenablog_com, "jp-b44-k0kubun-hatenablog-com",
     "k0kubun", "k0kubun",
     "social", "technology",
     "https://k0kubun.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:k0kubun\",\"tech\",\"blog\"]",
     3600,
     "k0kubun — tech blog")

RSSX(jp_b44_kentaro_hatenablog_com, "jp-b44-kentaro-hatenablog-com",
     "kentaro", "kentaro",
     "social", "technology",
     "https://kentaro.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:kentaro\",\"tech\",\"blog\"]",
     3600,
     "kentaro — tech blog")

RSSX(jp_b44_konifar_hatenablog_com, "jp-b44-konifar-hatenablog-com",
     "konifar", "konifar",
     "social", "technology",
     "https://konifar.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:konifar\",\"tech\",\"blog\"]",
     3600,
     "konifar — tech blog")

RSSX(jp_b44_kumagi_hatenablog_com, "jp-b44-kumagi-hatenablog-com",
     "kumagi", "kumagi",
     "social", "technology",
     "https://kumagi.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:kumagi\",\"tech\",\"blog\"]",
     3600,
     "kumagi — tech blog")

RSSX(jp_b44_laiso_hatenablog_com, "jp-b44-laiso-hatenablog-com",
     "laiso", "laiso",
     "social", "technology",
     "https://laiso.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:laiso\",\"tech\",\"blog\"]",
     3600,
     "laiso — tech blog")

RSSX(jp_b44_lestrrat_hatenablog_com, "jp-b44-lestrrat-hatenablog-com",
     "lestrrat", "lestrrat",
     "social", "technology",
     "https://lestrrat.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:lestrrat\",\"tech\",\"blog\"]",
     3600,
     "lestrrat — tech blog")

RSSX(jp_b44_mixi_developers_mixi_co_jp, "jp-b44-mixi-developers-mixi-co-jp",
     "MIXI Developers", "MIXI Developers",
     "social", "technology",
     "https://mixi-developers.mixi.co.jp/feed",
     "ja", "[\"news\",\"publisher:MIXI Developers\",\"tech\",\"blog\"]",
     3600,
     "MIXI Developers — techblog")

RSSX(jp_b44_mizchi_hatenablog_com, "jp-b44-mizchi-hatenablog-com",
     "mizchi", "mizchi",
     "social", "technology",
     "https://mizchi.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:mizchi\",\"tech\",\"blog\"]",
     3600,
     "mizchi — tech blog")

RSSX(jp_b44_motemen_hatenablog_com, "jp-b44-motemen-hatenablog-com",
     "motemen", "motemen",
     "social", "technology",
     "https://motemen.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:motemen\",\"tech\",\"blog\"]",
     3600,
     "motemen — tech blog")

RSSX(jp_b44_nhiroki_jp, "jp-b44-nhiroki-jp",
     "nhiroki", "nhiroki",
     "social", "technology",
     "https://nhiroki.jp/feed.xml",
     "ja", "[\"news\",\"publisher:nhiroki\",\"tech\",\"blog\"]",
     3600,
     "nhiroki — tech blog")

RSSX(jp_b44_ninjinkun_hatenablog_com, "jp-b44-ninjinkun-hatenablog-com",
     "ninjinkun", "ninjinkun",
     "social", "technology",
     "https://ninjinkun.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:ninjinkun\",\"tech\",\"blog\"]",
     3600,
     "ninjinkun — tech blog")

RSSX(jp_b44_nishiohirokazu_hatenadiary_org, "jp-b44-nishiohirokazu-hatenadiary-org",
     "Nishio Hirokazu", "西尾泰和",
     "social", "technology",
     "https://nishiohirokazu.hatenadiary.org/feed",
     "ja", "[\"news\",\"publisher:Nishio Hirokazu\",\"tech\",\"blog\"]",
     3600,
     "Nishio Hirokazu (西尾泰和) — tech blog")

RSSX(jp_b44_r7kamura_com, "jp-b44-r7kamura-com",
     "r7kamura", "r7kamura",
     "social", "technology",
     "https://r7kamura.com/feed.xml",
     "ja", "[\"news\",\"publisher:r7kamura\",\"tech\",\"blog\"]",
     3600,
     "r7kamura — tech blog")

RSSX(jp_b44_shiba_yan_hatenablog_jp, "jp-b44-shiba-yan-hatenablog-jp",
     "shibayan", "しばやん",
     "social", "technology",
     "https://shiba-yan.hatenablog.jp/feed",
     "ja", "[\"news\",\"publisher:shibayan\",\"tech\",\"blog\"]",
     3600,
     "shibayan (しばやん) — tech blog")

RSSX(jp_b44_swet_dena_com, "jp-b44-swet-dena-com",
     "DeNA SWET", "DeNA SWET",
     "social", "technology",
     "https://swet.dena.com/feed",
     "ja", "[\"news\",\"publisher:DeNA SWET\",\"tech\",\"blog\"]",
     3600,
     "DeNA SWET — techblog")

RSSX(jp_b44_t_wada_hatenablog_jp, "jp-b44-t-wada-hatenablog-jp",
     "t-wada", "t-wada",
     "social", "technology",
     "https://t-wada.hatenablog.jp/feed",
     "ja", "[\"news\",\"publisher:t-wada\",\"tech\",\"blog\"]",
     3600,
     "t-wada — tech blog")

RSSX(jp_b44_takezoe_hatenablog_com, "jp-b44-takezoe-hatenablog-com",
     "takezoe blog", "竹添直樹",
     "social", "technology",
     "https://takezoe.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:takezoe blog\",\"tech\",\"blog\"]",
     3600,
     "takezoe blog (竹添直樹) — techblog")

RSSX(jp_b44_tech_blog_abeja_asia, "jp-b44-tech-blog-abeja-asia",
     "ABEJA Tech", "ABEJA テックブログ",
     "social", "technology",
     "https://tech-blog.abeja.asia/feed",
     "ja", "[\"news\",\"publisher:ABEJA Tech\",\"tech\",\"blog\"]",
     3600,
     "ABEJA Tech (ABEJA テックブログ) — tech blog")

RSSX(jp_b44_tech_connehito_com, "jp-b44-tech-connehito-com",
     "Connehito Tech", "コネヒト開発者ブログ",
     "social", "technology",
     "https://tech.connehito.com/feed",
     "ja", "[\"news\",\"publisher:Connehito Tech\",\"tech\",\"blog\"]",
     3600,
     "Connehito Tech (コネヒト開発者ブログ) — tech blog")

RSSX(jp_b44_tech_smartcamp_co_jp, "jp-b44-tech-smartcamp-co-jp",
     "SMARTCAMP Tech", "SMARTCAMP テックブログ",
     "social", "technology",
     "https://tech.smartcamp.co.jp/feed",
     "ja", "[\"news\",\"publisher:SMARTCAMP Tech\",\"tech\",\"blog\"]",
     3600,
     "SMARTCAMP Tech (SMARTCAMP テックブログ) — techblog")

RSSX(jp_b44_tech_speee_jp, "jp-b44-tech-speee-jp",
     "Speee Tech", "Speee テックブログ",
     "social", "technology",
     "https://tech.speee.jp/feed",
     "ja", "[\"news\",\"publisher:Speee Tech\",\"tech\",\"blog\"]",
     3600,
     "Speee Tech (Speee テックブログ) — techblog")

RSSX(jp_b44_tech_visasq_com, "jp-b44-tech-visasq-com",
     "VisasQ Tech", "ビザスクTechブログ",
     "social", "technology",
     "https://tech.visasq.com/feed",
     "ja", "[\"news\",\"publisher:VisasQ Tech\",\"tech\",\"blog\"]",
     3600,
     "VisasQ Tech (ビザスクTechブログ) — techblog")

RSSX(jp_b44_techlife_mercari_com, "jp-b44-techlife-mercari-com",
     "Mercari Eng EN", "Mercari Engineering",
     "social", "technology",
     "https://engineering.mercari.com/en/blog/feed.xml",
     "en", "[\"news\",\"publisher:Mercari Eng EN\",\"tech\",\"blog\"]",
     3600,
     "Mercari Eng EN (Mercari Engineering) — tech blog")

RSSX(jp_b44_yosuke_furukawa_hatenablog_com, "jp-b44-yosuke-furukawa-hatenablog-com",
     "yosuke-furukawa", "古川陽介",
     "social", "technology",
     "https://yosuke-furukawa.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:yosuke-furukawa\",\"tech\",\"blog\"]",
     3600,
     "yosuke-furukawa (古川陽介) — tech blog")

/* ── Web Design / Dev ──────────────────────────────────────────── */

RSSX(jp_b44_axis_design_co_jp, "jp-b44-axis-design-co-jp",
     "AXIS", "AXIS",
     "social", "technology",
     "https://www.axismag.jp/feed",
     "ja", "[\"news\",\"publisher:AXIS\",\"design\",\"webdev\"]",
     3600,
     "AXIS — design")

RSSX(jp_b44_www_creators_com, "jp-b44-www-creators-com",
     "WWW CREATORS", "ウェブクリエイターズ",
     "social", "technology",
     "https://www-creators.com/feed/",
     "ja", "[\"news\",\"publisher:WWW CREATORS\",\"design\",\"webdev\"]",
     3600,
     "WWW CREATORS (ウェブクリエイターズ) — design")

RSSX(jp_b44_yulily100_hatenablog_jp, "jp-b44-yulily100-hatenablog-jp",
     "yulily", "yulily",
     "social", "technology",
     "https://yulily100.hatenablog.jp/feed",
     "ja", "[\"news\",\"publisher:yulily\",\"design\",\"webdev\"]",
     3600,
     "yulily — design")

/* ── Game Development ──────────────────────────────────────────── */

RSSX(jp_b44_gamemakers_jp, "jp-b44-gamemakers-jp",
     "GameMakers", "ゲームメーカーズ",
     "social", "technology",
     "https://gamemakers.jp/feed/",
     "ja", "[\"news\",\"publisher:GameMakers\",\"tech\",\"gamedev\"]",
     3600,
     "GameMakers (ゲームメーカーズ) — gamedev")

RSSX(jp_b44_gamesindustry_biz, "jp-b44-gamesindustry-biz",
     "GamesIndustry JP", "GamesIndustry Japan",
     "social", "technology",
     "https://jp.gamesindustry.biz/feed/",
     "ja", "[\"news\",\"publisher:GamesIndustry JP\",\"tech\",\"gamedev\"]",
     3600,
     "GamesIndustry JP (GamesIndustry Japan) — gamedev")

RSSX(jp_b44_learning_unity3d_jp, "jp-b44-learning-unity3d-jp",
     "Unity Learning Japan", "Unity Japan",
     "social", "technology",
     "https://learning.unity3d.jp/feed/",
     "ja", "[\"news\",\"publisher:Unity Learning Japan\",\"tech\",\"gamedev\"]",
     3600,
     "Unity Learning Japan (Unity Japan) — gamedev")

/* ── News / Media ──────────────────────────────────────────────── */

RSSX(jp_b44_ajito_fm, "jp-b44-ajito-fm",
     "ajito.fm", "ajito.fm",
     "social", "media",
     "https://ajito.fm/index.xml",
     "ja", "[\"news\",\"publisher:ajito.fm\",\"media\",\"podcast\"]",
     3600,
     "ajito.fm — podcast")

RSSX(jp_b44_bbc_com, "jp-b44-bbc-com",
     "BBC Japanese", "BBC日本語",
     "social", "media",
     "https://feeds.bbci.co.uk/japanese/rss.xml",
     "ja", "[\"news\",\"publisher:BBC Japanese\",\"media\"]",
     3600,
     "BBC Japanese (BBC日本語) — news")

RSSX(jp_b44_fukabori_fm, "jp-b44-fukabori-fm",
     "fukabori.fm", "fukabori.fm",
     "social", "media",
     "https://fukabori.fm/feed.xml",
     "ja", "[\"news\",\"publisher:fukabori.fm\",\"media\",\"podcast\"]",
     3600,
     "fukabori.fm — podcast")

RSSX(jp_b44_hazardlab_jp, "jp-b44-hazardlab-jp",
     "Hazard Lab", "ハザードラボ",
     "social", "media",
     "https://www.hazardlab.jp/rss/",
     "ja", "[\"news\",\"publisher:Hazard Lab\",\"news\",\"disaster\"]",
     3600,
     "Hazard Lab (ハザードラボ) — disaster")

RSSX(jp_b44_misreading_chat, "jp-b44-misreading-chat",
     "Misreading Chat", "Misreading Chat",
     "social", "media",
     "https://misreading.chat/feed/",
     "ja", "[\"news\",\"publisher:Misreading Chat\",\"media\",\"podcast\"]",
     3600,
     "Misreading Chat — podcast")

RSSX(jp_b44_mozaic_fm, "jp-b44-mozaic-fm",
     "mozaic.fm", "mozaic.fm",
     "social", "media",
     "https://feed.mozaic.fm/",
     "ja", "[\"news\",\"publisher:mozaic.fm\",\"media\",\"podcast\"]",
     3600,
     "mozaic.fm — podcast")

RSSX(jp_b44_note_mu, "jp-b44-note-mu",
     "note magazine", "noteマガジン",
     "social", "media",
     "https://note.com/notemag/rss",
     "ja", "[\"news\",\"publisher:note magazine\",\"media\"]",
     3600,
     "note magazine (noteマガジン) — media")

RSSX(jp_b44_rebuild_fm, "jp-b44-rebuild-fm",
     "Rebuild.fm", "Rebuild.fm",
     "social", "media",
     "https://feeds.rebuild.fm/rebuildfm",
     "ja", "[\"news\",\"publisher:Rebuild.fm\",\"media\",\"podcast\"]",
     3600,
     "Rebuild.fm — podcast")

RSSX(jp_b44_researchat_fm, "jp-b44-researchat-fm",
     "researchat.fm", "researchat.fm",
     "social", "media",
     "https://researchat.fm/feed.xml",
     "ja", "[\"news\",\"publisher:researchat.fm\",\"media\",\"podcast\"]",
     3600,
     "researchat.fm — podcast")

RSSX(jp_b44_saigai_me, "jp-b44-saigai-me",
     "Saigai.me", "災害.me",
     "social", "media",
     "https://saigai.me/feed/",
     "ja", "[\"news\",\"publisher:Saigai.me\",\"news\",\"disaster\"]",
     3600,
     "Saigai.me (災害.me) — disaster")

RSSX(jp_b44_tsunagu_local_com, "jp-b44-tsunagu-local-com",
     "Tsunagu Local", "つなぐローカル",
     "social", "media",
     "https://tsunagu-local.com/feed/",
     "ja", "[\"news\",\"publisher:Tsunagu Local\",\"regional\"]",
     3600,
     "Tsunagu Local (つなぐローカル) — regional")

RSSX(jp_b44_yatteiki_fm, "jp-b44-yatteiki-fm",
     "yatteiki.fm", "yatteiki.fm",
     "social", "media",
     "https://yatteiki.fm/feed",
     "ja", "[\"news\",\"publisher:yatteiki.fm\",\"media\",\"podcast\"]",
     3600,
     "yatteiki.fm — podcast")

/* ── Culture / History ─────────────────────────────────────────── */

RSSX(jp_b44_bonsaitonight_com, "jp-b44-bonsaitonight-com",
     "Bonsai Tonight", "Bonsai Tonight",
     "social", "media",
     "https://bonsaitonight.com/feed/",
     "en", "[\"news\",\"publisher:Bonsai Tonight\",\"culture\",\"bonsai\"]",
     3600,
     "Bonsai Tonight — bonsai")

RSSX(jp_b44_brutus_jp, "jp-b44-brutus-jp",
     "BRUTUS", "BRUTUS",
     "social", "media",
     "https://brutus.jp/feed/",
     "ja", "[\"news\",\"publisher:BRUTUS\",\"culture\"]",
     3600,
     "BRUTUS — culture")

RSSX(jp_b44_buddhanet_net, "jp-b44-buddhanet-net",
     "BuddhaNet", "BuddhaNet",
     "social", "media",
     "https://www.buddhanet.net/feed/",
     "en", "[\"news\",\"publisher:BuddhaNet\",\"culture\",\"buddhism\"]",
     3600,
     "BuddhaNet — buddhism")

RSSX(jp_b44_bushoo_japan_com, "jp-b44-bushoo-japan-com",
     "Bushoojapan", "武将JAPAN",
     "social", "media",
     "https://bushoojapan.com/feed/",
     "ja", "[\"news\",\"publisher:Bushoojapan\",\"culture\",\"history\"]",
     3600,
     "Bushoojapan (武将JAPAN) — history")

RSSX(jp_b44_goshuin_meguri_com, "jp-b44-goshuin-meguri-com",
     "Goshuin Meguri", "御朱印巡り",
     "social", "media",
     "https://goshuin-meguri.com/feed/",
     "ja", "[\"news\",\"publisher:Goshuin Meguri\",\"culture\",\"goshuin\"]",
     3600,
     "Goshuin Meguri (御朱印巡り) — goshuin")

RSSX(jp_b44_hellotokyo_jp, "jp-b44-hellotokyo-jp",
     "Hello Tokyo", "Hello Tokyo",
     "social", "media",
     "https://hellotokyo.jp/feed/",
     "en", "[\"news\",\"publisher:Hello Tokyo\",\"culture\"]",
     3600,
     "Hello Tokyo — culture")

RSSX(jp_b44_ikebana_org, "jp-b44-ikebana-org",
     "Ikebana International", "池坊",
     "social", "media",
     "https://www.ikebana.org/feed",
     "en", "[\"news\",\"publisher:Ikebana International\",\"culture\",\"ikebana\"]",
     3600,
     "Ikebana International (池坊) — ikebana")

RSSX(jp_b44_japanhistory_info, "jp-b44-japanhistory-info",
     "Japan History", "日本歴史",
     "social", "media",
     "https://japanhistory.info/feed/",
     "en", "[\"news\",\"publisher:Japan History\",\"culture\",\"history\"]",
     3600,
     "Japan History (日本歴史) — history")

RSSX(jp_b44_japansauce_net, "jp-b44-japansauce-net",
     "Japan Sauce", "Japan Sauce",
     "social", "media",
     "https://www.japansauce.net/feed/",
     "en", "[\"news\",\"publisher:Japan Sauce\",\"culture\"]",
     3600,
     "Japan Sauce — culture")

RSSX(jp_b44_jinja_tera_gosyuin_meguri_com, "jp-b44-jinja-tera-gosyuin-meguri-com",
     "Jinja Goshuin", "神社寺御朱印巡り",
     "social", "media",
     "https://jinja-tera-gosyuin-meguri.com/feed/",
     "ja", "[\"news\",\"publisher:Jinja Goshuin\",\"culture\",\"goshuin\"]",
     3600,
     "Jinja Goshuin (神社寺御朱印巡り) — goshuin")

RSSX(jp_b44_kabuki_bito_jp, "jp-b44-kabuki-bito-jp",
     "Kabuki Bito", "歌舞伎美人",
     "social", "media",
     "https://www.kabuki-bito.jp/feed",
     "ja", "[\"news\",\"publisher:Kabuki Bito\",\"culture\",\"theater\"]",
     3600,
     "Kabuki Bito (歌舞伎美人) — theater")

RSSX(jp_b44_kawaiikakkoiisugoi_com, "jp-b44-kawaiikakkoiisugoi-com",
     "Kawaii Kakkoii Sugoi", "KKS",
     "social", "media",
     "https://kawaiikakkoiisugoi.com/feed/",
     "en", "[\"news\",\"publisher:Kawaii Kakkoii Sugoi\",\"culture\"]",
     3600,
     "Kawaii Kakkoii Sugoi (KKS) — culture")

RSSX(jp_b44_kogeistandard_com, "jp-b44-kogeistandard-com",
     "Kogei Standard", "工芸スタンダード",
     "social", "media",
     "https://kogeistandard.com/feed/",
     "ja", "[\"news\",\"publisher:Kogei Standard\",\"culture\",\"craft\"]",
     3600,
     "Kogei Standard (工芸スタンダード) — traditional craft")

RSSX(jp_b44_kougeihin_jp, "jp-b44-kougeihin-jp",
     "Kougeihin", "伝統工芸品",
     "social", "media",
     "https://kougeihin.jp/feed/",
     "ja", "[\"news\",\"publisher:Kougeihin\",\"culture\",\"craft\"]",
     3600,
     "Kougeihin (伝統工芸品) — craft")

RSSX(jp_b44_kyotojournal_org, "jp-b44-kyotojournal-org",
     "Kyoto Journal", "京都ジャーナル",
     "social", "media",
     "https://www.kyotojournal.org/feed/",
     "en", "[\"news\",\"publisher:Kyoto Journal\",\"culture\"]",
     3600,
     "Kyoto Journal (京都ジャーナル) — culture")

RSSX(jp_b44_lionsroar_com, "jp-b44-lionsroar-com",
     "Lions Roar", "Lion's Roar",
     "social", "media",
     "https://www.lionsroar.com/feed/",
     "en", "[\"news\",\"publisher:Lions Roar\",\"culture\",\"buddhism\"]",
     3600,
     "Lions Roar (Lion's Roar) — buddhism")

RSSX(jp_b44_nihonsi_jiten_com, "jp-b44-nihonsi-jiten-com",
     "Nihon History", "日本史辞典",
     "social", "media",
     "https://nihonsi-jiten.com/feed/",
     "ja", "[\"news\",\"publisher:Nihon History\",\"culture\",\"history\"]",
     3600,
     "Nihon History (日本史辞典) — history")

RSSX(jp_b44_omatsuri_com, "jp-b44-omatsuri-com",
     "Omatsurijapan", "オマツリジャパン",
     "social", "media",
     "https://omatsurijapan.com/blog/feed/",
     "ja", "[\"news\",\"publisher:Omatsurijapan\",\"culture\",\"festival\"]",
     3600,
     "Omatsurijapan (オマツリジャパン) — festival")

RSSX(jp_b44_pinktentacle_com, "jp-b44-pinktentacle-com",
     "Pink Tentacle", "Pink Tentacle",
     "social", "media",
     "https://pinktentacle.com/feed/",
     "en", "[\"news\",\"publisher:Pink Tentacle\",\"culture\"]",
     3600,
     "Pink Tentacle — culture")

RSSX(jp_b44_rensai_jp, "jp-b44-rensai-jp",
     "Rensai.jp", "連載JP",
     "social", "media",
     "https://rensai.jp/feed",
     "ja", "[\"news\",\"publisher:Rensai.jp\",\"culture\"]",
     3600,
     "Rensai.jp (連載JP) — culture")

RSSX(jp_b44_retrogamegoods_com, "jp-b44-retrogamegoods-com",
     "Retro Game Goods", "レトロゲームグッズ",
     "social", "media",
     "https://retrogamegoods.com/feed/",
     "ja", "[\"news\",\"publisher:Retro Game Goods\",\"culture\",\"retro\"]",
     3600,
     "Retro Game Goods (レトロゲームグッズ) — retro")

RSSX(jp_b44_shochiku_co_jp, "jp-b44-shochiku-co-jp",
     "Shochiku", "松竹",
     "social", "media",
     "https://www.shochiku.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Shochiku\",\"culture\",\"theater\"]",
     3600,
     "Shochiku (松竹) — theater")

RSSX(jp_b44_sotokoto_online_jp, "jp-b44-sotokoto-online-jp",
     "Sotokoto", "ソトコト",
     "social", "media",
     "https://sotokoto-online.jp/feed/",
     "ja", "[\"news\",\"publisher:Sotokoto\",\"culture\"]",
     3600,
     "Sotokoto (ソトコト) — culture")

RSSX(jp_b44_suigenkyo_com, "jp-b44-suigenkyo-com",
     "Suigenkyo", "粋玄京",
     "social", "media",
     "https://suigenkyo.com/en/blog/feed",
     "en", "[\"news\",\"publisher:Suigenkyo\",\"culture\",\"craft\"]",
     3600,
     "Suigenkyo (粋玄京) — craft")

RSSX(jp_b44_thisjapaneselife_org, "jp-b44-thisjapaneselife-org",
     "This Japanese Life", "This Japanese Life",
     "social", "media",
     "https://thisjapaneselife.org/feed/",
     "en", "[\"news\",\"publisher:This Japanese Life\",\"culture\"]",
     3600,
     "This Japanese Life — culture")

RSSX(jp_b44_tokyonightowl_com, "jp-b44-tokyonightowl-com",
     "Tokyo Night Owl", "東京ナイトオウル",
     "social", "media",
     "https://tokyonightowl.com/feed/",
     "en", "[\"news\",\"publisher:Tokyo Night Owl\",\"culture\"]",
     3600,
     "Tokyo Night Owl (東京ナイトオウル) — culture")

RSSX(jp_b44_tricycle_org, "jp-b44-tricycle-org",
     "Tricycle", "Tricycle",
     "social", "media",
     "https://tricycle.org/feed/",
     "en", "[\"news\",\"publisher:Tricycle\",\"culture\",\"buddhism\"]",
     3600,
     "Tricycle — buddhism")

RSSX(jp_b44_wattention_com, "jp-b44-wattention-com",
     "WAttention", "WAttention",
     "social", "media",
     "https://www.wattention.com/feed/",
     "en", "[\"news\",\"publisher:WAttention\",\"culture\"]",
     3600,
     "WAttention — culture")

RSSX(jp_b44_zoomingjapan_com, "jp-b44-zoomingjapan-com",
     "Zooming Japan", "Zooming Japan",
     "social", "media",
     "https://zoomingjapan.com/feed/",
     "en", "[\"news\",\"publisher:Zooming Japan\",\"culture\"]",
     3600,
     "Zooming Japan — culture")

/* ── Entertainment / Anime ─────────────────────────────────────── */

RSSX(jp_b44_denfaminicogamer_jp, "jp-b44-denfaminicogamer-jp",
     "DenfamiNico", "電ファミニコゲーマー",
     "social", "media",
     "https://news.denfaminicogamer.jp/feed/",
     "ja", "[\"news\",\"publisher:DenfamiNico\",\"gaming\"]",
     3600,
     "DenfamiNico (電ファミニコゲーマー) — gaming deep")

RSSX(jp_b44_destructoid_com, "jp-b44-destructoid-com",
     "Destructoid", "Destructoid",
     "social", "media",
     "https://www.destructoid.com/feed/",
     "en", "[\"news\",\"publisher:Destructoid\",\"gaming\"]",
     3600,
     "Destructoid — gaming")

RSSX(jp_b44_dualshockers_com, "jp-b44-dualshockers-com",
     "DualShockers", "DualShockers",
     "social", "media",
     "https://www.dualshockers.com/feed/",
     "en", "[\"news\",\"publisher:DualShockers\",\"gaming\"]",
     3600,
     "DualShockers — gaming")

RSSX(jp_b44_easternkicks_com, "jp-b44-easternkicks-com",
     "Eastern Kicks", "Eastern Kicks",
     "social", "media",
     "https://www.easternkicks.com/feed",
     "en", "[\"news\",\"publisher:Eastern Kicks\",\"film\"]",
     3600,
     "Eastern Kicks — film")

RSSX(jp_b44_gematsu_com, "jp-b44-gematsu-com",
     "Gematsu", "Gematsu",
     "social", "media",
     "https://www.gematsu.com/feed",
     "en", "[\"news\",\"publisher:Gematsu\",\"gaming\"]",
     3600,
     "Gematsu — gaming")

RSSX(jp_b44_honeysanime_com, "jp-b44-honeysanime-com",
     "Honey's Anime", "Honey's Anime",
     "social", "media",
     "https://honeysanime.com/feed/",
     "en", "[\"news\",\"publisher:Honey's Anime\",\"anime\"]",
     3600,
     "Honey's Anime — anime")

RSSX(jp_b44_kaiyodo_co_jp, "jp-b44-kaiyodo-co-jp",
     "Kaiyodo", "海洋堂",
     "social", "media",
     "https://www.kaiyodo.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Kaiyodo\",\"otaku\",\"figures\"]",
     3600,
     "Kaiyodo (海洋堂) — figure")

RSSX(jp_b44_kinmaweb_jp, "jp-b44-kinmaweb-jp",
     "Kinma Web", "近代麻雀Web",
     "social", "media",
     "https://kinmaweb.jp/feed/",
     "ja", "[\"news\",\"publisher:Kinma Web\",\"gaming\",\"mahjong\"]",
     3600,
     "Kinma Web (近代麻雀Web) — mahjong")

RSSX(jp_b44_kotaku_com, "jp-b44-kotaku-com",
     "Kotaku", "Kotaku",
     "social", "media",
     "https://kotaku.com/rss",
     "en", "[\"news\",\"publisher:Kotaku\",\"gaming\"]",
     3600,
     "Kotaku — gaming")

RSSX(jp_b44_otocoto_jp, "jp-b44-otocoto-jp",
     "Otocoto", "Otocoto",
     "social", "media",
     "https://otocoto.jp/feed/",
     "ja", "[\"news\",\"publisher:Otocoto\",\"music\"]",
     3600,
     "Otocoto — music")

RSSX(jp_b44_qetic_jp, "jp-b44-qetic-jp",
     "Qetic", "Qetic",
     "social", "media",
     "https://www.qetic.jp/feed/",
     "ja", "[\"news\",\"publisher:Qetic\",\"music\"]",
     3600,
     "Qetic — music")

RSSX(jp_b44_siliconera_com, "jp-b44-siliconera-com",
     "Siliconera", "Siliconera",
     "social", "media",
     "https://www.siliconera.com/feed/",
     "en", "[\"news\",\"publisher:Siliconera\",\"gaming\"]",
     3600,
     "Siliconera — gaming")

RSSX(jp_b44_tokyotosho_info, "jp-b44-tokyotosho-info",
     "Tokyo Toshokan", "東京図書館",
     "social", "media",
     "https://tokyotosho.info/rss.php",
     "en", "[\"news\",\"publisher:Tokyo Toshokan\",\"anime\"]",
     3600,
     "Tokyo Toshokan (東京図書館) — anime")

/* ── Creative / Art / Photo ────────────────────────────────────── */

RSSX(jp_b44_100life_jp, "jp-b44-100life-jp",
     "100%LiFE", "100%LiFE",
     "social", "media",
     "https://100life.jp/feed/",
     "ja", "[\"news\",\"publisher:100%LiFE\",\"architecture\"]",
     3600,
     "100%LiFE — architecture")

RSSX(jp_b44_archdaily_com, "jp-b44-archdaily-com",
     "ArchDaily", "ArchDaily",
     "social", "media",
     "https://www.archdaily.com/feed",
     "en", "[\"news\",\"publisher:ArchDaily\",\"architecture\"]",
     3600,
     "ArchDaily — architecture")

RSSX(jp_b44_art_it_asia, "jp-b44-art-it-asia",
     "ART iT", "ART iT",
     "social", "media",
     "https://www.art-it.asia/feed/",
     "en", "[\"news\",\"publisher:ART iT\",\"art\"]",
     3600,
     "ART iT — art")

RSSX(jp_b44_camera_kb_com, "jp-b44-camera-kb-com",
     "Camera KB", "カメラKB",
     "social", "media",
     "https://camera-kb.com/feed/",
     "ja", "[\"news\",\"publisher:Camera KB\",\"photography\"]",
     3600,
     "Camera KB (カメラKB) — photo")

RSSX(jp_b44_daiki55_com, "jp-b44-daiki55-com",
     "Daiki Photography", "Daiki",
     "social", "media",
     "https://daiki55.com/feed/",
     "en", "[\"news\",\"publisher:Daiki Photography\",\"photography\"]",
     3600,
     "Daiki Photography (Daiki) — photo")

RSSX(jp_b44_dezeen_com, "jp-b44-dezeen-com",
     "Dezeen", "Dezeen",
     "social", "media",
     "https://www.dezeen.com/feed/",
     "en", "[\"news\",\"publisher:Dezeen\",\"architecture\"]",
     3600,
     "Dezeen — architecture")

RSSX(jp_b44_kenchiku_co_jp, "jp-b44-kenchiku-co-jp",
     "Kenchiku", "建築",
     "social", "media",
     "https://kenchiku.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Kenchiku\",\"architecture\"]",
     3600,
     "Kenchiku (建築) — architecture")

RSSX(jp_b44_reco_photo_com, "jp-b44-reco-photo-com",
     "Reco Photo", "おすすめフォト",
     "social", "media",
     "https://reco-photo.com/feed/",
     "ja", "[\"news\",\"publisher:Reco Photo\",\"photography\"]",
     3600,
     "Reco Photo (おすすめフォト) — photo")

RSSX(jp_b44_tecture_jp, "jp-b44-tecture-jp",
     "TECTURE MAG", "TECTURE MAG",
     "social", "media",
     "https://mag.tecture.jp/feed/",
     "ja", "[\"news\",\"publisher:TECTURE MAG\",\"architecture\"]",
     3600,
     "TECTURE MAG — architecture")

/* ── Sports / Martial Arts ─────────────────────────────────────── */

RSSX(jp_b44_aikidojournal_com, "jp-b44-aikidojournal-com",
     "Aikido Journal", "合気道ジャーナル",
     "social", "media",
     "https://aikidojournal.com/feed/",
     "en", "[\"news\",\"publisher:Aikido Journal\",\"sports\",\"martial-arts\"]",
     3600,
     "Aikido Journal (合気道ジャーナル) — martial arts")

RSSX(jp_b44_bicycleclub_jp, "jp-b44-bicycleclub-jp",
     "BiCYCLE CLUB", "バイシクルクラブ",
     "social", "media",
     "https://www.bicycleclub.jp/feed/",
     "ja", "[\"news\",\"publisher:BiCYCLE CLUB\",\"sports\"]",
     3600,
     "BiCYCLE CLUB (バイシクルクラブ) — sports")

RSSX(jp_b44_cyclesports_jp, "jp-b44-cyclesports-jp",
     "Cycle Sports", "サイクルスポーツ",
     "social", "media",
     "https://www.cyclesports.jp/feed/",
     "ja", "[\"news\",\"publisher:Cycle Sports\",\"sports\"]",
     3600,
     "Cycle Sports (サイクルスポーツ) — sports")

RSSX(jp_b44_kendoinfo_net, "jp-b44-kendoinfo-net",
     "Kenshi 24/7", "Kenshi 24/7",
     "social", "media",
     "https://kenshi247.net/blog/feed/",
     "en", "[\"news\",\"publisher:Kenshi 24/7\",\"sports\",\"martial-arts\"]",
     3600,
     "Kenshi 24/7 — martial arts")

RSSX(jp_b44_sherdog_com, "jp-b44-sherdog-com",
     "Sherdog", "Sherdog",
     "social", "media",
     "https://www.sherdog.com/rss/news.xml",
     "en", "[\"news\",\"publisher:Sherdog\",\"sports\",\"martial-arts\"]",
     3600,
     "Sherdog — martial arts")

/* ── Travel / Outdoor / Transport ──────────────────────────────── */

RSSX(jp_b44_be_pal_net, "jp-b44-be-pal-net",
     "BE-PAL", "BE-PAL",
     "social", "tourism",
     "https://www.bepal.net/feed/",
     "ja", "[\"news\",\"publisher:BE-PAL\",\"outdoor\"]",
     3600,
     "BE-PAL — outdoor")

RSSX(jp_b44_boutiquejapan_com, "jp-b44-boutiquejapan-com",
     "Boutique Japan", "Boutique Japan",
     "social", "tourism",
     "https://boutiquejapan.com/feed/",
     "en", "[\"news\",\"publisher:Boutique Japan\",\"travel\"]",
     3600,
     "Boutique Japan — travel")

RSSX(jp_b44_diver_online_com, "jp-b44-diver-online-com",
     "DIVER Online", "DIVER",
     "social", "tourism",
     "https://diver-online.com/feed/",
     "ja", "[\"news\",\"publisher:DIVER Online\",\"travel\",\"diving\"]",
     3600,
     "DIVER Online (DIVER) — diving")

RSSX(jp_b44_hyakkei_me, "jp-b44-hyakkei-me",
     "Hyakkei", "百景",
     "social", "tourism",
     "https://hyakkei.me/feed/",
     "ja", "[\"news\",\"publisher:Hyakkei\",\"outdoor\"]",
     3600,
     "Hyakkei (百景) — outdoor")

RSSX(jp_b44_insidejapantours_com, "jp-b44-insidejapantours-com",
     "InsideJapan", "InsideJapan",
     "social", "tourism",
     "https://www.insidejapantours.com/blog/feed/",
     "en", "[\"news\",\"publisher:InsideJapan\",\"travel\"]",
     3600,
     "InsideJapan — travel")

RSSX(jp_b44_jrailpass_com, "jp-b44-jrailpass-com",
     "JRailPass", "JRailPass",
     "social", "tourism",
     "https://www.jrailpass.com/blog/feed",
     "en", "[\"news\",\"publisher:JRailPass\",\"travel\"]",
     3600,
     "JRailPass — travel")

RSSX(jp_b44_kaijipress_com, "jp-b44-kaijipress-com",
     "Kaiji Press", "海事プレス",
     "social", "transport",
     "https://www.kaijipress.com/feed/",
     "ja", "[\"news\",\"publisher:Kaiji Press\",\"transport\",\"shipping\"]",
     3600,
     "Kaiji Press (海事プレス) — shipping")

RSSX(jp_b44_kyototourism_org, "jp-b44-kyototourism-org",
     "Kyoto Tourism", "京都観光",
     "social", "tourism",
     "https://kyototourism.org/en/feed/",
     "en", "[\"news\",\"publisher:Kyoto Tourism\",\"travel\"]",
     3600,
     "Kyoto Tourism (京都観光) — travel")

RSSX(jp_b44_marinediving_com, "jp-b44-marinediving-com",
     "Marine Diving", "マリンダイビング",
     "social", "tourism",
     "https://marinediving.com/feed/",
     "ja", "[\"news\",\"publisher:Marine Diving\",\"travel\",\"diving\"]",
     3600,
     "Marine Diving (マリンダイビング) — diving")

RSSX(jp_b44_rail_hobidas_com, "jp-b44-rail-hobidas-com",
     "Rail Hobidas", "鉄道ホビダス",
     "social", "transport",
     "https://rail.hobidas.com/feed/",
     "ja", "[\"news\",\"publisher:Rail Hobidas\",\"transport\",\"railway\"]",
     3600,
     "Rail Hobidas (鉄道ホビダス) — railway")

RSSX(jp_b44_snownavi_com, "jp-b44-snownavi-com",
     "Snow Navi", "スノーナビ",
     "social", "tourism",
     "https://snownavi.com/feed/",
     "ja", "[\"news\",\"publisher:Snow Navi\",\"travel\",\"ski\"]",
     3600,
     "Snow Navi (スノーナビ) — ski")

RSSX(jp_b44_steep_jp, "jp-b44-steep-jp",
     "STEEP", "STEEP",
     "social", "tourism",
     "https://steep.jp/feed/",
     "ja", "[\"news\",\"publisher:STEEP\",\"travel\",\"ski\"]",
     3600,
     "STEEP — ski")

RSSX(jp_b44_surfnews_jp, "jp-b44-surfnews-jp",
     "Surf News", "サーフニュース",
     "social", "tourism",
     "https://surfnews.jp/feed/",
     "ja", "[\"news\",\"publisher:Surf News\",\"outdoor\",\"surfing\"]",
     3600,
     "Surf News (サーフニュース) — surfing")

RSSX(jp_b44_thenomadicpeople_com, "jp-b44-thenomadicpeople-com",
     "The Nomadic People", "ノマディック",
     "social", "tourism",
     "https://thenomadicpeople.com/feed/",
     "en", "[\"news\",\"publisher:The Nomadic People\",\"travel\"]",
     3600,
     "The Nomadic People (ノマディック) — travel")

RSSX(jp_b44_traveltoku_com, "jp-b44-traveltoku-com",
     "Travel Toku", "旅とく",
     "social", "tourism",
     "https://traveltoku.com/feed/",
     "ja", "[\"news\",\"publisher:Travel Toku\",\"travel\"]",
     3600,
     "Travel Toku (旅とく) — travel")

RSSX(jp_b44_yamahack_com, "jp-b44-yamahack-com",
     "YAMA HACK", "ヤマハック",
     "social", "tourism",
     "https://yamahack.com/feed",
     "ja", "[\"news\",\"publisher:YAMA HACK\",\"outdoor\"]",
     3600,
     "YAMA HACK (ヤマハック) — outdoor")

RSSX(jp_b44_yutty_jp, "jp-b44-yutty-jp",
     "Yutty", "ゆっ旅",
     "social", "tourism",
     "https://yutty.jp/feed/",
     "ja", "[\"news\",\"publisher:Yutty\",\"travel\",\"onsen\"]",
     3600,
     "Yutty (ゆっ旅) — onsen")

/* ── Food / Drink ──────────────────────────────────────────────── */

RSSX(jp_b44_coffeemecca_jp, "jp-b44-coffeemecca-jp",
     "Coffee Mecca", "コーヒーメッカ",
     "social", "lifestyle",
     "https://coffeemecca.jp/feed/",
     "ja", "[\"news\",\"publisher:Coffee Mecca\",\"food\",\"coffee\"]",
     3600,
     "Coffee Mecca (コーヒーメッカ) — coffee")

RSSX(jp_b44_every_coffee_com, "jp-b44-every-coffee-com",
     "Every Coffee", "毎日コーヒー",
     "social", "lifestyle",
     "https://every-coffee.com/feed/",
     "ja", "[\"news\",\"publisher:Every Coffee\",\"food\",\"coffee\"]",
     3600,
     "Every Coffee (毎日コーヒー) — coffee")

RSSX(jp_b44_goodcoffee_me, "jp-b44-goodcoffee-me",
     "Good Coffee", "Good Coffee",
     "social", "lifestyle",
     "https://goodcoffee.me/feed/",
     "ja", "[\"news\",\"publisher:Good Coffee\",\"food\",\"coffee\"]",
     3600,
     "Good Coffee — coffee")

RSSX(jp_b44_jbja_jp, "jp-b44-jbja-jp",
     "Japan Beer Journalists", "日本ビアジャーナリスト",
     "social", "lifestyle",
     "https://jbja.jp/feed/",
     "ja", "[\"news\",\"publisher:Japan Beer Journalists\",\"food\",\"craft-beer\"]",
     3600,
     "Japan Beer Journalists (日本ビアジャーナリスト) — craft beer")

RSSX(jp_b44_macrobiotic_daisuki_jp, "jp-b44-macrobiotic-daisuki-jp",
     "Macrobiotic", "マクロビ",
     "social", "lifestyle",
     "https://macrobiotic-daisuki.jp/feed/",
     "ja", "[\"news\",\"publisher:Macrobiotic\",\"food\"]",
     3600,
     "Macrobiotic (マクロビ) — food")

RSSX(jp_b44_matcha_jp_com, "jp-b44-matcha-jp-com",
     "MATCHA (JA)", "MATCHA日本語",
     "social", "lifestyle",
     "https://matcha-jp.com/jp/feed",
     "ja", "[\"news\",\"publisher:MATCHA (JA)\",\"food\",\"tea\"]",
     3600,
     "MATCHA (JA) (MATCHA日本語) — tea")

RSSX(jp_b44_oisixradaichi_co_jp, "jp-b44-oisixradaichi-co-jp",
     "Oisix", "Oisix",
     "social", "lifestyle",
     "https://www.oisixradaichi.co.jp/feed",
     "ja", "[\"news\",\"publisher:Oisix\",\"food\"]",
     3600,
     "Oisix — food")

RSSX(jp_b44_wagashi_or_jp, "jp-b44-wagashi-or-jp",
     "Wagashi Association", "全国和菓子協会",
     "social", "lifestyle",
     "https://www.wagashi.or.jp/feed",
     "ja", "[\"news\",\"publisher:Wagashi Association\",\"food\",\"wagashi\"]",
     3600,
     "Wagashi Association (全国和菓子協会) — wagashi")

RSSX(jp_b44_whiskymag_jp, "jp-b44-whiskymag-jp",
     "Whisky Magazine JP", "ウイスキーマガジン",
     "social", "lifestyle",
     "https://whiskymag.jp/feed/",
     "ja", "[\"news\",\"publisher:Whisky Magazine JP\",\"food\",\"whisky\"]",
     3600,
     "Whisky Magazine JP (ウイスキーマガジン) — whisky")

RSSX(jp_b44_winenavi_com, "jp-b44-winenavi-com",
     "Wine Navi", "ワインナビ",
     "social", "lifestyle",
     "https://www.winenavi.com/feed/",
     "ja", "[\"news\",\"publisher:Wine Navi\",\"food\",\"wine\"]",
     3600,
     "Wine Navi (ワインナビ) — wine")

/* ── Lifestyle / Fashion ───────────────────────────────────────── */

RSSX(jp_b44_apparel_web_com, "jp-b44-apparel-web-com",
     "Apparel Web", "アパレルウェブ",
     "social", "lifestyle",
     "https://apparel-web.com/feed/",
     "ja", "[\"news\",\"publisher:Apparel Web\",\"fashion\",\"business\"]",
     3600,
     "Apparel Web (アパレルウェブ) — fashion biz")

RSSX(jp_b44_danro_bar, "jp-b44-danro-bar",
     "DANRO", "DANRO",
     "social", "lifestyle",
     "https://danro.bar/feed/",
     "ja", "[\"news\",\"publisher:DANRO\",\"lifestyle\"]",
     3600,
     "DANRO — lifestyle solo")

RSSX(jp_b44_fumihiro1192_hatenablog_com, "jp-b44-fumihiro1192-hatenablog-com",
     "Stationery Evaluation", "文房具評価ブログ",
     "social", "lifestyle",
     "https://fumihiro1192.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:Stationery Evaluation\",\"stationery\"]",
     3600,
     "Stationery Evaluation (文房具評価ブログ) — stationery")

RSSX(jp_b44_hanako_tokyo, "jp-b44-hanako-tokyo",
     "Hanako", "Hanako",
     "social", "lifestyle",
     "https://hanako.tokyo/feed/",
     "ja", "[\"news\",\"publisher:Hanako\",\"lifestyle\"]",
     3600,
     "Hanako — lifestyle")

RSSX(jp_b44_mens_ex_jp, "jp-b44-mens-ex-jp",
     "MEN'S EX", "MEN'S EX",
     "social", "lifestyle",
     "https://www.mens-ex.jp/feed/",
     "ja", "[\"news\",\"publisher:MEN'S EX\",\"fashion\"]",
     3600,
     "MEN'S EX — fashion")

RSSX(jp_b44_nekogazou_com, "jp-b44-nekogazou-com",
     "Neko Gazou", "ねこ画像",
     "social", "lifestyle",
     "https://nekogazou.com/feed/",
     "ja", "[\"news\",\"publisher:Neko Gazou\",\"pets\"]",
     3600,
     "Neko Gazou (ねこ画像) — pet")

RSSX(jp_b44_nekonavi_jp, "jp-b44-nekonavi-jp",
     "Neko Navi", "猫ナビ",
     "social", "lifestyle",
     "https://nekonavi.jp/feed/",
     "ja", "[\"news\",\"publisher:Neko Navi\",\"pets\"]",
     3600,
     "Neko Navi (猫ナビ) — pet")

RSSX(jp_b44_otonasalone_jp, "jp-b44-otonasalone-jp",
     "Otona Salone", "OTONA SALONE",
     "social", "lifestyle",
     "https://otonasalone.jp/feed/",
     "ja", "[\"news\",\"publisher:Otona Salone\",\"lifestyle\"]",
     3600,
     "Otona Salone (OTONA SALONE) — lifestyle")

RSSX(jp_b44_ovo_kyodo_co_jp, "jp-b44-ovo-kyodo-co-jp",
     "OVO", "OVO（オーヴォ）",
     "social", "lifestyle",
     "https://ovo.kyodo.co.jp/feed/",
     "ja", "[\"news\",\"publisher:OVO\",\"lifestyle\",\"news\"]",
     3600,
     "OVO (OVO（オーヴォ）) — news lifestyle")

RSSX(jp_b44_seikatsu110_jp, "jp-b44-seikatsu110-jp",
     "Seikatsu110", "生活110番",
     "social", "lifestyle",
     "https://www.seikatsu110.jp/feed/",
     "ja", "[\"news\",\"publisher:Seikatsu110\",\"lifestyle\"]",
     3600,
     "Seikatsu110 (生活110番) — lifestyle service")

RSSX(jp_b44_watchnavi_com, "jp-b44-watchnavi-com",
     "WATCHNAVI", "ウオッチナビ",
     "social", "lifestyle",
     "https://www.watchnavi.com/feed/",
     "ja", "[\"news\",\"publisher:WATCHNAVI\",\"lifestyle\",\"watches\"]",
     3600,
     "WATCHNAVI (ウオッチナビ) — watches")

RSSX(jp_b44_webchronos_net, "jp-b44-webchronos-net",
     "Web Chronos", "クロノス日本版",
     "social", "lifestyle",
     "https://www.webchronos.net/feed/",
     "ja", "[\"news\",\"publisher:Web Chronos\",\"lifestyle\",\"watches\"]",
     3600,
     "Web Chronos (クロノス日本版) — watches")

/* ── Parenting / Education ─────────────────────────────────────── */

RSSX(jp_b44_cotoacademy_com, "jp-b44-cotoacademy-com",
     "Coto Academy", "コトアカデミー",
     "social", "education",
     "https://cotoacademy.com/feed/",
     "en", "[\"news\",\"publisher:Coto Academy\",\"education\",\"japanese\"]",
     3600,
     "Coto Academy (コトアカデミー) — jpn learn")

RSSX(jp_b44_ict_enews_net, "jp-b44-ict-enews-net",
     "ICT教育ニュース", "ICT教育ニュース",
     "social", "education",
     "https://ict-enews.net/feed/",
     "ja", "[\"news\",\"publisher:ICT教育ニュース\",\"education\"]",
     3600,
     "ICT教育ニュース — edu")

/* ── Health / Pharma ───────────────────────────────────────────── */

RSSX(jp_b44_cocokara_next_com, "jp-b44-cocokara-next-com",
     "CocoKara Next", "ココカラネクスト",
     "social", "health",
     "https://cocokara-next.com/feed/",
     "ja", "[\"news\",\"publisher:CocoKara Next\",\"health\"]",
     3600,
     "CocoKara Next (ココカラネクスト) — health deep")

RSSX(jp_b44_joint_kaigo_com, "jp-b44-joint-kaigo-com",
     "JOINT Kaigo", "JOINT介護",
     "social", "health",
     "https://joint-kaigo.com/feed/",
     "ja", "[\"news\",\"publisher:JOINT Kaigo\",\"health\",\"elderly\"]",
     3600,
     "JOINT Kaigo (JOINT介護) — elderly care")

RSSX(jp_b44_qlifepro_com, "jp-b44-qlifepro-com",
     "QLifePro", "QLifePro",
     "social", "health",
     "https://www.qlifepro.com/feed/",
     "ja", "[\"news\",\"publisher:QLifePro\",\"health\",\"pharma\"]",
     3600,
     "QLifePro — pharma")

RSSX(jp_b44_yakuzaic_com, "jp-b44-yakuzaic-com",
     "Yakuzaic", "ヤクザイク",
     "social", "health",
     "https://yakuzaic.com/feed/",
     "ja", "[\"news\",\"publisher:Yakuzaic\",\"health\",\"pharma\"]",
     3600,
     "Yakuzaic (ヤクザイク) — pharmacy")

/* ── Business / Finance ────────────────────────────────────────── */

RSSX(jp_b44_biz_moneyforward_com, "jp-b44-biz-moneyforward-com",
     "MF Biz Blog", "マネーフォワード",
     "social", "business",
     "https://biz.moneyforward.com/blog/feed/",
     "ja", "[\"news\",\"publisher:MF Biz Blog\",\"business\",\"accounting\"]",
     3600,
     "MF Biz Blog (マネーフォワード) — accounting")

RSSX(jp_b44_fastgrow_jp, "jp-b44-fastgrow-jp",
     "FastGrow", "FastGrow",
     "social", "business",
     "https://www.fastgrow.jp/feed",
     "ja", "[\"news\",\"publisher:FastGrow\",\"business\",\"startup\"]",
     3600,
     "FastGrow — biz startup")

RSSX(jp_b44_ieul_jp, "jp-b44-ieul-jp",
     "ieul", "イエウール",
     "social", "business",
     "https://ieul.jp/column/feed/",
     "ja", "[\"news\",\"publisher:ieul\",\"realestate\"]",
     3600,
     "ieul (イエウール) — realestate deep")

RSSX(jp_b44_kensetsu_kikin_or_jp, "jp-b44-kensetsu-kikin-or-jp",
     "Kensetsu Kikin", "建設業振興基金",
     "social", "business",
     "https://www.kensetsu-kikin.or.jp/feed/",
     "ja", "[\"news\",\"publisher:Kensetsu Kikin\",\"business\",\"construction\"]",
     3600,
     "Kensetsu Kikin (建設業振興基金) — construction")

RSSX(jp_b44_roumu_com, "jp-b44-roumu-com",
     "Roumu.com", "労務.com",
     "social", "business",
     "https://www.roumu.com/feed/",
     "ja", "[\"news\",\"publisher:Roumu.com\",\"business\",\"labor\"]",
     3600,
     "Roumu.com (労務.com) — labor")

RSSX(jp_b44_tokkyo_lab_com, "jp-b44-tokkyo-lab-com",
     "Tokkyo Lab", "特許ラボ",
     "social", "business",
     "https://tokkyo-lab.com/feed/",
     "ja", "[\"news\",\"publisher:Tokkyo Lab\",\"business\",\"patent\"]",
     3600,
     "Tokkyo Lab (特許ラボ) — patent")

/* ── Agriculture / Fishing ─────────────────────────────────────── */

RSSX(jp_b44_lurenewsr_com, "jp-b44-lurenewsr-com",
     "Lure News R", "ルアーニュースR",
     "social", "agriculture",
     "https://www.lurenewsr.com/feed/",
     "ja", "[\"news\",\"publisher:Lure News R\",\"agriculture\",\"fishing\"]",
     3600,
     "Lure News R (ルアーニュースR) — fishing")

RSSX(jp_b44_organic_press_com, "jp-b44-organic-press-com",
     "Organic Press", "オーガニックプレス",
     "social", "agriculture",
     "https://organic-press.com/feed/",
     "ja", "[\"news\",\"publisher:Organic Press\",\"agriculture\"]",
     3600,
     "Organic Press (オーガニックプレス) — agriculture")

RSSX(jp_b44_smartagri_jp_com, "jp-b44-smartagri-jp-com",
     "Smart Agri", "スマート農業",
     "social", "agriculture",
     "https://smartagri-jp.com/feed",
     "ja", "[\"news\",\"publisher:Smart Agri\",\"agriculture\"]",
     3600,
     "Smart Agri (スマート農業) — agriculture")

RSSX(jp_b44_tsurihack_com, "jp-b44-tsurihack-com",
     "TSURI HACK", "TSURI HACK",
     "social", "agriculture",
     "https://tsurihack.com/feed",
     "ja", "[\"news\",\"publisher:TSURI HACK\",\"agriculture\",\"fishing\"]",
     3600,
     "TSURI HACK — fishing")

RSSX(jp_b44_tsurinews_jp, "jp-b44-tsurinews-jp",
     "Tsuri News", "TSURINEWS",
     "social", "agriculture",
     "https://tsurinews.jp/feed/",
     "ja", "[\"news\",\"publisher:Tsuri News\",\"agriculture\",\"fishing\"]",
     3600,
     "Tsuri News (TSURINEWS) — fishing")

/* ── Environment / Energy ──────────────────────────────────────── */

RSSX(jp_b44_denki_or_jp, "jp-b44-denki-or-jp",
     "Denki Shimbun", "電気新聞",
     "social", "environment",
     "https://www.denki.or.jp/feed/",
     "ja", "[\"news\",\"publisher:Denki Shimbun\",\"energy\"]",
     3600,
     "Denki Shimbun (電気新聞) — energy")

RSSX(jp_b44_ecotopia_earth, "jp-b44-ecotopia-earth",
     "Ecotopia", "エコトピア",
     "social", "environment",
     "https://ecotopia.earth/feed/",
     "ja", "[\"news\",\"publisher:Ecotopia\",\"environment\"]",
     3600,
     "Ecotopia (エコトピア) — environment")

RSSX(jp_b44_ene_fro_com, "jp-b44-ene-fro-com",
     "Ene-fro", "エネフロ",
     "social", "environment",
     "https://ene-fro.com/feed/",
     "ja", "[\"news\",\"publisher:Ene-fro\",\"energy\",\"nuclear\"]",
     3600,
     "Ene-fro (エネフロ) — nuclear")

RSSX(jp_b44_solar_journal_jp, "jp-b44-solar-journal-jp",
     "Solar Journal", "ソーラージャーナル",
     "social", "environment",
     "https://solarjournal.jp/feed/",
     "ja", "[\"news\",\"publisher:Solar Journal\",\"energy\",\"renewable\"]",
     3600,
     "Solar Journal (ソーラージャーナル) — renewable")

RSSX(jp_b44_wildbird_org, "jp-b44-wildbird-org",
     "Wild Bird Society", "日本野鳥の会",
     "social", "environment",
     "https://www.wbsj.org/feed/",
     "ja", "[\"news\",\"publisher:Wild Bird Society\",\"environment\",\"nature\"]",
     3600,
     "Wild Bird Society (日本野鳥の会) — nature")

/* ── Science / Research ────────────────────────────────────────── */

RSSX(jp_b44_jcer_or_jp, "jp-b44-jcer-or-jp",
     "JCER", "日本経済研究センター",
     "social", "science",
     "https://www.jcer.or.jp/feed/",
     "ja", "[\"news\",\"publisher:JCER\",\"research\"]",
     3600,
     "JCER (日本経済研究センター) — research")

RSSX(jp_b44_sci_news_co_jp, "jp-b44-sci-news-co-jp",
     "ScienceNews", "サイエンスニュース",
     "social", "science",
     "https://sci-news.co.jp/feed/",
     "ja", "[\"news\",\"publisher:ScienceNews\",\"science\"]",
     3600,
     "ScienceNews (サイエンスニュース) — science")

RSSX(jp_b44_sipstudio_com, "jp-b44-sipstudio-com",
     "SIP Studio", "SIPスタジオ",
     "social", "science",
     "https://sipstudio.com/feed/",
     "ja", "[\"news\",\"publisher:SIP Studio\",\"research\"]",
     3600,
     "SIP Studio (SIPスタジオ) — research")

RSSX(jp_b44_synodos_jp, "jp-b44-synodos-jp",
     "SYNODOS", "シノドス",
     "social", "science",
     "https://synodos.jp/feed/",
     "ja", "[\"news\",\"publisher:SYNODOS\",\"research\",\"thinktank\"]",
     3600,
     "SYNODOS (シノドス) — thinktank")

/* ── English-language Japan ────────────────────────────────────── */

RSSX(jp_b44_fukuoka_now_com, "jp-b44-fukuoka-now-com",
     "Fukuoka Now", "フクオカ・ナウ",
     "social", "media",
     "https://www.fukuoka-now.com/feed/",
     "en", "[\"news\",\"publisher:Fukuoka Now\",\"regional\",\"english\"]",
     3600,
     "Fukuoka Now (フクオカ・ナウ) — local en")

RSSX(jp_b44_guidable_co, "jp-b44-guidable-co",
     "Guidable", "Guidable",
     "social", "media",
     "https://guidable.co/feed/",
     "en", "[\"news\",\"publisher:Guidable\",\"japan\",\"english\",\"expat\"]",
     3600,
     "Guidable — en expat")

RSSX(jp_b44_hikinginjapan_com, "jp-b44-hikinginjapan-com",
     "Hiking in Japan", "Hiking in Japan",
     "social", "tourism",
     "https://hikinginjapan.com/feed/",
     "en", "[\"news\",\"publisher:Hiking in Japan\",\"outdoor\",\"english\"]",
     3600,
     "Hiking in Japan — en outdoor")

RSSX(jp_b44_japancheapo_com, "jp-b44-japancheapo-com",
     "Japan Cheapo", "Japan Cheapo",
     "social", "tourism",
     "https://japancheapo.com/feed/",
     "en", "[\"news\",\"publisher:Japan Cheapo\",\"travel\",\"english\"]",
     3600,
     "Japan Cheapo — en travel")

RSSX(jp_b44_japanesecooking101_com, "jp-b44-japanesecooking101-com",
     "Japanese Cooking 101", "Japanese Cooking 101",
     "social", "lifestyle",
     "https://www.japanesecooking101.com/feed/",
     "en", "[\"news\",\"publisher:Japanese Cooking 101\",\"food\",\"english\"]",
     3600,
     "Japanese Cooking 101 — en food")

RSSX(jp_b44_japanesemythology_jp, "jp-b44-japanesemythology-jp",
     "Japanese Mythology", "Japanese Mythology",
     "social", "media",
     "https://japanesemythology.jp/feed/",
     "en", "[\"news\",\"publisher:Japanese Mythology\",\"culture\",\"english\",\"mythology\"]",
     3600,
     "Japanese Mythology — en mythology")

RSSX(jp_b44_japanpowered_com, "jp-b44-japanpowered-com",
     "Japan Powered", "Japan Powered",
     "social", "media",
     "https://www.japanpowered.com/feed",
     "en", "[\"news\",\"publisher:Japan Powered\",\"culture\",\"english\"]",
     3600,
     "Japan Powered — en culture")

RSSX(jp_b44_kanazawastation_com, "jp-b44-kanazawastation-com",
     "Kanazawa Station", "Kanazawa Station",
     "social", "tourism",
     "https://kanazawastation.com/feed/",
     "en", "[\"news\",\"publisher:Kanazawa Station\",\"travel\",\"english\",\"kanazawa\"]",
     3600,
     "Kanazawa Station — en kanazawa")

RSSX(jp_b44_nbr_org, "jp-b44-nbr-org",
     "National Bureau of Asian Research", "NBR",
     "social", "media",
     "https://www.nbr.org/feed/",
     "en", "[\"news\",\"publisher:National Bureau of Asian Research\",\"japan\",\"english\",\"analysis\"]",
     3600,
     "National Bureau of Asian Research (NBR) — en analysis")

RSSX(jp_b44_tokyoreview_net, "jp-b44-tokyoreview-net",
     "Tokyo Review", "Tokyo Review",
     "social", "media",
     "https://tokyoreview.net/feed/",
     "en", "[\"news\",\"publisher:Tokyo Review\",\"japan\",\"english\",\"analysis\"]",
     3600,
     "Tokyo Review — en analysis")

RSSX(jp_b44_voyapon_com, "jp-b44-voyapon-com",
     "Voyapon", "Voyapon",
     "social", "tourism",
     "https://voyapon.com/feed/",
     "en", "[\"news\",\"publisher:Voyapon\",\"travel\",\"english\"]",
     3600,
     "Voyapon — en travel")

/* ── Blog / Personal ───────────────────────────────────────────── */

RSSX(jp_b44_blog_sushi_money, "jp-b44-blog-sushi-money",
     "sushi.money", "寿司.money",
     "social", "media",
     "https://blog.sushi.money/feed",
     "ja", "[\"news\",\"publisher:sushi.money\",\"blog\"]",
     3600,
     "sushi.money (寿司.money) — blog jp")

RSSX(jp_b44_fujipon_hatenablog_com, "jp-b44-fujipon-hatenablog-com",
     "fujipon", "fujipon",
     "social", "media",
     "https://fujipon.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:fujipon\",\"blog\"]",
     3600,
     "fujipon — blog jp")

RSSX(jp_b44_jkondo_hatenablog_com, "jp-b44-jkondo-hatenablog-com",
     "jkondo", "近藤淳也",
     "social", "media",
     "https://jkondo.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:jkondo\",\"blog\"]",
     3600,
     "jkondo (近藤淳也) — blog jp")

RSSX(jp_b44_orangestar_hatenadiary_jp, "jp-b44-orangestar-hatenadiary-jp",
     "orangestar", "orangestar",
     "social", "media",
     "https://orangestar.hatenadiary.jp/feed",
     "ja", "[\"news\",\"publisher:orangestar\",\"blog\"]",
     3600,
     "orangestar — blog jp")

/* ── Local / Regional ──────────────────────────────────────────── */

RSSX(jp_b44_kamakura_guide_jp, "jp-b44-kamakura-guide-jp",
     "Kamakura Guide", "鎌倉ガイド",
     "social", "media",
     "https://kamakura-guide.jp/feed",
     "ja", "[\"news\",\"publisher:Kamakura Guide\",\"regional\",\"kamakura\"]",
     3600,
     "Kamakura Guide (鎌倉ガイド) — local kamakura")

RSSX(jp_b44_myfuna_net, "jp-b44-myfuna-net",
     "MyFuna", "マイファナ",
     "social", "media",
     "https://myfuna.net/feed/",
     "ja", "[\"news\",\"publisher:MyFuna\",\"regional\",\"funabashi\"]",
     3600,
     "MyFuna (マイファナ) — local funabashi")
