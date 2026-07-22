/* Gap-region national CERTs and outlets (best-effort real RSS) — deeper country coverage, via rss_collect. */
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

RSSX(gap_cert_gr, "cert-gr", "GR-CSIRT (Greece)", "GR-CSIRT (Greece)", "cyber", "cyber",
  "https://csirt.cynet.gov.cy/feed/", "el", "[\"cyber\",\"cyprus\",\"national\"]", 3600,
  "GR-CSIRT (Greece) — national cyber feed (cyprus)");

RSSX(gap_cert_hu, "cert-hu", "NBSZ NCSC Hungary", "NBSZ NCSC Hungary", "cyber", "cyber",
  "https://nki.gov.hu/feed/", "hu", "[\"cyber\",\"hungary\",\"national\"]", 3600,
  "NBSZ NCSC Hungary — national cyber feed (hungary)");

RSSX(gap_cert_ro, "cert-ro", "CERT-RO / DNSC Romania", "CERT-RO / DNSC Romania", "cyber", "cyber",
  "https://dnsc.ro/feed", "ro", "[\"cyber\",\"romania\",\"national\"]", 3600,
  "CERT-RO / DNSC Romania — national cyber feed (romania)");

RSSX(gap_cert_hr, "cert-hr", "CERT.hr (Croatia)", "CERT.hr (Croatia)", "cyber", "cyber",
  "https://www.cert.hr/feed/", "hr", "[\"cyber\",\"croatia\",\"national\"]", 3600,
  "CERT.hr (Croatia) — national cyber feed (croatia)");

RSSX(gap_cert_bg, "cert-bg", "CERT Bulgaria", "CERT Bulgaria", "cyber", "cyber",
  "https://www.govcert.bg/feed/", "bg", "[\"cyber\",\"bulgaria\",\"national\"]", 3600,
  "CERT Bulgaria — national cyber feed (bulgaria)");

RSSX(gap_cert_tr, "cert-tr", "USOM Turkey", "USOM Turkey", "cyber", "cyber",
  "https://www.usom.gov.tr/rss/duyuru.rss", "tr", "[\"cyber\",\"turkey\",\"national\"]", 3600,
  "USOM Turkey — national cyber feed (turkey)");

RSSX(gap_cert_lk, "cert-lk", "Sri Lanka CERT|CC", "Sri Lanka CERT|CC", "cyber", "cyber",
  "https://www.cert.gov.lk/feed/", "en", "[\"cyber\",\"sri-lanka\",\"national\"]", 3600,
  "Sri Lanka CERT|CC — national cyber feed (sri-lanka)");

RSSX(gap_cert_za, "cert-za", "CSIRT South Africa", "CSIRT South Africa", "cyber", "cyber",
  "https://www.cybersecurityhub.gov.za/feed/", "en", "[\"cyber\",\"south-africa\",\"national\"]", 3600,
  "CSIRT South Africa — national cyber feed (south-africa)");

RSSX(gap_cert_ng, "cert-ng", "ngCERT Nigeria", "ngCERT Nigeria", "cyber", "cyber",
  "https://cert.gov.ng/feed/", "en", "[\"cyber\",\"nigeria\",\"national\"]", 3600,
  "ngCERT Nigeria — national cyber feed (nigeria)");

RSSX(gap_cert_my, "cert-my", "MyCERT Malaysia", "MyCERT Malaysia", "cyber", "cyber",
  "https://www.mycert.org.my/portal/rss", "en", "[\"cyber\",\"malaysia\",\"national\"]", 3600,
  "MyCERT Malaysia — national cyber feed (malaysia)");

RSSX(gap_cert_id, "cert-id", "BSSN Indonesia", "BSSN Indonesia", "cyber", "cyber",
  "https://bssn.go.id/feed/", "id", "[\"cyber\",\"indonesia\",\"national\"]", 3600,
  "BSSN Indonesia — national cyber feed (indonesia)");

RSSX(gap_cert_ph, "cert-ph", "NCERT Philippines", "NCERT Philippines", "cyber", "cyber",
  "https://ncert.gov.ph/feed/", "en", "[\"cyber\",\"philippines\",\"national\"]", 3600,
  "NCERT Philippines — national cyber feed (philippines)");

RSSX(gap_cert_vn, "cert-vn", "VNCERT Vietnam", "VNCERT Vietnam", "cyber", "cyber",
  "https://vncert.vn/rss", "vi", "[\"cyber\",\"vietnam\",\"national\"]", 3600,
  "VNCERT Vietnam — national cyber feed (vietnam)");

RSSX(gap_cert_ae, "cert-ae", "aeCERT UAE", "aeCERT UAE", "cyber", "cyber",
  "https://www.tra.gov.ae/en/rss.aspx", "en", "[\"cyber\",\"uae\",\"national\"]", 3600,
  "aeCERT UAE — national cyber feed (uae)");

RSSX(gap_cert_sa, "cert-sa", "Saudi NCA", "Saudi NCA", "cyber", "cyber",
  "https://nca.gov.sa/rss", "ar", "[\"cyber\",\"saudi-arabia\",\"national\"]", 3600,
  "Saudi NCA — national cyber feed (saudi-arabia)");

RSSX(gap_outlet_kompas, "outlet-kompas", "Kompas (Indonesia)", "Kompas (Indonesia)", "osint", "news",
  "https://www.kompas.com/rss", "id", "[\"news\",\"indonesia\",\"national\"]", 3600,
  "Kompas (Indonesia) — national news feed (indonesia)");

RSSX(gap_outlet_thairath, "outlet-thairath", "Thairath (Thailand)", "Thairath (Thailand)", "osint", "news",
  "https://www.thairath.co.th/rss/news.xml", "th", "[\"news\",\"thailand\",\"national\"]", 3600,
  "Thairath (Thailand) — national news feed (thailand)");

RSSX(gap_outlet_vanguard_ng, "outlet-vanguard-ng", "Vanguard (Nigeria)", "Vanguard (Nigeria)", "osint", "news",
  "https://www.vanguardngr.com/feed/", "en", "[\"news\",\"nigeria\",\"national\"]", 3600,
  "Vanguard (Nigeria) — national news feed (nigeria)");

RSSX(gap_outlet_standard_ke, "outlet-standard-ke", "The Standard (Kenya)", "The Standard (Kenya)", "osint", "news",
  "https://www.standardmedia.co.ke/rss/headlines.php", "en", "[\"news\",\"kenya\",\"national\"]", 3600,
  "The Standard (Kenya) — national news feed (kenya)");

RSSX(gap_outlet_el_comercio_pe, "outlet-el-comercio-pe", "El Comercio (Peru)", "El Comercio (Peru)", "osint", "news",
  "https://elcomercio.pe/arcio/rss/", "es", "[\"news\",\"peru\",\"national\"]", 3600,
  "El Comercio (Peru) — national news feed (peru)");
