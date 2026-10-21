/* collectors/sources/jp_feeds_batch42.c
 * 100 new Japan-focused RSS/Atom feeds, all proven live (HTTP 200) and
 * verified absent from the existing tree on 2026-10-17.
 *
 * Categories covered: technology, tech blogs, news/media, gaming, cars,
 * entertainment, sports, lifestyle, fashion/design, science/academic,
 * English-language Japan media, career, and events.
 *
 * Every URL was tested with curl and returned a valid RSS/Atom body.
 * Uses the RSSX macro from _source_macros.inc for concise registration. */
#include "_source_macros.inc"

/* ── Technology / IT ────────────────────────────────────────────────── */

RSSX(jp_gihyo, "jp-gihyo",
     "gihyo.jp (WEB+DB PRESS)", "gihyo.jp（技術評論社）",
     "jp_webmedia", "technology",
     "https://gihyo.jp/feed/rss2",
     "ja", "[\"news\",\"publisher:gihyo\",\"tech\",\"development\"]",
     3600,
     "gihyo.jp — developer articles, WEB+DB PRESS online")

RSSX(jp_markezine, "jp-markezine",
     "MarkeZine", "MarkeZine",
     "jp_webmedia", "technology",
     "https://markezine.jp/rss/new/20/index.xml",
     "ja", "[\"news\",\"publisher:MarkeZine\",\"marketing\",\"digital\"]",
     3600,
     "MarkeZine — digital marketing news and strategy")

RSSX(jp_edtechzine, "jp-edtechzine",
     "EdTechZine", "EdTechZine",
     "jp_webmedia", "technology",
     "https://edtechzine.jp/rss/new/20/index.xml",
     "ja", "[\"news\",\"publisher:EdTechZine\",\"edtech\",\"education\"]",
     3600,
     "EdTechZine — education technology news and tools")

RSSX(jp_smhn, "jp-smhn",
     "SMHN (Smartphone Headlines)", "すまほん!!",
     "jp_webmedia", "technology",
     "https://smhn.info/feed",
     "ja", "[\"news\",\"publisher:SMHN\",\"tech\",\"mobile\"]",
     3600,
     "SMHN — smartphone, mobile and telecom news")

RSSX(jp_newsphere, "jp-newsphere",
     "NewSphere", "NewSphere",
     "jp_webmedia", "media",
     "https://newsphere.jp/feed/",
     "ja", "[\"news\",\"publisher:NewSphere\",\"international\"]",
     3600,
     "NewSphere — international news from a Japanese perspective")

RSSX(jp_new_economy, "jp-new-economy",
     "Atarashii Keizai", "あたらしい経済",
     "jp_webmedia", "technology",
     "https://www.neweconomy.jp/feed",
     "ja", "[\"news\",\"publisher:Atarashii Keizai\",\"blockchain\",\"web3\"]",
     3600,
     "Atarashii Keizai — blockchain, web3 and crypto-asset news")

RSSX(jp_ainow, "jp-ainow",
     "AI Now", "AI Now",
     "jp_webmedia", "technology",
     "https://ainow.ai/feed/",
     "ja", "[\"news\",\"publisher:AI Now\",\"tech\",\"ai\"]",
     3600,
     "AI Now — artificial intelligence industry news and use cases")

RSSX(jp_moguravr, "jp-moguravr",
     "Mogura VR", "Mogura VR",
     "jp_webmedia", "technology",
     "https://www.moguravr.com/feed/",
     "ja", "[\"news\",\"publisher:Mogura VR\",\"tech\",\"vr\",\"metaverse\"]",
     3600,
     "Mogura VR — VR, AR, metaverse and spatial computing news")

RSSX(jp_dglab, "jp-dglab",
     "DG Lab Haus", "DG Lab Haus",
     "jp_webmedia", "technology",
     "https://dg-lab.com/feed/",
     "ja", "[\"news\",\"publisher:DG Lab\",\"tech\",\"innovation\"]",
     7200,
     "DG Lab Haus — deep tech, AI and fintech innovation")

RSSX(jp_rbbtoday, "jp-rbbtoday",
     "RBB TODAY", "RBB TODAY",
     "jp_webmedia", "technology",
     "https://www.rbbtoday.com/rss/index.rdf",
     "ja", "[\"news\",\"publisher:RBB TODAY\",\"tech\",\"broadband\"]",
     3600,
     "RBB TODAY — broadband, mobile and IT infrastructure news")

RSSX(jp_digiday, "jp-digiday",
     "DIGIDAY Japan", "DIGIDAY[日本版]",
     "jp_webmedia", "technology",
     "https://digiday.jp/feed/",
     "ja", "[\"news\",\"publisher:DIGIDAY\",\"marketing\",\"media\"]",
     3600,
     "DIGIDAY Japan — digital media, marketing and advertising industry")

