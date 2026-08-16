/* collectors/sources/intel_vuln_world.c — vulnerability / exploit / advisory
 * disclosure feeds. Real RSS/Atom endpoints (CVE aggregators, exploit
 * archives, coordinated-disclosure programs) routed through rss_collect so each
 * disclosure lands as an intel row keyed by guid/link. Scheduled hourly to
 * surface fresh CVEs and PoCs alongside the JP cyber collectors. */
#include "source.h"
#include "lib/rss_atom.h"

#define COLL     "cyber"
#define INTERVAL 3600

#include "_source_macros.inc"

RSS(vu_cvefeed, "cvefeed-latest", "CVEFeed Latest CVEs",
  "CVEフィード 最新CVE", "cyber",
  "https://cvefeed.io/rssfeed/latest.xml", "en",
  "[\"cyber\",\"cve\",\"vulnerability\"]",
  "Latest published CVEs with CVSS scoring — near-real-time vulnerability feed");

RSS(vu_cvefeed_news, "cvefeed-newsroom", "CVEFeed Newsroom",
  "CVEフィード ニュース", "cyber",
  "https://cvefeed.io/rssfeed/newsroom.xml", "en",
  "[\"cyber\",\"cve\",\"news\"]",
  "CVEFeed newsroom — exploited and trending vulnerability reporting");

RSS(vu_exploitdb, "exploit-db", "Exploit Database",
  "Exploit-DB", "cyber",
  "https://www.exploit-db.com/rss.xml", "en",
  "[\"cyber\",\"exploit\",\"poc\"]",
  "Offensive Security Exploit-DB — public exploit and PoC archive");

RSS(vu_packetstorm, "packetstorm", "Packet Storm Security",
  "Packet Storm", "cyber",
  "https://rss.packetstormsecurity.com/files/", "en",
  "[\"cyber\",\"exploit\",\"advisory\",\"tools\"]",
  "Packet Storm — exploits, advisories and security tool releases");

RSS(vu_fulldisc, "full-disclosure", "Full Disclosure Mailing List",
  "Full Disclosure ML", "cyber",
  "https://seclists.org/rss/fulldisclosure.rss", "en",
  "[\"cyber\",\"disclosure\",\"vuln\"]",
  "Seclists Full Disclosure — unmoderated vulnerability disclosure list");

RSS(vu_osssec, "oss-security", "oss-security Mailing List",
  "oss-security ML", "cyber",
  "https://seclists.org/rss/oss-sec.rss", "en",
  "[\"cyber\",\"disclosure\",\"open-source\"]",
  "Seclists oss-security — open-source software vulnerability disclosure");

RSS(vu_zdi_pub, "zdi-published", "Zero Day Initiative (Published)",
  "ZDI 公開済み", "cyber",
  "https://www.zerodayinitiative.com/rss/published/", "en",
  "[\"cyber\",\"vuln\",\"zdi\",\"advisory\"]",
  "Trend Micro ZDI — published coordinated-disclosure advisories");

RSS(vu_zdi_up, "zdi-upcoming", "Zero Day Initiative (Upcoming)",
  "ZDI 予定", "cyber",
  "https://www.zerodayinitiative.com/rss/upcoming/", "en",
  "[\"cyber\",\"vuln\",\"zdi\",\"advisory\"]",
  "Trend Micro ZDI — upcoming advisories awaiting vendor patch");

RSS(vu_vuldb, "vuldb-recent", "VulDB Recent Entries",
  "VulDB 最新", "cyber",
  "https://vuldb.com/?rss.recent", "en",
  "[\"cyber\",\"cve\",\"vulnerability\"]",
  "VulDB — recently added vulnerability database entries with risk scoring");

RSS(vu_tenable, "tenable-tra", "Tenable Research Advisories",
  "Tenable リサーチ勧告", "cyber",
  "https://www.tenable.com/security/research/feed", "en",
  "[\"cyber\",\"vuln\",\"advisory\"]",
  "Tenable Research advisories — vendor-coordinated vulnerability disclosures");

RSS(vu_wpscan, "wpscan-vulndb", "WPScan Vulnerability Feed",
  "WPScan 脆弱性", "cyber",
  "https://wpscan.com/feed/", "en",
  "[\"cyber\",\"wordpress\",\"vuln\"]",
  "WPScan — WordPress core, plugin and theme vulnerability reporting");

/* talosintelligence.com/feeds/vuln_reports.xml was retired (404 since at least
 * 2026-07); Talos now publishes its coordinated-disclosure write-ups only
 * through the research blog feed, so that is what we consume. */
RSS(vu_talos_disc, "talos-disclosures", "Talos Intelligence Research",
  "Talos リサーチ", "cyber",
  "https://blog.talosintelligence.com/rss/", "en",
  "[\"cyber\",\"vuln\",\"advisory\",\"talos\"]",
  "Cisco Talos research blog — vulnerability disclosures and threat research");
