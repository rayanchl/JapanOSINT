/* Curated national news outlets / agencies worldwide, part 2, via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
#include "_source_macros.inc"

RSSX(out_kommersant, "kommersant", "Kommersant (Russia)", "Kommersant (Russia)", "osint", "news",
  "https://www.kommersant.ru/RSS/news.xml", "ru", "[\"news\",\"russia\",\"national-outlet\"]", 3600,
  "Kommersant (Russia) — national/regional press feed (russia)");

RSSX(out_interfax, "interfax", "Interfax (Russia)", "Interfax (Russia)", "osint", "news",
  "https://www.interfax.ru/rss.asp", "ru", "[\"news\",\"russia\",\"national-outlet\"]", 3600,
  "Interfax (Russia) — national/regional press feed (russia)");

RSSX(out_belta_by, "belta-by", "BelTA (Belarus)", "BelTA (Belarus)", "osint", "news",
  "https://eng.belta.by/rss", "en", "[\"news\",\"belarus\",\"national-outlet\"]", 3600,
  "BelTA (Belarus) — national/regional press feed (belarus)");

RSSX(out_astana_times, "astana-times", "Astana Times (Kazakhstan)", "Astana Times (Kazakhstan)", "osint", "news",
  "https://astanatimes.com/feed/", "en", "[\"news\",\"kazakhstan\",\"national-outlet\"]", 3600,
  "Astana Times (Kazakhstan) — national/regional press feed (kazakhstan)");

RSSX(out_rappler_ph, "rappler-ph", "Rappler (Philippines)", "Rappler (Philippines)", "osint", "news",
  "https://www.rappler.com/feed/", "en", "[\"news\",\"philippines\",\"national-outlet\"]", 3600,
  "Rappler (Philippines) — national/regional press feed (philippines)");

RSSX(out_vnexpress, "vnexpress", "VnExpress (Vietnam)", "VnExpress (Vietnam)", "osint", "news",
  "https://vnexpress.net/rss/tin-moi-nhat.rss", "vi", "[\"news\",\"vietnam\",\"national-outlet\"]", 3600,
  "VnExpress (Vietnam) — national/regional press feed (vietnam)");

RSSX(out_tuoitre, "tuoitre", "Tuoi Tre News (Vietnam)", "Tuoi Tre News (Vietnam)", "osint", "news",
  "https://tuoitrenews.vn/rss/homepage.rss", "en", "[\"news\",\"vietnam\",\"national-outlet\"]", 3600,
  "Tuoi Tre News (Vietnam) — national/regional press feed (vietnam)");

RSSX(out_nzherald, "nzherald", "NZ Herald", "NZ Herald", "osint", "news",
  /* without ?outputType=xml the Arc endpoint renders the HTML article shell
   * (200, 128 KB of Fusion markup, zero items) — the query param is what
   * selects the RSS renderer. */
  "https://www.nzherald.co.nz/arc/outboundfeeds/rss/curated/78/?outputType=xml", "en", "[\"news\",\"new-zealand\",\"national-outlet\"]", 3600,
  "NZ Herald — national/regional press feed (new-zealand)");

RSSX(out_guardian_au, "guardian-au", "Guardian Australia", "Guardian Australia", "osint", "news",
  "https://www.theguardian.com/australia-news/rss", "en", "[\"news\",\"australia\",\"national-outlet\"]", 3600,
  "Guardian Australia — national/regional press feed (australia)");

RSSX(out_smh_au, "smh-au", "Sydney Morning Herald", "Sydney Morning Herald", "osint", "news",
  "https://www.smh.com.au/rss/feed.xml", "en", "[\"news\",\"australia\",\"national-outlet\"]", 3600,
  "Sydney Morning Herald — national/regional press feed (australia)");