RSSX(jp_xtrend, "jp-xtrend",
     "Nikkei xTREND", "日経クロストレンド",
     "jp_webmedia", "technology",
     "https://xtrend.nikkei.com/rss/index.rdf",
     "ja", "[\"news\",\"publisher:Nikkei xTREND\",\"marketing\",\"tech\"]",
     3600,
     "Nikkei xTREND — marketing, consumer trends and technology")

RSSX(jp_creator_economy, "jp-creator-economy",
     "Creator Economy Lab", "クリエイターエコノミーラボ",
     "jp_webmedia", "technology",
     "https://creatoreconomy.jp/feed",
     "ja", "[\"news\",\"publisher:Creator Economy Lab\",\"creator\",\"platform\"]",
     7200,
     "Creator Economy Lab — creator platforms and monetization")

/* ── ITmedia sub-brand ──────────────────────────────────────────────── */

RSSX(jp_netlab, "jp-netlab",
     "Netolab", "ねとらぼ",
     "jp_webmedia", "media",
     "https://rss.itmedia.co.jp/rss/2.0/netlab.xml",
     "ja", "[\"news\",\"publisher:Netolab\",\"internet\",\"culture\"]",
     1800,
     "Netolab (ITmedia) — internet culture, viral and social trends")

/* ── Tech Blogs ─────────────────────────────────────────────────────── */

RSSX(jp_mercari_eng, "jp-mercari-eng",
     "Mercari Engineering", "Mercari Engineering Blog",
     "jp_webmedia", "technology",
     "https://engineering.mercari.com/blog/feed.xml",
     "en", "[\"tech\",\"publisher:Mercari\",\"engineering\",\"blog\"]",
     7200,
     "Mercari Engineering — tech blog from Japan's largest marketplace")

RSSX(jp_google_dev, "jp-google-dev",
     "Google Developers Japan", "Google Developers Japan",
     "jp_webmedia", "technology",
     "https://developers-jp.googleblog.com/atom.xml",
     "ja", "[\"tech\",\"publisher:Google\",\"development\",\"blog\"]",
     7200,
     "Google Developers Japan — official Japanese developer blog")

RSSX(jp_smarthr_tech, "jp-smarthr-tech",
     "SmartHR Tech Blog", "SmartHR Tech Blog",
     "jp_webmedia", "technology",
     "https://tech.smarthr.jp/feed",
     "ja", "[\"tech\",\"publisher:SmartHR\",\"engineering\",\"blog\"]",
     7200,
     "SmartHR — HR SaaS engineering blog")

RSSX(jp_cookpad_tech, "jp-cookpad-tech",
     "Cookpad Techlife", "Cookpad 開発者ブログ",
     "jp_webmedia", "technology",
     "https://techlife.cookpad.com/feed",
     "ja", "[\"tech\",\"publisher:Cookpad\",\"engineering\",\"blog\"]",
     7200,
     "Cookpad — recipe platform engineering blog")

RSSX(jp_cyberagent_dev, "jp-cyberagent-dev",
     "CyberAgent Developers", "CyberAgent Developers Blog",
     "jp_webmedia", "technology",
     "https://developers.cyberagent.co.jp/blog/feed/",
     "ja", "[\"tech\",\"publisher:CyberAgent\",\"engineering\",\"blog\"]",
     7200,
     "CyberAgent — ad-tech and media engineering blog")

RSSX(jp_uzabase_tech, "jp-uzabase-tech",
     "Uzabase Tech", "Uzabase Tech Blog",
     "jp_webmedia", "technology",
     "https://tech.uzabase.com/feed",
     "ja", "[\"tech\",\"publisher:Uzabase\",\"engineering\",\"blog\"]",
     7200,
     "Uzabase (NewsPicks) — SaaS and media engineering blog")

RSSX(jp_cybozu_inside, "jp-cybozu-inside",
     "Cybozu Inside Out", "Cybozu Inside Out",
     "jp_webmedia", "technology",
     "https://blog.cybozu.io/feed",
     "ja", "[\"tech\",\"publisher:Cybozu\",\"engineering\",\"blog\"]",
     7200,
     "Cybozu — groupware and kintone engineering blog")

RSSX(jp_hatena_dev, "jp-hatena-dev",
     "Hatena Developer Blog", "Hatena Developer Blog",
     "jp_webmedia", "technology",
     "https://developer.hatenastaff.com/feed",
     "ja", "[\"tech\",\"publisher:Hatena\",\"engineering\",\"blog\"]",
     7200,
     "Hatena — bookmark/blog platform engineering blog")

RSSX(jp_sansan_builders, "jp-sansan-builders",
     "Sansan Builders Box", "Sansan Builders Box",
     "jp_webmedia", "technology",
     "https://buildersbox.corp-sansan.com/feed",
     "ja", "[\"tech\",\"publisher:Sansan\",\"engineering\",\"blog\"]",
     7200,
     "Sansan — business card and invoice SaaS engineering blog")

