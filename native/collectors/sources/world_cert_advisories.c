/* National CERT / CSIRT cyber-advisory feeds worldwide (real RSS), via rss_collect. Complements the JP jpcert/ipa collectors with global coverage. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
#include "_source_macros.inc"

RSSX(cert_cert_fr, "cert-fr", "CERT-FR (ANSSI, France)", "CERT-FR (ANSSI, France)", "cyber", "cyber",
  "https://www.cert.ssi.gouv.fr/feed/", "fr", "[\"cyber\",\"cert\",\"advisory\",\"france\"]", 3600,
  "CERT-FR (ANSSI, France) — national cyber-security advisory feed");

RSSX(cert_cert_se, "cert-se", "CERT-SE (Sweden)", "CERT-SE (Sweden)", "cyber", "cyber",
  "https://www.cert.se/feed", "sv", "[\"cyber\",\"cert\",\"advisory\",\"sweden\"]", 3600,
  "CERT-SE (Sweden) — national cyber-security advisory feed");

RSSX(cert_cert_be, "cert-be", "CERT.be (Belgium)", "CERT.be (Belgium)", "cyber", "cyber",
  "https://cert.be/en/rss", "en", "[\"cyber\",\"cert\",\"advisory\",\"belgium\"]", 3600,
  "CERT.be (Belgium) — national cyber-security advisory feed");

RSSX(cert_cert_pl, "cert-pl", "CERT Polska", "CERT Polska", "cyber", "cyber",
  /* /en/posts/index.xml 404s after the site rebuild; /en/rss.xml is live */
  "https://cert.pl/en/rss.xml", "en", "[\"cyber\",\"cert\",\"advisory\",\"poland\"]", 3600,
  "CERT Polska — national cyber-security advisory feed");

RSSX(cert_ncsc_nl, "ncsc-nl", "NCSC-NL (Netherlands)", "NCSC-NL (Netherlands)", "cyber", "cyber",
  /* www.ncsc.nl became a Next.js SPA and /rss/actueel 404s; the machine-
   * readable advisory feed lives on the advisories subdomain. */
  "https://advisories.ncsc.nl/rss/advisories", "nl", "[\"cyber\",\"cert\",\"advisory\",\"netherlands\"]", 3600,
  "NCSC-NL (Netherlands) — national cyber-security advisory feed");

RSSX(cert_cert_in, "cert-in", "CERT-In (India)", "CERT-In (India)", "cyber", "cyber",
  "https://www.cert-in.org.in/RSS/CertIn.xml", "en", "[\"cyber\",\"cert\",\"advisory\",\"india\"]", 3600,
  "CERT-In (India) — national cyber-security advisory feed");

RSSX(cert_jpcert_en, "jpcert-en", "JPCERT/CC (English)", "JPCERT/CC (English)", "cyber", "cyber",
  "https://www.jpcert.or.jp/english/rss/jpcert-en.rdf", "en", "[\"cyber\",\"cert\",\"advisory\",\"japan\"]", 3600,
  "JPCERT/CC (English) — national cyber-security advisory feed");

RSSX(cert_cert_nz, "cert-nz", "CERT NZ", "CERT NZ", "cyber", "cyber",
  "https://www.cert.govt.nz/individuals/rss/", "en", "[\"cyber\",\"cert\",\"advisory\",\"new-zealand\"]", 3600,
  "CERT NZ — national cyber-security advisory feed");

RSSX(cert_acsc_au, "acsc-au", "ACSC (Australia)", "ACSC (Australia)", "cyber", "cyber",
  "https://www.cyber.gov.au/rss/advisories", "en", "[\"cyber\",\"cert\",\"advisory\",\"australia\"]", 3600,
  "ACSC (Australia) — national cyber-security advisory feed");

RSSX(cert_cccs_ca, "cccs-ca", "Canadian Centre for Cyber Security", "Canadian Centre for Cyber Security", "cyber", "cyber",
  "https://www.cyber.gc.ca/webservice/en/rss/alerts", "en", "[\"cyber\",\"cert\",\"advisory\",\"canada\"]", 3600,
  "Canadian Centre for Cyber Security — national cyber-security advisory feed");