RSSX(out_irish_times, "irish-times", "The Irish Times", "The Irish Times", "osint", "news",
  /* the newsdesk feed is gone (404 "Not Found", 9 bytes); the site-wide Arc
   * RSS still serves ~100 items. */
  "https://www.irishtimes.com/arc/outboundfeeds/rss/?outputType=xml", "en", "[\"news\",\"ireland\",\"national-outlet\"]", 3600,
  "The Irish Times — national/regional press feed (ireland)");

RSSX(out_thelocal_eu, "thelocal-eu", "The Local (Europe)", "The Local (Europe)", "osint", "news",
  "https://www.thelocal.com/feeds/rss.php", "en", "[\"news\",\"europe\",\"national-outlet\"]", 3600,
  "The Local (Europe) — national/regional press feed (europe)");

RSSX(out_euractiv, "euractiv", "EURACTIV", "EURACTIV", "osint", "news",
  "https://www.euractiv.com/feed/", "en", "[\"news\",\"europe\",\"national-outlet\"]", 3600,
  "EURACTIV — national/regional press feed (europe)");

RSSX(out_politico_eu, "politico-eu", "POLITICO Europe", "POLITICO Europe", "osint", "news",
  "https://www.politico.eu/feed/", "en", "[\"news\",\"europe\",\"national-outlet\"]", 3600,
  "POLITICO Europe — national/regional press feed (europe)");

RSSX(out_balkan_insight, "balkan-insight", "Balkan Insight", "Balkan Insight", "osint", "news",
  "https://balkaninsight.com/feed/", "en", "[\"news\",\"balkans\",\"national-outlet\"]", 3600,
  "Balkan Insight — national/regional press feed (balkans)");

RSSX(out_kyiv_independent_2, "kyiv-independent-2", "Kyiv Independent War", "Kyiv Independent War", "osint", "news",
  /* /feed/ 404s; the Ghost feed lives at /feed/rss/. */
  "https://kyivindependent.com/feed/rss/", "en", "[\"news\",\"ukraine\",\"national-outlet\"]", 3600,
  "Kyiv Independent War — national/regional press feed (ukraine)");

RSSX(out_notes_poland, "notes-poland", "Notes from Poland", "Notes from Poland", "osint", "news",
  "https://notesfrompoland.com/feed/", "en", "[\"news\",\"poland\",\"national-outlet\"]", 3600,
  "Notes from Poland — national/regional press feed (poland)");

RSSX(out_baltic_times, "baltic-times", "The Baltic Times", "The Baltic Times", "osint", "news",
  "https://www.baltictimes.com/rss/", "en", "[\"news\",\"baltics\",\"national-outlet\"]", 3600,
  "The Baltic Times — national/regional press feed (baltics)");

RSSX(out_swissinfo, "swissinfo", "SWI swissinfo.ch", "SWI swissinfo.ch", "osint", "news",
  "https://www.swissinfo.ch/eng/rss", "en", "[\"news\",\"switzerland\",\"national-outlet\"]", 3600,
  "SWI swissinfo.ch — national/regional press feed (switzerland)");

RSSX(out_dutch_news, "dutch-news", "DutchNews.nl", "DutchNews.nl", "osint", "news",
  "https://www.dutchnews.nl/feed/", "en", "[\"news\",\"netherlands\",\"national-outlet\"]", 3600,
  "DutchNews.nl — national/regional press feed (netherlands)");

RSSX(out_brussels_times, "brussels-times", "The Brussels Times", "The Brussels Times", "osint", "news",
  /* www.brusselstimes.com/feed returns the Angular app shell (200 HTML, no
   * items); the actual RSS is served by the API host. */
  "https://api.brusselstimes.com/rss", "en", "[\"news\",\"belgium\",\"national-outlet\"]", 3600,
  "The Brussels Times — national/regional press feed (belgium)");

RSSX(out_local_se, "local-se", "The Local Sweden", "The Local Sweden", "osint", "news",
  "https://www.thelocal.se/feeds/rss.php", "en", "[\"news\",\"sweden\",\"national-outlet\"]", 3600,
  "The Local Sweden — national/regional press feed (sweden)");

