/* Curated national news outlets / agencies worldwide (diverse providers, known RSS). Provider diversity beyond the aggregators, via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
#define RSSX(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC)  \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

RSSX(out_tass, "tass", "TASS (Russia)", "TASS (Russia)", "osint", "news",
  "https://tass.com/rss/v2.xml", "en", "[\"news\",\"russia\",\"national-outlet\"]", 3600,
  "TASS (Russia) — national/regional press feed (russia)");

RSSX(out_rt_news, "rt-news", "RT (Russia)", "RT (Russia)", "osint", "news",
  "https://www.rt.com/rss/", "en", "[\"news\",\"russia\",\"national-outlet\"]", 3600,
  "RT (Russia) — national/regional press feed (russia)");

RSSX(out_global_times, "global-times", "Global Times (China)", "Global Times (China)", "osint", "news",
  "https://www.globaltimes.cn/rss/outbrain.xml", "en", "[\"news\",\"china\",\"national-outlet\"]", 3600,
  "Global Times (China) — national/regional press feed (china)");

RSSX(out_china_daily, "china-daily", "China Daily", "China Daily", "osint", "news",
  "http://www.chinadaily.com.cn/rss/china_rss.xml", "en", "[\"news\",\"china\",\"national-outlet\"]", 3600,
  "China Daily — national/regional press feed (china)");

RSSX(out_scmp_china2, "scmp-china2", "SCMP China", "SCMP China", "osint", "news",
  "https://www.scmp.com/rss/4/feed", "en", "[\"news\",\"china\",\"national-outlet\"]", 3600,
  "SCMP China — national/regional press feed (china)");

RSSX(out_abc_au, "abc-au", "ABC News (Australia)", "ABC News (Australia)", "osint", "news",
  "https://www.abc.net.au/news/feed/51120/rss.xml", "en", "[\"news\",\"australia\",\"national-outlet\"]", 3600,
  "ABC News (Australia) — national/regional press feed (australia)");

RSSX(out_cbc_top, "cbc-top", "CBC News (Canada)", "CBC News (Canada)", "osint", "news",
  "https://www.cbc.ca/webfeed/rss/rss-topstories", "en", "[\"news\",\"canada\",\"national-outlet\"]", 3600,
  "CBC News (Canada) — national/regional press feed (canada)");

RSSX(out_cbc_world, "cbc-world", "CBC World", "CBC World", "osint", "news",
  "https://www.cbc.ca/webfeed/rss/rss-world", "en", "[\"news\",\"canada\",\"national-outlet\"]", 3600,
  "CBC World — national/regional press feed (canada)");

RSSX(out_lemonde_une, "lemonde-une", "Le Monde (France)", "Le Monde (France)", "osint", "news",
  "https://www.lemonde.fr/rss/une.xml", "fr", "[\"news\",\"france\",\"national-outlet\"]", 3600,
  "Le Monde (France) — national/regional press feed (france)");

RSSX(out_lefigaro, "lefigaro", "Le Figaro (France)", "Le Figaro (France)", "osint", "news",
  "https://www.lefigaro.fr/rss/figaro_actualites.xml", "fr", "[\"news\",\"france\",\"national-outlet\"]", 3600,
  "Le Figaro (France) — national/regional press feed (france)");

RSSX(out_rfi_fr, "rfi-fr", "RFI (France)", "RFI (France)", "osint", "news",
  "https://www.rfi.fr/fr/rss", "fr", "[\"news\",\"france\",\"national-outlet\"]", 3600,
  "RFI (France) — national/regional press feed (france)");

RSSX(out_spiegel_intl, "spiegel-intl", "Der Spiegel International", "Der Spiegel International", "osint", "news",
  "https://www.spiegel.de/international/index.rss", "en", "[\"news\",\"germany\",\"national-outlet\"]", 3600,
  "Der Spiegel International — national/regional press feed (germany)");

RSSX(out_dw_top, "dw-top", "Deutsche Welle Top", "Deutsche Welle Top", "osint", "news",
  "https://rss.dw.com/rdf/rss-en-top", "en", "[\"news\",\"germany\",\"national-outlet\"]", 3600,
  "Deutsche Welle Top — national/regional press feed (germany)");

RSSX(out_elpais_port, "elpais-port", "El País (Spain)", "El País (Spain)", "osint", "news",
  "https://feeds.elpais.com/mrss-s/pages/ep/site/elpais.com/portada", "es", "[\"news\",\"spain\",\"national-outlet\"]", 3600,
  "El País (Spain) — national/regional press feed (spain)");

RSSX(out_elmundo, "elmundo", "El Mundo (Spain)", "El Mundo (Spain)", "osint", "news",
  "https://e00-elmundo.uecdn.es/elmundo/rss/portada.xml", "es", "[\"news\",\"spain\",\"national-outlet\"]", 3600,
  "El Mundo (Spain) — national/regional press feed (spain)");

RSSX(out_ansa_it, "ansa-it", "ANSA (Italy)", "ANSA (Italy)", "osint", "news",
  "https://www.ansa.it/sito/ansait_rss.xml", "it", "[\"news\",\"italy\",\"national-outlet\"]", 3600,
  "ANSA (Italy) — national/regional press feed (italy)");

RSSX(out_repubblica, "repubblica", "La Repubblica (Italy)", "La Repubblica (Italy)", "osint", "news",
  "https://www.repubblica.it/rss/homepage/rss2.0.xml", "it", "[\"news\",\"italy\",\"national-outlet\"]", 3600,
  "La Repubblica (Italy) — national/regional press feed (italy)");

RSSX(out_dawn_pk, "dawn-pk", "Dawn (Pakistan)", "Dawn (Pakistan)", "osint", "news",
  "https://www.dawn.com/feeds/home", "en", "[\"news\",\"pakistan\",\"national-outlet\"]", 3600,
  "Dawn (Pakistan) — national/regional press feed (pakistan)");

RSSX(out_kyiv_post, "kyiv-post", "Kyiv Post (Ukraine)", "Kyiv Post (Ukraine)", "osint", "news",
  "https://www.kyivpost.com/feed", "en", "[\"news\",\"ukraine\",\"national-outlet\"]", 3600,
  "Kyiv Post (Ukraine) — national/regional press feed (ukraine)");

RSSX(out_pravda_ua, "pravda-ua", "Ukrainska Pravda", "Ukrainska Pravda", "osint", "news",
  "https://www.pravda.com.ua/rss/", "uk", "[\"news\",\"ukraine\",\"national-outlet\"]", 3600,
  "Ukrainska Pravda — national/regional press feed (ukraine)");

RSSX(out_anadolu, "anadolu", "Anadolu Agency (Turkey)", "Anadolu Agency (Turkey)", "osint", "news",
  "https://www.aa.com.tr/en/rss/default?cat=guncel", "en", "[\"news\",\"turkey\",\"national-outlet\"]", 3600,
  "Anadolu Agency (Turkey) — national/regional press feed (turkey)");

RSSX(out_hurriyet_en, "hurriyet-en", "Hürriyet Daily News", "Hürriyet Daily News", "osint", "news",
  "https://www.hurriyetdailynews.com/rss", "en", "[\"news\",\"turkey\",\"national-outlet\"]", 3600,
  "Hürriyet Daily News — national/regional press feed (turkey)");

RSSX(out_bangkok_post, "bangkok-post", "Bangkok Post", "Bangkok Post", "osint", "news",
  "https://www.bangkokpost.com/rss/data/topstories.xml", "en", "[\"news\",\"thailand\",\"national-outlet\"]", 3600,
  "Bangkok Post — national/regional press feed (thailand)");

RSSX(out_jakarta_post, "jakarta-post", "Jakarta Post", "Jakarta Post", "osint", "news",
  "https://www.thejakartapost.com/feed", "en", "[\"news\",\"indonesia\",\"national-outlet\"]", 3600,
  "Jakarta Post — national/regional press feed (indonesia)");

RSSX(out_straits_top, "straits-top", "Straits Times Top", "Straits Times Top", "osint", "news",
  "https://www.straitstimes.com/news/singapore/rss.xml", "en", "[\"news\",\"singapore\",\"national-outlet\"]", 3600,
  "Straits Times Top — national/regional press feed (singapore)");

RSSX(out_cna_sg, "cna-sg", "Channel NewsAsia", "Channel NewsAsia", "osint", "news",
  "https://www.channelnewsasia.com/rssfeeds/8395986", "en", "[\"news\",\"singapore\",\"national-outlet\"]", 3600,
  "Channel NewsAsia — national/regional press feed (singapore)");

RSSX(out_manila_times, "manila-times", "Manila Times", "Manila Times", "osint", "news",
  "https://www.manilatimes.net/feed", "en", "[\"news\",\"philippines\",\"national-outlet\"]", 3600,
  "Manila Times — national/regional press feed (philippines)");

RSSX(out_hindustan_times, "hindustan-times", "Hindustan Times (India)", "Hindustan Times (India)", "osint", "news",
  "https://www.hindustantimes.com/feeds/rss/world-news/rssfeed.xml", "en", "[\"news\",\"india\",\"national-outlet\"]", 3600,
  "Hindustan Times (India) — national/regional press feed (india)");

RSSX(out_the_hindu_nat, "the-hindu-nat", "The Hindu National", "The Hindu National", "osint", "news",
  "https://www.thehindu.com/news/national/feeder/default.rss", "en", "[\"news\",\"india\",\"national-outlet\"]", 3600,
  "The Hindu National — national/regional press feed (india)");

RSSX(out_ndtv, "ndtv", "NDTV (India)", "NDTV (India)", "osint", "news",
  "https://feeds.feedburner.com/ndtvnews-top-stories", "en", "[\"news\",\"india\",\"national-outlet\"]", 3600,
  "NDTV (India) — national/regional press feed (india)");

RSSX(out_dhaka_tribune, "dhaka-tribune", "Dhaka Tribune", "Dhaka Tribune", "osint", "news",
  "https://www.dhakatribune.com/feed", "en", "[\"news\",\"bangladesh\",\"national-outlet\"]", 3600,
  "Dhaka Tribune — national/regional press feed (bangladesh)");

RSSX(out_korea_times, "korea-times", "Korea Times", "Korea Times", "osint", "news",
  "https://www.koreatimes.co.kr/www/rss/rss.xml", "en", "[\"news\",\"korea\",\"national-outlet\"]", 3600,
  "Korea Times — national/regional press feed (korea)");

RSSX(out_japan_times, "japan-times", "The Japan Times", "The Japan Times", "osint", "news",
  "https://www.japantimes.co.jp/feed/", "en", "[\"news\",\"japan\",\"national-outlet\"]", 3600,
  "The Japan Times — national/regional press feed (japan)");

RSSX(out_mainichi_en, "mainichi-en", "Mainichi (Japan, EN)", "Mainichi (Japan, EN)", "osint", "news",
  "https://mainichi.jp/rss/etc/mainichi-flash.rss", "ja", "[\"news\",\"japan\",\"national-outlet\"]", 3600,
  "Mainichi (Japan, EN) — national/regional press feed (japan)");

RSSX(out_taipei_times, "taipei-times", "Taipei Times", "Taipei Times", "osint", "news",
  "https://www.taipeitimes.com/xml/index.rss", "en", "[\"news\",\"taiwan\",\"national-outlet\"]", 3600,
  "Taipei Times — national/regional press feed (taiwan)");

RSSX(out_folha_br, "folha-br", "Folha de S.Paulo", "Folha de S.Paulo", "osint", "news",
  "https://feeds.folha.uol.com.br/emcimadahora/rss091.xml", "pt", "[\"news\",\"brazil\",\"national-outlet\"]", 3600,
  "Folha de S.Paulo — national/regional press feed (brazil)");

RSSX(out_g1_globo, "g1-globo", "G1 Globo (Brazil)", "G1 Globo (Brazil)", "osint", "news",
  "https://g1.globo.com/rss/g1/", "pt", "[\"news\",\"brazil\",\"national-outlet\"]", 3600,
  "G1 Globo (Brazil) — national/regional press feed (brazil)");

RSSX(out_clarin_ar, "clarin-ar", "Clarín (Argentina)", "Clarín (Argentina)", "osint", "news",
  "https://www.clarin.com/rss/lo-ultimo/", "es", "[\"news\",\"argentina\",\"national-outlet\"]", 3600,
  "Clarín (Argentina) — national/regional press feed (argentina)");

RSSX(out_infobae, "infobae", "Infobae (Argentina)", "Infobae (Argentina)", "osint", "news",
  "https://www.infobae.com/feeds/rss/", "es", "[\"news\",\"argentina\",\"national-outlet\"]", 3600,
  "Infobae (Argentina) — national/regional press feed (argentina)");

RSSX(out_eluniversal_mx, "eluniversal-mx", "El Universal (Mexico)", "El Universal (Mexico)", "osint", "news",
  "https://www.eluniversal.com.mx/rss.xml", "es", "[\"news\",\"mexico\",\"national-outlet\"]", 3600,
  "El Universal (Mexico) — national/regional press feed (mexico)");

RSSX(out_milenio, "milenio", "Milenio (Mexico)", "Milenio (Mexico)", "osint", "news",
  "https://www.milenio.com/rss", "es", "[\"news\",\"mexico\",\"national-outlet\"]", 3600,
  "Milenio (Mexico) — national/regional press feed (mexico)");

RSSX(out_eltiempo_co, "eltiempo-co", "El Tiempo (Colombia)", "El Tiempo (Colombia)", "osint", "news",
  "https://www.eltiempo.com/rss/mundo.xml", "es", "[\"news\",\"colombia\",\"national-outlet\"]", 3600,
  "El Tiempo (Colombia) — national/regional press feed (colombia)");

RSSX(out_emol_cl, "emol-cl", "Emol (Chile)", "Emol (Chile)", "osint", "news",
  "https://www.emol.com/rss/rss.asp?canal=nacional", "es", "[\"news\",\"chile\",\"national-outlet\"]", 3600,
  "Emol (Chile) — national/regional press feed (chile)");

RSSX(out_news24_za, "news24-za", "News24 (South Africa)", "News24 (South Africa)", "osint", "news",
  "https://feeds.24.com/articles/news24/TopStories/rss", "en", "[\"news\",\"south-africa\",\"national-outlet\"]", 3600,
  "News24 (South Africa) — national/regional press feed (south-africa)");

RSSX(out_punch_ng, "punch-ng", "The Punch (Nigeria)", "The Punch (Nigeria)", "osint", "news",
  "https://punchng.com/feed/", "en", "[\"news\",\"nigeria\",\"national-outlet\"]", 3600,
  "The Punch (Nigeria) — national/regional press feed (nigeria)");

RSSX(out_premium_times, "premium-times", "Premium Times (Nigeria)", "Premium Times (Nigeria)", "osint", "news",
  "https://www.premiumtimesng.com/feed", "en", "[\"news\",\"nigeria\",\"national-outlet\"]", 3600,
  "Premium Times (Nigeria) — national/regional press feed (nigeria)");

RSSX(out_nation_ke, "nation-ke", "Nation (Kenya)", "Nation (Kenya)", "osint", "news",
  "https://nation.africa/kenya/rss", "en", "[\"news\",\"kenya\",\"national-outlet\"]", 3600,
  "Nation (Kenya) — national/regional press feed (kenya)");

RSSX(out_ahram_eg, "ahram-eg", "Al-Ahram (Egypt)", "Al-Ahram (Egypt)", "osint", "news",
  "https://english.ahram.org.eg/rss/RssHome.aspx", "en", "[\"news\",\"egypt\",\"national-outlet\"]", 3600,
  "Al-Ahram (Egypt) — national/regional press feed (egypt)");

RSSX(out_arab_news, "arab-news", "Arab News (Saudi)", "Arab News (Saudi)", "osint", "news",
  "https://www.arabnews.com/rss.xml", "en", "[\"news\",\"saudi-arabia\",\"national-outlet\"]", 3600,
  "Arab News (Saudi) — national/regional press feed (saudi-arabia)");

RSSX(out_gulf_news, "gulf-news", "Gulf News (UAE)", "Gulf News (UAE)", "osint", "news",
  "https://gulfnews.com/rss?generatorType=Section&query=%2Fnews", "en", "[\"news\",\"uae\",\"national-outlet\"]", 3600,
  "Gulf News (UAE) — national/regional press feed (uae)");

RSSX(out_the_national_ae, "the-national-ae", "The National (UAE)", "The National (UAE)", "osint", "news",
  "https://www.thenationalnews.com/arc/outboundfeeds/rss/", "en", "[\"news\",\"uae\",\"national-outlet\"]", 3600,
  "The National (UAE) — national/regional press feed (uae)");

RSSX(out_al_jazeera_ar, "al-jazeera-ar", "Al Jazeera Arabic", "Al Jazeera Arabic", "osint", "news",
  "https://www.aljazeera.net/aljazeerarss/a7c186be-1baa-4bd4-9d80-a84db769f779/73d0e1b4-532f-45ef-b135-bfdff8b8cab9", "ar", "[\"news\",\"qatar\",\"national-outlet\"]", 3600,
  "Al Jazeera Arabic — national/regional press feed (qatar)");

RSSX(out_times_israel_2, "times-israel-2", "Times of Israel Ops", "Times of Israel Ops", "osint", "news",
  "https://www.timesofisrael.com/feed/", "en", "[\"news\",\"israel\",\"national-outlet\"]", 3600,
  "Times of Israel Ops — national/regional press feed (israel)");

RSSX(out_haaretz_2, "haaretz-2", "Haaretz Headlines", "Haaretz Headlines", "osint", "news",
  "https://www.haaretz.com/srv/htz---all-articles", "en", "[\"news\",\"israel\",\"national-outlet\"]", 3600,
  "Haaretz Headlines — national/regional press feed (israel)");

RSSX(out_tehran_times, "tehran-times", "Tehran Times (Iran)", "Tehran Times (Iran)", "osint", "news",
  "https://www.tehrantimes.com/rss", "en", "[\"news\",\"iran\",\"national-outlet\"]", 3600,
  "Tehran Times (Iran) — national/regional press feed (iran)");

RSSX(out_presstv, "presstv", "Press TV (Iran)", "Press TV (Iran)", "osint", "news",
  "https://www.presstv.ir/rss.xml", "en", "[\"news\",\"iran\",\"national-outlet\"]", 3600,
  "Press TV (Iran) — national/regional press feed (iran)");

RSSX(out_moscow_times_2, "moscow-times-2", "Moscow Times Business", "Moscow Times Business", "osint", "news",
  "https://www.themoscowtimes.com/rss/news", "en", "[\"news\",\"russia\",\"national-outlet\"]", 3600,
  "Moscow Times Business — national/regional press feed (russia)");
