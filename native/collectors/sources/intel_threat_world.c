/* collectors/sources/intel_threat_world.c — batch of high-leverage global
 * cyber-threat / SIGINT open-source feeds (vendor threat-research blogs, CERT
 * advisories, incident reporting). Each is a real RSS/Atom endpoint routed
 * through the shared rss_collect toolkit (parseFeed → intel envelope), so every
 * entry becomes one FTS-indexed, entity-hookable intel row. Scheduled hourly.
 *
 * These complement the existing JP-centric cyber collectors (jpcert-alerts,
 * ipa-alerts, sans-isc) with worldwide vendor + government threat intel. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

#define COLL     "cyber"
#define INTERVAL 3600

/* SYM, id, name, name_ja, category, url, lang, tags_json, description */
#include "_source_macros.inc"

RSS(ti_cisa_adv, "cisa-advisories", "CISA Cybersecurity Advisories",
  "CISA サイバーセキュリティ勧告", "cyber",
  "https://www.cisa.gov/cybersecurity-advisories/all.xml", "en",
  "[\"cyber\",\"advisory\",\"cisa\",\"us-gov\"]",
  "US CISA all cybersecurity advisories (ICS, alerts, KEV) — official government threat feed");

RSS(ti_cisa_news, "cisa-news", "CISA News & Alerts",
  "CISA ニュース", "cyber",
  "https://www.cisa.gov/news.xml", "en",
  "[\"cyber\",\"cisa\",\"news\"]",
  "US CISA news releases and national cyber awareness alerts");

RSS(ti_ncsc_uk, "ncsc-uk", "UK NCSC News",
  "英国 NCSC", "cyber",
  "https://www.ncsc.gov.uk/api/1/services/v1/all-rss-feed.xml", "en",
  "[\"cyber\",\"ncsc\",\"uk-gov\",\"advisory\"]",
  "UK National Cyber Security Centre reports, advisories and guidance");

RSS(ti_cert_eu, "cert-eu", "CERT-EU Threat Intelligence",
  "CERT-EU", "cyber",
  "https://cert.europa.eu/publications/threat-intelligence-rss", "en",
  "[\"cyber\",\"cert-eu\",\"eu\",\"advisory\"]",
  "CERT-EU security advisories and threat-intelligence briefs for EU institutions");

RSS(ti_hackernews, "hacker-news-sec", "The Hacker News",
  "ザ・ハッカー・ニュース", "cyber",
  "https://feeds.feedburner.com/TheHackersNews", "en",
  "[\"cyber\",\"news\",\"malware\"]",
  "The Hacker News — breaking cybersecurity, breach and malware reporting");

RSS(ti_bleeping, "bleepingcomputer", "BleepingComputer",
  "BleepingComputer", "cyber",
  "https://www.bleepingcomputer.com/feed/", "en",
  "[\"cyber\",\"news\",\"ransomware\",\"breach\"]",
  "BleepingComputer — ransomware, data breach and vulnerability news");

RSS(ti_krebs, "krebs-on-security", "Krebs on Security",
  "Krebs on Security", "cyber",
  "https://krebsonsecurity.com/feed/", "en",
  "[\"cyber\",\"investigation\",\"cybercrime\"]",
  "Brian Krebs investigative reporting on cybercrime and fraud");

RSS(ti_darkreading, "dark-reading", "Dark Reading",
  "Dark Reading", "cyber",
  "https://www.darkreading.com/rss.xml", "en",
  "[\"cyber\",\"news\",\"threats\"]",
  "Dark Reading — enterprise security news, threats and analysis");

RSS(ti_secweek, "securityweek", "SecurityWeek",
  "SecurityWeek", "cyber",
  "https://www.securityweek.com/feed/", "en",
  "[\"cyber\",\"news\",\"threats\"]",
  "SecurityWeek — cybersecurity news, ICS/OT and threat intelligence");

RSS(ti_schneier, "schneier", "Schneier on Security",
  "Schneier on Security", "cyber",
  "https://www.schneier.com/feed/atom/", "en",
  "[\"cyber\",\"analysis\",\"crypto\"]",
  "Bruce Schneier — security, cryptography and surveillance analysis");

RSS(ti_talos, "cisco-talos", "Cisco Talos Intelligence",
  "Cisco Talos", "cyber",
  "https://blog.talosintelligence.com/rss/", "en",
  "[\"cyber\",\"threat-research\",\"malware\"]",
  "Cisco Talos — threat research, campaign tracking and IOC reporting");

RSS(ti_unit42, "unit42", "Palo Alto Unit 42",
  "Unit 42", "cyber",
  "https://unit42.paloaltonetworks.com/feed/", "en",
  "[\"cyber\",\"threat-research\",\"apt\"]",
  "Palo Alto Networks Unit 42 threat research and adversary tracking");

/* msrc.microsoft.com/blog/feed/ 301s to an SPA that serves HTML, and
 * msrc-blog.microsoft.com is gone — the blog no longer publishes a feed. The
 * MSRC Security Update Guide RSS is the surviving machine-readable channel
 * and is the actual vulnerability-disclosure stream this source promises.
 * NOTE: it carries the full CVE backlog (~5k items), so the first run is
 * large; a maxitems cap in rss_collect would be worth adding. */