RSSX(out_ice_news, "ice-news", "Iceland Monitor", "Iceland Monitor", "osint", "news",
  "https://icelandmonitor.mbl.is/rss/", "en", "[\"news\",\"iceland\",\"national-outlet\"]", 3600,
  "Iceland Monitor — national/regional press feed (iceland)");

RSSX(out_greek_reporter, "greek-reporter", "Greek Reporter", "Greek Reporter", "osint", "news",
  "https://greekreporter.com/feed/", "en", "[\"news\",\"greece\",\"national-outlet\"]", 3600,
  "Greek Reporter — national/regional press feed (greece)");

RSSX(out_daily_sabah, "daily-sabah", "Daily Sabah (Turkey)", "Daily Sabah (Turkey)", "osint", "news",
  "https://www.dailysabah.com/rssFeed/homepage", "en", "[\"news\",\"turkey\",\"national-outlet\"]", 3600,
  "Daily Sabah (Turkey) — national/regional press feed (turkey)");

RSSX(out_caucasus_watch, "caucasus-watch", "Caucasus Watch", "Caucasus Watch", "osint", "news",
  "https://caucasuswatch.de/en/rss.xml", "en", "[\"news\",\"caucasus\",\"national-outlet\"]", 3600,
  "Caucasus Watch — national/regional press feed (caucasus)");

RSSX(out_eurasianet, "eurasianet", "Eurasianet", "Eurasianet", "osint", "news",
  "https://eurasianet.org/rss.xml", "en", "[\"news\",\"central-asia\",\"national-outlet\"]", 3600,
  "Eurasianet — national/regional press feed (central-asia)");

RSSX(out_the_diplomat_2, "the-diplomat-2", "The Diplomat Security", "The Diplomat Security", "osint", "news",
  "https://thediplomat.com/feed/", "en", "[\"news\",\"asia\",\"national-outlet\"]", 3600,
  "The Diplomat Security — national/regional press feed (asia)");

RSSX(out_nikkei_asia_2, "nikkei-asia-2", "Nikkei Asia Politics", "Nikkei Asia Politics", "osint", "news",
  "https://asia.nikkei.com/rss/feed/nar", "en", "[\"news\",\"asia\",\"national-outlet\"]", 3600,
  "Nikkei Asia Politics — national/regional press feed (asia)");

RSSX(out_scmp_china3, "scmp-china3", "SCMP China Diplomacy", "SCMP China Diplomacy", "osint", "news",
  "https://www.scmp.com/rss/4/feed", "en", "[\"news\",\"china\",\"national-outlet\"]", 3600,
  "SCMP China Diplomacy — national/regional press feed (china)");

RSSX(out_caixin, "caixin", "Caixin Global (China)", "Caixin Global (China)", "osint", "news",
  "https://www.caixinglobal.com/rss/economics.xml", "en", "[\"news\",\"china\",\"national-outlet\"]", 3600,
  "Caixin Global (China) — national/regional press feed (china)");

RSSX(out_indian_express, "indian-express", "Indian Express", "Indian Express", "osint", "news",
  "https://indianexpress.com/feed/", "en", "[\"news\",\"india\",\"national-outlet\"]", 3600,
  "Indian Express — national/regional press feed (india)");

RSSX(out_dawn_world, "dawn-world", "Dawn World", "Dawn World", "osint", "news",
  "https://www.dawn.com/feeds/world", "en", "[\"news\",\"pakistan\",\"national-outlet\"]", 3600,
  "Dawn World — national/regional press feed (pakistan)");

RSSX(out_al_monitor, "al-monitor", "Al-Monitor (Middle East)", "Al-Monitor (Middle East)", "osint", "news",
  "https://www.al-monitor.com/rss", "en", "[\"news\",\"middle-east\",\"national-outlet\"]", 3600,
  "Al-Monitor (Middle East) — national/regional press feed (middle-east)");