RSSX(jp_line_eng, "jp-line-eng",
     "LINE Engineering", "LINE Engineering",
     "jp_webmedia", "technology",
     "https://engineering.linecorp.com/ja/feed",
     "ja", "[\"tech\",\"publisher:LINE\",\"engineering\",\"blog\"]",
     7200,
     "LINE — messaging platform engineering blog")

RSSX(jp_qiita_zine, "jp-qiita-zine",
     "Qiita Zine", "Qiita Zine",
     "jp_webmedia", "technology",
     "https://zine.qiita.com/feed",
     "ja", "[\"tech\",\"publisher:Qiita\",\"development\",\"community\"]",
     7200,
     "Qiita Zine — developer community magazine by Qiita")

RSSX(jp_google_blog, "jp-google-blog",
     "Google Japan Blog", "Google Japan Blog",
     "jp_webmedia", "technology",
     "https://japan.googleblog.com/feeds/posts/default",
     "ja", "[\"tech\",\"publisher:Google\",\"blog\",\"japan\"]",
     7200,
     "Google Japan — official company blog")

RSSX(jp_azure_blog, "jp-azure-blog",
     "Azure Blog Japan", "Azure Blog 日本語",
     "jp_webmedia", "technology",
     "https://azure.microsoft.com/ja-jp/blog/feed/",
     "ja", "[\"tech\",\"publisher:Microsoft Azure\",\"cloud\",\"blog\"]",
     7200,
     "Microsoft Azure — Japanese cloud and AI blog")

/* ── News / Media ───────────────────────────────────────────────────── */

RSSX(jp_getnews, "jp-getnews",
     "GetNews (Getnews)", "ガジェット通信",
     "jp_webmedia", "media",
     "https://getnews.jp/rss",
     "ja", "[\"news\",\"publisher:GetNews\",\"internet\",\"gadgets\"]",
     1800,
     "GetNews — gadgets, internet culture and entertainment news")

RSSX(jp_sirabee, "jp-sirabee",
     "Sirabee", "しらべぇ",
     "jp_webmedia", "media",
     "https://sirabee.com/feed/",
     "ja", "[\"news\",\"publisher:Sirabee\",\"survey\",\"social\"]",
     3600,
     "Sirabee — survey-based social trends and lifestyle research")

RSSX(jp_agora, "jp-agora",
     "AGORA", "アゴラ",
     "jp_webmedia", "media",
     "https://agora-web.jp/feed",
     "ja", "[\"news\",\"publisher:AGORA\",\"opinion\",\"policy\"]",
     3600,
     "AGORA — opinion and policy commentary platform")

RSSX(jp_nippon_en, "jp-nippon-en",
     "Nippon.com (EN)", "nippon.com（英語）",
     "jp_webmedia", "media",
     "https://www.nippon.com/en/feed/",
     "en", "[\"news\",\"publisher:Nippon.com\",\"japan\",\"english\"]",
     3600,
     "Nippon.com — authoritative Japan coverage in English")

RSSX(jp_nippon_ja, "jp-nippon-ja",
     "Nippon.com (JA)", "nippon.com（日本語）",
     "jp_webmedia", "media",
     "https://www.nippon.com/ja/feed/",
     "ja", "[\"news\",\"publisher:Nippon.com\",\"japan\"]",
     3600,
     "Nippon.com — in-depth Japan coverage in Japanese")

RSSX(jp_mag2_news, "jp-mag2-news",
     "MAG2 NEWS", "まぐまぐニュース",
     "jp_webmedia", "media",
     "https://www.mag2.com/p/news/feed",
     "ja", "[\"news\",\"publisher:MAG2\",\"opinion\",\"society\"]",
     3600,
     "MAG2 NEWS — newsletter-derived news and opinion")

RSSX(jp_advertimes, "jp-advertimes",
     "AdverTimes", "AdverTimes（アドタイ）",
     "jp_webmedia", "media",
     "https://www.advertimes.com/feed/",
     "ja", "[\"news\",\"publisher:AdverTimes\",\"advertising\",\"media\"]",
     3600,
     "AdverTimes — advertising, PR and media industry news")

/* ── Gaming ──────────────────────────────────────────────────────────── */

RSSX(jp_gamespark, "jp-gamespark",
     "Game*Spark", "Game*Spark",
     "jp_webmedia", "media",
     "https://www.gamespark.jp/rss/index.rdf",
     "ja", "[\"news\",\"publisher:Game*Spark\",\"gaming\"]",
     3600,
     "Game*Spark — core gamer news, reviews and features")

RSSX(jp_inside_games, "jp-inside-games",
     "Inside Games", "インサイド",
     "jp_webmedia", "media",
     "https://www.inside-games.jp/rss/index.rdf",
     "ja", "[\"news\",\"publisher:Inside Games\",\"gaming\",\"nintendo\"]",
     3600,
     "Inside Games — Nintendo, console and mobile gaming news")