RSS(ti_msrc, "msrc-blog", "Microsoft Security Update Guide",
  "Microsoft MSRC", "cyber",
  "https://api.msrc.microsoft.com/update-guide/rss", "en",
  "[\"cyber\",\"microsoft\",\"advisory\"]",
  "Microsoft Security Response Center — CVE and security-update disclosures");

RSS(ti_p0, "project-zero", "Google Project Zero",
  "Project Zero", "cyber",
  "https://googleprojectzero.blogspot.com/feeds/posts/default", "en",
  "[\"cyber\",\"0day\",\"vuln-research\"]",
  "Google Project Zero — zero-day vulnerability research and exploitation");

RSS(ti_gsb, "google-security-blog", "Google Online Security Blog",
  "Google セキュリティ", "cyber",
  "https://security.googleblog.com/feeds/posts/default", "en",
  "[\"cyber\",\"google\",\"threats\"]",
  "Google Online Security Blog — TAG threat analysis and defensive research");

RSS(ti_dfir, "dfir-report", "The DFIR Report",
  "The DFIR Report", "cyber",
  "https://thedfirreport.com/feed/", "en",
  "[\"cyber\",\"dfir\",\"intrusion\",\"ttps\"]",
  "The DFIR Report — real-world intrusion analysis with TTPs and IOCs");

RSS(ti_mwbytes, "malwarebytes-labs", "Malwarebytes Labs",
  "Malwarebytes Labs", "cyber",
  "https://www.malwarebytes.com/blog/feed/index.xml", "en",
  "[\"cyber\",\"malware\",\"threats\"]",
  "Malwarebytes Labs — malware, scam and threat research");

RSS(ti_securelist, "securelist", "Securelist by Kaspersky",
  "Securelist", "cyber",
  "https://securelist.com/feed/", "en",
  "[\"cyber\",\"apt\",\"malware\",\"threat-research\"]",
  "Kaspersky Securelist — APT campaigns, malware and threat research");

RSS(ti_eset, "welivesecurity", "WeLiveSecurity by ESET",
  "WeLiveSecurity", "cyber",
  "https://www.welivesecurity.com/en/rss/feed/", "en",
  "[\"cyber\",\"malware\",\"threat-research\"]",
  "ESET WeLiveSecurity — malware analysis and threat intelligence");

RSS(ti_checkpoint, "checkpoint-research", "Check Point Research",
  "Check Point Research", "cyber",
  "https://research.checkpoint.com/feed/", "en",
  "[\"cyber\",\"threat-research\",\"malware\"]",
  "Check Point Research — global threat intelligence and reverse engineering");

RSS(ti_rapid7, "rapid7-blog", "Rapid7 Blog",
  "Rapid7", "cyber",
  "https://blog.rapid7.com/rss/", "en",
  "[\"cyber\",\"vuln\",\"research\"]",
  "Rapid7 — vulnerability research, emergent threats and detection engineering");

RSS(ti_sophos, "sophos-news", "Sophos News",
  "Sophos News", "cyber",
  "https://news.sophos.com/en-us/feed/", "en",
  "[\"cyber\",\"malware\",\"threats\"]",
  "Sophos X-Ops threat research and security news");

RSS(ti_recfut, "recorded-future", "Recorded Future Blog",
  "Recorded Future", "cyber",
  "https://www.recordedfuture.com/feed", "en",
  "[\"cyber\",\"threat-intel\",\"geopolitics\"]",
  "Recorded Future — threat intelligence and geopolitical cyber analysis");

RSS(ti_therecord, "the-record", "The Record by Recorded Future",
  "The Record", "cyber",
  "https://therecord.media/feed/", "en",
  "[\"cyber\",\"news\",\"nation-state\"]",
  "The Record — cybercrime, nation-state and cyber-policy journalism");

RSS(ti_cybdive, "cybersecurity-dive", "Cybersecurity Dive",
  "Cybersecurity Dive", "cyber",
  "https://www.cybersecuritydive.com/feeds/news/", "en",
  "[\"cyber\",\"news\",\"enterprise\"]",
  "Cybersecurity Dive — industry news, breaches and regulation");

RSS(ti_grahamc, "graham-cluley", "Graham Cluley",
  "Graham Cluley", "cyber",
  "https://grahamcluley.com/feed/", "en",
  "[\"cyber\",\"news\",\"analysis\"]",
  "Graham Cluley — security news, scams and commentary");

RSS(ti_troyhunt, "troy-hunt", "Troy Hunt",
  "Troy Hunt", "cyber",
  "https://www.troyhunt.com/rss/", "en",
  "[\"cyber\",\"breach\",\"privacy\"]",
  "Troy Hunt (Have I Been Pwned) — breach analysis and web security");

RSS(ti_intel471, "intel471", "Intel 471 Blog",
  "Intel 471", "cyber",
  "https://intel471.com/blog/feed", "en",
  "[\"cyber\",\"cybercrime\",\"threat-intel\"]",
  "Intel 471 — cybercrime underground and adversary intelligence");
