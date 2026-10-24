/* collectors/sources/jp_feeds_batch43.c
 * 200 new Japan-focused RSS/Atom feeds, all proven live (HTTP 200) and
 * verified absent from the existing tree on 2026-10-18.
 *
 * Every URL was tested with curl and returned a valid RSS/Atom body.
 * Uses the RSSX macro from _source_macros.inc for concise registration. */
#include "_source_macros.inc"

/* ── Technology / IT ───────────────────────────────────────────── */

RSSX(jp_b43_aizine_ai, "jp-b43-aizine-ai",
     "AI ZINE", "AIZINE",
     "jp_webmedia", "technology",
     "https://aizine.ai/feed/",
     "ja", "[\"news\",\"publisher:AI ZINE\",\"tech\",\"ai\"]",
     3600,
     "AI ZINE (AIZINE) — tech ai")

RSSX(jp_b43_applech2_com, "jp-b43-applech2-com",
     "applech2", "applech2",
     "jp_webmedia", "technology",
     "https://applech2.com/feed",
     "ja", "[\"news\",\"publisher:applech2\",\"tech\"]",
     3600,
     "applech2 — tech")

RSSX(jp_b43_bitwave_showcase_tv_com, "jp-b43-bitwave-showcase-tv-com",
     "bitWave", "bitWave",
     "jp_webmedia", "technology",
     "https://bitwave.showcase-tv.com/feed/",
     "ja", "[\"news\",\"publisher:bitWave\",\"tech\"]",
     3600,
     "bitWave — tech")

RSSX(jp_b43_builderscon_io, "jp-b43-builderscon-io",
     "builderscon blog", "builderscon",
     "jp_webmedia", "technology",
     "https://blog.builderscon.io/feed",
     "ja", "[\"news\",\"publisher:builderscon blog\",\"tech\"]",
     3600,
     "builderscon blog (builderscon) — tech")

RSSX(jp_b43_businessnetwork_jp, "jp-b43-businessnetwork-jp",
     "Business Network", "ビジネスネットワーク",
     "jp_webmedia", "technology",
     "https://businessnetwork.jp/feed",
     "ja", "[\"news\",\"publisher:Business Network\",\"tech\"]",
     3600,
     "Business Network (ビジネスネットワーク) — tech")

RSSX(jp_b43_buzzap_jp, "jp-b43-buzzap-jp",
     "BUZZAP!", "BUZZAP!",
     "jp_webmedia", "technology",
     "https://buzzap.jp/feed/",
     "ja", "[\"news\",\"publisher:BUZZAP!\",\"tech\",\"gadget\"]",
     3600,
     "BUZZAP! — tech gadget")

RSSX(jp_b43_eetimes_itmedia_co_jp, "jp-b43-eetimes-itmedia-co-jp",
     "EE Times Japan (ITmedia)", "EE Times Japan",
     "jp_webmedia", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/eetimes.xml",
     "ja", "[\"news\",\"publisher:EE Times Japan (ITmedia)\",\"tech\",\"semiconductor\"]",
     3600,
     "EE Times Japan (ITmedia) (EE Times Japan) — tech semi")

RSSX(jp_b43_enterprisezine_jp, "jp-b43-enterprisezine-jp",
     "EnterpriseZine", "EnterpriseZine",
     "jp_webmedia", "technology",
     "https://enterprisezine.jp/rss/new/20/index.xml",
     "ja", "[\"news\",\"publisher:EnterpriseZine\",\"tech\"]",
     3600,
     "EnterpriseZine — tech")

RSSX(jp_b43_fav_itmedia_co_jp, "jp-b43-fav-itmedia-co-jp",
     "ITmedia Fav", "ITmedia Fav",
     "jp_webmedia", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/fav.xml",
     "ja", "[\"news\",\"publisher:ITmedia Fav\",\"tech\"]",
     3600,
     "ITmedia Fav — tech")

RSSX(jp_b43_gadget_shot_com, "jp-b43-gadget-shot-com",
     "GadgetShot", "ガジェットショット",
     "jp_webmedia", "technology",
     "https://gadget-shot.com/feed",
     "ja", "[\"news\",\"publisher:GadgetShot\",\"tech\"]",
     3600,
     "GadgetShot (ガジェットショット) — tech")

RSSX(jp_b43_gori_me, "jp-b43-gori-me",
     "gori.me", "gori.me",
     "jp_webmedia", "technology",
     "https://gori.me/feed",
     "ja", "[\"news\",\"publisher:gori.me\",\"tech\"]",
     3600,
     "gori.me — tech")

RSSX(jp_b43_hackaday_com, "jp-b43-hackaday-com",
     "Hackaday", "Hackaday",
     "jp_webmedia", "technology",
     "https://hackaday.com/feed/",
     "en", "[\"news\",\"publisher:Hackaday\",\"tech\"]",
     3600,
     "Hackaday — tech")

RSSX(jp_b43_io_cyberdefense_jp, "jp-b43-io-cyberdefense-jp",
     "Cyber Defense Institute", "サイバーディフェンス研究所",
     "jp_webmedia", "technology",
     "https://io.cyberdefense.jp/feed",
     "ja", "[\"news\",\"publisher:Cyber Defense Institute\",\"security\"]",
     3600,
     "Cyber Defense Institute (サイバーディフェンス研究所) — security")

RSSX(jp_b43_iotnews_jp, "jp-b43-iotnews-jp",
     "IoT News", "IoTニュース",
     "jp_webmedia", "technology",
     "https://iotnews.jp/feed/",
     "ja", "[\"news\",\"publisher:IoT News\",\"tech\"]",
     3600,
     "IoT News (IoTニュース) — tech")

RSSX(jp_b43_iphone_mania_jp, "jp-b43-iphone-mania-jp",
     "iPhone Mania", "iPhone Mania",
     "jp_webmedia", "technology",
     "https://iphone-mania.jp/feed/",
     "ja", "[\"news\",\"publisher:iPhone Mania\",\"tech\"]",
     3600,
     "iPhone Mania — tech")

RSSX(jp_b43_jidounten_lab_com, "jp-b43-jidounten-lab-com",
     "Autonomous Driving Lab", "自動運転ラボ",
     "jp_webmedia", "technology",
     "https://jidounten-lab.com/feed",
     "ja", "[\"news\",\"publisher:Autonomous Driving Lab\",\"tech\",\"automotive\"]",
     3600,
     "Autonomous Driving Lab (自動運転ラボ) — tech auto")

RSSX(jp_b43_lifestyle_itmedia_co_jp, "jp-b43-lifestyle-itmedia-co-jp",
     "ITmedia Lifestyle", "ITmedia LifeStyle",
     "jp_webmedia", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/lifestyle.xml",
     "ja", "[\"news\",\"publisher:ITmedia Lifestyle\",\"tech\",\"lifestyle\"]",
     3600,
     "ITmedia Lifestyle (ITmedia LifeStyle) — tech lifestyle")

RSSX(jp_b43_mobile_itmedia_co_jp, "jp-b43-mobile-itmedia-co-jp",
     "ITmedia Mobile", "ITmedia Mobile",
     "jp_webmedia", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/mobile.xml",
     "ja", "[\"news\",\"publisher:ITmedia Mobile\",\"tech\",\"mobile\"]",
     3600,
     "ITmedia Mobile — tech mobile")

RSSX(jp_b43_mugendai_web_jp, "jp-b43-mugendai-web-jp",
     "Mugendai (IBM)", "Mugendai（無限大）",
     "jp_webmedia", "technology",
     "https://mugendai-web.jp/feed",
     "ja", "[\"news\",\"publisher:Mugendai (IBM)\",\"tech\"]",
     3600,
     "Mugendai (IBM) (Mugendai（無限大）) — tech")

RSSX(jp_b43_netafull_net, "jp-b43-netafull-net",
     "Netafull", "ネタフル",
     "jp_webmedia", "technology",
     "https://netafull.net/feed",
     "ja", "[\"news\",\"publisher:Netafull\",\"tech\"]",
     3600,
     "Netafull (ネタフル) — tech")

RSSX(jp_b43_panora_tokyo, "jp-b43-panora-tokyo",
     "PANORA VR", "PANORA",
     "jp_webmedia", "technology",
     "https://panora.tokyo/feed",
     "ja", "[\"news\",\"publisher:PANORA VR\",\"tech\",\"vr\"]",
     3600,
     "PANORA VR (PANORA) — niche tech")

RSSX(jp_b43_piyolog_hatenadiary_jp, "jp-b43-piyolog-hatenadiary-jp",
     "piyolog", "piyolog",
     "jp_webmedia", "technology",
     "https://piyolog.hatenadiary.jp/feed",
     "ja", "[\"news\",\"publisher:piyolog\",\"security\"]",
     3600,
     "piyolog — security")

RSSX(jp_b43_productzine_jp, "jp-b43-productzine-jp",
     "ProductZine", "ProductZine",
     "jp_webmedia", "technology",
     "https://productzine.jp/rss/new/20/index.xml",
     "ja", "[\"news\",\"publisher:ProductZine\",\"tech\"]",
     3600,
     "ProductZine — tech")

RSSX(jp_b43_s_max_jp, "jp-b43-s-max-jp",
     "S-MAX", "S-MAX",
     "jp_webmedia", "technology",
     "https://s-max.jp/feed",
     "ja", "[\"news\",\"publisher:S-MAX\",\"tech\",\"mobile\"]",
     3600,
     "S-MAX — tech mobile")