/* ── Car / Motorsport ───────────────────────────────────────────────── */

RSSX(jp_car_watch, "jp-car-watch",
     "Car Watch", "Car Watch",
     "jp_webmedia", "technology",
     "https://car.watch.impress.co.jp/data/rss/1.0/car/feed.rdf",
     "ja", "[\"news\",\"publisher:Car Watch\",\"automotive\"]",
     3600,
     "Car Watch (Impress) — automobile news, reviews and test drives")

RSSX(jp_webcartop, "jp-webcartop",
     "WEB CARTOP", "WEB CARTOP",
     "jp_webmedia", "media",
     "https://www.webcartop.jp/feed/",
     "ja", "[\"news\",\"publisher:WEB CARTOP\",\"automotive\"]",
     3600,
     "WEB CARTOP — car reviews, comparisons and automotive culture")

RSSX(jp_clicccar, "jp-clicccar",
     "clicccar", "clicccar",
     "jp_webmedia", "media",
     "https://clicccar.com/feed/",
     "ja", "[\"news\",\"publisher:clicccar\",\"automotive\"]",
     3600,
     "clicccar — new car info, tuning and car-related lifestyle")

RSSX(jp_automesseweb, "jp-automesseweb",
     "Auto Messe Web", "Auto Messe Web",
     "jp_webmedia", "media",
     "https://www.automesseweb.jp/feed",
     "ja", "[\"news\",\"publisher:Auto Messe Web\",\"automotive\",\"custom\"]",
     3600,
     "Auto Messe Web — auto shows, custom cars and car events")

RSSX(jp_as_web, "jp-as-web",
     "auto sport web", "auto sport web",
     "jp_webmedia", "media",
     "https://www.as-web.jp/feed",
     "ja", "[\"news\",\"publisher:auto sport web\",\"motorsport\",\"f1\"]",
     3600,
     "auto sport web — F1, Super GT and motorsport news")

RSSX(jp_supergt, "jp-supergt",
     "SUPER GT Official", "SUPER GT 公式",
     "jp_webmedia", "media",
     "https://supergt.net/feed",
     "ja", "[\"news\",\"publisher:SUPER GT\",\"motorsport\",\"racing\"]",
     7200,
     "SUPER GT — official Japanese touring car championship feed")

/* ── Entertainment / Culture ────────────────────────────────────────── */

RSSX(jp_cinra, "jp-cinra",
     "CINRA", "CINRA",
     "jp_webmedia", "media",
     "https://www.cinra.net/feed",
     "ja", "[\"news\",\"publisher:CINRA\",\"art\",\"culture\",\"music\"]",
     3600,
     "CINRA — art, music, film and culture magazine")

RSSX(jp_barks, "jp-barks",
     "BARKS", "BARKS",
     "jp_webmedia", "media",
     "https://www.barks.jp/feed",
     "ja", "[\"news\",\"publisher:BARKS\",\"music\"]",
     3600,
     "BARKS — Japanese and international music news and interviews")

RSSX(jp_animeanime, "jp-animeanime",
     "Anime!Anime!", "アニメ!アニメ!",
     "jp_webmedia", "media",
     "https://animeanime.jp/rss/index.rdf",
     "ja", "[\"news\",\"publisher:Anime!Anime!\",\"anime\"]",
     3600,
     "Anime!Anime! — anime industry news, rankings and features")

RSSX(jp_lisani, "jp-lisani",
     "LisAni!", "リスアニ!",
     "jp_webmedia", "media",
     "https://www.lisani.jp/feed/",
     "ja", "[\"news\",\"publisher:LisAni!\",\"anime\",\"music\"]",
     3600,
     "LisAni! — anime music, voice actor and live event news")

RSSX(jp_grapee, "jp-grapee",
     "grape", "grape",
     "jp_webmedia", "media",
     "https://grapee.jp/feed",
     "ja", "[\"news\",\"publisher:grape\",\"heartwarming\",\"society\"]",
     3600,
     "grape — heartwarming stories, life hacks and social topics")

RSSX(jp_otonanswer, "jp-otonanswer",
     "Otonano Answer", "オトナンサー",
     "jp_webmedia", "media",
     "https://otonanswer.jp/feed/",
     "ja", "[\"news\",\"publisher:Otonano Answer\",\"lifestyle\",\"qa\"]",
     3600,
     "Otonano Answer — adult life Q&A, manners and health")

RSSX(jp_encount, "jp-encount",
     "ENCOUNT", "ENCOUNT",
     "jp_webmedia", "media",
     "https://encount.press/feed/",
     "ja", "[\"news\",\"publisher:ENCOUNT\",\"entertainment\",\"sports\"]",
     3600,
     "ENCOUNT — entertainment, sports interviews and features")

