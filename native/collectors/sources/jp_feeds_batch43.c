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
     "social", "technology",
     "https://aizine.ai/feed/",
     "ja", "[\"news\",\"publisher:AI ZINE\",\"tech\",\"ai\"]",
     3600,
     "AI ZINE (AIZINE) — tech ai")

RSSX(jp_b43_applech2_com, "jp-b43-applech2-com",
     "applech2", "applech2",
     "social", "technology",
     "https://applech2.com/feed",
     "ja", "[\"news\",\"publisher:applech2\",\"tech\"]",
     3600,
     "applech2 — tech")

RSSX(jp_b43_bitwave_showcase_tv_com, "jp-b43-bitwave-showcase-tv-com",
     "bitWave", "bitWave",
     "social", "technology",
     "https://bitwave.showcase-tv.com/feed/",
     "ja", "[\"news\",\"publisher:bitWave\",\"tech\"]",
     3600,
     "bitWave — tech")

RSSX(jp_b43_builderscon_io, "jp-b43-builderscon-io",
     "builderscon blog", "builderscon",
     "social", "technology",
     "https://blog.builderscon.io/feed",
     "ja", "[\"news\",\"publisher:builderscon blog\",\"tech\"]",
     3600,
     "builderscon blog (builderscon) — tech")

RSSX(jp_b43_businessnetwork_jp, "jp-b43-businessnetwork-jp",
     "Business Network", "ビジネスネットワーク",
     "social", "technology",
     "https://businessnetwork.jp/feed",
     "ja", "[\"news\",\"publisher:Business Network\",\"tech\"]",
     3600,
     "Business Network (ビジネスネットワーク) — tech")

RSSX(jp_b43_buzzap_jp, "jp-b43-buzzap-jp",
     "BUZZAP!", "BUZZAP!",
     "social", "technology",
     "https://buzzap.jp/feed/",
     "ja", "[\"news\",\"publisher:BUZZAP!\",\"tech\",\"gadget\"]",
     3600,
     "BUZZAP! — tech gadget")

RSSX(jp_b43_eetimes_itmedia_co_jp, "jp-b43-eetimes-itmedia-co-jp",
     "EE Times Japan (ITmedia)", "EE Times Japan",
     "social", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/eetimes.xml",
     "ja", "[\"news\",\"publisher:EE Times Japan (ITmedia)\",\"tech\",\"semiconductor\"]",
     3600,
     "EE Times Japan (ITmedia) (EE Times Japan) — tech semi")

RSSX(jp_b43_enterprisezine_jp, "jp-b43-enterprisezine-jp",
     "EnterpriseZine", "EnterpriseZine",
     "social", "technology",
     "https://enterprisezine.jp/rss/new/20/index.xml",
     "ja", "[\"news\",\"publisher:EnterpriseZine\",\"tech\"]",
     3600,
     "EnterpriseZine — tech")

RSSX(jp_b43_fav_itmedia_co_jp, "jp-b43-fav-itmedia-co-jp",
     "ITmedia Fav", "ITmedia Fav",
     "social", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/fav.xml",
     "ja", "[\"news\",\"publisher:ITmedia Fav\",\"tech\"]",
     3600,
     "ITmedia Fav — tech")

RSSX(jp_b43_gadget_shot_com, "jp-b43-gadget-shot-com",
     "GadgetShot", "ガジェットショット",
     "social", "technology",
     "https://gadget-shot.com/feed",
     "ja", "[\"news\",\"publisher:GadgetShot\",\"tech\"]",
     3600,
     "GadgetShot (ガジェットショット) — tech")

RSSX(jp_b43_gori_me, "jp-b43-gori-me",
     "gori.me", "gori.me",
     "social", "technology",
     "https://gori.me/feed",
     "ja", "[\"news\",\"publisher:gori.me\",\"tech\"]",
     3600,
     "gori.me — tech")

RSSX(jp_b43_hackaday_com, "jp-b43-hackaday-com",
     "Hackaday", "Hackaday",
     "social", "technology",
     "https://hackaday.com/feed/",
     "en", "[\"news\",\"publisher:Hackaday\",\"tech\"]",
     3600,
     "Hackaday — tech")

RSSX(jp_b43_io_cyberdefense_jp, "jp-b43-io-cyberdefense-jp",
     "Cyber Defense Institute", "サイバーディフェンス研究所",
     "social", "technology",
     "https://io.cyberdefense.jp/feed",
     "ja", "[\"news\",\"publisher:Cyber Defense Institute\",\"security\"]",
     3600,
     "Cyber Defense Institute (サイバーディフェンス研究所) — security")

RSSX(jp_b43_iotnews_jp, "jp-b43-iotnews-jp",
     "IoT News", "IoTニュース",
     "social", "technology",
     "https://iotnews.jp/feed/",
     "ja", "[\"news\",\"publisher:IoT News\",\"tech\"]",
     3600,
     "IoT News (IoTニュース) — tech")

RSSX(jp_b43_iphone_mania_jp, "jp-b43-iphone-mania-jp",
     "iPhone Mania", "iPhone Mania",
     "social", "technology",
     "https://iphone-mania.jp/feed/",
     "ja", "[\"news\",\"publisher:iPhone Mania\",\"tech\"]",
     3600,
     "iPhone Mania — tech")

RSSX(jp_b43_jidounten_lab_com, "jp-b43-jidounten-lab-com",
     "Autonomous Driving Lab", "自動運転ラボ",
     "social", "technology",
     "https://jidounten-lab.com/feed",
     "ja", "[\"news\",\"publisher:Autonomous Driving Lab\",\"tech\",\"automotive\"]",
     3600,
     "Autonomous Driving Lab (自動運転ラボ) — tech auto")

RSSX(jp_b43_lifestyle_itmedia_co_jp, "jp-b43-lifestyle-itmedia-co-jp",
     "ITmedia Lifestyle", "ITmedia LifeStyle",
     "social", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/lifestyle.xml",
     "ja", "[\"news\",\"publisher:ITmedia Lifestyle\",\"tech\",\"lifestyle\"]",
     3600,
     "ITmedia Lifestyle (ITmedia LifeStyle) — tech lifestyle")

RSSX(jp_b43_mobile_itmedia_co_jp, "jp-b43-mobile-itmedia-co-jp",
     "ITmedia Mobile", "ITmedia Mobile",
     "social", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/mobile.xml",
     "ja", "[\"news\",\"publisher:ITmedia Mobile\",\"tech\",\"mobile\"]",
     3600,
     "ITmedia Mobile — tech mobile")

RSSX(jp_b43_mugendai_web_jp, "jp-b43-mugendai-web-jp",
     "Mugendai (IBM)", "Mugendai（無限大）",
     "social", "technology",
     "https://mugendai-web.jp/feed",
     "ja", "[\"news\",\"publisher:Mugendai (IBM)\",\"tech\"]",
     3600,
     "Mugendai (IBM) (Mugendai（無限大）) — tech")

RSSX(jp_b43_netafull_net, "jp-b43-netafull-net",
     "Netafull", "ネタフル",
     "social", "technology",
     "https://netafull.net/feed",
     "ja", "[\"news\",\"publisher:Netafull\",\"tech\"]",
     3600,
     "Netafull (ネタフル) — tech")

RSSX(jp_b43_panora_tokyo, "jp-b43-panora-tokyo",
     "PANORA VR", "PANORA",
     "social", "technology",
     "https://panora.tokyo/feed",
     "ja", "[\"news\",\"publisher:PANORA VR\",\"tech\",\"vr\"]",
     3600,
     "PANORA VR (PANORA) — niche tech")

RSSX(jp_b43_piyolog_hatenadiary_jp, "jp-b43-piyolog-hatenadiary-jp",
     "piyolog", "piyolog",
     "social", "technology",
     "https://piyolog.hatenadiary.jp/feed",
     "ja", "[\"news\",\"publisher:piyolog\",\"security\"]",
     3600,
     "piyolog — security")

RSSX(jp_b43_productzine_jp, "jp-b43-productzine-jp",
     "ProductZine", "ProductZine",
     "social", "technology",
     "https://productzine.jp/rss/new/20/index.xml",
     "ja", "[\"news\",\"publisher:ProductZine\",\"tech\"]",
     3600,
     "ProductZine — tech")

RSSX(jp_b43_s_max_jp, "jp-b43-s-max-jp",
     "S-MAX", "S-MAX",
     "social", "technology",
     "https://s-max.jp/feed",
     "ja", "[\"news\",\"publisher:S-MAX\",\"tech\",\"mobile\"]",
     3600,
     "S-MAX — tech mobile")

RSSX(jp_b43_smartjapan_itmedia_co_jp, "jp-b43-smartjapan-itmedia-co-jp",
     "Smart Japan (ITmedia)", "スマートジャパン",
     "social", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/smartjapan.xml",
     "ja", "[\"news\",\"publisher:Smart Japan (ITmedia)\",\"tech\",\"energy\"]",
     3600,
     "Smart Japan (ITmedia) (スマートジャパン) — tech energy")