RSSX(jp_b43_smartjapan_itmedia_co_jp, "jp-b43-smartjapan-itmedia-co-jp",
     "Smart Japan (ITmedia)", "スマートジャパン",
     "jp_webmedia", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/smartjapan.xml",
     "ja", "[\"news\",\"publisher:Smart Japan (ITmedia)\",\"tech\",\"energy\"]",
     3600,
     "Smart Japan (ITmedia) (スマートジャパン) — tech energy")

RSSX(jp_b43_smatu_net, "jp-b43-smatu-net",
     "Smatu.net", "Smatu.net",
     "jp_webmedia", "technology",
     "https://smatu.net/feed/",
     "ja", "[\"news\",\"publisher:Smatu.net\",\"tech\"]",
     3600,
     "Smatu.net — tech")

RSSX(jp_b43_sumahosupportline_com, "jp-b43-sumahosupportline-com",
     "Sumaho Support Line", "スマホサポートライン",
     "jp_webmedia", "technology",
     "https://sumahosupportline.com/feed/",
     "ja", "[\"news\",\"publisher:Sumaho Support Line\",\"tech\"]",
     3600,
     "Sumaho Support Line (スマホサポートライン) — tech")

RSSX(jp_b43_taisy0_com, "jp-b43-taisy0-com",
     "Taisy0", "酔いどれオヤジのブログwp",
     "jp_webmedia", "technology",
     "https://taisy0.com/feed",
     "ja", "[\"news\",\"publisher:Taisy0\",\"tech\"]",
     3600,
     "Taisy0 (酔いどれオヤジのブログwp) — tech")

RSSX(jp_b43_techtarget_itmedia_co_jp, "jp-b43-techtarget-itmedia-co-jp",
     "TechTarget Japan", "TechTarget Japan",
     "jp_webmedia", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/techtarget.xml",
     "ja", "[\"news\",\"publisher:TechTarget Japan\",\"tech\"]",
     3600,
     "TechTarget Japan — tech")

RSSX(jp_b43_teradas_net, "jp-b43-teradas-net",
     "Teradas", "寺田's",
     "jp_webmedia", "technology",
     "https://teradas.net/feed/",
     "ja", "[\"news\",\"publisher:Teradas\",\"tech\"]",
     3600,
     "Teradas (寺田's) — tech")

RSSX(jp_b43_touchlab_jp, "jp-b43-touchlab-jp",
     "Touch Lab", "Touch Lab",
     "jp_webmedia", "technology",
     "https://touchlab.jp/feed/",
     "ja", "[\"news\",\"publisher:Touch Lab\",\"tech\"]",
     3600,
     "Touch Lab — tech")

RSSX(jp_b43_usedoor_jp, "jp-b43-usedoor-jp",
     "usedoor", "usedoor",
     "jp_webmedia", "technology",
     "https://usedoor.jp/feed/",
     "ja", "[\"news\",\"publisher:usedoor\",\"tech\"]",
     3600,
     "usedoor — tech")

RSSX(jp_b43_videosalon_jp, "jp-b43-videosalon-jp",
     "VIDEO SALON", "ビデオSALON",
     "jp_webmedia", "technology",
     "https://videosalon.jp/feed/",
     "ja", "[\"news\",\"publisher:VIDEO SALON\",\"tech\",\"video\"]",
     3600,
     "VIDEO SALON (ビデオSALON) — tech video")

RSSX(jp_b43_web_camp_io, "jp-b43-web-camp-io",
     "Web Camp", "DMM WEBCAMP",
     "jp_webmedia", "technology",
     "https://web-camp.io/magazine/feed",
     "ja", "[\"news\",\"publisher:Web Camp\",\"tech\"]",
     3600,
     "Web Camp (DMM WEBCAMP) — tech")

RSSX(jp_b43_weblog_life_net, "jp-b43-weblog-life-net",
     "Weblog Life", "Weblog Life",
     "jp_webmedia", "technology",
     "https://weblog-life.net/feed/",
     "ja", "[\"news\",\"publisher:Weblog Life\",\"tech\"]",
     3600,
     "Weblog Life — tech")

/* ── Tech Blogs ────────────────────────────────────────────────── */