RSSX(jp_thefilament, "jp-thefilament",
     "The Filament", "The Filament",
     "jp_webmedia", "media",
     "https://thefilament.jp/feed",
     "ja", "[\"news\",\"publisher:The Filament\",\"culture\",\"design\"]",
     7200,
     "The Filament — culture, design and creative industry")

RSSX(jp_realsound_tech, "jp-realsound-tech",
     "Real Sound Tech", "リアルサウンドテック",
     "jp_webmedia", "technology",
     "https://realsound.jp/tech/feed",
     "ja", "[\"news\",\"publisher:Real Sound\",\"tech\",\"gadgets\"]",
     3600,
     "Real Sound Tech — consumer tech, gadgets and digital culture")

RSSX(jp_natalie_eiga, "jp-natalie-eiga",
     "Natalie Eiga (Film)", "映画ナタリー",
     "jp_webmedia", "media",
     "https://natalie.mu/eiga/feed/news",
     "ja", "[\"news\",\"publisher:Natalie\",\"film\",\"movie\"]",
     3600,
     "Natalie — Japanese and international film news and reviews")

RSSX(jp_natalie_owarai, "jp-natalie-owarai",
     "Natalie Owarai (Comedy)", "お笑いナタリー",
     "jp_webmedia", "media",
     "https://natalie.mu/owarai/feed/news",
     "ja", "[\"news\",\"publisher:Natalie\",\"comedy\",\"entertainment\"]",
     3600,
     "Natalie — Japanese comedy, manzai and variety show news")

RSSX(jp_natalie_stage, "jp-natalie-stage",
     "Natalie Stage (Theater)", "ステージナタリー",
     "jp_webmedia", "media",
     "https://natalie.mu/stage/feed/news",
     "ja", "[\"news\",\"publisher:Natalie\",\"theater\",\"stage\"]",
     3600,
     "Natalie — theater, musical and stage performance news")

RSSX(jp_jimocoro, "jp-jimocoro",
     "Jimocoro", "ジモコロ",
     "jp_webmedia", "media",
     "https://www.e-aidem.com/ch/jimocoro/feed/",
     "ja", "[\"news\",\"publisher:Jimocoro\",\"local\",\"culture\"]",
     7200,
     "Jimocoro — local culture, craft and regional stories across Japan")

RSSX(jp_otakumode, "jp-otakumode",
     "Tokyo Otaku Mode", "Tokyo Otaku Mode",
     "jp_webmedia", "media",
     "https://otakumode.com/feed",
     "en", "[\"news\",\"publisher:Tokyo Otaku Mode\",\"otaku\",\"anime\"]",
     3600,
     "Tokyo Otaku Mode — anime, manga, cosplay and J-pop culture")

RSSX(jp_magcul, "jp-magcul",
     "MAGCUL", "マグカル",
     "jp_webmedia", "media",
     "https://magcul.net/feed",
     "ja", "[\"news\",\"publisher:MAGCUL\",\"art\",\"kanagawa\"]",
     7200,
     "MAGCUL — arts, culture and events in Kanagawa prefecture")

/* ── Sports ──────────────────────────────────────────────────────────── */

RSSX(jp_baseballking, "jp-baseballking",
     "Baseball King", "ベースボールキング",
     "jp_webmedia", "media",
     "https://baseballking.jp/feed/",
     "ja", "[\"news\",\"publisher:Baseball King\",\"sports\",\"baseball\"]",
     3600,
     "Baseball King — NPB and MLB baseball news and stats")

RSSX(jp_footballchannel, "jp-footballchannel",
     "Football Channel", "フットボールチャンネル",
     "jp_webmedia", "media",
     "https://www.footballchannel.jp/feed/",
     "ja", "[\"news\",\"publisher:Football Channel\",\"sports\",\"football\"]",
     3600,
     "Football Channel — J-League, European and world football news")

RSSX(jp_the_answer, "jp-the-answer",
     "THE ANSWER", "THE ANSWER",
     "jp_webmedia", "media",
     "https://the-ans.jp/feed/",
     "ja", "[\"news\",\"publisher:THE ANSWER\",\"sports\"]",
     3600,
     "THE ANSWER — multi-sport news, athlete interviews and analysis")

RSSX(jp_fullcount, "jp-fullcount",
     "Full-Count", "Full-Count",
     "jp_webmedia", "media",
     "https://full-count.jp/feed/",
     "ja", "[\"news\",\"publisher:Full-Count\",\"sports\",\"baseball\"]",
     3600,
     "Full-Count — in-depth baseball coverage, NPB and MLB")

RSSX(jp_tachiai, "jp-tachiai",
     "Tachiai (Sumo)", "Tachiai（相撲）",
     "jp_webmedia", "media",
     "https://tachiai.org/feed/",
     "en", "[\"news\",\"publisher:Tachiai\",\"sports\",\"sumo\"]",
     7200,
     "Tachiai — English-language sumo wrestling coverage and analysis")