RSSX(jp_b43_smatu_net, "jp-b43-smatu-net",
     "Smatu.net", "Smatu.net",
     "social", "technology",
     "https://smatu.net/feed/",
     "ja", "[\"news\",\"publisher:Smatu.net\",\"tech\"]",
     3600,
     "Smatu.net — tech")

RSSX(jp_b43_sumahosupportline_com, "jp-b43-sumahosupportline-com",
     "Sumaho Support Line", "スマホサポートライン",
     "social", "technology",
     "https://sumahosupportline.com/feed/",
     "ja", "[\"news\",\"publisher:Sumaho Support Line\",\"tech\"]",
     3600,
     "Sumaho Support Line (スマホサポートライン) — tech")

RSSX(jp_b43_taisy0_com, "jp-b43-taisy0-com",
     "Taisy0", "酔いどれオヤジのブログwp",
     "social", "technology",
     "https://taisy0.com/feed",
     "ja", "[\"news\",\"publisher:Taisy0\",\"tech\"]",
     3600,
     "Taisy0 (酔いどれオヤジのブログwp) — tech")

RSSX(jp_b43_techtarget_itmedia_co_jp, "jp-b43-techtarget-itmedia-co-jp",
     "TechTarget Japan", "TechTarget Japan",
     "social", "technology",
     "https://rss.itmedia.co.jp/rss/2.0/techtarget.xml",
     "ja", "[\"news\",\"publisher:TechTarget Japan\",\"tech\"]",
     3600,
     "TechTarget Japan — tech")

RSSX(jp_b43_teradas_net, "jp-b43-teradas-net",
     "Teradas", "寺田's",
     "social", "technology",
     "https://teradas.net/feed/",
     "ja", "[\"news\",\"publisher:Teradas\",\"tech\"]",
     3600,
     "Teradas (寺田's) — tech")

RSSX(jp_b43_touchlab_jp, "jp-b43-touchlab-jp",
     "Touch Lab", "Touch Lab",
     "social", "technology",
     "https://touchlab.jp/feed/",
     "ja", "[\"news\",\"publisher:Touch Lab\",\"tech\"]",
     3600,
     "Touch Lab — tech")

RSSX(jp_b43_usedoor_jp, "jp-b43-usedoor-jp",
     "usedoor", "usedoor",
     "social", "technology",
     "https://usedoor.jp/feed/",
     "ja", "[\"news\",\"publisher:usedoor\",\"tech\"]",
     3600,
     "usedoor — tech")

RSSX(jp_b43_videosalon_jp, "jp-b43-videosalon-jp",
     "VIDEO SALON", "ビデオSALON",
     "social", "technology",
     "https://videosalon.jp/feed/",
     "ja", "[\"news\",\"publisher:VIDEO SALON\",\"tech\",\"video\"]",
     3600,
     "VIDEO SALON (ビデオSALON) — tech video")

RSSX(jp_b43_web_camp_io, "jp-b43-web-camp-io",
     "Web Camp", "DMM WEBCAMP",
     "social", "technology",
     "https://web-camp.io/magazine/feed",
     "ja", "[\"news\",\"publisher:Web Camp\",\"tech\"]",
     3600,
     "Web Camp (DMM WEBCAMP) — tech")

RSSX(jp_b43_weblog_life_net, "jp-b43-weblog-life-net",
     "Weblog Life", "Weblog Life",
     "social", "technology",
     "https://weblog-life.net/feed/",
     "ja", "[\"news\",\"publisher:Weblog Life\",\"tech\"]",
     3600,
     "Weblog Life — tech")

/* ── Tech Blogs ────────────────────────────────────────────────── */