RSSX(cert_cert_at, "cert-at", "CERT.at (Austria)", "CERT.at (Austria)", "cyber", "cyber",
  /* /de/warnungen/warnungen.xml 404s; CERT.at's feed index (/de/services/feeds)
   * lists cert-at.de.warnings.rss_2.0.xml as the live warnings feed. */
  "https://www.cert.at/cert-at.de.warnings.rss_2.0.xml", "de", "[\"cyber\",\"cert\",\"advisory\",\"austria\"]", 3600,
  "CERT.at (Austria) — national cyber-security advisory feed");

RSSX(cert_circl_lu, "circl-lu", "CIRCL (Luxembourg)", "CIRCL (Luxembourg)", "cyber", "cyber",
  "https://www.circl.lu/rss.xml", "en", "[\"cyber\",\"cert\",\"advisory\",\"luxembourg\"]", 3600,
  "CIRCL (Luxembourg) — national cyber-security advisory feed");

RSSX(cert_cert_ee, "cert-ee", "CERT-EE (Estonia)", "CERT-EE (Estonia)", "cyber", "cyber",
  /* ria.ee moved its feeds under /rss-feeds/ ; /en/rss.xml now 404s */
  "https://www.ria.ee/en/rss-feeds/rss.xml", "en", "[\"cyber\",\"cert\",\"advisory\",\"estonia\"]", 3600,
  "CERT-EE (Estonia) — national cyber-security advisory feed");

RSSX(cert_ncsc_ch, "ncsc-ch", "NCSC Switzerland", "NCSC Switzerland", "cyber", "cyber",
  /* the *.rss.html paths all serve HTML; the Confederation publishes NCSC/BACS
   * (org-nr 1101) news through the central newsd.admin.ch RSS gateway. */
  "https://www.newsd.admin.ch/newsd/feeds/rss?lang=en&org-nr=1101", "en", "[\"cyber\",\"cert\",\"advisory\",\"switzerland\"]", 3600,
  "NCSC Switzerland — national cyber-security advisory feed");

RSSX(cert_cert_ee2, "cert-ee2", "RIA Estonia Advisories", "RIA Estonia Advisories", "cyber", "cyber",
  /* Estonian-language variant of the same relocated feed index */
  "https://www.ria.ee/rss-feeds/rss.xml", "et", "[\"cyber\",\"cert\",\"advisory\",\"estonia\"]", 3600,
  "RIA Estonia Advisories — national cyber-security advisory feed");

RSSX(cert_cert_si, "cert-si", "SI-CERT (Slovenia)", "SI-CERT (Slovenia)", "cyber", "cyber",
  "https://www.cert.si/feed/", "sl", "[\"cyber\",\"cert\",\"advisory\",\"slovenia\"]", 3600,
  "SI-CERT (Slovenia) — national cyber-security advisory feed");

RSSX(cert_cert_fi, "cert-fi", "NCSC-FI (Finland)", "NCSC-FI (Finland)", "cyber", "cyber",
  /* /en/rss.xml 404s (the site moved to Next.js); the live feed is /feed/rss/en */
  "https://www.kyberturvallisuuskeskus.fi/feed/rss/en", "en", "[\"cyber\",\"cert\",\"advisory\",\"finland\"]", 3600,
  "NCSC-FI (Finland) — national cyber-security advisory feed");

RSSX(cert_cert_br, "cert-br", "CERT.br (Brazil)", "CERT.br (Brazil)", "cyber", "cyber",
  "https://www.cert.br/rss/certbr-rss.xml", "pt", "[\"cyber\",\"cert\",\"advisory\",\"brazil\"]", 3600,
  "CERT.br (Brazil) — national cyber-security advisory feed");

RSSX(cert_cert_gov_uk, "cert-gov-uk", "NCSC-UK Reports", "NCSC-UK Reports", "cyber", "cyber",
  "https://www.ncsc.gov.uk/api/1/services/v1/report-rss-feed.xml", "en", "[\"cyber\",\"cert\",\"advisory\",\"uk\"]", 3600,
  "NCSC-UK Reports — national cyber-security advisory feed");

RSSX(cert_cisa_ics, "cisa-ics", "CISA ICS Advisories", "CISA ICS Advisories", "cyber", "cyber",
  "https://www.cisa.gov/cybersecurity-advisories/ics-advisories.xml", "en", "[\"cyber\",\"cert\",\"advisory\",\"usa\"]", 3600,
  "CISA ICS Advisories — national cyber-security advisory feed");