RSSX(out_middle_east_eye, "middle-east-eye", "Middle East Eye", "Middle East Eye", "osint", "news",
  "https://www.middleeasteye.net/rss", "en", "[\"news\",\"middle-east\",\"national-outlet\"]", 3600,
  "Middle East Eye — national/regional press feed (middle-east)");

RSSX(out_mees, "mees", "Middle East Institute", "Middle East Institute", "osint", "news",
  "https://www.mei.edu/rss.xml", "en", "[\"news\",\"middle-east\",\"national-outlet\"]", 3600,
  "Middle East Institute — national/regional press feed (middle-east)");

RSSX(out_africa_report, "africa-report", "The Africa Report", "The Africa Report", "osint", "news",
  "https://www.theafricareport.com/feed/", "en", "[\"news\",\"africa\",\"national-outlet\"]", 3600,
  "The Africa Report — national/regional press feed (africa)");

RSSX(out_mg_africa_2, "mg-africa-2", "Daily Maverick (SA)", "Daily Maverick (SA)", "osint", "news",
  "https://www.dailymaverick.co.za/dmrss/", "en", "[\"news\",\"south-africa\",\"national-outlet\"]", 3600,
  "Daily Maverick (SA) — national/regional press feed (south-africa)");

RSSX(out_addis_standard, "addis-standard", "Addis Standard (Ethiopia)", "Addis Standard (Ethiopia)", "osint", "news",
  "https://addisstandard.com/feed/", "en", "[\"news\",\"ethiopia\",\"national-outlet\"]", 3600,
  "Addis Standard (Ethiopia) — national/regional press feed (ethiopia)");

RSSX(out_the_east_african, "the-east-african", "The East African", "The East African", "osint", "news",
  "https://www.theeastafrican.co.ke/rss", "en", "[\"news\",\"east-africa\",\"national-outlet\"]", 3600,
  "The East African — national/regional press feed (east-africa)");

RSSX(out_mail_guardian_2, "mail-guardian-2", "Mail & Guardian Africa", "Mail & Guardian Africa", "osint", "news",
  /* /feed/ 404s since the site rebuild; /rss/ carries 50 items. */
  "https://mg.co.za/rss/", "en", "[\"news\",\"south-africa\",\"national-outlet\"]", 3600,
  "Mail & Guardian Africa — national/regional press feed (south-africa)");

RSSX(out_semafor_africa, "semafor-africa", "Semafor Africa", "Semafor Africa", "osint", "news",
  "https://www.semafor.com/rss.xml", "en", "[\"news\",\"africa\",\"national-outlet\"]", 3600,
  "Semafor Africa — national/regional press feed (africa)");

RSSX(out_americas_quarterly, "americas-quarterly", "Americas Quarterly", "Americas Quarterly", "osint", "news",
  "https://americasquarterly.org/feed/", "en", "[\"news\",\"latam\",\"national-outlet\"]", 3600,
  "Americas Quarterly — national/regional press feed (latam)");

RSSX(out_mercopress_2, "mercopress-2", "MercoPress South Atlantic", "MercoPress South Atlantic", "osint", "news",
  "https://en.mercopress.com/rss/", "en", "[\"news\",\"latam\",\"national-outlet\"]", 3600,
  "MercoPress South Atlantic — national/regional press feed (latam)");

RSSX(out_caracas_chronicles, "caracas-chronicles", "Caracas Chronicles", "Caracas Chronicles", "osint", "news",
  "https://www.caracaschronicles.com/feed/", "en", "[\"news\",\"venezuela\",\"national-outlet\"]", 3600,
  "Caracas Chronicles — national/regional press feed (venezuela)");

RSSX(out_tico_times, "tico-times", "The Tico Times (Costa Rica)", "The Tico Times (Costa Rica)", "osint", "news",
  "https://ticotimes.net/feed", "en", "[\"news\",\"costa-rica\",\"national-outlet\"]", 3600,
  "The Tico Times (Costa Rica) — national/regional press feed (costa-rica)");

