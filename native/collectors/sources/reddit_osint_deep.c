/* Deep OSINT / SIGINT / GEOINT / cyber subreddit feeds (deterministic .rss) — specialist community signal, via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#define RSSX(SYM, ID, NAME, NAMEJA, COLL, CAT, URL, LANG, TAGS, IVAL, DESC)  \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    int n = rss_collect(c, s, URL, LANG, TAGS); return n < 0 ? -1 : 0; }     \
  static const source_def SYM = {                                           \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,            \
    .update_interval_sec = IVAL, .run = run_##SYM,                           \
    .category = CAT, .type = "web_request", .url = URL,                      \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

RSSX(rsub_asknetsec, "reddit-asknetsec", "Reddit r/AskNetsec", "Reddit r/AskNetsec", "osint", "social",
  "https://www.reddit.com/r/AskNetsec/.rss", "en", "[\"reddit\",\"social\",\"cyber\"]", 1800,
  "Reddit r/AskNetsec — cyber community signal");

RSSX(rsub_blueteamsec, "reddit-blueteamsec", "Reddit r/blueteamsec", "Reddit r/blueteamsec", "osint", "social",
  "https://www.reddit.com/r/blueteamsec/.rss", "en", "[\"reddit\",\"social\",\"cyber\"]", 1800,
  "Reddit r/blueteamsec — cyber community signal");

RSSX(rsub_redteamsec, "reddit-redteamsec", "Reddit r/redteamsec", "Reddit r/redteamsec", "osint", "social",
  "https://www.reddit.com/r/redteamsec/.rss", "en", "[\"reddit\",\"social\",\"cyber\"]", 1800,
  "Reddit r/redteamsec — cyber community signal");

RSSX(rsub_threatintel, "reddit-threatintel", "Reddit r/threatintel", "Reddit r/threatintel", "osint", "social",
  "https://www.reddit.com/r/threatintel/.rss", "en", "[\"reddit\",\"social\",\"threat-intel\"]", 1800,
  "Reddit r/threatintel — threat-intel community signal");

RSSX(rsub_pentesting, "reddit-pentesting", "Reddit r/Pentesting", "Reddit r/Pentesting", "osint", "social",
  "https://www.reddit.com/r/Pentesting/.rss", "en", "[\"reddit\",\"social\",\"cyber\"]", 1800,
  "Reddit r/Pentesting — cyber community signal");

RSSX(rsub_howtohack, "reddit-howtohack", "Reddit r/HowToHack", "Reddit r/HowToHack", "osint", "social",
  "https://www.reddit.com/r/HowToHack/.rss", "en", "[\"reddit\",\"social\",\"cyber\"]", 1800,
  "Reddit r/HowToHack — cyber community signal");

RSSX(rsub_reverseengineering, "reddit-reverseengineering", "Reddit r/ReverseEngineering", "Reddit r/ReverseEngineering", "osint", "social",
  "https://www.reddit.com/r/ReverseEngineering/.rss", "en", "[\"reddit\",\"social\",\"reversing\"]", 1800,
  "Reddit r/ReverseEngineering — reversing community signal");

RSSX(rsub_computerforensics, "reddit-computerforensics", "Reddit r/ComputerForensics", "Reddit r/ComputerForensics", "osint", "social",
  "https://www.reddit.com/r/ComputerForensics/.rss", "en", "[\"reddit\",\"social\",\"forensics\"]", 1800,
  "Reddit r/ComputerForensics — forensics community signal");

RSSX(rsub_computerforensics_2, "reddit-computerforensics-2", "Reddit r/computerforensics", "Reddit r/computerforensics", "osint", "social",
  "https://www.reddit.com/r/computerforensics/.rss", "en", "[\"reddit\",\"social\",\"forensics\"]", 1800,
  "Reddit r/computerforensics — forensics community signal");

RSSX(rsub_digitalforensics, "reddit-digitalforensics", "Reddit r/digitalforensics", "Reddit r/digitalforensics", "osint", "social",
  "https://www.reddit.com/r/digitalforensics/.rss", "en", "[\"reddit\",\"social\",\"forensics\"]", 1800,
  "Reddit r/digitalforensics — forensics community signal");

RSSX(rsub_privacy, "reddit-privacy", "Reddit r/privacy", "Reddit r/privacy", "osint", "social",
  "https://www.reddit.com/r/privacy/.rss", "en", "[\"reddit\",\"social\",\"privacy\"]", 1800,
  "Reddit r/privacy — privacy community signal");

RSSX(rsub_opsec, "reddit-opsec", "Reddit r/opsec", "Reddit r/opsec", "osint", "social",
  "https://www.reddit.com/r/opsec/.rss", "en", "[\"reddit\",\"social\",\"opsec\"]", 1800,
  "Reddit r/opsec — opsec community signal");

RSSX(rsub_onions, "reddit-onions", "Reddit r/onions", "Reddit r/onions", "osint", "social",
  "https://www.reddit.com/r/onions/.rss", "en", "[\"reddit\",\"social\",\"dark-web\"]", 1800,
  "Reddit r/onions — dark-web community signal");

RSSX(rsub_deepweb, "reddit-deepweb", "Reddit r/deepweb", "Reddit r/deepweb", "osint", "social",
  "https://www.reddit.com/r/deepweb/.rss", "en", "[\"reddit\",\"social\",\"dark-web\"]", 1800,
  "Reddit r/deepweb — dark-web community signal");

RSSX(rsub_remotesensing, "reddit-remotesensing", "Reddit r/RemoteSensing", "Reddit r/RemoteSensing", "osint", "social",
  "https://www.reddit.com/r/RemoteSensing/.rss", "en", "[\"reddit\",\"social\",\"geoint\"]", 1800,
  "Reddit r/RemoteSensing — geoint community signal");

RSSX(rsub_gis, "reddit-gis", "Reddit r/gis", "Reddit r/gis", "osint", "social",
  "https://www.reddit.com/r/gis/.rss", "en", "[\"reddit\",\"social\",\"geoint\"]", 1800,
  "Reddit r/gis — geoint community signal");

RSSX(rsub_qgis, "reddit-qgis", "Reddit r/QGIS", "Reddit r/QGIS", "osint", "social",
  "https://www.reddit.com/r/QGIS/.rss", "en", "[\"reddit\",\"social\",\"geoint\"]", 1800,
  "Reddit r/QGIS — geoint community signal");

RSSX(rsub_geospatial, "reddit-geospatial", "Reddit r/geospatial", "Reddit r/geospatial", "osint", "social",
  "https://www.reddit.com/r/geospatial/.rss", "en", "[\"reddit\",\"social\",\"geoint\"]", 1800,
  "Reddit r/geospatial — geoint community signal");

RSSX(rsub_adsb, "reddit-adsb", "Reddit r/adsb", "Reddit r/adsb", "osint", "social",
  "https://www.reddit.com/r/adsb/.rss", "en", "[\"reddit\",\"social\",\"aviation-tracking\"]", 1800,
  "Reddit r/adsb — aviation-tracking community signal");

RSSX(rsub_rtlsdr, "reddit-rtlsdr", "Reddit r/RTLSDR", "Reddit r/RTLSDR", "osint", "social",
  "https://www.reddit.com/r/RTLSDR/.rss", "en", "[\"reddit\",\"social\",\"sigint\"]", 1800,
  "Reddit r/RTLSDR — sigint community signal");

RSSX(rsub_amateurradio, "reddit-amateurradio", "Reddit r/amateurradio", "Reddit r/amateurradio", "osint", "social",
  "https://www.reddit.com/r/amateurradio/.rss", "en", "[\"reddit\",\"social\",\"sigint\"]", 1800,
  "Reddit r/amateurradio — sigint community signal");

RSSX(rsub_shortwave, "reddit-shortwave", "Reddit r/shortwave", "Reddit r/shortwave", "osint", "social",
  "https://www.reddit.com/r/shortwave/.rss", "en", "[\"reddit\",\"social\",\"sigint\"]", 1800,
  "Reddit r/shortwave — sigint community signal");

RSSX(rsub_numberstations, "reddit-numberstations", "Reddit r/numberstations", "Reddit r/numberstations", "osint", "social",
  "https://www.reddit.com/r/numberstations/.rss", "en", "[\"reddit\",\"social\",\"sigint\"]", 1800,
  "Reddit r/numberstations — sigint community signal");

RSSX(rsub_hacking, "reddit-hacking", "Reddit r/hacking", "Reddit r/hacking", "osint", "social",
  "https://www.reddit.com/r/hacking/.rss", "en", "[\"reddit\",\"social\",\"cyber\"]", 1800,
  "Reddit r/hacking — cyber community signal");

RSSX(rsub_blackhatlounge, "reddit-blackhatlounge", "Reddit r/blackhatlounge", "Reddit r/blackhatlounge", "osint", "social",
  "https://www.reddit.com/r/blackhatlounge/.rss", "en", "[\"reddit\",\"social\",\"cyber\"]", 1800,
  "Reddit r/blackhatlounge — cyber community signal");

RSSX(rsub_information_security, "reddit-information-security", "Reddit r/Information_Security", "Reddit r/Information_Security", "osint", "social",
  "https://www.reddit.com/r/Information_Security/.rss", "en", "[\"reddit\",\"social\",\"cyber\"]", 1800,
  "Reddit r/Information_Security — cyber community signal");

RSSX(rsub_scams, "reddit-scams", "Reddit r/Scams", "Reddit r/Scams", "osint", "social",
  "https://www.reddit.com/r/Scams/.rss", "en", "[\"reddit\",\"social\",\"fraud\"]", 1800,
  "Reddit r/Scams — fraud community signal");

RSSX(rsub_phishing, "reddit-phishing", "Reddit r/phishing", "Reddit r/phishing", "osint", "social",
  "https://www.reddit.com/r/phishing/.rss", "en", "[\"reddit\",\"social\",\"fraud\"]", 1800,
  "Reddit r/phishing — fraud community signal");

RSSX(rsub_bitcoin, "reddit-bitcoin", "Reddit r/Bitcoin", "Reddit r/Bitcoin", "osint", "social",
  "https://www.reddit.com/r/Bitcoin/.rss", "en", "[\"reddit\",\"social\",\"crypto\"]", 1800,
  "Reddit r/Bitcoin — crypto community signal");

RSSX(rsub_cryptocurrency, "reddit-cryptocurrency", "Reddit r/CryptoCurrency", "Reddit r/CryptoCurrency", "osint", "social",
  "https://www.reddit.com/r/CryptoCurrency/.rss", "en", "[\"reddit\",\"social\",\"crypto\"]", 1800,
  "Reddit r/CryptoCurrency — crypto community signal");

RSSX(rsub_monero, "reddit-monero", "Reddit r/Monero", "Reddit r/Monero", "osint", "social",
  "https://www.reddit.com/r/Monero/.rss", "en", "[\"reddit\",\"social\",\"crypto\"]", 1800,
  "Reddit r/Monero — crypto community signal");

RSSX(rsub_warshipporn, "reddit-warshipporn", "Reddit r/WarshipPorn", "Reddit r/WarshipPorn", "osint", "social",
  "https://www.reddit.com/r/WarshipPorn/.rss", "en", "[\"reddit\",\"social\",\"military\"]", 1800,
  "Reddit r/WarshipPorn — military community signal");

RSSX(rsub_militaryporn, "reddit-militaryporn", "Reddit r/MilitaryPorn", "Reddit r/MilitaryPorn", "osint", "social",
  "https://www.reddit.com/r/MilitaryPorn/.rss", "en", "[\"reddit\",\"social\",\"military\"]", 1800,
  "Reddit r/MilitaryPorn — military community signal");

RSSX(rsub_interestingasfuck, "reddit-interestingasfuck", "Reddit r/interestingasfuck", "Reddit r/interestingasfuck", "osint", "social",
  "https://www.reddit.com/r/interestingasfuck/.rss", "en", "[\"reddit\",\"social\",\"world\"]", 1800,
  "Reddit r/interestingasfuck — world community signal");

RSSX(rsub_truereddit, "reddit-truereddit", "Reddit r/TrueReddit", "Reddit r/TrueReddit", "osint", "social",
  "https://www.reddit.com/r/TrueReddit/.rss", "en", "[\"reddit\",\"social\",\"analysis\"]", 1800,
  "Reddit r/TrueReddit — analysis community signal");

RSSX(rsub_foreignpolicy, "reddit-foreignpolicy", "Reddit r/foreignpolicy", "Reddit r/foreignpolicy", "osint", "social",
  "https://www.reddit.com/r/foreignpolicy/.rss", "en", "[\"reddit\",\"social\",\"geopolitics\"]", 1800,
  "Reddit r/foreignpolicy — geopolitics community signal");

RSSX(rsub_china, "reddit-china-2", "Reddit r/China", "Reddit r/China", "osint", "social",
  "https://www.reddit.com/r/China/.rss", "en", "[\"reddit\",\"social\",\"china-2\"]", 1800,
  "Reddit r/China — china-2 community signal");

RSSX(rsub_ukraine, "reddit-ukraine", "Reddit r/ukraine", "Reddit r/ukraine", "osint", "social",
  "https://www.reddit.com/r/ukraine/.rss", "en", "[\"reddit\",\"social\",\"ukraine-2\"]", 1800,
  "Reddit r/ukraine — ukraine-2 community signal");