RSSX(cert_krcert, "krcert", "KrCERT/KISA (Korea)", "KrCERT/KISA (Korea)", "cyber", "cyber",
  /* /rss/security.do 404s; the 보안공지 board feed is /kr/rss.do?bbsId=B0000133 */
  "https://www.krcert.or.kr/kr/rss.do?bbsId=B0000133", "ko", "[\"cyber\",\"cert\",\"advisory\",\"korea\"]", 3600,
  "KrCERT/KISA (Korea) — national cyber-security advisory feed");

RSSX(cert_ncsc_ie, "ncsc-ie", "NCSC Ireland", "NCSC Ireland", "cyber", "cyber",
  /* /feed/ 404s; the alerts & advisories feed is /news/alerts.rss */
  "https://www.ncsc.gov.ie/news/alerts.rss", "en", "[\"cyber\",\"cert\",\"advisory\",\"ireland\"]", 3600,
  "NCSC Ireland — national cyber-security advisory feed");

RSSX(cert_csa_sg, "csa-sg", "CSA SingCERT", "CSA SingCERT", "cyber", "cyber",
  "https://www.csa.gov.sg/rss/alerts-advisories", "en", "[\"cyber\",\"cert\",\"advisory\",\"singapore\"]", 3600,
  "CSA SingCERT — national cyber-security advisory feed");

RSSX(cert_cert_es, "cert-es", "CCN-CERT (Spain)", "CCN-CERT (Spain)", "cyber", "cyber",
  "https://www.ccn-cert.cni.es/component/obrss/rss-ultimas-vulnerabilidades.feed", "es", "[\"cyber\",\"cert\",\"advisory\",\"spain\"]", 3600,
  "CCN-CERT (Spain) — national cyber-security advisory feed");

RSSX(cert_cert_it, "cert-it", "CSIRT Italia", "CSIRT Italia", "cyber", "cyber",
  "https://www.csirt.gov.it/rss", "it", "[\"cyber\",\"cert\",\"advisory\",\"italy\"]", 3600,
  "CSIRT Italia — national cyber-security advisory feed");

RSSX(cert_cert_pt, "cert-pt", "CERT.PT (Portugal)", "CERT.PT (Portugal)", "cyber", "cyber",
  "https://www.cncs.gov.pt/pt/feed/", "pt", "[\"cyber\",\"cert\",\"advisory\",\"portugal\"]", 3600,
  "CERT.PT (Portugal) — national cyber-security advisory feed");

RSSX(cert_cert_no, "cert-no", "NSM/NCSC Norway", "NSM/NCSC Norway", "cyber", "cyber",
  /* /rss/ serves the HTML feed index; the real feed is linked from it */
  "https://nsm.no/rss/alle-oppdateringer-fra-nsm/", "no", "[\"cyber\",\"cert\",\"advisory\",\"norway\"]", 3600,
  "NSM/NCSC Norway — national cyber-security advisory feed");

/* CFCS was folded into Styrelsen for Samfundssikkerhed; cfcs.dk 301s to
 * samsik.dk, whose WordPress feed is the surviving publication channel. */
RSSX(cert_cert_dk, "cert-dk", "CFCS Denmark (Samfundssikkerhed)", "CFCS Denmark (Samfundssikkerhed)", "cyber", "cyber",
  "https://samsik.dk/feed/", "da", "[\"cyber\",\"cert\",\"advisory\",\"denmark\"]", 3600,
  "CFCS Denmark — national cyber-security advisory feed (now Styrelsen for Samfundssikkerhed)");

RSSX(cert_cert_lt, "cert-lt", "NKSC Lithuania", "NKSC Lithuania", "cyber", "cyber",
  "https://www.nksc.lt/rss.xml", "lt", "[\"cyber\",\"cert\",\"advisory\",\"lithuania\"]", 3600,
  "NKSC Lithuania — national cyber-security advisory feed");

RSSX(cert_cert_ua, "cert-ua", "CERT-UA (Ukraine)", "CERT-UA (Ukraine)", "cyber", "cyber",
  "https://cert.gov.ua/api/articles/rss", "uk", "[\"cyber\",\"cert\",\"advisory\",\"ukraine\"]", 3600,
  "CERT-UA (Ukraine) — national cyber-security advisory feed");