RSSX(jp_sportingnews, "jp-sportingnews",
     "Sporting News Japan", "スポーティングニュース日本",
     "jp_webmedia", "media",
     "https://www.sportingnews.com/jp/rss",
     "ja", "[\"news\",\"publisher:Sporting News\",\"sports\"]",
     3600,
     "Sporting News Japan — multi-sport coverage in Japanese")

/* ── Lifestyle / Travel ─────────────────────────────────────────────── */

RSSX(jp_roomie, "jp-roomie",
     "ROOMIE", "ROOMIE",
     "jp_webmedia", "media",
     "https://www.roomie.jp/feed/",
     "ja", "[\"news\",\"publisher:ROOMIE\",\"lifestyle\",\"interior\"]",
     3600,
     "ROOMIE — home, interior and daily lifestyle ideas")

RSSX(jp_tabi_labo, "jp-tabi-labo",
     "TABI LABO", "TABI LABO",
     "jp_webmedia", "media",
     "https://tabi-labo.com/feed",
     "ja", "[\"news\",\"publisher:TABI LABO\",\"travel\",\"lifestyle\"]",
     3600,
     "TABI LABO — travel, lifestyle and global culture")

RSSX(jp_colocal, "jp-colocal",
     "colocal", "コロカル",
     "jp_webmedia", "media",
     "https://colocal.jp/feed",
     "ja", "[\"news\",\"publisher:colocal\",\"local\",\"community\"]",
     3600,
     "colocal — local living, community design and regional revitalization")

RSSX(jp_jisin, "jp-jisin",
     "JISIN (Josei Jishin)", "女性自身",
     "jp_webmedia", "media",
     "https://jisin.jp/feed/",
     "ja", "[\"news\",\"publisher:Josei Jishin\",\"entertainment\",\"lifestyle\"]",
     3600,
     "Josei Jishin — celebrity, entertainment and women's lifestyle")

RSSX(jp_getnavi, "jp-getnavi",
     "GetNavi web", "GetNavi web",
     "jp_webmedia", "media",
     "https://getnavi.jp/feed/",
     "ja", "[\"news\",\"publisher:GetNavi\",\"gadgets\",\"reviews\"]",
     3600,
     "GetNavi web — gadgets, home electronics and product reviews")

RSSX(jp_wwdjapan, "jp-wwdjapan",
     "WWD Japan", "WWD JAPAN",
     "jp_webmedia", "media",
     "https://www.wwdjapan.com/feed",
     "ja", "[\"news\",\"publisher:WWD Japan\",\"fashion\",\"business\"]",
     3600,
     "WWD Japan — fashion and beauty business news")

RSSX(jp_goodspress, "jp-goodspress",
     "GOODS PRESS", "GOODS PRESS",
     "jp_webmedia", "media",
     "https://www.goodspress.jp/feed/",
     "ja", "[\"news\",\"publisher:GOODS PRESS\",\"gadgets\",\"lifestyle\"]",
     3600,
     "GOODS PRESS — product reviews, gadgets and outdoor gear")

RSSX(jp_soredoko, "jp-soredoko",
     "SoreDoko (Rakuten)", "ソレドコ",
     "jp_webmedia", "media",
     "https://srdk.rakuten.jp/feed",
     "ja", "[\"news\",\"publisher:SoreDoko\",\"lifestyle\",\"recommendation\"]",
     7200,
     "SoreDoko — recommendations, hobbies and lifestyle by Rakuten")

/* ── Fashion / Design ───────────────────────────────────────────────── */

RSSX(jp_gqjapan, "jp-gqjapan",
     "GQ Japan", "GQ JAPAN",
     "jp_webmedia", "media",
     "https://www.gqjapan.jp/feed",
     "ja", "[\"news\",\"publisher:GQ Japan\",\"fashion\",\"lifestyle\"]",
     3600,
     "GQ Japan — men's fashion, culture and lifestyle magazine")

RSSX(jp_designboom, "jp-designboom",
     "Designboom", "Designboom",
     "jp_webmedia", "media",
     "https://www.designboom.com/feed/",
     "en", "[\"news\",\"publisher:Designboom\",\"design\",\"architecture\"]",
     3600,
     "Designboom — architecture, design, art and technology")

RSSX(jp_designstories, "jp-designstories",
     "Design Stories", "Design Stories",
     "jp_webmedia", "media",
     "https://www.designstoriesinc.com/feed/",
     "ja", "[\"news\",\"publisher:Design Stories\",\"design\",\"lifestyle\"]",
     7200,
     "Design Stories — Japanese design, architecture and craftsmanship")

RSSX(jp_moshimoshi, "jp-moshimoshi",
     "MOSHI MOSHI NIPPON", "もしもしにっぽん",
     "jp_webmedia", "media",
     "https://www.moshimoshi-nippon.jp/feed",
     "ja", "[\"news\",\"publisher:MOSHI MOSHI NIPPON\",\"kawaii\",\"pop\"]",
     3600,
     "MOSHI MOSHI NIPPON — Harajuku, kawaii and Japanese pop culture")