RSSX(jp_b43_caddi_tech, "jp-b43-caddi-tech",
     "CADDi Tech Blog", "CADDi テックブログ",
     "jp_webmedia", "technology",
     "https://caddi.tech/feed",
     "ja", "[\"news\",\"publisher:CADDi Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "CADDi Tech Blog (CADDi テックブログ) — tech blog")

RSSX(jp_b43_dev_classmethod_jp, "jp-b43-dev-classmethod-jp",
     "Classmethod Dev", "クラスメソッド",
     "jp_webmedia", "technology",
     "https://dev.classmethod.jp/feed/",
     "ja", "[\"news\",\"publisher:Classmethod Dev\",\"tech\",\"blog\"]",
     3600,
     "Classmethod Dev (クラスメソッド) — tech blog")

RSSX(jp_b43_devblog_thebase_in, "jp-b43-devblog-thebase-in",
     "BASE Dev Blog", "BASE開発チームブログ",
     "jp_webmedia", "technology",
     "https://devblog.thebase.in/feed",
     "ja", "[\"news\",\"publisher:BASE Dev Blog\",\"tech\",\"blog\"]",
     3600,
     "BASE Dev Blog (BASE開発チームブログ) — tech blog")

RSSX(jp_b43_developers_freee_co_jp, "jp-b43-developers-freee-co-jp",
     "freee Developers", "freee Developers",
     "jp_webmedia", "technology",
     "https://developers.freee.co.jp/feed",
     "ja", "[\"news\",\"publisher:freee Developers\",\"tech\",\"blog\"]",
     3600,
     "freee Developers — techblog")

RSSX(jp_b43_eng_blog_iij_ad_jp, "jp-b43-eng-blog-iij-ad-jp",
     "IIJ Engineers Blog", "IIJ Engineers Blog",
     "jp_webmedia", "technology",
     "https://eng-blog.iij.ad.jp/feed",
     "ja", "[\"news\",\"publisher:IIJ Engineers Blog\",\"tech\",\"blog\"]",
     3600,
     "IIJ Engineers Blog — tech blog")

RSSX(jp_b43_engineer_retty_me, "jp-b43-engineer-retty-me",
     "Retty Tech Blog", "Retty Tech Blog",
     "jp_webmedia", "technology",
     "https://engineer.retty.me/feed",
     "ja", "[\"news\",\"publisher:Retty Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "Retty Tech Blog — tech blog")

RSSX(jp_b43_labs_gree_jp, "jp-b43-labs-gree-jp",
     "GREE Engineers Blog", "GREE Engineers' Blog",
     "jp_webmedia", "technology",
     "https://labs.gree.jp/blog/feed/",
     "ja", "[\"news\",\"publisher:GREE Engineers Blog\",\"tech\",\"blog\"]",
     3600,
     "GREE Engineers Blog (GREE Engineers' Blog) — tech blog")

RSSX(jp_b43_made_livesense_co_jp, "jp-b43-made-livesense-co-jp",
     "Livesense Made Blog", "Livesense Made",
     "jp_webmedia", "technology",
     "https://made.livesense.co.jp/feed",
     "ja", "[\"news\",\"publisher:Livesense Made Blog\",\"tech\",\"blog\"]",
     3600,
     "Livesense Made Blog (Livesense Made) — tech blog")

RSSX(jp_b43_moneyforward_dev_jp, "jp-b43-moneyforward-dev-jp",
     "MoneyForward Dev Blog", "マネーフォワード エンジニアブログ",
     "jp_webmedia", "technology",
     "https://moneyforward-dev.jp/feed",
     "ja", "[\"news\",\"publisher:MoneyForward Dev Blog\",\"tech\",\"blog\"]",
     3600,
     "MoneyForward Dev Blog (マネーフォワード エンジニアブログ) — tech blog")

RSSX(jp_b43_paiza_hatenablog_com, "jp-b43-paiza-hatenablog-com",
     "paiza blog", "paiza 開発日誌",
     "jp_webmedia", "technology",
     "https://paiza.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:paiza blog\",\"tech\",\"blog\"]",
     3600,
     "paiza blog (paiza 開発日誌) — tech blog")

RSSX(jp_b43_product_st_inc, "jp-b43-product-st-inc",
     "Sansan Product Blog", "Sansan Product Blog",
     "jp_webmedia", "technology",
     "https://product.st.inc/feed",
     "ja", "[\"news\",\"publisher:Sansan Product Blog\",\"tech\",\"blog\"]",
     3600,
     "Sansan Product Blog — techblog")

RSSX(jp_b43_tech_blog_optim_co_jp, "jp-b43-tech-blog-optim-co-jp",
     "OPTiM Tech Blog", "OPTiM TECH BLOG",
     "jp_webmedia", "technology",
     "https://tech-blog.optim.co.jp/feed",
     "ja", "[\"news\",\"publisher:OPTiM Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "OPTiM Tech Blog (OPTiM TECH BLOG) — tech blog")

RSSX(jp_b43_tech_andpad_co_jp, "jp-b43-tech-andpad-co-jp",
     "ANDPAD Tech", "ANDPAD Tech Blog",
     "jp_webmedia", "technology",
     "https://tech.andpad.co.jp/feed",
     "ja", "[\"news\",\"publisher:ANDPAD Tech\",\"tech\",\"blog\"]",
     3600,
     "ANDPAD Tech (ANDPAD Tech Blog) — tech blog")

RSSX(jp_b43_tech_drecom_co_jp, "jp-b43-tech-drecom-co-jp",
     "Drecom Tech", "Drecom Tech Blog",
     "jp_webmedia", "technology",
     "https://tech.drecom.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Drecom Tech\",\"tech\",\"blog\"]",
     3600,
     "Drecom Tech (Drecom Tech Blog) — tech blog")

RSSX(jp_b43_tech_every_tv, "jp-b43-tech-every-tv",
     "every Tech", "every Tech Blog",
     "jp_webmedia", "technology",
     "https://tech.every.tv/feed",
     "ja", "[\"news\",\"publisher:every Tech\",\"tech\",\"blog\"]",
     3600,
     "every Tech (every Tech Blog) — tech blog")

RSSX(jp_b43_tech_gunosy_io, "jp-b43-tech-gunosy-io",
     "Gunosy Tech Blog", "Gunosy テックブログ",
     "jp_webmedia", "technology",
     "https://tech.gunosy.io/feed",
     "ja", "[\"news\",\"publisher:Gunosy Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "Gunosy Tech Blog (Gunosy テックブログ) — tech blog")

RSSX(jp_b43_tech_layerx_co_jp, "jp-b43-tech-layerx-co-jp",
     "LayerX Tech Blog", "LayerX テックブログ",
     "jp_webmedia", "technology",
     "https://tech.layerx.co.jp/feed",
     "ja", "[\"news\",\"publisher:LayerX Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "LayerX Tech Blog (LayerX テックブログ) — tech blog")

RSSX(jp_b43_tech_medpeer_co_jp, "jp-b43-tech-medpeer-co-jp",
     "MedPeer Tech Blog", "MedPeer Tech Blog",
     "jp_webmedia", "technology",
     "https://tech.medpeer.co.jp/feed",
     "ja", "[\"news\",\"publisher:MedPeer Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "MedPeer Tech Blog — techblog")

RSSX(jp_b43_tech_pepabo_com, "jp-b43-tech-pepabo-com",
     "GMO Pepabo Tech", "GMO ペパボ テックブログ",
     "jp_webmedia", "technology",
     "https://tech.pepabo.com/feed.xml",
     "ja", "[\"news\",\"publisher:GMO Pepabo Tech\",\"tech\",\"blog\"]",
     3600,
     "GMO Pepabo Tech (GMO ペパボ テックブログ) — tech blog")

RSSX(jp_b43_tech_timee_co_jp, "jp-b43-tech-timee-co-jp",
     "Timee Tech Blog", "タイミー テックブログ",
     "jp_webmedia", "technology",
     "https://tech.timee.co.jp/feed",
     "ja", "[\"news\",\"publisher:Timee Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "Timee Tech Blog (タイミー テックブログ) — tech blog")

RSSX(jp_b43_tech_willgate_co_jp, "jp-b43-tech-willgate-co-jp",
     "WillGate Tech Blog", "ウィルゲートTech Blog",
     "jp_webmedia", "technology",
     "https://tech.willgate.co.jp/feed",
     "ja", "[\"news\",\"publisher:WillGate Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "WillGate Tech Blog (ウィルゲートTech Blog) — techblog")

RSSX(jp_b43_techblog_lycorp_co_jp, "jp-b43-techblog-lycorp-co-jp",
     "LY Corp Tech Blog", "LY Corporation テックブログ",
     "jp_webmedia", "technology",
     "https://techblog.lycorp.co.jp/ja/feed/index.xml",
     "ja", "[\"news\",\"publisher:LY Corp Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "LY Corp Tech Blog (LY Corporation テックブログ) — tech blog")

RSSX(jp_b43_techblog_zozo_com, "jp-b43-techblog-zozo-com",
     "ZOZO Tech Blog", "ZOZO テックブログ",
     "jp_webmedia", "technology",
     "https://techblog.zozo.com/feed",
     "ja", "[\"news\",\"publisher:ZOZO Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "ZOZO Tech Blog (ZOZO テックブログ) — tech blog")

/* ── Web Design / Dev ──────────────────────────────────────────── */

RSSX(jp_b43_baigie_me, "jp-b43-baigie-me",
     "Baigie Blog", "ベイジブログ",
     "jp_webmedia", "technology",
     "https://baigie.me/officialblog/feed/",
     "ja", "[\"news\",\"publisher:Baigie Blog\",\"design\",\"webdev\"]",
     3600,
     "Baigie Blog (ベイジブログ) — webdesign")

RSSX(jp_b43_coliss_com, "jp-b43-coliss-com",
     "Coliss", "コリス",
     "jp_webmedia", "technology",
     "https://coliss.com/feed/",
     "ja", "[\"news\",\"publisher:Coliss\",\"tech\",\"webdev\"]",
     3600,
     "Coliss (コリス) — tech webdev")

RSSX(jp_b43_commte_net, "jp-b43-commte-net",
     "commte", "commte",
     "jp_webmedia", "technology",
     "https://commte.net/feed",
     "ja", "[\"news\",\"publisher:commte\",\"design\",\"webdev\"]",
     3600,
     "commte — design")

RSSX(jp_b43_findy_code_io, "jp-b43-findy-code-io",
     "Findy Engineer Lab", "Findy Engineer Lab",
     "jp_webmedia", "technology",
     "https://findy-code.io/engineer-lab/feed",
     "ja", "[\"news\",\"publisher:Findy Engineer Lab\",\"tech\",\"career\"]",
     3600,
     "Findy Engineer Lab — tech career")

RSSX(jp_b43_jser_info, "jp-b43-jser-info",
     "JSer.info", "JSer.info",
     "jp_webmedia", "technology",
     "https://jser.info/rss",
     "ja", "[\"news\",\"publisher:JSer.info\",\"tech\",\"webdev\"]",
     3600,
     "JSer.info — tech webdev")

RSSX(jp_b43_liginc_co_jp, "jp-b43-liginc-co-jp",
     "LIG Blog", "LIG",
     "jp_webmedia", "technology",
     "https://liginc.co.jp/feed",
     "ja", "[\"news\",\"publisher:LIG Blog\",\"tech\",\"webdev\"]",
     3600,
     "LIG Blog (LIG) — tech webdev")

RSSX(jp_b43_photoshopvip_net, "jp-b43-photoshopvip-net",
     "PhotoshopVIP", "PhotoshopVIP",
     "jp_webmedia", "technology",
     "https://photoshopvip.net/feed",
     "ja", "[\"news\",\"publisher:PhotoshopVIP\",\"tech\",\"design\"]",
     3600,
     "PhotoshopVIP — tech design")

RSSX(jp_b43_webdesignfacts_net, "jp-b43-webdesignfacts-net",
     "Web Design Facts", "ウェブデザインファクツ",
     "jp_webmedia", "technology",
     "https://webdesignfacts.net/feed/",
     "ja", "[\"news\",\"publisher:Web Design Facts\",\"design\",\"webdev\"]",
     3600,
     "Web Design Facts (ウェブデザインファクツ) — webdesign")

RSSX(jp_b43_webdesignmagazine_net, "jp-b43-webdesignmagazine-net",
     "Web Design Magazine", "Webデザインマガジン",
     "jp_webmedia", "technology",
     "https://webdesignmagazine.net/feed/",
     "ja", "[\"news\",\"publisher:Web Design Magazine\",\"design\",\"webdev\"]",
     3600,
     "Web Design Magazine (Webデザインマガジン) — design")

/* ── News / Media ──────────────────────────────────────────────── */

RSSX(jp_b43_asiatimes_com, "jp-b43-asiatimes-com",
     "Asia Times", "Asia Times",
     "jp_webmedia", "media",
     "https://asiatimes.com/feed/",
     "en", "[\"news\",\"publisher:Asia Times\",\"media\"]",
     3600,
     "Asia Times — news")

RSSX(jp_b43_nikkan_spa_jp, "jp-b43-nikkan-spa-jp",
     "Nikkan SPA!", "日刊SPA!",
     "jp_webmedia", "media",
     "https://nikkan-spa.jp/feed",
     "ja", "[\"news\",\"publisher:Nikkan SPA!\",\"media\"]",
     3600,
     "Nikkan SPA! (日刊SPA!) — news")

RSSX(jp_b43_turns_jp, "jp-b43-turns-jp",
     "TURNS", "TURNS",
     "jp_webmedia", "media",
     "https://turns.jp/feed",
     "ja", "[\"news\",\"publisher:TURNS\",\"regional\"]",
     3600,
     "TURNS — regional")

/* ── Culture / Society ─────────────────────────────────────────── */

RSSX(jp_b43_buzzmag_jp, "jp-b43-buzzmag-jp",
     "BuzzMag", "バズマグ",
     "jp_webmedia", "media",
     "https://buzzmag.jp/feed",
     "ja", "[\"news\",\"publisher:BuzzMag\",\"culture\"]",
     3600,
     "BuzzMag (バズマグ) — culture")

RSSX(jp_b43_casabrutus_com, "jp-b43-casabrutus-com",
     "Casa BRUTUS", "Casa BRUTUS",
     "jp_webmedia", "media",
     "https://casabrutus.com/feed",
     "ja", "[\"news\",\"publisher:Casa BRUTUS\",\"culture\",\"design\"]",
     3600,
     "Casa BRUTUS — culture design")

RSSX(jp_b43_coconutsjapan_com, "jp-b43-coconutsjapan-com",
     "Coconuts Japan", "ココナッツジャパン",
     "jp_webmedia", "media",
     "https://coconutsjapan.com/feed",
     "ja", "[\"news\",\"publisher:Coconuts Japan\",\"culture\"]",
     3600,
     "Coconuts Japan (ココナッツジャパン) — culture")

RSSX(jp_b43_fundo_jp, "jp-b43-fundo-jp",
     "FUNDO", "FUNDO",
     "jp_webmedia", "media",
     "https://fundo.jp/feed",
     "ja", "[\"news\",\"publisher:FUNDO\",\"culture\"]",
     3600,
     "FUNDO — culture")

RSSX(jp_b43_gaijinass_com, "jp-b43-gaijinass-com",
     "GaijinAss", "GaijinAss",
     "jp_webmedia", "media",
     "https://gaijinass.com/feed/",
     "en", "[\"news\",\"publisher:GaijinAss\",\"culture\"]",
     3600,
     "GaijinAss — culture")

RSSX(jp_b43_japaaan_com, "jp-b43-japaaan-com",
     "Japaaan Magazine", "Japaaan",
     "jp_webmedia", "media",
     "https://mag.japaaan.com/feed",
     "ja", "[\"news\",\"publisher:Japaaan Magazine\",\"culture\"]",
     3600,
     "Japaaan Magazine (Japaaan) — culture")

RSSX(jp_b43_kokoro_jp_com, "jp-b43-kokoro-jp-com",
     "Kokoro JP", "ココロJP",
     "jp_webmedia", "media",
     "https://kokoro-jp.com/feed/",
     "en", "[\"news\",\"publisher:Kokoro JP\",\"culture\"]",
     3600,
     "Kokoro JP (ココロJP) — culture")

RSSX(jp_b43_ninjareports_com, "jp-b43-ninjareports-com",
     "Ninja Reports", "忍者レポート",
     "jp_webmedia", "media",
     "https://ninjareports.com/feed/",
     "en", "[\"news\",\"publisher:Ninja Reports\",\"japan\"]",
     3600,
     "Ninja Reports (忍者レポート) — japan")

RSSX(jp_b43_omocoro_jp, "jp-b43-omocoro-jp",
     "Omocoro", "オモコロ",
     "jp_webmedia", "media",
     "https://omocoro.jp/feed",
     "ja", "[\"news\",\"publisher:Omocoro\",\"culture\"]",
     3600,
     "Omocoro (オモコロ) — culture")

RSSX(jp_b43_thejapanguy_com, "jp-b43-thejapanguy-com",
     "The Japan Guy", "The Japan Guy",
     "jp_webmedia", "media",
     "https://thejapanguy.com/feed/",
     "en", "[\"news\",\"publisher:The Japan Guy\",\"culture\"]",
     3600,
     "The Japan Guy — culture")

/* jp-b43-tokyogirlsupdate-com moved to collectors/pivot/table/hp3b43_jprss.c
 * (2026-10-21): its 2 MB feed takes ~84 s and rss_collect() allows 25. */

RSSX(jp_b43_unseen_japan_com, "jp-b43-unseen-japan-com",
     "Unseen Japan", "Unseen Japan",
     "jp_webmedia", "media",
     "https://unseen-japan.com/feed/",
     "en", "[\"news\",\"publisher:Unseen Japan\",\"culture\"]",
     3600,
     "Unseen Japan — culture")

RSSX(jp_b43_wa_gokoro_jp, "jp-b43-wa-gokoro-jp",
     "wa-gokoro", "和ごころ",
     "jp_webmedia", "media",
     "https://wa-gokoro.jp/feed",
     "ja", "[\"news\",\"publisher:wa-gokoro\",\"culture\"]",
     3600,
     "wa-gokoro (和ごころ) — culture")

RSSX(jp_b43_wotopi_jp, "jp-b43-wotopi-jp",
     "Wotopi", "ウートピ",
     "jp_webmedia", "media",
     "https://wotopi.jp/feed",
     "ja", "[\"news\",\"publisher:Wotopi\",\"culture\"]",
     3600,
     "Wotopi (ウートピ) — culture")

/* ── Entertainment ─────────────────────────────────────────────── */

RSSX(jp_b43_animate_co_jp, "jp-b43-animate-co-jp",
     "Animate", "アニメイト",
     "jp_webmedia", "media",
     "https://www.animate.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Animate\",\"anime\"]",
     3600,
     "Animate (アニメイト) — anime")

RSSX(jp_b43_architecturephoto_net, "jp-b43-architecturephoto-net",
     "architecturephoto", "アーキテクチャーフォト",
     "jp_webmedia", "media",
     "https://architecturephoto.net/feed/",
     "ja", "[\"news\",\"publisher:architecturephoto\",\"architecture\"]",
     3600,
     "architecturephoto (アーキテクチャーフォト) — architecture")

RSSX(jp_b43_artscape_jp, "jp-b43-artscape-jp",
     "Artscape", "アートスケープ",
     "jp_webmedia", "media",
     "https://artscape.jp/feed",
     "ja", "[\"news\",\"publisher:Artscape\",\"art\"]",
     3600,
     "Artscape (アートスケープ) — art")

RSSX(jp_b43_booklive_jp, "jp-b43-booklive-jp",
     "BookLive", "BookLive",
     "jp_webmedia", "media",
     "https://booklive.jp/bkmr/feed",
     "ja", "[\"news\",\"publisher:BookLive\",\"entertainment\"]",
     3600,
     "BookLive — entertainment")

RSSX(jp_b43_comicbook_com, "jp-b43-comicbook-com",
     "ComicBook Anime", "ComicBook",
     "jp_webmedia", "media",
     "https://comicbook.com/anime/feed/",
     "en", "[\"news\",\"publisher:ComicBook Anime\",\"entertainment\"]",
     3600,
     "ComicBook Anime (ComicBook) — entertainment")

RSSX(jp_b43_doope_jp, "jp-b43-doope-jp",
     "doope!", "doope!",
     "jp_webmedia", "media",
     "https://doope.jp/feed/",
     "ja", "[\"news\",\"publisher:doope!\",\"gaming\"]",
     3600,
     "doope! — gaming")

RSSX(jp_b43_esports_world_jp, "jp-b43-esports-world-jp",
     "e-Sports World", "eスポーツワールド",
     "jp_webmedia", "media",
     "https://esports-world.jp/feed",
     "ja", "[\"news\",\"publisher:e-Sports World\",\"esports\"]",
     3600,
     "e-Sports World (eスポーツワールド) — esports")

RSSX(jp_b43_figsoku_net, "jp-b43-figsoku-net",
     "Figsoku", "fig速",
     "jp_webmedia", "media",
     "https://figsoku.net/feed/",
     "ja", "[\"news\",\"publisher:Figsoku\",\"otaku\",\"figures\"]",
     3600,
     "Figsoku (fig速) — otaku")

RSSX(jp_b43_filmaga_filmarks_com, "jp-b43-filmaga-filmarks-com",
     "Filmaga", "FILMAGA",
     "jp_webmedia", "media",
     "https://filmaga.filmarks.com/feed",
     "ja", "[\"news\",\"publisher:Filmaga\",\"film\"]",
     3600,
     "Filmaga (FILMAGA) — film")

RSSX(jp_b43_hon_hikidashi_jp, "jp-b43-hon-hikidashi-jp",
     "Hon Hikidashi", "ほんのひきだし",
     "jp_webmedia", "media",
     "https://hon-hikidashi.jp/feed/",
     "ja", "[\"news\",\"publisher:Hon Hikidashi\",\"entertainment\"]",
     3600,
     "Hon Hikidashi (ほんのひきだし) — entertainment")

RSSX(jp_b43_indienative_com, "jp-b43-indienative-com",
     "indienative", "indienative",
     "jp_webmedia", "media",
     "https://indienative.com/feed/",
     "ja", "[\"news\",\"publisher:indienative\",\"music\"]",
     3600,
     "indienative — music")

RSSX(jp_b43_jp_ign_com, "jp-b43-jp-ign-com",
     "IGN Japan", "IGN Japan",
     "jp_webmedia", "media",
     "https://jp.ign.com/feed.xml",
     "ja", "[\"news\",\"publisher:IGN Japan\",\"gaming\"]",
     3600,
     "IGN Japan — gaming")

RSSX(jp_b43_ln_news_com, "jp-b43-ln-news-com",
     "LN News", "ラノベニュースオンライン",
     "jp_webmedia", "media",
     "https://ln-news.com/feed/",
     "ja", "[\"news\",\"publisher:LN News\",\"entertainment\"]",
     3600,
     "LN News (ラノベニュースオンライン) — entertainment")

RSSX(jp_b43_moeyo_com, "jp-b43-moeyo-com",
     "Moeyo", "もえよん",
     "jp_webmedia", "media",
     "https://moeyo.com/feed",
     "ja", "[\"news\",\"publisher:Moeyo\",\"anime\"]",
     3600,
     "Moeyo (もえよん) — anime")

RSSX(jp_b43_nijimen_net, "jp-b43-nijimen-net",
     "nijimen", "にじめん",
     "jp_webmedia", "media",
     "https://nijimen.net/feed",
     "ja", "[\"news\",\"publisher:nijimen\",\"anime\"]",
     3600,
     "nijimen (にじめん) — anime")

RSSX(jp_b43_otakuusamagazine_com, "jp-b43-otakuusamagazine-com",
     "Otaku USA", "Otaku USA",
     "jp_webmedia", "media",
     "https://otakuusamagazine.com/feed/",
     "en", "[\"news\",\"publisher:Otaku USA\",\"entertainment\"]",
     3600,
     "Otaku USA — entertainment")

RSSX(jp_b43_otapol_jp, "jp-b43-otapol-jp",
     "Otapol", "おたぽる",
     "jp_webmedia", "media",
     "https://otapol.jp/feed/",
     "ja", "[\"news\",\"publisher:Otapol\",\"entertainment\"]",
     3600,
     "Otapol (おたぽる) — entertainment")

RSSX(jp_b43_seigura_com, "jp-b43-seigura-com",
     "Seigura (Seiyuu)", "声グラ",
     "jp_webmedia", "media",
     "https://seigura.com/feed/",
     "ja", "[\"news\",\"publisher:Seigura (Seiyuu)\",\"otaku\",\"figures\"]",
     3600,
     "Seigura (Seiyuu) (声グラ) — otaku")

RSSX(jp_b43_shuffle_genkosha_com, "jp-b43-shuffle-genkosha-com",
     "SHUFFLE", "シャッフル",
     "jp_webmedia", "media",
     "https://shuffle.genkosha.com/feed",
     "ja", "[\"news\",\"publisher:SHUFFLE\",\"photography\"]",
     3600,
     "SHUFFLE (シャッフル) — photo")

RSSX(jp_b43_spincoaster_com, "jp-b43-spincoaster-com",
     "Spincoaster", "Spincoaster",
     "jp_webmedia", "media",
     "https://spincoaster.com/feed",
     "ja", "[\"news\",\"publisher:Spincoaster\",\"music\"]",
     3600,
     "Spincoaster — music")

RSSX(jp_b43_tvfan_kyodo_co_jp, "jp-b43-tvfan-kyodo-co-jp",
     "TV Fan", "TV Fan",
     "jp_webmedia", "media",
     "https://tvfan.kyodo.co.jp/feed/",
     "ja", "[\"news\",\"publisher:TV Fan\",\"entertainment\"]",
     3600,
     "TV Fan — entertainment")

/* ── Sports / Fitness ──────────────────────────────────────────── */

RSSX(jp_b43_basketballking_jp, "jp-b43-basketballking-jp",
     "Basketball King", "バスケットボールキング",
     "jp_webmedia", "media",
     "https://basketballking.jp/feed/",
     "ja", "[\"news\",\"publisher:Basketball King\",\"sports\"]",
     3600,
     "Basketball King (バスケットボールキング) — sports")

RSSX(jp_b43_funq_jp, "jp-b43-funq-jp",
     "FunQ", "FunQ",
     "jp_webmedia", "media",
     "https://funq.jp/feed/",
     "ja", "[\"news\",\"publisher:FunQ\",\"cycling\"]",
     3600,
     "FunQ — cycle")

RSSX(jp_b43_funride_jp, "jp-b43-funride-jp",
     "FunRide", "ファンライド",
     "jp_webmedia", "media",
     "https://funride.jp/feed/",
     "ja", "[\"news\",\"publisher:FunRide\",\"sports\"]",
     3600,
     "FunRide (ファンライド) — sports")

RSSX(jp_b43_gonkaku_jp, "jp-b43-gonkaku-jp",
     "Gonkaku", "ゴング格闘技",
     "jp_webmedia", "media",
     "https://gonkaku.jp/feed/",
     "ja", "[\"news\",\"publisher:Gonkaku\",\"sports\"]",
     3600,
     "Gonkaku (ゴング格闘技) — sports")

RSSX(jp_b43_melos_media, "jp-b43-melos-media",
     "MELOS", "MELOS",
     "jp_webmedia", "media",
     "https://melos.media/feed/",
     "ja", "[\"news\",\"publisher:MELOS\",\"fitness\"]",
     3600,
     "MELOS — fitness")

RSSX(jp_b43_mmaplanet_jp, "jp-b43-mmaplanet-jp",
     "MMA Planet", "MMA Planet",
     "jp_webmedia", "media",
     "https://mmaplanet.jp/feed/",
     "ja", "[\"news\",\"publisher:MMA Planet\",\"sports\"]",
     3600,
     "MMA Planet — sports")

RSSX(jp_b43_rallys_online, "jp-b43-rallys-online",
     "Rallys Table Tennis", "Rallys",
     "jp_webmedia", "media",
     "https://rallys.online/feed/",
     "ja", "[\"news\",\"publisher:Rallys Table Tennis\",\"sports\"]",
     3600,
     "Rallys Table Tennis (Rallys) — sports")

RSSX(jp_b43_real_sports_jp, "jp-b43-real-sports-jp",
     "REAL SPORTS", "REAL SPORTS",
     "jp_webmedia", "media",
     "https://real-sports.jp/feed/",
     "ja", "[\"news\",\"publisher:REAL SPORTS\",\"sports\"]",
     3600,
     "REAL SPORTS — sports")

RSSX(jp_b43_rugbyrepublic_jp, "jp-b43-rugbyrepublic-jp",
     "Rugby Republic", "ラグビーリパブリック",
     "jp_webmedia", "media",
     "https://rugby-rp.com/feed/",
     "ja", "[\"news\",\"publisher:Rugby Republic\",\"sports\"]",
     3600,
     "Rugby Republic (ラグビーリパブリック) — sports")

RSSX(jp_b43_vitup_jp, "jp-b43-vitup-jp",
     "VITUP!", "VITUP!",
     "jp_webmedia", "media",
     "https://vitup.jp/feed/",
     "ja", "[\"news\",\"publisher:VITUP!\",\"fitness\"]",
     3600,
     "VITUP! — fitness")

RSSX(jp_b43_volleyballking_jp, "jp-b43-volleyballking-jp",
     "Volleyball King", "バレーボールキング",
     "jp_webmedia", "media",
     "https://volleyballking.jp/feed/",
     "ja", "[\"news\",\"publisher:Volleyball King\",\"sports\"]",
     3600,
     "Volleyball King (バレーボールキング) — sports")

/* ── Travel / Transport ────────────────────────────────────────── */

RSSX(jp_b43_bestcarweb_jp, "jp-b43-bestcarweb-jp",
     "Best Car Web", "ベストカーWeb",
     "jp_webmedia", "transport",
     "https://bestcarweb.jp/feed/",
     "ja", "[\"news\",\"publisher:Best Car Web\",\"automotive\"]",
     3600,
     "Best Car Web (ベストカーWeb) — car")

RSSX(jp_b43_camp_hack_com, "jp-b43-camp-hack-com",
     "CAMP HACK", "CAMP HACK",
     "jp_webmedia", "tourism",
     "https://camp-hack.com/feed",
     "ja", "[\"news\",\"publisher:CAMP HACK\",\"outdoor\"]",
     3600,
     "CAMP HACK — outdoor")

RSSX(jp_b43_camphack_nap_camp_com, "jp-b43-camphack-nap-camp-com",
     "CAMP HACK (nap-camp)", "キャンプハック",
     "jp_webmedia", "tourism",
     "https://camphack.nap-camp.com/feed",
     "ja", "[\"news\",\"publisher:CAMP HACK (nap-camp)\",\"outdoor\"]",
     3600,
     "CAMP HACK (nap-camp) (キャンプハック) — outdoor")

RSSX(jp_b43_co_trip_jp, "jp-b43-co-trip-jp",
     "co-trip", "ことりっぷ",
     "jp_webmedia", "tourism",
     "https://co-trip.jp/feed/",
     "ja", "[\"news\",\"publisher:co-trip\",\"travel\"]",
     3600,
     "co-trip (ことりっぷ) — travel")

RSSX(jp_b43_ev_times_com, "jp-b43-ev-times-com",
     "EV Times", "EVタイムス",
     "jp_webmedia", "transport",
     "https://ev-times.com/feed",
     "ja", "[\"news\",\"publisher:EV Times\",\"automotive\",\"ev\"]",
     3600,
     "EV Times (EVタイムス) — car ev")

RSSX(jp_b43_hokkaido_labo_com, "jp-b43-hokkaido-labo-com",
     "Hokkaido Labo", "北海道ラボ",
     "jp_webmedia", "tourism",
     "https://hokkaido-labo.com/feed",
     "ja", "[\"news\",\"publisher:Hokkaido Labo\",\"travel\"]",
     3600,
     "Hokkaido Labo (北海道ラボ) — travel")

RSSX(jp_b43_ishikawa19_com, "jp-b43-ishikawa19-com",
     "Ishikawa19", "いしかわ観光",
     "jp_webmedia", "tourism",
     "https://ishikawa19.com/feed/",
     "ja", "[\"news\",\"publisher:Ishikawa19\",\"travel\"]",
     3600,
     "Ishikawa19 (いしかわ観光) — travel")

RSSX(jp_b43_kurumanews_jp, "jp-b43-kurumanews-jp",
     "Kuruma News", "くるまのニュース",
     "jp_webmedia", "transport",
     "https://kuruma-news.jp/feed",
     "ja", "[\"news\",\"publisher:Kuruma News\",\"automotive\"]",
     3600,
     "Kuruma News (くるまのニュース) — car")

RSSX(jp_b43_motor_fan_jp, "jp-b43-motor-fan-jp",
     "Motor Fan", "Motor Fan",
     "jp_webmedia", "transport",
     "https://motor-fan.jp/feed",
     "ja", "[\"news\",\"publisher:Motor Fan\",\"automotive\"]",
     3600,
     "Motor Fan — car")

RSSX(jp_b43_okinawatravelinfo_com, "jp-b43-okinawatravelinfo-com",
     "Okinawa Travel Info", "沖縄旅行情報",
     "jp_webmedia", "tourism",
     "https://okinawatravelinfo.com/feed/",
     "en", "[\"news\",\"publisher:Okinawa Travel Info\",\"travel\"]",
     3600,
     "Okinawa Travel Info (沖縄旅行情報) — travel")

RSSX(jp_b43_sky_budget_com, "jp-b43-sky-budget-com",
     "Sky Budget", "スカイバジェット",
     "jp_webmedia", "transport",
     "https://sky-budget.com/feed/",
     "ja", "[\"news\",\"publisher:Sky Budget\",\"transport\",\"aviation\"]",
     3600,
     "Sky Budget (スカイバジェット) — aviation")

RSSX(jp_b43_tabippo_net, "jp-b43-tabippo-net",
     "TABIPPO", "TABIPPO",
     "jp_webmedia", "tourism",
     "https://tabippo.net/feed/",
     "ja", "[\"news\",\"publisher:TABIPPO\",\"travel\"]",
     3600,
     "TABIPPO — travel")

RSSX(jp_b43_tabiris_com, "jp-b43-tabiris-com",
     "Tabiris", "タビリス",
     "jp_webmedia", "transport",
     "https://tabiris.com/feed/",
     "ja", "[\"news\",\"publisher:Tabiris\",\"transport\",\"railway\"]",
     3600,
     "Tabiris (タビリス) — railway")

RSSX(jp_b43_travelersnavi_com, "jp-b43-travelersnavi-com",
     "Traveler's Navi", "旅行者ナビ",
     "jp_webmedia", "tourism",
     "https://travelersnavi.com/feed/",
     "ja", "[\"news\",\"publisher:Traveler's Navi\",\"travel\"]",
     3600,
     "Traveler's Navi (旅行者ナビ) — travel")

RSSX(jp_b43_tripeditor_com, "jp-b43-tripeditor-com",
     "TRiP EDiTOR", "TRiP EDiTOR",
     "jp_webmedia", "tourism",
     "https://tripeditor.com/feed/",
     "ja", "[\"news\",\"publisher:TRiP EDiTOR\",\"travel\"]",
     3600,
     "TRiP EDiTOR — travel")

/* ── Food / Drink ──────────────────────────────────────────────── */

RSSX(jp_b43_cookpad_diet_jp, "jp-b43-cookpad-diet-jp",
     "Cookpad Diet", "クックパッドダイエット",
     "jp_webmedia", "lifestyle",
     "https://cookpad-diet.jp/feed",
     "ja", "[\"news\",\"publisher:Cookpad Diet\",\"food\"]",
     3600,
     "Cookpad Diet (クックパッドダイエット) — food")

RSSX(jp_b43_food_stadium_com, "jp-b43-food-stadium-com",
     "Food Stadium", "フードスタジアム",
     "jp_webmedia", "lifestyle",
     "https://food-stadium.com/feed/",
     "ja", "[\"news\",\"publisher:Food Stadium\",\"food\"]",
     3600,
     "Food Stadium (フードスタジアム) — food")

RSSX(jp_b43_foodie_jp, "jp-b43-foodie-jp",
     "Foodie (Mitsukoshi Isetan)", "三越伊勢丹 FOODIE",
     "jp_webmedia", "lifestyle",
     "https://mi-journey.jp/foodie/feed/",
     "ja", "[\"news\",\"publisher:Foodie (Mitsukoshi Isetan)\",\"food\"]",
     3600,
     "Foodie (Mitsukoshi Isetan) (三越伊勢丹 FOODIE) — food")

RSSX(jp_b43_ippin_gourmet_com, "jp-b43-ippin-gourmet-com",
     "ippin", "ippin",
     "jp_webmedia", "lifestyle",
     "https://ippin-gourmet.com/feed",
     "ja", "[\"news\",\"publisher:ippin\",\"food\"]",
     3600,
     "ippin — food")

RSSX(jp_b43_irodori_co_jp, "jp-b43-irodori-co-jp",
     "irodori", "irodori",
     "jp_webmedia", "lifestyle",
     "https://irodori.co.jp/feed",
     "ja", "[\"news\",\"publisher:irodori\",\"food\"]",
     3600,
     "irodori — food")

RSSX(jp_b43_kurand_jp, "jp-b43-kurand-jp",
     "KURAND", "KURAND",
     "jp_webmedia", "lifestyle",
     "https://kurand.jp/blogs/magazine.atom",
     "ja", "[\"news\",\"publisher:KURAND\",\"food\"]",
     3600,
     "KURAND — food")

RSSX(jp_b43_mogumogunews_com, "jp-b43-mogumogunews-com",
     "Mogumogu News", "もぐもぐニュース",
     "jp_webmedia", "lifestyle",
     "https://mogumogunews.com/feed/",
     "ja", "[\"news\",\"publisher:Mogumogu News\",\"food\"]",
     3600,
     "Mogumogu News (もぐもぐニュース) — food")

RSSX(jp_b43_nomunication_jp, "jp-b43-nomunication-jp",
     "Nomunication", "Nomunication",
     "jp_webmedia", "lifestyle",
     "https://nomunication.jp/feed/",
     "en", "[\"news\",\"publisher:Nomunication\",\"food\"]",
     3600,
     "Nomunication — food")

RSSX(jp_b43_retty_news, "jp-b43-retty-news",
     "Retty", "Retty",
     "jp_webmedia", "lifestyle",
     "https://retty.news/feed/",
     "ja", "[\"news\",\"publisher:Retty\",\"food\"]",
     3600,
     "Retty — food")

RSSX(jp_b43_saketimes_jp, "jp-b43-saketimes-jp",
     "SAKETIMES", "SAKETIMES",
     "jp_webmedia", "lifestyle",
     "https://jp.sake-times.com/feed",
     "ja", "[\"news\",\"publisher:SAKETIMES\",\"food\"]",
     3600,
     "SAKETIMES — food")

/* ── Lifestyle / Fashion ───────────────────────────────────────── */

RSSX(jp_b43_cancam_jp, "jp-b43-cancam-jp",
     "CanCam", "CanCam",
     "jp_webmedia", "lifestyle",
     "https://cancam.jp/feed",
     "ja", "[\"news\",\"publisher:CanCam\",\"fashion\"]",
     3600,
     "CanCam — fashion")

RSSX(jp_b43_hobby_watch_impress_co_jp, "jp-b43-hobby-watch-impress-co-jp",
     "Hobby Watch", "ホビーWatch",
     "jp_webmedia", "lifestyle",
     "https://hobby.watch.impress.co.jp/data/rss/1.0/hbw/feed.rdf",
     "ja", "[\"news\",\"publisher:Hobby Watch\",\"hobby\"]",
     3600,
     "Hobby Watch (ホビーWatch) — hobby")

RSSX(jp_b43_iecolle_com, "jp-b43-iecolle-com",
     "iecolle", "イエコレクション",
     "jp_webmedia", "lifestyle",
     "https://iecolle.com/feed",
     "ja", "[\"news\",\"publisher:iecolle\",\"lifestyle\",\"home\"]",
     3600,
     "iecolle (イエコレクション) — home")

RSSX(jp_b43_josei_bigaku_jp, "jp-b43-josei-bigaku-jp",
     "Josei Bigaku", "女性の美学",
     "jp_webmedia", "lifestyle",
     "https://josei-bigaku.jp/feed/",
     "ja", "[\"news\",\"publisher:Josei Bigaku\",\"beauty\"]",
     3600,
     "Josei Bigaku (女性の美学) — beauty")

RSSX(jp_b43_kokocara_pal_system_co_jp, "jp-b43-kokocara-pal-system-co-jp",
     "kokocara", "こここら",
     "jp_webmedia", "lifestyle",
     "https://kokocara.pal-system.co.jp/feed/",
     "ja", "[\"news\",\"publisher:kokocara\",\"lifestyle\"]",
     3600,
     "kokocara (こここら) — lifestyle")

RSSX(jp_b43_locari_jp, "jp-b43-locari-jp",
     "Locari", "Locari",
     "jp_webmedia", "lifestyle",
     "https://locari.jp/feed",
     "ja", "[\"news\",\"publisher:Locari\",\"lifestyle\"]",
     3600,
     "Locari — lifestyle")

RSSX(jp_b43_lovegreen_net, "jp-b43-lovegreen-net",
     "LOVEGREEN", "LOVEGREEN",
     "jp_webmedia", "lifestyle",
     "https://lovegreen.net/feed/",
     "ja", "[\"news\",\"publisher:LOVEGREEN\",\"garden\"]",
     3600,
     "LOVEGREEN — garden")

RSSX(jp_b43_mendy_jp, "jp-b43-mendy-jp",
     "MENDY", "MENDY",
     "jp_webmedia", "lifestyle",
     "https://mendy.jp/feed",
     "ja", "[\"news\",\"publisher:MENDY\",\"lifestyle\"]",
     3600,
     "MENDY — lifestyle")

RSSX(jp_b43_mery_jp, "jp-b43-mery-jp",
     "MERY", "MERY",
     "jp_webmedia", "lifestyle",
     "https://mery.jp/feed",
     "ja", "[\"news\",\"publisher:MERY\",\"lifestyle\"]",
     3600,
     "MERY — lifestyle")

RSSX(jp_b43_moov_ooo, "jp-b43-moov-ooo",
     "Moov", "Moov Magazine",
     "jp_webmedia", "lifestyle",
     "https://moov.ooo/feed/",
     "ja", "[\"news\",\"publisher:Moov\",\"lifestyle\"]",
     3600,
     "Moov (Moov Magazine) — lifestyle")

RSSX(jp_b43_oggi_jp, "jp-b43-oggi-jp",
     "Oggi", "Oggi",
     "jp_webmedia", "lifestyle",
     "https://oggi.jp/feed",
     "ja", "[\"news\",\"publisher:Oggi\",\"fashion\"]",
     3600,
     "Oggi — fashion")

RSSX(jp_b43_psnews_jp, "jp-b43-psnews-jp",
     "PS News Dog", "PS保険ニュース 犬",
     "jp_webmedia", "lifestyle",
     "https://psnews.jp/dog/feed/",
     "ja", "[\"news\",\"publisher:PS News Dog\",\"lifestyle\"]",
     3600,
     "PS News Dog (PS保険ニュース 犬) — lifestyle")

RSSX(jp_b43_roomclip_jp, "jp-b43-roomclip-jp",
     "RoomClip", "ルームクリップ",
     "jp_webmedia", "lifestyle",
     "https://roomclip.jp/mag/feed",
     "ja", "[\"news\",\"publisher:RoomClip\",\"lifestyle\",\"diy\"]",
     3600,
     "RoomClip (ルームクリップ) — diy")

RSSX(jp_b43_serai_jp, "jp-b43-serai-jp",
     "Serai", "サライ",
     "jp_webmedia", "lifestyle",
     "https://serai.jp/feed",
     "ja", "[\"news\",\"publisher:Serai\",\"lifestyle\"]",
     3600,
     "Serai (サライ) — lifestyle")

RSSX(jp_b43_smartlog_jp, "jp-b43-smartlog-jp",
     "Smartlog", "Smartlog",
     "jp_webmedia", "lifestyle",
     "https://smartlog.jp/feed",
     "ja", "[\"news\",\"publisher:Smartlog\",\"lifestyle\"]",
     3600,
     "Smartlog — lifestyle")

RSSX(jp_b43_soen_tokyo, "jp-b43-soen-tokyo",
     "SO-EN", "装苑",
     "jp_webmedia", "lifestyle",
     "https://soen.tokyo/feed/",
     "ja", "[\"news\",\"publisher:SO-EN\",\"fashion\"]",
     3600,
     "SO-EN (装苑) — fashion")

RSSX(jp_b43_sumikawa_net, "jp-b43-sumikawa-net",
     "Sumikawa", "sumikawa",
     "jp_webmedia", "lifestyle",
     "https://sumikawa.net/feed/",
     "ja", "[\"news\",\"publisher:Sumikawa\",\"lifestyle\"]",
     3600,
     "Sumikawa (sumikawa) — lifestyle")

RSSX(jp_b43_tokila_jp, "jp-b43-tokila-jp",
     "TOKILA", "トキラ",
     "jp_webmedia", "lifestyle",
     "https://tokila.jp/feed/",
     "ja", "[\"news\",\"publisher:TOKILA\",\"beauty\"]",
     3600,
     "TOKILA (トキラ) — beauty")

RSSX(jp_b43_woman_mynavi_jp, "jp-b43-woman-mynavi-jp",
     "mynavi Woman", "マイナビウーマン",
     "jp_webmedia", "lifestyle",
     "https://woman.mynavi.jp/feed/",
     "ja", "[\"news\",\"publisher:mynavi Woman\",\"lifestyle\"]",
     3600,
     "mynavi Woman (マイナビウーマン) — lifestyle")

/* ── Parenting / Education ─────────────────────────────────────── */

RSSX(jp_b43_chiik_jp, "jp-b43-chiik-jp",
     "Chiik!", "Chiik!",
     "jp_webmedia", "lifestyle",
     "https://chiik.jp/feed/",
     "ja", "[\"news\",\"publisher:Chiik!\",\"parenting\"]",
     3600,
     "Chiik! — parenting")

RSSX(jp_b43_kodomo_manabi_labo_net, "jp-b43-kodomo-manabi-labo-net",
     "KodomoManabi", "こどもまなびラボ",
     "jp_webmedia", "lifestyle",
     "https://kodomo-manabi-labo.net/feed",
     "ja", "[\"news\",\"publisher:KodomoManabi\",\"parenting\"]",
     3600,
     "KodomoManabi (こどもまなびラボ) — parenting")

RSSX(jp_b43_kosodate_march_jp, "jp-b43-kosodate-march-jp",
     "Kosodate March", "子育てマーチ",
     "jp_webmedia", "lifestyle",
     "https://kosodate-march.jp/feed/",
     "ja", "[\"news\",\"publisher:Kosodate March\",\"parenting\"]",
     3600,
     "Kosodate March (子育てマーチ) — parenting")

RSSX(jp_b43_laxic_me, "jp-b43-laxic-me",
     "LAXIC", "LAXIC",
     "jp_webmedia", "lifestyle",
     "https://laxic.me/feed/",
     "ja", "[\"news\",\"publisher:LAXIC\",\"parenting\"]",
     3600,
     "LAXIC — parenting")

RSSX(jp_b43_mamapress_jp, "jp-b43-mamapress-jp",
     "MAMA PRESS", "ママプレス",
     "jp_webmedia", "lifestyle",
     "https://mamapress.jp/feed",
     "ja", "[\"news\",\"publisher:MAMA PRESS\",\"parenting\"]",
     3600,
     "MAMA PRESS (ママプレス) — parenting")

RSSX(jp_b43_ninaru_baby_net, "jp-b43-ninaru-baby-net",
     "ninaru baby", "ninaru baby",
     "jp_webmedia", "lifestyle",
     "https://ninaru-baby.net/feed",
     "ja", "[\"news\",\"publisher:ninaru baby\",\"parenting\"]",
     3600,
     "ninaru baby — parenting")

RSSX(jp_b43_resemom_jp, "jp-b43-resemom-jp",
     "ReseEd (ReseMom)", "リセマム",
     "jp_webmedia", "education",
     "https://resemom.jp/rss/index.rdf",
     "ja", "[\"news\",\"publisher:ReseEd (ReseMom)\",\"education\"]",
     3600,
     "ReseEd (ReseMom) (リセマム) — edu")

RSSX(jp_b43_up_to_you_me, "jp-b43-up-to-you-me",
     "UP to you!", "UP to you!",
     "jp_webmedia", "lifestyle",
     "https://up-to-you.me/feed/",
     "ja", "[\"news\",\"publisher:UP to you!\",\"parenting\"]",
     3600,
     "UP to you! — parenting")

/* ── Health ────────────────────────────────────────────────────── */

RSSX(jp_b43_answers_news_com, "jp-b43-answers-news-com",
     "Answers News", "AnswersNews",
     "jp_webmedia", "health",
     "https://answers.ten-navi.com/pharmanews/feed/",
     "ja", "[\"news\",\"publisher:Answers News\",\"health\"]",
     3600,
     "Answers News (AnswersNews) — health")

RSSX(jp_b43_fytte_jp, "jp-b43-fytte-jp",
     "FYTTE", "FYTTE",
     "jp_webmedia", "health",
     "https://fytte.jp/feed",
     "ja", "[\"news\",\"publisher:FYTTE\",\"health\"]",
     3600,
     "FYTTE — health")

/* ── Business / Finance ────────────────────────────────────────── */

RSSX(jp_b43_athome_inc_jp, "jp-b43-athome-inc-jp",
     "at home", "アットホーム",
     "jp_webmedia", "business",
     "https://athome-inc.jp/feed/",
     "ja", "[\"news\",\"publisher:at home\",\"realestate\"]",
     3600,
     "at home (アットホーム) — realestate")

RSSX(jp_b43_bizid_itmedia_co_jp, "jp-b43-bizid-itmedia-co-jp",
     "ITmedia Biz.ID", "ITmedia Biz.ID",
     "jp_webmedia", "business",
     "https://rss.itmedia.co.jp/rss/2.0/bizid.xml",
     "ja", "[\"news\",\"publisher:ITmedia Biz.ID\",\"business\"]",
     3600,
     "ITmedia Biz.ID — biz")

RSSX(jp_b43_crypto_times_jp, "jp-b43-crypto-times-jp",
     "Crypto Times", "CRYPTO TIMES",
     "jp_webmedia", "finance",
     "https://crypto-times.jp/feed/",
     "ja", "[\"news\",\"publisher:Crypto Times\",\"crypto\"]",
     3600,
     "Crypto Times (CRYPTO TIMES) — crypto")

RSSX(jp_b43_gaiax_socialmedialab_jp, "jp-b43-gaiax-socialmedialab-jp",
     "Gaiax Social Media Lab", "ガイアックス ソーシャルメディアラボ",
     "jp_webmedia", "business",
     "https://gaiax-socialmedialab.jp/feed/",
     "ja", "[\"news\",\"publisher:Gaiax Social Media Lab\",\"marketing\"]",
     3600,
     "Gaiax Social Media Lab (ガイアックス ソーシャルメディアラボ) — marketing")

RSSX(jp_b43_hedge_guide, "jp-b43-hedge-guide",
     "HEDGE GUIDE", "HEDGE GUIDE",
     "jp_webmedia", "business",
     "https://hedge.guide/feed",
     "ja", "[\"news\",\"publisher:HEDGE GUIDE\",\"business\"]",
     3600,
     "HEDGE GUIDE — business")

RSSX(jp_b43_iemone_jp, "jp-b43-iemone-jp",
     "Iemone", "イエモネ",
     "jp_webmedia", "business",
     "https://iemone.jp/feed/",
     "ja", "[\"news\",\"publisher:Iemone\",\"housing\"]",
     3600,
     "Iemone (イエモネ) — housing")

RSSX(jp_b43_liskul_com, "jp-b43-liskul-com",
     "LISKUL", "LISKUL",
     "jp_webmedia", "business",
     "https://liskul.com/feed",
     "ja", "[\"news\",\"publisher:LISKUL\",\"marketing\"]",
     3600,
     "LISKUL — marketing")

RSSX(jp_b43_marketeer_jp, "jp-b43-marketeer-jp",
     "Marketeer", "マーケティア",
     "jp_webmedia", "business",
     "https://marketeer.jp/feed/",
     "ja", "[\"news\",\"publisher:Marketeer\",\"marketing\"]",
     3600,
     "Marketeer (マーケティア) — marketing")

RSSX(jp_b43_marketing_itmedia_co_jp, "jp-b43-marketing-itmedia-co-jp",
     "ITmedia Marketing", "ITmedia マーケティング",
     "jp_webmedia", "business",
     "https://rss.itmedia.co.jp/rss/2.0/marketing.xml",
     "ja", "[\"news\",\"publisher:ITmedia Marketing\",\"marketing\"]",
     3600,
     "ITmedia Marketing (ITmedia マーケティング) — marketing")

RSSX(jp_b43_money_bu_jpx_com, "jp-b43-money-bu-jpx-com",
     "JPX Money Book", "JPX マネーBu",
     "jp_webmedia", "business",
     "https://money-bu-jpx.com/feed/",
     "ja", "[\"news\",\"publisher:JPX Money Book\",\"business\"]",
     3600,
     "JPX Money Book (JPX マネーBu) — business")

RSSX(jp_b43_money_lab_jp, "jp-b43-money-lab-jp",
     "Money Lab", "マネーラボ",
     "jp_webmedia", "finance",
     "https://money-lab.jp/feed/",
     "ja", "[\"news\",\"publisher:Money Lab\",\"finance\"]",
     3600,
     "Money Lab (マネーラボ) — finance")

RSSX(jp_b43_news_careerconnection_jp, "jp-b43-news-careerconnection-jp",
     "Career Connection", "キャリコネニュース",
     "jp_webmedia", "business",
     "https://news.careerconnection.jp/feed/",
     "ja", "[\"news\",\"publisher:Career Connection\",\"business\"]",
     3600,
     "Career Connection (キャリコネニュース) — business")

RSSX(jp_b43_o_uccino_jp, "jp-b43-o-uccino-jp",
     "O-uccino", "O-uccino",
     "jp_webmedia", "business",
     "https://o-uccino.jp/article/feed/",
     "ja", "[\"news\",\"publisher:O-uccino\",\"realestate\"]",
     3600,
     "O-uccino — realestate")

RSSX(jp_b43_realestate_co_jp, "jp-b43-realestate-co-jp",
     "Real Estate Japan", "Real Estate Japan",
     "jp_webmedia", "business",
     "https://resources.realestate.co.jp/feed/",
     "en", "[\"news\",\"publisher:Real Estate Japan\",\"housing\"]",
     3600,
     "Real Estate Japan — housing")

RSSX(jp_b43_seleqt_net, "jp-b43-seleqt-net",
     "Seleqt", "Seleqt",
     "jp_webmedia", "business",
     "https://seleqt.net/feed/",
     "ja", "[\"news\",\"publisher:Seleqt\",\"marketing\"]",
     3600,
     "Seleqt — marketing")

RSSX(jp_b43_sogyotecho_jp, "jp-b43-sogyotecho-jp",
     "Sogyotecho", "創業手帳",
     "jp_webmedia", "business",
     "https://sogyotecho.jp/feed/",
     "ja", "[\"news\",\"publisher:Sogyotecho\",\"business\"]",
     3600,
     "Sogyotecho (創業手帳) — business")

RSSX(jp_b43_venturetimes_jp, "jp-b43-venturetimes-jp",
     "Venture Times", "ベンチャータイムス",
     "jp_webmedia", "business",
     "https://venturetimes.jp/feed",
     "ja", "[\"news\",\"publisher:Venture Times\",\"business\",\"startup\"]",
     3600,
     "Venture Times (ベンチャータイムス) — biz startup")

/* ── Environment / Energy ──────────────────────────────────────── */

RSSX(jp_b43_alternas_jp, "jp-b43-alternas-jp",
     "Alterna", "オルタナ",
     "jp_webmedia", "environment",
     "https://www.alterna.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Alterna\",\"environment\"]",
     3600,
     "Alterna (オルタナ) — sustainability")

RSSX(jp_b43_energyshift_com, "jp-b43-energyshift-com",
     "EnergyShift", "EnergyShift",
     "jp_webmedia", "environment",
     "https://energyshift.com/feed",
     "ja", "[\"news\",\"publisher:EnergyShift\",\"energy\"]",
     3600,
     "EnergyShift — energy")

RSSX(jp_b43_greenz_jp, "jp-b43-greenz-jp",
     "greenz.jp", "greenz.jp",
     "jp_webmedia", "environment",
     "https://greenz.jp/feed/",
     "ja", "[\"news\",\"publisher:greenz.jp\",\"environment\"]",
     3600,
     "greenz.jp — sustainability")

RSSX(jp_b43_ideasforgood_jp, "jp-b43-ideasforgood-jp",
     "IDEAS FOR GOOD", "IDEAS FOR GOOD",
     "jp_webmedia", "environment",
     "https://ideasforgood.jp/feed/",
     "ja", "[\"news\",\"publisher:IDEAS FOR GOOD\",\"environment\"]",
     3600,
     "IDEAS FOR GOOD — environment")

/* ── Science / Research ────────────────────────────────────────── */

RSSX(jp_b43_karapaia_com, "jp-b43-karapaia-com",
     "Karapaia", "カラパイア",
     "jp_webmedia", "science",
     "https://karapaia.com/feed",
     "ja", "[\"news\",\"publisher:Karapaia\",\"science\"]",
     3600,
     "Karapaia (カラパイア) — science")

RSSX(jp_b43_spacemedia_jp, "jp-b43-spacemedia-jp",
     "Space Media", "Space Media",
     "jp_webmedia", "science",
     "https://spacemedia.jp/feed",
     "ja", "[\"news\",\"publisher:Space Media\",\"science\",\"space\"]",
     3600,
     "Space Media — science space")

RSSX(jp_b43_thinktank_php_co_jp, "jp-b43-thinktank-php-co-jp",
     "PHP Research Institute", "PHP総合研究所",
     "jp_webmedia", "science",
     "https://thinktank.php.co.jp/feed/",
     "ja", "[\"news\",\"publisher:PHP Research Institute\",\"research\"]",
     3600,
     "PHP Research Institute (PHP総合研究所) — research")

/* ── English-language Japan ────────────────────────────────────── */

RSSX(jp_b43_deepjapan_org, "jp-b43-deepjapan-org",
     "DeepJapan", "DeepJapan",
     "jp_webmedia", "media",
     "https://deepjapan.org/feed/",
     "en", "[\"news\",\"publisher:DeepJapan\",\"japan\",\"english\"]",
     3600,
     "DeepJapan — en japan")

RSSX(jp_b43_neojaponisme_com, "jp-b43-neojaponisme-com",
     "Neojaponisme", "Neojaponisme",
     "jp_webmedia", "media",
     "https://neojaponisme.com/feed/",
     "en", "[\"news\",\"publisher:Neojaponisme\",\"japan\",\"english\"]",
     3600,
     "Neojaponisme — en japan")

RSSX(jp_b43_teamjapanese_com, "jp-b43-teamjapanese-com",
     "Team Japanese", "Team Japanese",
     "jp_webmedia", "media",
     "https://teamjapanese.com/feed/",
     "en", "[\"news\",\"publisher:Team Japanese\",\"japan\",\"english\"]",
     3600,
     "Team Japanese — en japan")

RSSX(jp_b43_thetokyotourist_com, "jp-b43-thetokyotourist-com",
     "The Tokyo Tourist", "The Tokyo Tourist",
     "jp_webmedia", "media",
     "https://thetokyotourist.com/feed/",
     "en", "[\"news\",\"publisher:The Tokyo Tourist\",\"japan\",\"english\"]",
     3600,
     "The Tokyo Tourist — en japan")