RSSX(jp_b43_caddi_tech, "jp-b43-caddi-tech",
     "CADDi Tech Blog", "CADDi テックブログ",
     "social", "technology",
     "https://caddi.tech/feed",
     "ja", "[\"news\",\"publisher:CADDi Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "CADDi Tech Blog (CADDi テックブログ) — tech blog")

RSSX(jp_b43_dev_classmethod_jp, "jp-b43-dev-classmethod-jp",
     "Classmethod Dev", "クラスメソッド",
     "social", "technology",
     "https://dev.classmethod.jp/feed/",
     "ja", "[\"news\",\"publisher:Classmethod Dev\",\"tech\",\"blog\"]",
     3600,
     "Classmethod Dev (クラスメソッド) — tech blog")

RSSX(jp_b43_devblog_thebase_in, "jp-b43-devblog-thebase-in",
     "BASE Dev Blog", "BASE開発チームブログ",
     "social", "technology",
     "https://devblog.thebase.in/feed",
     "ja", "[\"news\",\"publisher:BASE Dev Blog\",\"tech\",\"blog\"]",
     3600,
     "BASE Dev Blog (BASE開発チームブログ) — tech blog")

RSSX(jp_b43_developers_freee_co_jp, "jp-b43-developers-freee-co-jp",
     "freee Developers", "freee Developers",
     "social", "technology",
     "https://developers.freee.co.jp/feed",
     "ja", "[\"news\",\"publisher:freee Developers\",\"tech\",\"blog\"]",
     3600,
     "freee Developers — techblog")

RSSX(jp_b43_eng_blog_iij_ad_jp, "jp-b43-eng-blog-iij-ad-jp",
     "IIJ Engineers Blog", "IIJ Engineers Blog",
     "social", "technology",
     "https://eng-blog.iij.ad.jp/feed",
     "ja", "[\"news\",\"publisher:IIJ Engineers Blog\",\"tech\",\"blog\"]",
     3600,
     "IIJ Engineers Blog — tech blog")

RSSX(jp_b43_engineer_retty_me, "jp-b43-engineer-retty-me",
     "Retty Tech Blog", "Retty Tech Blog",
     "social", "technology",
     "https://engineer.retty.me/feed",
     "ja", "[\"news\",\"publisher:Retty Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "Retty Tech Blog — tech blog")

RSSX(jp_b43_labs_gree_jp, "jp-b43-labs-gree-jp",
     "GREE Engineers Blog", "GREE Engineers' Blog",
     "social", "technology",
     "https://labs.gree.jp/blog/feed/",
     "ja", "[\"news\",\"publisher:GREE Engineers Blog\",\"tech\",\"blog\"]",
     3600,
     "GREE Engineers Blog (GREE Engineers' Blog) — tech blog")

RSSX(jp_b43_made_livesense_co_jp, "jp-b43-made-livesense-co-jp",
     "Livesense Made Blog", "Livesense Made",
     "social", "technology",
     "https://made.livesense.co.jp/feed",
     "ja", "[\"news\",\"publisher:Livesense Made Blog\",\"tech\",\"blog\"]",
     3600,
     "Livesense Made Blog (Livesense Made) — tech blog")

RSSX(jp_b43_moneyforward_dev_jp, "jp-b43-moneyforward-dev-jp",
     "MoneyForward Dev Blog", "マネーフォワード エンジニアブログ",
     "social", "technology",
     "https://moneyforward-dev.jp/feed",
     "ja", "[\"news\",\"publisher:MoneyForward Dev Blog\",\"tech\",\"blog\"]",
     3600,
     "MoneyForward Dev Blog (マネーフォワード エンジニアブログ) — tech blog")

RSSX(jp_b43_paiza_hatenablog_com, "jp-b43-paiza-hatenablog-com",
     "paiza blog", "paiza 開発日誌",
     "social", "technology",
     "https://paiza.hatenablog.com/feed",
     "ja", "[\"news\",\"publisher:paiza blog\",\"tech\",\"blog\"]",
     3600,
     "paiza blog (paiza 開発日誌) — tech blog")

RSSX(jp_b43_product_st_inc, "jp-b43-product-st-inc",
     "Sansan Product Blog", "Sansan Product Blog",
     "social", "technology",
     "https://product.st.inc/feed",
     "ja", "[\"news\",\"publisher:Sansan Product Blog\",\"tech\",\"blog\"]",
     3600,
     "Sansan Product Blog — techblog")

RSSX(jp_b43_tech_blog_optim_co_jp, "jp-b43-tech-blog-optim-co-jp",
     "OPTiM Tech Blog", "OPTiM TECH BLOG",
     "social", "technology",
     "https://tech-blog.optim.co.jp/feed",
     "ja", "[\"news\",\"publisher:OPTiM Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "OPTiM Tech Blog (OPTiM TECH BLOG) — tech blog")

RSSX(jp_b43_tech_andpad_co_jp, "jp-b43-tech-andpad-co-jp",
     "ANDPAD Tech", "ANDPAD Tech Blog",
     "social", "technology",
     "https://tech.andpad.co.jp/feed",
     "ja", "[\"news\",\"publisher:ANDPAD Tech\",\"tech\",\"blog\"]",
     3600,
     "ANDPAD Tech (ANDPAD Tech Blog) — tech blog")

RSSX(jp_b43_tech_drecom_co_jp, "jp-b43-tech-drecom-co-jp",
     "Drecom Tech", "Drecom Tech Blog",
     "social", "technology",
     "https://tech.drecom.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Drecom Tech\",\"tech\",\"blog\"]",
     3600,
     "Drecom Tech (Drecom Tech Blog) — tech blog")

RSSX(jp_b43_tech_every_tv, "jp-b43-tech-every-tv",
     "every Tech", "every Tech Blog",
     "social", "technology",
     "https://tech.every.tv/feed",
     "ja", "[\"news\",\"publisher:every Tech\",\"tech\",\"blog\"]",
     3600,
     "every Tech (every Tech Blog) — tech blog")

RSSX(jp_b43_tech_gunosy_io, "jp-b43-tech-gunosy-io",
     "Gunosy Tech Blog", "Gunosy テックブログ",
     "social", "technology",
     "https://tech.gunosy.io/feed",
     "ja", "[\"news\",\"publisher:Gunosy Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "Gunosy Tech Blog (Gunosy テックブログ) — tech blog")

RSSX(jp_b43_tech_layerx_co_jp, "jp-b43-tech-layerx-co-jp",
     "LayerX Tech Blog", "LayerX テックブログ",
     "social", "technology",
     "https://tech.layerx.co.jp/feed",
     "ja", "[\"news\",\"publisher:LayerX Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "LayerX Tech Blog (LayerX テックブログ) — tech blog")

RSSX(jp_b43_tech_medpeer_co_jp, "jp-b43-tech-medpeer-co-jp",
     "MedPeer Tech Blog", "MedPeer Tech Blog",
     "social", "technology",
     "https://tech.medpeer.co.jp/feed",
     "ja", "[\"news\",\"publisher:MedPeer Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "MedPeer Tech Blog — techblog")

RSSX(jp_b43_tech_pepabo_com, "jp-b43-tech-pepabo-com",
     "GMO Pepabo Tech", "GMO ペパボ テックブログ",
     "social", "technology",
     "https://tech.pepabo.com/feed.xml",
     "ja", "[\"news\",\"publisher:GMO Pepabo Tech\",\"tech\",\"blog\"]",
     3600,
     "GMO Pepabo Tech (GMO ペパボ テックブログ) — tech blog")

RSSX(jp_b43_tech_timee_co_jp, "jp-b43-tech-timee-co-jp",
     "Timee Tech Blog", "タイミー テックブログ",
     "social", "technology",
     "https://tech.timee.co.jp/feed",
     "ja", "[\"news\",\"publisher:Timee Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "Timee Tech Blog (タイミー テックブログ) — tech blog")

RSSX(jp_b43_tech_willgate_co_jp, "jp-b43-tech-willgate-co-jp",
     "WillGate Tech Blog", "ウィルゲートTech Blog",
     "social", "technology",
     "https://tech.willgate.co.jp/feed",
     "ja", "[\"news\",\"publisher:WillGate Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "WillGate Tech Blog (ウィルゲートTech Blog) — techblog")

RSSX(jp_b43_techblog_lycorp_co_jp, "jp-b43-techblog-lycorp-co-jp",
     "LY Corp Tech Blog", "LY Corporation テックブログ",
     "social", "technology",
     "https://techblog.lycorp.co.jp/ja/feed/index.xml",
     "ja", "[\"news\",\"publisher:LY Corp Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "LY Corp Tech Blog (LY Corporation テックブログ) — tech blog")

RSSX(jp_b43_techblog_zozo_com, "jp-b43-techblog-zozo-com",
     "ZOZO Tech Blog", "ZOZO テックブログ",
     "social", "technology",
     "https://techblog.zozo.com/feed",
     "ja", "[\"news\",\"publisher:ZOZO Tech Blog\",\"tech\",\"blog\"]",
     3600,
     "ZOZO Tech Blog (ZOZO テックブログ) — tech blog")

/* ── Web Design / Dev ──────────────────────────────────────────── */

RSSX(jp_b43_baigie_me, "jp-b43-baigie-me",
     "Baigie Blog", "ベイジブログ",
     "social", "technology",
     "https://baigie.me/officialblog/feed/",
     "ja", "[\"news\",\"publisher:Baigie Blog\",\"design\",\"webdev\"]",
     3600,
     "Baigie Blog (ベイジブログ) — webdesign")

RSSX(jp_b43_coliss_com, "jp-b43-coliss-com",
     "Coliss", "コリス",
     "social", "technology",
     "https://coliss.com/feed/",
     "ja", "[\"news\",\"publisher:Coliss\",\"tech\",\"webdev\"]",
     3600,
     "Coliss (コリス) — tech webdev")

RSSX(jp_b43_commte_net, "jp-b43-commte-net",
     "commte", "commte",
     "social", "technology",
     "https://commte.net/feed",
     "ja", "[\"news\",\"publisher:commte\",\"design\",\"webdev\"]",
     3600,
     "commte — design")

RSSX(jp_b43_findy_code_io, "jp-b43-findy-code-io",
     "Findy Engineer Lab", "Findy Engineer Lab",
     "social", "technology",
     "https://findy-code.io/engineer-lab/feed",
     "ja", "[\"news\",\"publisher:Findy Engineer Lab\",\"tech\",\"career\"]",
     3600,
     "Findy Engineer Lab — tech career")

RSSX(jp_b43_jser_info, "jp-b43-jser-info",
     "JSer.info", "JSer.info",
     "social", "technology",
     "https://jser.info/rss",
     "ja", "[\"news\",\"publisher:JSer.info\",\"tech\",\"webdev\"]",
     3600,
     "JSer.info — tech webdev")

RSSX(jp_b43_liginc_co_jp, "jp-b43-liginc-co-jp",
     "LIG Blog", "LIG",
     "social", "technology",
     "https://liginc.co.jp/feed",
     "ja", "[\"news\",\"publisher:LIG Blog\",\"tech\",\"webdev\"]",
     3600,
     "LIG Blog (LIG) — tech webdev")

RSSX(jp_b43_photoshopvip_net, "jp-b43-photoshopvip-net",
     "PhotoshopVIP", "PhotoshopVIP",
     "social", "technology",
     "https://photoshopvip.net/feed",
     "ja", "[\"news\",\"publisher:PhotoshopVIP\",\"tech\",\"design\"]",
     3600,
     "PhotoshopVIP — tech design")

RSSX(jp_b43_webdesignfacts_net, "jp-b43-webdesignfacts-net",
     "Web Design Facts", "ウェブデザインファクツ",
     "social", "technology",
     "https://webdesignfacts.net/feed/",
     "ja", "[\"news\",\"publisher:Web Design Facts\",\"design\",\"webdev\"]",
     3600,
     "Web Design Facts (ウェブデザインファクツ) — webdesign")

RSSX(jp_b43_webdesignmagazine_net, "jp-b43-webdesignmagazine-net",
     "Web Design Magazine", "Webデザインマガジン",
     "social", "technology",
     "https://webdesignmagazine.net/feed/",
     "ja", "[\"news\",\"publisher:Web Design Magazine\",\"design\",\"webdev\"]",
     3600,
     "Web Design Magazine (Webデザインマガジン) — design")

/* ── News / Media ──────────────────────────────────────────────── */

RSSX(jp_b43_asiatimes_com, "jp-b43-asiatimes-com",
     "Asia Times", "Asia Times",
     "social", "media",
     "https://asiatimes.com/feed/",
     "en", "[\"news\",\"publisher:Asia Times\",\"media\"]",
     3600,
     "Asia Times — news")

RSSX(jp_b43_nikkan_spa_jp, "jp-b43-nikkan-spa-jp",
     "Nikkan SPA!", "日刊SPA!",
     "social", "media",
     "https://nikkan-spa.jp/feed",
     "ja", "[\"news\",\"publisher:Nikkan SPA!\",\"media\"]",
     3600,
     "Nikkan SPA! (日刊SPA!) — news")

RSSX(jp_b43_turns_jp, "jp-b43-turns-jp",
     "TURNS", "TURNS",
     "social", "media",
     "https://turns.jp/feed",
     "ja", "[\"news\",\"publisher:TURNS\",\"regional\"]",
     3600,
     "TURNS — regional")

/* ── Culture / Society ─────────────────────────────────────────── */

RSSX(jp_b43_buzzmag_jp, "jp-b43-buzzmag-jp",
     "BuzzMag", "バズマグ",
     "social", "media",
     "https://buzzmag.jp/feed",
     "ja", "[\"news\",\"publisher:BuzzMag\",\"culture\"]",
     3600,
     "BuzzMag (バズマグ) — culture")

RSSX(jp_b43_casabrutus_com, "jp-b43-casabrutus-com",
     "Casa BRUTUS", "Casa BRUTUS",
     "social", "media",
     "https://casabrutus.com/feed",
     "ja", "[\"news\",\"publisher:Casa BRUTUS\",\"culture\",\"design\"]",
     3600,
     "Casa BRUTUS — culture design")

RSSX(jp_b43_coconutsjapan_com, "jp-b43-coconutsjapan-com",
     "Coconuts Japan", "ココナッツジャパン",
     "social", "media",
     "https://coconutsjapan.com/feed",
     "ja", "[\"news\",\"publisher:Coconuts Japan\",\"culture\"]",
     3600,
     "Coconuts Japan (ココナッツジャパン) — culture")

RSSX(jp_b43_fundo_jp, "jp-b43-fundo-jp",
     "FUNDO", "FUNDO",
     "social", "media",
     "https://fundo.jp/feed",
     "ja", "[\"news\",\"publisher:FUNDO\",\"culture\"]",
     3600,
     "FUNDO — culture")

RSSX(jp_b43_gaijinass_com, "jp-b43-gaijinass-com",
     "GaijinAss", "GaijinAss",
     "social", "media",
     "https://gaijinass.com/feed/",
     "en", "[\"news\",\"publisher:GaijinAss\",\"culture\"]",
     3600,
     "GaijinAss — culture")

RSSX(jp_b43_japaaan_com, "jp-b43-japaaan-com",
     "Japaaan Magazine", "Japaaan",
     "social", "media",
     "https://mag.japaaan.com/feed",
     "ja", "[\"news\",\"publisher:Japaaan Magazine\",\"culture\"]",
     3600,
     "Japaaan Magazine (Japaaan) — culture")

RSSX(jp_b43_kokoro_jp_com, "jp-b43-kokoro-jp-com",
     "Kokoro JP", "ココロJP",
     "social", "media",
     "https://kokoro-jp.com/feed/",
     "en", "[\"news\",\"publisher:Kokoro JP\",\"culture\"]",
     3600,
     "Kokoro JP (ココロJP) — culture")

RSSX(jp_b43_ninjareports_com, "jp-b43-ninjareports-com",
     "Ninja Reports", "忍者レポート",
     "social", "media",
     "https://ninjareports.com/feed/",
     "en", "[\"news\",\"publisher:Ninja Reports\",\"japan\"]",
     3600,
     "Ninja Reports (忍者レポート) — japan")

RSSX(jp_b43_omocoro_jp, "jp-b43-omocoro-jp",
     "Omocoro", "オモコロ",
     "social", "media",
     "https://omocoro.jp/feed",
     "ja", "[\"news\",\"publisher:Omocoro\",\"culture\"]",
     3600,
     "Omocoro (オモコロ) — culture")

RSSX(jp_b43_thejapanguy_com, "jp-b43-thejapanguy-com",
     "The Japan Guy", "The Japan Guy",
     "social", "media",
     "https://thejapanguy.com/feed/",
     "en", "[\"news\",\"publisher:The Japan Guy\",\"culture\"]",
     3600,
     "The Japan Guy — culture")

RSSX(jp_b43_tokyogirlsupdate_com, "jp-b43-tokyogirlsupdate-com",
     "Tokyo Girls Update", "Tokyo Girls Update",
     "social", "media",
     "https://tokyogirlsupdate.com/feed",
     "en", "[\"news\",\"publisher:Tokyo Girls Update\",\"culture\"]",
     3600,
     "Tokyo Girls Update — culture")

RSSX(jp_b43_unseen_japan_com, "jp-b43-unseen-japan-com",
     "Unseen Japan", "Unseen Japan",
     "social", "media",
     "https://unseen-japan.com/feed/",
     "en", "[\"news\",\"publisher:Unseen Japan\",\"culture\"]",
     3600,
     "Unseen Japan — culture")

RSSX(jp_b43_wa_gokoro_jp, "jp-b43-wa-gokoro-jp",
     "wa-gokoro", "和ごころ",
     "social", "media",
     "https://wa-gokoro.jp/feed",
     "ja", "[\"news\",\"publisher:wa-gokoro\",\"culture\"]",
     3600,
     "wa-gokoro (和ごころ) — culture")

RSSX(jp_b43_wotopi_jp, "jp-b43-wotopi-jp",
     "Wotopi", "ウートピ",
     "social", "media",
     "https://wotopi.jp/feed",
     "ja", "[\"news\",\"publisher:Wotopi\",\"culture\"]",
     3600,
     "Wotopi (ウートピ) — culture")

/* ── Entertainment ─────────────────────────────────────────────── */

RSSX(jp_b43_animate_co_jp, "jp-b43-animate-co-jp",
     "Animate", "アニメイト",
     "social", "media",
     "https://www.animate.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Animate\",\"anime\"]",
     3600,
     "Animate (アニメイト) — anime")

RSSX(jp_b43_architecturephoto_net, "jp-b43-architecturephoto-net",
     "architecturephoto", "アーキテクチャーフォト",
     "social", "media",
     "https://architecturephoto.net/feed/",
     "ja", "[\"news\",\"publisher:architecturephoto\",\"architecture\"]",
     3600,
     "architecturephoto (アーキテクチャーフォト) — architecture")

RSSX(jp_b43_artscape_jp, "jp-b43-artscape-jp",
     "Artscape", "アートスケープ",
     "social", "media",
     "https://artscape.jp/feed",
     "ja", "[\"news\",\"publisher:Artscape\",\"art\"]",
     3600,
     "Artscape (アートスケープ) — art")

RSSX(jp_b43_booklive_jp, "jp-b43-booklive-jp",
     "BookLive", "BookLive",
     "social", "media",
     "https://booklive.jp/bkmr/feed",
     "ja", "[\"news\",\"publisher:BookLive\",\"entertainment\"]",
     3600,
     "BookLive — entertainment")

RSSX(jp_b43_comicbook_com, "jp-b43-comicbook-com",
     "ComicBook Anime", "ComicBook",
     "social", "media",
     "https://comicbook.com/anime/feed/",
     "en", "[\"news\",\"publisher:ComicBook Anime\",\"entertainment\"]",
     3600,
     "ComicBook Anime (ComicBook) — entertainment")

RSSX(jp_b43_doope_jp, "jp-b43-doope-jp",
     "doope!", "doope!",
     "social", "media",
     "https://doope.jp/feed/",
     "ja", "[\"news\",\"publisher:doope!\",\"gaming\"]",
     3600,
     "doope! — gaming")

RSSX(jp_b43_esports_world_jp, "jp-b43-esports-world-jp",
     "e-Sports World", "eスポーツワールド",
     "social", "media",
     "https://esports-world.jp/feed",
     "ja", "[\"news\",\"publisher:e-Sports World\",\"esports\"]",
     3600,
     "e-Sports World (eスポーツワールド) — esports")

RSSX(jp_b43_figsoku_net, "jp-b43-figsoku-net",
     "Figsoku", "fig速",
     "social", "media",
     "https://figsoku.net/feed/",
     "ja", "[\"news\",\"publisher:Figsoku\",\"otaku\",\"figures\"]",
     3600,
     "Figsoku (fig速) — otaku")

RSSX(jp_b43_filmaga_filmarks_com, "jp-b43-filmaga-filmarks-com",
     "Filmaga", "FILMAGA",
     "social", "media",
     "https://filmaga.filmarks.com/feed",
     "ja", "[\"news\",\"publisher:Filmaga\",\"film\"]",
     3600,
     "Filmaga (FILMAGA) — film")

RSSX(jp_b43_hon_hikidashi_jp, "jp-b43-hon-hikidashi-jp",
     "Hon Hikidashi", "ほんのひきだし",
     "social", "media",
     "https://hon-hikidashi.jp/feed/",
     "ja", "[\"news\",\"publisher:Hon Hikidashi\",\"entertainment\"]",
     3600,
     "Hon Hikidashi (ほんのひきだし) — entertainment")

RSSX(jp_b43_indienative_com, "jp-b43-indienative-com",
     "indienative", "indienative",
     "social", "media",
     "https://indienative.com/feed/",
     "ja", "[\"news\",\"publisher:indienative\",\"music\"]",
     3600,
     "indienative — music")

RSSX(jp_b43_jp_ign_com, "jp-b43-jp-ign-com",
     "IGN Japan", "IGN Japan",
     "social", "media",
     "https://jp.ign.com/feed.xml",
     "ja", "[\"news\",\"publisher:IGN Japan\",\"gaming\"]",
     3600,
     "IGN Japan — gaming")

RSSX(jp_b43_ln_news_com, "jp-b43-ln-news-com",
     "LN News", "ラノベニュースオンライン",
     "social", "media",
     "https://ln-news.com/feed/",
     "ja", "[\"news\",\"publisher:LN News\",\"entertainment\"]",
     3600,
     "LN News (ラノベニュースオンライン) — entertainment")

RSSX(jp_b43_moeyo_com, "jp-b43-moeyo-com",
     "Moeyo", "もえよん",
     "social", "media",
     "https://moeyo.com/feed",
     "ja", "[\"news\",\"publisher:Moeyo\",\"anime\"]",
     3600,
     "Moeyo (もえよん) — anime")

RSSX(jp_b43_nijimen_net, "jp-b43-nijimen-net",
     "nijimen", "にじめん",
     "social", "media",
     "https://nijimen.net/feed",
     "ja", "[\"news\",\"publisher:nijimen\",\"anime\"]",
     3600,
     "nijimen (にじめん) — anime")

RSSX(jp_b43_otakuusamagazine_com, "jp-b43-otakuusamagazine-com",
     "Otaku USA", "Otaku USA",
     "social", "media",
     "https://otakuusamagazine.com/feed/",
     "en", "[\"news\",\"publisher:Otaku USA\",\"entertainment\"]",
     3600,
     "Otaku USA — entertainment")

RSSX(jp_b43_otapol_jp, "jp-b43-otapol-jp",
     "Otapol", "おたぽる",
     "social", "media",
     "https://otapol.jp/feed/",
     "ja", "[\"news\",\"publisher:Otapol\",\"entertainment\"]",
     3600,
     "Otapol (おたぽる) — entertainment")

RSSX(jp_b43_seigura_com, "jp-b43-seigura-com",
     "Seigura (Seiyuu)", "声グラ",
     "social", "media",
     "https://seigura.com/feed/",
     "ja", "[\"news\",\"publisher:Seigura (Seiyuu)\",\"otaku\",\"figures\"]",
     3600,
     "Seigura (Seiyuu) (声グラ) — otaku")

RSSX(jp_b43_shuffle_genkosha_com, "jp-b43-shuffle-genkosha-com",
     "SHUFFLE", "シャッフル",
     "social", "media",
     "https://shuffle.genkosha.com/feed",
     "ja", "[\"news\",\"publisher:SHUFFLE\",\"photography\"]",
     3600,
     "SHUFFLE (シャッフル) — photo")

RSSX(jp_b43_spincoaster_com, "jp-b43-spincoaster-com",
     "Spincoaster", "Spincoaster",
     "social", "media",
     "https://spincoaster.com/feed",
     "ja", "[\"news\",\"publisher:Spincoaster\",\"music\"]",
     3600,
     "Spincoaster — music")

RSSX(jp_b43_tvfan_kyodo_co_jp, "jp-b43-tvfan-kyodo-co-jp",
     "TV Fan", "TV Fan",
     "social", "media",
     "https://tvfan.kyodo.co.jp/feed/",
     "ja", "[\"news\",\"publisher:TV Fan\",\"entertainment\"]",
     3600,
     "TV Fan — entertainment")

/* ── Sports / Fitness ──────────────────────────────────────────── */

RSSX(jp_b43_basketballking_jp, "jp-b43-basketballking-jp",
     "Basketball King", "バスケットボールキング",
     "social", "media",
     "https://basketballking.jp/feed/",
     "ja", "[\"news\",\"publisher:Basketball King\",\"sports\"]",
     3600,
     "Basketball King (バスケットボールキング) — sports")

RSSX(jp_b43_funq_jp, "jp-b43-funq-jp",
     "FunQ", "FunQ",
     "social", "media",
     "https://funq.jp/feed/",
     "ja", "[\"news\",\"publisher:FunQ\",\"cycling\"]",
     3600,
     "FunQ — cycle")

RSSX(jp_b43_funride_jp, "jp-b43-funride-jp",
     "FunRide", "ファンライド",
     "social", "media",
     "https://funride.jp/feed/",
     "ja", "[\"news\",\"publisher:FunRide\",\"sports\"]",
     3600,
     "FunRide (ファンライド) — sports")

RSSX(jp_b43_gonkaku_jp, "jp-b43-gonkaku-jp",
     "Gonkaku", "ゴング格闘技",
     "social", "media",
     "https://gonkaku.jp/feed/",
     "ja", "[\"news\",\"publisher:Gonkaku\",\"sports\"]",
     3600,
     "Gonkaku (ゴング格闘技) — sports")

RSSX(jp_b43_melos_media, "jp-b43-melos-media",
     "MELOS", "MELOS",
     "social", "media",
     "https://melos.media/feed/",
     "ja", "[\"news\",\"publisher:MELOS\",\"fitness\"]",
     3600,
     "MELOS — fitness")

RSSX(jp_b43_mmaplanet_jp, "jp-b43-mmaplanet-jp",
     "MMA Planet", "MMA Planet",
     "social", "media",
     "https://mmaplanet.jp/feed/",
     "ja", "[\"news\",\"publisher:MMA Planet\",\"sports\"]",
     3600,
     "MMA Planet — sports")

RSSX(jp_b43_rallys_online, "jp-b43-rallys-online",
     "Rallys Table Tennis", "Rallys",
     "social", "media",
     "https://rallys.online/feed/",
     "ja", "[\"news\",\"publisher:Rallys Table Tennis\",\"sports\"]",
     3600,
     "Rallys Table Tennis (Rallys) — sports")

RSSX(jp_b43_real_sports_jp, "jp-b43-real-sports-jp",
     "REAL SPORTS", "REAL SPORTS",
     "social", "media",
     "https://real-sports.jp/feed/",
     "ja", "[\"news\",\"publisher:REAL SPORTS\",\"sports\"]",
     3600,
     "REAL SPORTS — sports")

RSSX(jp_b43_rugbyrepublic_jp, "jp-b43-rugbyrepublic-jp",
     "Rugby Republic", "ラグビーリパブリック",
     "social", "media",
     "https://rugby-rp.com/feed/",
     "ja", "[\"news\",\"publisher:Rugby Republic\",\"sports\"]",
     3600,
     "Rugby Republic (ラグビーリパブリック) — sports")

RSSX(jp_b43_vitup_jp, "jp-b43-vitup-jp",
     "VITUP!", "VITUP!",
     "social", "media",
     "https://vitup.jp/feed/",
     "ja", "[\"news\",\"publisher:VITUP!\",\"fitness\"]",
     3600,
     "VITUP! — fitness")

RSSX(jp_b43_volleyballking_jp, "jp-b43-volleyballking-jp",
     "Volleyball King", "バレーボールキング",
     "social", "media",
     "https://volleyballking.jp/feed/",
     "ja", "[\"news\",\"publisher:Volleyball King\",\"sports\"]",
     3600,
     "Volleyball King (バレーボールキング) — sports")

/* ── Travel / Transport ────────────────────────────────────────── */

RSSX(jp_b43_bestcarweb_jp, "jp-b43-bestcarweb-jp",
     "Best Car Web", "ベストカーWeb",
     "social", "transport",
     "https://bestcarweb.jp/feed/",
     "ja", "[\"news\",\"publisher:Best Car Web\",\"automotive\"]",
     3600,
     "Best Car Web (ベストカーWeb) — car")

RSSX(jp_b43_camp_hack_com, "jp-b43-camp-hack-com",
     "CAMP HACK", "CAMP HACK",
     "social", "tourism",
     "https://camp-hack.com/feed",
     "ja", "[\"news\",\"publisher:CAMP HACK\",\"outdoor\"]",
     3600,
     "CAMP HACK — outdoor")

RSSX(jp_b43_camphack_nap_camp_com, "jp-b43-camphack-nap-camp-com",
     "CAMP HACK (nap-camp)", "キャンプハック",
     "social", "tourism",
     "https://camphack.nap-camp.com/feed",
     "ja", "[\"news\",\"publisher:CAMP HACK (nap-camp)\",\"outdoor\"]",
     3600,
     "CAMP HACK (nap-camp) (キャンプハック) — outdoor")

RSSX(jp_b43_co_trip_jp, "jp-b43-co-trip-jp",
     "co-trip", "ことりっぷ",
     "social", "tourism",
     "https://co-trip.jp/feed/",
     "ja", "[\"news\",\"publisher:co-trip\",\"travel\"]",
     3600,
     "co-trip (ことりっぷ) — travel")

RSSX(jp_b43_ev_times_com, "jp-b43-ev-times-com",
     "EV Times", "EVタイムス",
     "social", "transport",
     "https://ev-times.com/feed",
     "ja", "[\"news\",\"publisher:EV Times\",\"automotive\",\"ev\"]",
     3600,
     "EV Times (EVタイムス) — car ev")

RSSX(jp_b43_hokkaido_labo_com, "jp-b43-hokkaido-labo-com",
     "Hokkaido Labo", "北海道ラボ",
     "social", "tourism",
     "https://hokkaido-labo.com/feed",
     "ja", "[\"news\",\"publisher:Hokkaido Labo\",\"travel\"]",
     3600,
     "Hokkaido Labo (北海道ラボ) — travel")

RSSX(jp_b43_ishikawa19_com, "jp-b43-ishikawa19-com",
     "Ishikawa19", "いしかわ観光",
     "social", "tourism",
     "https://ishikawa19.com/feed/",
     "ja", "[\"news\",\"publisher:Ishikawa19\",\"travel\"]",
     3600,
     "Ishikawa19 (いしかわ観光) — travel")

RSSX(jp_b43_kurumanews_jp, "jp-b43-kurumanews-jp",
     "Kuruma News", "くるまのニュース",
     "social", "transport",
     "https://kuruma-news.jp/feed",
     "ja", "[\"news\",\"publisher:Kuruma News\",\"automotive\"]",
     3600,
     "Kuruma News (くるまのニュース) — car")

RSSX(jp_b43_motor_fan_jp, "jp-b43-motor-fan-jp",
     "Motor Fan", "Motor Fan",
     "social", "transport",
     "https://motor-fan.jp/feed",
     "ja", "[\"news\",\"publisher:Motor Fan\",\"automotive\"]",
     3600,
     "Motor Fan — car")

RSSX(jp_b43_okinawatravelinfo_com, "jp-b43-okinawatravelinfo-com",
     "Okinawa Travel Info", "沖縄旅行情報",
     "social", "tourism",
     "https://okinawatravelinfo.com/feed/",
     "en", "[\"news\",\"publisher:Okinawa Travel Info\",\"travel\"]",
     3600,
     "Okinawa Travel Info (沖縄旅行情報) — travel")

RSSX(jp_b43_sky_budget_com, "jp-b43-sky-budget-com",
     "Sky Budget", "スカイバジェット",
     "social", "transport",
     "https://sky-budget.com/feed/",
     "ja", "[\"news\",\"publisher:Sky Budget\",\"transport\",\"aviation\"]",
     3600,
     "Sky Budget (スカイバジェット) — aviation")

RSSX(jp_b43_tabippo_net, "jp-b43-tabippo-net",
     "TABIPPO", "TABIPPO",
     "social", "tourism",
     "https://tabippo.net/feed/",
     "ja", "[\"news\",\"publisher:TABIPPO\",\"travel\"]",
     3600,
     "TABIPPO — travel")

RSSX(jp_b43_tabiris_com, "jp-b43-tabiris-com",
     "Tabiris", "タビリス",
     "social", "transport",
     "https://tabiris.com/feed/",
     "ja", "[\"news\",\"publisher:Tabiris\",\"transport\",\"railway\"]",
     3600,
     "Tabiris (タビリス) — railway")

RSSX(jp_b43_travelersnavi_com, "jp-b43-travelersnavi-com",
     "Traveler's Navi", "旅行者ナビ",
     "social", "tourism",
     "https://travelersnavi.com/feed/",
     "ja", "[\"news\",\"publisher:Traveler's Navi\",\"travel\"]",
     3600,
     "Traveler's Navi (旅行者ナビ) — travel")

RSSX(jp_b43_tripeditor_com, "jp-b43-tripeditor-com",
     "TRiP EDiTOR", "TRiP EDiTOR",
     "social", "tourism",
     "https://tripeditor.com/feed/",
     "ja", "[\"news\",\"publisher:TRiP EDiTOR\",\"travel\"]",
     3600,
     "TRiP EDiTOR — travel")

/* ── Food / Drink ──────────────────────────────────────────────── */

RSSX(jp_b43_cookpad_diet_jp, "jp-b43-cookpad-diet-jp",
     "Cookpad Diet", "クックパッドダイエット",
     "social", "lifestyle",
     "https://cookpad-diet.jp/feed",
     "ja", "[\"news\",\"publisher:Cookpad Diet\",\"food\"]",
     3600,
     "Cookpad Diet (クックパッドダイエット) — food")

RSSX(jp_b43_food_stadium_com, "jp-b43-food-stadium-com",
     "Food Stadium", "フードスタジアム",
     "social", "lifestyle",
     "https://food-stadium.com/feed/",
     "ja", "[\"news\",\"publisher:Food Stadium\",\"food\"]",
     3600,
     "Food Stadium (フードスタジアム) — food")

RSSX(jp_b43_foodie_jp, "jp-b43-foodie-jp",
     "Foodie (Mitsukoshi Isetan)", "三越伊勢丹 FOODIE",
     "social", "lifestyle",
     "https://mi-journey.jp/foodie/feed/",
     "ja", "[\"news\",\"publisher:Foodie (Mitsukoshi Isetan)\",\"food\"]",
     3600,
     "Foodie (Mitsukoshi Isetan) (三越伊勢丹 FOODIE) — food")

RSSX(jp_b43_ippin_gourmet_com, "jp-b43-ippin-gourmet-com",
     "ippin", "ippin",
     "social", "lifestyle",
     "https://ippin-gourmet.com/feed",
     "ja", "[\"news\",\"publisher:ippin\",\"food\"]",
     3600,
     "ippin — food")

RSSX(jp_b43_irodori_co_jp, "jp-b43-irodori-co-jp",
     "irodori", "irodori",
     "social", "lifestyle",
     "https://irodori.co.jp/feed",
     "ja", "[\"news\",\"publisher:irodori\",\"food\"]",
     3600,
     "irodori — food")

RSSX(jp_b43_kurand_jp, "jp-b43-kurand-jp",
     "KURAND", "KURAND",
     "social", "lifestyle",
     "https://kurand.jp/blogs/magazine.atom",
     "ja", "[\"news\",\"publisher:KURAND\",\"food\"]",
     3600,
     "KURAND — food")

RSSX(jp_b43_mogumogunews_com, "jp-b43-mogumogunews-com",
     "Mogumogu News", "もぐもぐニュース",
     "social", "lifestyle",
     "https://mogumogunews.com/feed/",
     "ja", "[\"news\",\"publisher:Mogumogu News\",\"food\"]",
     3600,
     "Mogumogu News (もぐもぐニュース) — food")

RSSX(jp_b43_nomunication_jp, "jp-b43-nomunication-jp",
     "Nomunication", "Nomunication",
     "social", "lifestyle",
     "https://nomunication.jp/feed/",
     "en", "[\"news\",\"publisher:Nomunication\",\"food\"]",
     3600,
     "Nomunication — food")

RSSX(jp_b43_retty_news, "jp-b43-retty-news",
     "Retty", "Retty",
     "social", "lifestyle",
     "https://retty.news/feed/",
     "ja", "[\"news\",\"publisher:Retty\",\"food\"]",
     3600,
     "Retty — food")

RSSX(jp_b43_saketimes_jp, "jp-b43-saketimes-jp",
     "SAKETIMES", "SAKETIMES",
     "social", "lifestyle",
     "https://jp.sake-times.com/feed",
     "ja", "[\"news\",\"publisher:SAKETIMES\",\"food\"]",
     3600,
     "SAKETIMES — food")

/* ── Lifestyle / Fashion ───────────────────────────────────────── */

RSSX(jp_b43_cancam_jp, "jp-b43-cancam-jp",
     "CanCam", "CanCam",
     "social", "lifestyle",
     "https://cancam.jp/feed",
     "ja", "[\"news\",\"publisher:CanCam\",\"fashion\"]",
     3600,
     "CanCam — fashion")

RSSX(jp_b43_hobby_watch_impress_co_jp, "jp-b43-hobby-watch-impress-co-jp",
     "Hobby Watch", "ホビーWatch",
     "social", "lifestyle",
     "https://hobby.watch.impress.co.jp/data/rss/1.0/hbw/feed.rdf",
     "ja", "[\"news\",\"publisher:Hobby Watch\",\"hobby\"]",
     3600,
     "Hobby Watch (ホビーWatch) — hobby")

RSSX(jp_b43_iecolle_com, "jp-b43-iecolle-com",
     "iecolle", "イエコレクション",
     "social", "lifestyle",
     "https://iecolle.com/feed",
     "ja", "[\"news\",\"publisher:iecolle\",\"lifestyle\",\"home\"]",
     3600,
     "iecolle (イエコレクション) — home")

RSSX(jp_b43_josei_bigaku_jp, "jp-b43-josei-bigaku-jp",
     "Josei Bigaku", "女性の美学",
     "social", "lifestyle",
     "https://josei-bigaku.jp/feed/",
     "ja", "[\"news\",\"publisher:Josei Bigaku\",\"beauty\"]",
     3600,
     "Josei Bigaku (女性の美学) — beauty")

RSSX(jp_b43_kokocara_pal_system_co_jp, "jp-b43-kokocara-pal-system-co-jp",
     "kokocara", "こここら",
     "social", "lifestyle",
     "https://kokocara.pal-system.co.jp/feed/",
     "ja", "[\"news\",\"publisher:kokocara\",\"lifestyle\"]",
     3600,
     "kokocara (こここら) — lifestyle")

RSSX(jp_b43_locari_jp, "jp-b43-locari-jp",
     "Locari", "Locari",
     "social", "lifestyle",
     "https://locari.jp/feed",
     "ja", "[\"news\",\"publisher:Locari\",\"lifestyle\"]",
     3600,
     "Locari — lifestyle")

RSSX(jp_b43_lovegreen_net, "jp-b43-lovegreen-net",
     "LOVEGREEN", "LOVEGREEN",
     "social", "lifestyle",
     "https://lovegreen.net/feed/",
     "ja", "[\"news\",\"publisher:LOVEGREEN\",\"garden\"]",
     3600,
     "LOVEGREEN — garden")

RSSX(jp_b43_mendy_jp, "jp-b43-mendy-jp",
     "MENDY", "MENDY",
     "social", "lifestyle",
     "https://mendy.jp/feed",
     "ja", "[\"news\",\"publisher:MENDY\",\"lifestyle\"]",
     3600,
     "MENDY — lifestyle")

RSSX(jp_b43_mery_jp, "jp-b43-mery-jp",
     "MERY", "MERY",
     "social", "lifestyle",
     "https://mery.jp/feed",
     "ja", "[\"news\",\"publisher:MERY\",\"lifestyle\"]",
     3600,
     "MERY — lifestyle")

RSSX(jp_b43_moov_ooo, "jp-b43-moov-ooo",
     "Moov", "Moov Magazine",
     "social", "lifestyle",
     "https://moov.ooo/feed/",
     "ja", "[\"news\",\"publisher:Moov\",\"lifestyle\"]",
     3600,
     "Moov (Moov Magazine) — lifestyle")

RSSX(jp_b43_oggi_jp, "jp-b43-oggi-jp",
     "Oggi", "Oggi",
     "social", "lifestyle",
     "https://oggi.jp/feed",
     "ja", "[\"news\",\"publisher:Oggi\",\"fashion\"]",
     3600,
     "Oggi — fashion")

RSSX(jp_b43_psnews_jp, "jp-b43-psnews-jp",
     "PS News Dog", "PS保険ニュース 犬",
     "social", "lifestyle",
     "https://psnews.jp/dog/feed/",
     "ja", "[\"news\",\"publisher:PS News Dog\",\"lifestyle\"]",
     3600,
     "PS News Dog (PS保険ニュース 犬) — lifestyle")

RSSX(jp_b43_roomclip_jp, "jp-b43-roomclip-jp",
     "RoomClip", "ルームクリップ",
     "social", "lifestyle",
     "https://roomclip.jp/mag/feed",
     "ja", "[\"news\",\"publisher:RoomClip\",\"lifestyle\",\"diy\"]",
     3600,
     "RoomClip (ルームクリップ) — diy")

RSSX(jp_b43_serai_jp, "jp-b43-serai-jp",
     "Serai", "サライ",
     "social", "lifestyle",
     "https://serai.jp/feed",
     "ja", "[\"news\",\"publisher:Serai\",\"lifestyle\"]",
     3600,
     "Serai (サライ) — lifestyle")

RSSX(jp_b43_smartlog_jp, "jp-b43-smartlog-jp",
     "Smartlog", "Smartlog",
     "social", "lifestyle",
     "https://smartlog.jp/feed",
     "ja", "[\"news\",\"publisher:Smartlog\",\"lifestyle\"]",
     3600,
     "Smartlog — lifestyle")

RSSX(jp_b43_soen_tokyo, "jp-b43-soen-tokyo",
     "SO-EN", "装苑",
     "social", "lifestyle",
     "https://soen.tokyo/feed/",
     "ja", "[\"news\",\"publisher:SO-EN\",\"fashion\"]",
     3600,
     "SO-EN (装苑) — fashion")

RSSX(jp_b43_sumikawa_net, "jp-b43-sumikawa-net",
     "Sumikawa", "sumikawa",
     "social", "lifestyle",
     "https://sumikawa.net/feed/",
     "ja", "[\"news\",\"publisher:Sumikawa\",\"lifestyle\"]",
     3600,
     "Sumikawa (sumikawa) — lifestyle")

RSSX(jp_b43_tokila_jp, "jp-b43-tokila-jp",
     "TOKILA", "トキラ",
     "social", "lifestyle",
     "https://tokila.jp/feed/",
     "ja", "[\"news\",\"publisher:TOKILA\",\"beauty\"]",
     3600,
     "TOKILA (トキラ) — beauty")

RSSX(jp_b43_woman_mynavi_jp, "jp-b43-woman-mynavi-jp",
     "mynavi Woman", "マイナビウーマン",
     "social", "lifestyle",
     "https://woman.mynavi.jp/feed/",
     "ja", "[\"news\",\"publisher:mynavi Woman\",\"lifestyle\"]",
     3600,
     "mynavi Woman (マイナビウーマン) — lifestyle")

/* ── Parenting / Education ─────────────────────────────────────── */

RSSX(jp_b43_chiik_jp, "jp-b43-chiik-jp",
     "Chiik!", "Chiik!",
     "social", "lifestyle",
     "https://chiik.jp/feed/",
     "ja", "[\"news\",\"publisher:Chiik!\",\"parenting\"]",
     3600,
     "Chiik! — parenting")

RSSX(jp_b43_kodomo_manabi_labo_net, "jp-b43-kodomo-manabi-labo-net",
     "KodomoManabi", "こどもまなびラボ",
     "social", "lifestyle",
     "https://kodomo-manabi-labo.net/feed",
     "ja", "[\"news\",\"publisher:KodomoManabi\",\"parenting\"]",
     3600,
     "KodomoManabi (こどもまなびラボ) — parenting")

RSSX(jp_b43_kosodate_march_jp, "jp-b43-kosodate-march-jp",
     "Kosodate March", "子育てマーチ",
     "social", "lifestyle",
     "https://kosodate-march.jp/feed/",
     "ja", "[\"news\",\"publisher:Kosodate March\",\"parenting\"]",
     3600,
     "Kosodate March (子育てマーチ) — parenting")

RSSX(jp_b43_laxic_me, "jp-b43-laxic-me",
     "LAXIC", "LAXIC",
     "social", "lifestyle",
     "https://laxic.me/feed/",
     "ja", "[\"news\",\"publisher:LAXIC\",\"parenting\"]",
     3600,
     "LAXIC — parenting")

RSSX(jp_b43_mamapress_jp, "jp-b43-mamapress-jp",
     "MAMA PRESS", "ママプレス",
     "social", "lifestyle",
     "https://mamapress.jp/feed",
     "ja", "[\"news\",\"publisher:MAMA PRESS\",\"parenting\"]",
     3600,
     "MAMA PRESS (ママプレス) — parenting")

RSSX(jp_b43_ninaru_baby_net, "jp-b43-ninaru-baby-net",
     "ninaru baby", "ninaru baby",
     "social", "lifestyle",
     "https://ninaru-baby.net/feed",
     "ja", "[\"news\",\"publisher:ninaru baby\",\"parenting\"]",
     3600,
     "ninaru baby — parenting")

RSSX(jp_b43_resemom_jp, "jp-b43-resemom-jp",
     "ReseEd (ReseMom)", "リセマム",
     "social", "education",
     "https://resemom.jp/rss/index.rdf",
     "ja", "[\"news\",\"publisher:ReseEd (ReseMom)\",\"education\"]",
     3600,
     "ReseEd (ReseMom) (リセマム) — edu")

RSSX(jp_b43_up_to_you_me, "jp-b43-up-to-you-me",
     "UP to you!", "UP to you!",
     "social", "lifestyle",
     "https://up-to-you.me/feed/",
     "ja", "[\"news\",\"publisher:UP to you!\",\"parenting\"]",
     3600,
     "UP to you! — parenting")

/* ── Health ────────────────────────────────────────────────────── */

RSSX(jp_b43_answers_news_com, "jp-b43-answers-news-com",
     "Answers News", "AnswersNews",
     "social", "health",
     "https://answers.ten-navi.com/pharmanews/feed/",
     "ja", "[\"news\",\"publisher:Answers News\",\"health\"]",
     3600,
     "Answers News (AnswersNews) — health")

RSSX(jp_b43_fytte_jp, "jp-b43-fytte-jp",
     "FYTTE", "FYTTE",
     "social", "health",
     "https://fytte.jp/feed",
     "ja", "[\"news\",\"publisher:FYTTE\",\"health\"]",
     3600,
     "FYTTE — health")

/* ── Business / Finance ────────────────────────────────────────── */

RSSX(jp_b43_athome_inc_jp, "jp-b43-athome-inc-jp",
     "at home", "アットホーム",
     "social", "business",
     "https://athome-inc.jp/feed/",
     "ja", "[\"news\",\"publisher:at home\",\"realestate\"]",
     3600,
     "at home (アットホーム) — realestate")

RSSX(jp_b43_bizid_itmedia_co_jp, "jp-b43-bizid-itmedia-co-jp",
     "ITmedia Biz.ID", "ITmedia Biz.ID",
     "social", "business",
     "https://rss.itmedia.co.jp/rss/2.0/bizid.xml",
     "ja", "[\"news\",\"publisher:ITmedia Biz.ID\",\"business\"]",
     3600,
     "ITmedia Biz.ID — biz")

RSSX(jp_b43_crypto_times_jp, "jp-b43-crypto-times-jp",
     "Crypto Times", "CRYPTO TIMES",
     "social", "finance",
     "https://crypto-times.jp/feed/",
     "ja", "[\"news\",\"publisher:Crypto Times\",\"crypto\"]",
     3600,
     "Crypto Times (CRYPTO TIMES) — crypto")

RSSX(jp_b43_gaiax_socialmedialab_jp, "jp-b43-gaiax-socialmedialab-jp",
     "Gaiax Social Media Lab", "ガイアックス ソーシャルメディアラボ",
     "social", "business",
     "https://gaiax-socialmedialab.jp/feed/",
     "ja", "[\"news\",\"publisher:Gaiax Social Media Lab\",\"marketing\"]",
     3600,
     "Gaiax Social Media Lab (ガイアックス ソーシャルメディアラボ) — marketing")

RSSX(jp_b43_hedge_guide, "jp-b43-hedge-guide",
     "HEDGE GUIDE", "HEDGE GUIDE",
     "social", "business",
     "https://hedge.guide/feed",
     "ja", "[\"news\",\"publisher:HEDGE GUIDE\",\"business\"]",
     3600,
     "HEDGE GUIDE — business")

RSSX(jp_b43_iemone_jp, "jp-b43-iemone-jp",
     "Iemone", "イエモネ",
     "social", "business",
     "https://iemone.jp/feed/",
     "ja", "[\"news\",\"publisher:Iemone\",\"housing\"]",
     3600,
     "Iemone (イエモネ) — housing")

RSSX(jp_b43_liskul_com, "jp-b43-liskul-com",
     "LISKUL", "LISKUL",
     "social", "business",
     "https://liskul.com/feed",
     "ja", "[\"news\",\"publisher:LISKUL\",\"marketing\"]",
     3600,
     "LISKUL — marketing")

RSSX(jp_b43_marketeer_jp, "jp-b43-marketeer-jp",
     "Marketeer", "マーケティア",
     "social", "business",
     "https://marketeer.jp/feed/",
     "ja", "[\"news\",\"publisher:Marketeer\",\"marketing\"]",
     3600,
     "Marketeer (マーケティア) — marketing")

RSSX(jp_b43_marketing_itmedia_co_jp, "jp-b43-marketing-itmedia-co-jp",
     "ITmedia Marketing", "ITmedia マーケティング",
     "social", "business",
     "https://rss.itmedia.co.jp/rss/2.0/marketing.xml",
     "ja", "[\"news\",\"publisher:ITmedia Marketing\",\"marketing\"]",
     3600,
     "ITmedia Marketing (ITmedia マーケティング) — marketing")

RSSX(jp_b43_money_bu_jpx_com, "jp-b43-money-bu-jpx-com",
     "JPX Money Book", "JPX マネーBu",
     "social", "business",
     "https://money-bu-jpx.com/feed/",
     "ja", "[\"news\",\"publisher:JPX Money Book\",\"business\"]",
     3600,
     "JPX Money Book (JPX マネーBu) — business")

RSSX(jp_b43_money_lab_jp, "jp-b43-money-lab-jp",
     "Money Lab", "マネーラボ",
     "social", "finance",
     "https://money-lab.jp/feed/",
     "ja", "[\"news\",\"publisher:Money Lab\",\"finance\"]",
     3600,
     "Money Lab (マネーラボ) — finance")

RSSX(jp_b43_news_careerconnection_jp, "jp-b43-news-careerconnection-jp",
     "Career Connection", "キャリコネニュース",
     "social", "business",
     "https://news.careerconnection.jp/feed/",
     "ja", "[\"news\",\"publisher:Career Connection\",\"business\"]",
     3600,
     "Career Connection (キャリコネニュース) — business")

RSSX(jp_b43_o_uccino_jp, "jp-b43-o-uccino-jp",
     "O-uccino", "O-uccino",
     "social", "business",
     "https://o-uccino.jp/article/feed/",
     "ja", "[\"news\",\"publisher:O-uccino\",\"realestate\"]",
     3600,
     "O-uccino — realestate")

RSSX(jp_b43_realestate_co_jp, "jp-b43-realestate-co-jp",
     "Real Estate Japan", "Real Estate Japan",
     "social", "business",
     "https://resources.realestate.co.jp/feed/",
     "en", "[\"news\",\"publisher:Real Estate Japan\",\"housing\"]",
     3600,
     "Real Estate Japan — housing")

RSSX(jp_b43_seleqt_net, "jp-b43-seleqt-net",
     "Seleqt", "Seleqt",
     "social", "business",
     "https://seleqt.net/feed/",
     "ja", "[\"news\",\"publisher:Seleqt\",\"marketing\"]",
     3600,
     "Seleqt — marketing")

RSSX(jp_b43_sogyotecho_jp, "jp-b43-sogyotecho-jp",
     "Sogyotecho", "創業手帳",
     "social", "business",
     "https://sogyotecho.jp/feed/",
     "ja", "[\"news\",\"publisher:Sogyotecho\",\"business\"]",
     3600,
     "Sogyotecho (創業手帳) — business")

RSSX(jp_b43_venturetimes_jp, "jp-b43-venturetimes-jp",
     "Venture Times", "ベンチャータイムス",
     "social", "business",
     "https://venturetimes.jp/feed",
     "ja", "[\"news\",\"publisher:Venture Times\",\"business\",\"startup\"]",
     3600,
     "Venture Times (ベンチャータイムス) — biz startup")

/* ── Environment / Energy ──────────────────────────────────────── */

RSSX(jp_b43_alternas_jp, "jp-b43-alternas-jp",
     "Alterna", "オルタナ",
     "social", "environment",
     "https://www.alterna.co.jp/feed/",
     "ja", "[\"news\",\"publisher:Alterna\",\"environment\"]",
     3600,
     "Alterna (オルタナ) — sustainability")

RSSX(jp_b43_energyshift_com, "jp-b43-energyshift-com",
     "EnergyShift", "EnergyShift",
     "social", "environment",
     "https://energyshift.com/feed",
     "ja", "[\"news\",\"publisher:EnergyShift\",\"energy\"]",
     3600,
     "EnergyShift — energy")

RSSX(jp_b43_greenz_jp, "jp-b43-greenz-jp",
     "greenz.jp", "greenz.jp",
     "social", "environment",
     "https://greenz.jp/feed/",
     "ja", "[\"news\",\"publisher:greenz.jp\",\"environment\"]",
     3600,
     "greenz.jp — sustainability")

RSSX(jp_b43_ideasforgood_jp, "jp-b43-ideasforgood-jp",
     "IDEAS FOR GOOD", "IDEAS FOR GOOD",
     "social", "environment",
     "https://ideasforgood.jp/feed/",
     "ja", "[\"news\",\"publisher:IDEAS FOR GOOD\",\"environment\"]",
     3600,
     "IDEAS FOR GOOD — environment")

/* ── Science / Research ────────────────────────────────────────── */

RSSX(jp_b43_karapaia_com, "jp-b43-karapaia-com",
     "Karapaia", "カラパイア",
     "social", "science",
     "https://karapaia.com/feed",
     "ja", "[\"news\",\"publisher:Karapaia\",\"science\"]",
     3600,
     "Karapaia (カラパイア) — science")

RSSX(jp_b43_spacemedia_jp, "jp-b43-spacemedia-jp",
     "Space Media", "Space Media",
     "social", "science",
     "https://spacemedia.jp/feed",
     "ja", "[\"news\",\"publisher:Space Media\",\"science\",\"space\"]",
     3600,
     "Space Media — science space")

RSSX(jp_b43_thinktank_php_co_jp, "jp-b43-thinktank-php-co-jp",
     "PHP Research Institute", "PHP総合研究所",
     "social", "science",
     "https://thinktank.php.co.jp/feed/",
     "ja", "[\"news\",\"publisher:PHP Research Institute\",\"research\"]",
     3600,
     "PHP Research Institute (PHP総合研究所) — research")

/* ── English-language Japan ────────────────────────────────────── */

RSSX(jp_b43_deepjapan_org, "jp-b43-deepjapan-org",
     "DeepJapan", "DeepJapan",
     "social", "media",
     "https://deepjapan.org/feed/",
     "en", "[\"news\",\"publisher:DeepJapan\",\"japan\",\"english\"]",
     3600,
     "DeepJapan — en japan")

RSSX(jp_b43_neojaponisme_com, "jp-b43-neojaponisme-com",
     "Neojaponisme", "Neojaponisme",
     "social", "media",
     "https://neojaponisme.com/feed/",
     "en", "[\"news\",\"publisher:Neojaponisme\",\"japan\",\"english\"]",
     3600,
     "Neojaponisme — en japan")

RSSX(jp_b43_teamjapanese_com, "jp-b43-teamjapanese-com",
     "Team Japanese", "Team Japanese",
     "social", "media",
     "https://teamjapanese.com/feed/",
     "en", "[\"news\",\"publisher:Team Japanese\",\"japan\",\"english\"]",
     3600,
     "Team Japanese — en japan")

RSSX(jp_b43_thetokyotourist_com, "jp-b43-thetokyotourist-com",
     "The Tokyo Tourist", "The Tokyo Tourist",
     "social", "media",
     "https://thetokyotourist.com/feed/",
     "en", "[\"news\",\"publisher:The Tokyo Tourist\",\"japan\",\"english\"]",
     3600,
     "The Tokyo Tourist — en japan")