/* ── Science / Academic ─────────────────────────────────────────────── */

RSSX(jp_waseda_news, "jp-waseda-news",
     "Waseda University News", "早稲田大学ニュース",
     "jp_webmedia", "research",
     "https://www.waseda.jp/top/news/feed",
     "ja", "[\"news\",\"publisher:Waseda University\",\"academic\",\"research\"]",
     7200,
     "Waseda University — research news and campus announcements")

RSSX(jp_kyotou_en, "jp-kyotou-en",
     "Kyoto University News (EN)", "京都大学ニュース（英語）",
     "jp_webmedia", "research",
     "https://www.kyoto-u.ac.jp/en/rss.xml",
     "en", "[\"news\",\"publisher:Kyoto University\",\"academic\",\"research\"]",
     7200,
     "Kyoto University — English research and university news")

RSSX(jp_nazology, "jp-nazology",
     "Nazology", "ナゾロジー",
     "jp_webmedia", "research",
     "https://nazology.net/feed",
     "ja", "[\"news\",\"publisher:Nazology\",\"science\",\"discovery\"]",
     3600,
     "Nazology — science discoveries and curiosities explained")

RSSX(jp_scienceportal, "jp-scienceportal",
     "Science Portal", "サイエンスポータル",
     "jp_webmedia", "research",
     "https://scienceportal.jst.go.jp/feed",
     "ja", "[\"news\",\"publisher:Science Portal\",\"science\",\"jst\"]",
     3600,
     "Science Portal (JST) — Japanese science and technology news")

RSSX(jp_drone, "jp-drone",
     "DRONE.jp", "DRONE",
     "jp_webmedia", "technology",
     "https://www.drone.jp/feed",
     "ja", "[\"news\",\"publisher:DRONE.jp\",\"drone\",\"tech\"]",
     3600,
     "DRONE.jp — drone industry, regulations and aerial technology")

/* ── English-language Japan ─────────────────────────────────────────── */

RSSX(jp_spoon_tamago, "jp-spoon-tamago",
     "Spoon and Tamago", "Spoon & Tamago",
     "jp_webmedia", "media",
     "https://spoon-tamago.com/feed/",
     "en", "[\"news\",\"publisher:Spoon & Tamago\",\"art\",\"design\",\"japan\"]",
     7200,
     "Spoon & Tamago — Japanese art, design and culture in English")

RSSX(jp_tokyocheapo, "jp-tokyocheapo",
     "Tokyo Cheapo", "Tokyo Cheapo",
     "jp_webmedia", "media",
     "https://tokyocheapo.com/feed/",
     "en", "[\"news\",\"publisher:Tokyo Cheapo\",\"budget\",\"tokyo\"]",
     3600,
     "Tokyo Cheapo — budget living, events and deals in Tokyo")

RSSX(jp_gaijinpot, "jp-gaijinpot",
     "GaijinPot Blog", "GaijinPot Blog",
     "jp_webmedia", "media",
     "https://blog.gaijinpot.com/feed/",
     "en", "[\"news\",\"publisher:GaijinPot\",\"expat\",\"jobs\"]",
     3600,
     "GaijinPot Blog — living, working and traveling in Japan")

RSSX(jp_subculture, "jp-subculture",
     "Japan Subculture Research", "日本サブカルチャー研究センター",
     "jp_webmedia", "media",
     "https://www.japansubculture.com/feed/",
     "en", "[\"news\",\"publisher:JSRC\",\"investigative\",\"japan\"]",
     7200,
     "Japan Subculture Research Center — investigative journalism on Japan")

RSSX(jp_industry_news, "jp-industry-news",
     "Japan Industry News", "Japan Industry News",
     "jp_webmedia", "media",
     "https://www.japanindustrynews.com/feed/",
     "en", "[\"news\",\"publisher:Japan Industry News\",\"manufacturing\"]",
     7200,
     "Japan Industry News — Japanese manufacturing and technology")

RSSX(jp_policy_forum, "jp-policy-forum",
     "Japan Policy Forum", "Japan Policy Forum",
     "jp_webmedia", "media",
     "https://www.japanpolicyforum.jp/feed/",
     "en", "[\"news\",\"publisher:Japan Policy Forum\",\"policy\",\"analysis\"]",
     7200,
     "Japan Policy Forum — Japanese economic and policy analysis")

RSSX(jp_tofugu, "jp-tofugu",
     "Tofugu", "Tofugu",
     "jp_webmedia", "media",
     "https://www.tofugu.com/feed",
     "en", "[\"news\",\"publisher:Tofugu\",\"language\",\"culture\"]",
     7200,
     "Tofugu — Japanese language learning and culture articles")