RSSX(out_jamaica_gleaner, "jamaica-gleaner", "Jamaica Gleaner", "Jamaica Gleaner", "osint", "news",
  "https://jamaica-gleaner.com/feed/rss.xml", "en", "[\"news\",\"jamaica\",\"national-outlet\"]", 3600,
  "Jamaica Gleaner — national/regional press feed (jamaica)");

RSSX(out_thecable_ng, "thecable-ng", "TheCable (Nigeria)", "TheCable (Nigeria)", "osint", "news",
  "https://www.thecable.ng/feed", "en", "[\"news\",\"nigeria\",\"national-outlet\"]", 3600,
  "TheCable (Nigeria) — national/regional press feed (nigeria)");

RSSX(out_nation_thailand, "nation-thailand", "The Nation (Thailand)", "The Nation (Thailand)", "osint", "news",
  "https://www.nationthailand.com/rss", "en", "[\"news\",\"thailand\",\"national-outlet\"]", 3600,
  "The Nation (Thailand) — national/regional press feed (thailand)");

RSSX(out_khmer_times, "khmer-times", "Khmer Times (Cambodia)", "Khmer Times (Cambodia)", "osint", "news",
  "https://www.khmertimeskh.com/feed/", "en", "[\"news\",\"cambodia\",\"national-outlet\"]", 3600,
  "Khmer Times (Cambodia) — national/regional press feed (cambodia)");

RSSX(out_myanmar_now, "myanmar-now", "Myanmar Now", "Myanmar Now", "osint", "news",
  "https://myanmar-now.org/en/rss", "en", "[\"news\",\"myanmar\",\"national-outlet\"]", 3600,
  "Myanmar Now — national/regional press feed (myanmar)");

RSSX(out_irrawaddy, "irrawaddy", "The Irrawaddy (Myanmar)", "The Irrawaddy (Myanmar)", "osint", "news",
  "https://www.irrawaddy.com/feed", "en", "[\"news\",\"myanmar\",\"national-outlet\"]", 3600,
  "The Irrawaddy (Myanmar) — national/regional press feed (myanmar)");

RSSX(out_kathmandu_post, "kathmandu-post", "The Kathmandu Post (Nepal)", "The Kathmandu Post (Nepal)", "osint", "news",
  "https://kathmandupost.com/rss", "en", "[\"news\",\"nepal\",\"national-outlet\"]", 3600,
  "The Kathmandu Post (Nepal) — national/regional press feed (nepal)");

RSSX(out_daily_mirror_lk, "daily-mirror-lk", "Daily Mirror (Sri Lanka)", "Daily Mirror (Sri Lanka)", "osint", "news",
  "https://www.dailymirror.lk/rss", "en", "[\"news\",\"sri-lanka\",\"national-outlet\"]", 3600,
  "Daily Mirror (Sri Lanka) — national/regional press feed (sri-lanka)");

RSSX(out_fiji_times, "fiji-times", "The Fiji Times", "The Fiji Times", "osint", "news",
  "https://www.fijitimes.com.fj/feed/", "en", "[\"news\",\"fiji\",\"national-outlet\"]", 3600,
  "The Fiji Times — national/regional press feed (fiji)");

RSSX(out_pina_pacific, "pina-pacific", "Pacific Islands News", "Pacific Islands News", "osint", "news",
  "https://www.rnz.co.nz/rss/pacific.xml", "en", "[\"news\",\"pacific\",\"national-outlet\"]", 3600,
  "Pacific Islands News — national/regional press feed (pacific)");

RSSX(out_buenosaires_h, "buenosaires-h", "Buenos Aires Herald", "Buenos Aires Herald", "osint", "news",
  "https://buenosairesherald.com/feed", "en", "[\"news\",\"argentina\",\"national-outlet\"]", 3600,
  "Buenos Aires Herald — national/regional press feed (argentina)");