RSSX(jp_japankyo, "jp-japankyo",
     "Japankyo", "Japankyo",
     "jp_webmedia", "media",
     "https://www.japankyo.com/feed/",
     "en", "[\"news\",\"publisher:Japankyo\",\"culture\",\"society\"]",
     7200,
     "Japankyo — Japanese culture, education and society")

RSSX(jp_japankuru, "jp-japankuru",
     "JAPANKURU", "JAPANKURU",
     "jp_webmedia", "media",
     "https://www.japankuru.com/en/feed",
     "en", "[\"news\",\"publisher:JAPANKURU\",\"travel\",\"japan\"]",
     7200,
     "JAPANKURU — travel tips, food and culture across Japan")

RSSX(jp_waraku, "jp-waraku",
     "Into Japan Waraku", "和樂web",
     "jp_webmedia", "media",
     "https://intojapanwaraku.com/feed",
     "ja", "[\"news\",\"publisher:Waraku\",\"traditional\",\"culture\"]",
     7200,
     "Into Japan Waraku — traditional Japanese culture and art")

RSSX(jp_randomwire, "jp-randomwire",
     "Randomwire", "Randomwire",
     "jp_webmedia", "media",
     "https://randomwire.com/feed/",
     "en", "[\"news\",\"publisher:Randomwire\",\"travel\",\"japan\"]",
     7200,
     "Randomwire — Japan travel, food and cultural exploration")

RSSX(jp_concierge, "jp-concierge",
     "Japan Concierge", "Japan Concierge",
     "jp_webmedia", "media",
     "https://www.japanconcierge.com/feed/",
     "en", "[\"news\",\"publisher:Japan Concierge\",\"travel\",\"japan\"]",
     7200,
     "Japan Concierge — curated Japan travel and culture guide")

RSSX(jp_japanese_station, "jp-japanese-station",
     "Japanese Station", "Japanese Station",
     "jp_webmedia", "media",
     "https://japanesestation.com/feed/",
     "en", "[\"news\",\"publisher:Japanese Station\",\"pop\",\"japan\"]",
     3600,
     "Japanese Station — J-pop, anime and Japanese pop culture")

RSSX(jp_japanese_creations, "jp-japanese-creations",
     "Japanese Creations", "Japanese Creations",
     "jp_webmedia", "media",
     "https://japanesecreations.com/feed/",
     "en", "[\"news\",\"publisher:Japanese Creations\",\"craft\",\"culture\"]",
     7200,
     "Japanese Creations — traditional crafts, food and culture")

RSSX(jp_sustainablejp, "jp-sustainablejp",
     "Sustainable Japan", "Sustainable Japan",
     "jp_webmedia", "media",
     "https://sustainablejapan.jp/feed",
     "ja", "[\"news\",\"publisher:Sustainable Japan\",\"esg\",\"sustainability\"]",
     7200,
     "Sustainable Japan — ESG, sustainability and CSR news")

/* ── Career / Events ────────────────────────────────────────────────── */

RSSX(jp_rikunabi_journal, "jp-rikunabi-journal",
     "Rikunabi NEXT Journal", "リクナビNEXT ジャーナル",
     "jp_webmedia", "media",
     "https://next.rikunabi.com/journal/feed/",
     "ja", "[\"news\",\"publisher:Rikunabi\",\"career\",\"work\"]",
     7200,
     "Rikunabi NEXT Journal — career, work-style and job market trends")

RSSX(jp_engineer_type, "jp-engineer-type",
     "type Engineer Type", "type エンジニアtype",
     "jp_webmedia", "technology",
     "https://type.jp/et/feature/feed/",
     "ja", "[\"news\",\"publisher:type\",\"career\",\"engineering\"]",
     7200,
     "type Engineer Type — engineer career stories and tech interviews")

RSSX(jp_careerhack, "jp-careerhack",
     "CAREER HACK", "CAREER HACK",
     "jp_webmedia", "media",
     "https://careerhack.en-japan.com/feed",
     "ja", "[\"news\",\"publisher:CAREER HACK\",\"career\",\"startup\"]",
     7200,
     "CAREER HACK — startup and tech career stories by en Japan")

RSSX(jp_flexy, "jp-flexy",
     "FLEXY", "FLEXY",
     "jp_webmedia", "technology",
     "https://flxy.jp/article/feed",
     "ja", "[\"news\",\"publisher:FLEXY\",\"freelance\",\"tech\"]",
     7200,
     "FLEXY — freelance engineering and tech industry insights")

RSSX(jp_connpass, "jp-connpass",
     "connpass Events", "connpass イベント",
     "jp_webmedia", "technology",
     "https://connpass.com/explore/ja.atom",
     "ja", "[\"news\",\"publisher:connpass\",\"events\",\"tech\"]",
     3600,
     "connpass — Japanese tech meetups, study groups and events")
