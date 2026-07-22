/* Leak / ransomware / breach trackers and specialist threat-research blogs (real RSS) — high-penetrancy adversary and exposure signal, via rss_collect. */
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

RSSX(leak_databreaches_net, "databreaches-net", "DataBreaches.net", "DataBreaches.net", "cyber", "cyber",
  "https://www.databreaches.net/feed/", "en", "[\"cyber\",\"breach\",\"threat-intel\"]", 3600,
  "DataBreaches.net — breach threat-intelligence feed");

RSSX(leak_malware_traffic, "malware-traffic", "Malware-Traffic-Analysis", "Malware-Traffic-Analysis", "cyber", "cyber",
  "https://www.malware-traffic-analysis.net/blog-entries.rss", "en", "[\"cyber\",\"malware\",\"threat-intel\"]", 3600,
  "Malware-Traffic-Analysis — malware threat-intelligence feed");

RSSX(leak_objective_see, "objective-see", "Objective-See (macOS)", "Objective-See (macOS)", "cyber", "cyber",
  "https://objective-see.org/rss.xml", "en", "[\"cyber\",\"malware\",\"threat-intel\"]", 3600,
  "Objective-See (macOS) — malware threat-intelligence feed");

RSSX(leak_hexacorn, "hexacorn", "Hexacorn", "Hexacorn", "cyber", "cyber",
  "https://www.hexacorn.com/blog/feed/", "en", "[\"cyber\",\"dfir\",\"threat-intel\"]", 3600,
  "Hexacorn — dfir threat-intelligence feed");

RSSX(leak_didier_stevens, "didier-stevens", "Didier Stevens", "Didier Stevens", "cyber", "cyber",
  "https://blog.didierstevens.com/feed/", "en", "[\"cyber\",\"dfir\",\"threat-intel\"]", 3600,
  "Didier Stevens — dfir threat-intelligence feed");

RSSX(leak_specterops, "specterops", "SpecterOps Posts", "SpecterOps Posts", "cyber", "cyber",
  "https://posts.specterops.io/feed", "en", "[\"cyber\",\"offensive\",\"threat-intel\"]", 3600,
  "SpecterOps Posts — offensive threat-intelligence feed");

RSSX(leak_redcanary_blog, "redcanary-blog", "Red Canary Blog", "Red Canary Blog", "cyber", "cyber",
  "https://redcanary.com/blog/feed/", "en", "[\"cyber\",\"detection\",\"threat-intel\"]", 3600,
  "Red Canary Blog — detection threat-intelligence feed");

RSSX(leak_huntress_blog, "huntress-blog", "Huntress Blog", "Huntress Blog", "cyber", "cyber",
  "https://www.huntress.com/blog/rss.xml", "en", "[\"cyber\",\"detection\",\"threat-intel\"]", 3600,
  "Huntress Blog — detection threat-intelligence feed");

RSSX(leak_volexity_blog, "volexity-blog", "Volexity Blog", "Volexity Blog", "cyber", "cyber",
  "https://www.volexity.com/blog/feed/", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Volexity Blog — threat-research threat-intelligence feed");

RSSX(leak_group_ib, "group-ib", "Group-IB Blog", "Group-IB Blog", "cyber", "cyber",
  "https://www.group-ib.com/blog/rss/", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Group-IB Blog — threat-research threat-intelligence feed");

RSSX(leak_sentinellabs, "sentinellabs", "SentinelLabs", "SentinelLabs", "cyber", "cyber",
  "https://www.sentinelone.com/labs/feed/", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "SentinelLabs — threat-research threat-intelligence feed");

RSSX(leak_cyble_blog, "cyble-blog", "Cyble Research", "Cyble Research", "cyber", "cyber",
  "https://cyble.com/feed/", "en", "[\"cyber\",\"threat-intel\",\"threat-intel\"]", 3600,
  "Cyble Research — threat-intel threat-intelligence feed");

RSSX(leak_socradar, "socradar", "SOCRadar Blog", "SOCRadar Blog", "cyber", "cyber",
  "https://socradar.io/feed/", "en", "[\"cyber\",\"threat-intel\",\"threat-intel\"]", 3600,
  "SOCRadar Blog — threat-intel threat-intelligence feed");

RSSX(leak_flashpoint, "flashpoint", "Flashpoint", "Flashpoint", "cyber", "cyber",
  "https://flashpoint.io/feed/", "en", "[\"cyber\",\"threat-intel\",\"threat-intel\"]", 3600,
  "Flashpoint — threat-intel threat-intelligence feed");

RSSX(leak_fortiguard, "fortiguard", "Fortinet FortiGuard Labs", "Fortinet FortiGuard Labs", "cyber", "cyber",
  "https://feeds.fortinet.com/fortinet/blog/threat-research", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Fortinet FortiGuard Labs — threat-research threat-intelligence feed");

RSSX(leak_proofpoint, "proofpoint", "Proofpoint Threat Insight", "Proofpoint Threat Insight", "cyber", "cyber",
  "https://www.proofpoint.com/us/rss.xml", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Proofpoint Threat Insight — threat-research threat-intelligence feed");

RSSX(leak_trustwave_spiderlabs, "trustwave-spiderlabs", "Trustwave SpiderLabs", "Trustwave SpiderLabs", "cyber", "cyber",
  "https://www.trustwave.com/en-us/rss/spiderlabs-blog/", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Trustwave SpiderLabs — threat-research threat-intelligence feed");

RSSX(leak_mandiant_blog, "mandiant-blog", "Mandiant Threat Intel", "Mandiant Threat Intel", "cyber", "cyber",
  "https://www.mandiant.com/resources/blog/rss.xml", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Mandiant Threat Intel — threat-research threat-intelligence feed");

RSSX(leak_microsoft_sec_blog, "microsoft-sec-blog", "Microsoft Security Blog", "Microsoft Security Blog", "cyber", "cyber",
  "https://www.microsoft.com/en-us/security/blog/feed/", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Microsoft Security Blog — threat-research threat-intelligence feed");

RSSX(leak_cofense, "cofense", "Cofense Phishing", "Cofense Phishing", "cyber", "cyber",
  "https://cofense.com/feed/", "en", "[\"cyber\",\"phishing\",\"threat-intel\"]", 3600,
  "Cofense Phishing — phishing threat-intelligence feed");

RSSX(leak_abuse_ch_blog, "abuse-ch-blog", "abuse.ch Blog", "abuse.ch Blog", "cyber", "cyber",
  "https://abuse.ch/rss/", "en", "[\"cyber\",\"threat-intel\",\"threat-intel\"]", 3600,
  "abuse.ch Blog — threat-intel threat-intelligence feed");

RSSX(leak_greynoise_blog, "greynoise-blog", "GreyNoise Labs", "GreyNoise Labs", "cyber", "cyber",
  "https://www.greynoise.io/blog/rss.xml", "en", "[\"cyber\",\"scanning\",\"threat-intel\"]", 3600,
  "GreyNoise Labs — scanning threat-intelligence feed");

RSSX(leak_shadowserver, "shadowserver", "Shadowserver Foundation", "Shadowserver Foundation", "cyber", "cyber",
  "https://www.shadowserver.org/feed/", "en", "[\"cyber\",\"threat-intel\",\"threat-intel\"]", 3600,
  "Shadowserver Foundation — threat-intel threat-intelligence feed");

RSSX(leak_censys_blog, "censys-blog", "Censys Blog", "Censys Blog", "cyber", "cyber",
  "https://censys.com/feed/", "en", "[\"cyber\",\"attack-surface\",\"threat-intel\"]", 3600,
  "Censys Blog — attack-surface threat-intelligence feed");

RSSX(leak_bitsight, "bitsight", "Bitsight Research", "Bitsight Research", "cyber", "cyber",
  "https://www.bitsight.com/blog/rss.xml", "en", "[\"cyber\",\"threat-intel\",\"threat-intel\"]", 3600,
  "Bitsight Research — threat-intel threat-intelligence feed");

RSSX(leak_domaintools_blog, "domaintools-blog", "DomainTools Blog", "DomainTools Blog", "cyber", "cyber",
  "https://www.domaintools.com/resources/blog/feed/", "en", "[\"cyber\",\"infrastructure\",\"threat-intel\"]", 3600,
  "DomainTools Blog — infrastructure threat-intelligence feed");

RSSX(leak_riskiq_community, "riskiq-community", "Cisco Talos Blog Feed", "Cisco Talos Blog Feed", "cyber", "cyber",
  "https://blog.talosintelligence.com/rss/", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Cisco Talos Blog Feed — threat-research threat-intelligence feed");

RSSX(leak_malwarebytes_threat, "malwarebytes-threat", "Malwarebytes Threat Intel", "Malwarebytes Threat Intel", "cyber", "cyber",
  "https://www.malwarebytes.com/blog/threat-intelligence/feed/index.xml", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Malwarebytes Threat Intel — threat-research threat-intelligence feed");

RSSX(leak_kaspersky_ics, "kaspersky-ics", "Kaspersky ICS-CERT", "Kaspersky ICS-CERT", "cyber", "cyber",
  "https://ics-cert.kaspersky.com/feed/", "en", "[\"cyber\",\"ics\",\"threat-intel\"]", 3600,
  "Kaspersky ICS-CERT — ics threat-intelligence feed");

RSSX(leak_dragos_blog, "dragos-blog", "Dragos ICS/OT Blog", "Dragos ICS/OT Blog", "cyber", "cyber",
  "https://www.dragos.com/blog/feed/", "en", "[\"cyber\",\"ics\",\"threat-intel\"]", 3600,
  "Dragos ICS/OT Blog — ics threat-intelligence feed");

RSSX(leak_nozomi_labs, "nozomi-labs", "Nozomi Networks Labs", "Nozomi Networks Labs", "cyber", "cyber",
  "https://www.nozominetworks.com/blog/feed", "en", "[\"cyber\",\"ics\",\"threat-intel\"]", 3600,
  "Nozomi Networks Labs — ics threat-intelligence feed");

RSSX(leak_claroty_team82, "claroty-team82", "Claroty Team82", "Claroty Team82", "cyber", "cyber",
  "https://claroty.com/team82/feed", "en", "[\"cyber\",\"ics\",\"threat-intel\"]", 3600,
  "Claroty Team82 — ics threat-intelligence feed");

RSSX(leak_ransomware_live, "ransomware-live", "Ransomware.live Recent", "Ransomware.live Recent", "cyber", "cyber",
  "https://www.ransomware.live/rss.xml", "en", "[\"cyber\",\"ransomware\",\"threat-intel\"]", 3600,
  "Ransomware.live Recent — ransomware threat-intelligence feed");

RSSX(leak_darkfeed, "darkfeed", "DarkFeed Ransomware", "DarkFeed Ransomware", "cyber", "cyber",
  "https://darkfeed.io/feed/", "en", "[\"cyber\",\"ransomware\",\"threat-intel\"]", 3600,
  "DarkFeed Ransomware — ransomware threat-intelligence feed");

RSSX(leak_cyberscoop, "cyberscoop", "CyberScoop", "CyberScoop", "cyber", "cyber",
  "https://cyberscoop.com/feed/", "en", "[\"cyber\",\"news\",\"threat-intel\"]", 3600,
  "CyberScoop — news threat-intelligence feed");

RSSX(leak_therecord_ransom, "therecord-ransom", "The Record Ransomware", "The Record Ransomware", "cyber", "cyber",
  "https://therecord.media/tag/ransomware/feed/", "en", "[\"cyber\",\"ransomware\",\"threat-intel\"]", 3600,
  "The Record Ransomware — ransomware threat-intelligence feed");

RSSX(leak_infostealers, "infostealers", "InfoStealers.com", "InfoStealers.com", "cyber", "cyber",
  "https://www.infostealers.com/feed/", "en", "[\"cyber\",\"infostealer\",\"threat-intel\"]", 3600,
  "InfoStealers.com — infostealer threat-intelligence feed");

RSSX(leak_hudsonrock, "hudsonrock", "Hudson Rock Blog", "Hudson Rock Blog", "cyber", "cyber",
  "https://www.hudsonrock.com/blog/rss.xml", "en", "[\"cyber\",\"infostealer\",\"threat-intel\"]", 3600,
  "Hudson Rock Blog — infostealer threat-intelligence feed");

RSSX(leak_cl0udhax, "cl0udhax", "Rapid7 Emergent Threats", "Rapid7 Emergent Threats", "cyber", "cyber",
  "https://www.rapid7.com/blog/rss/", "en", "[\"cyber\",\"threat-research\",\"threat-intel\"]", 3600,
  "Rapid7 Emergent Threats — threat-research threat-intelligence feed");

RSSX(leak_virusbulletin, "virusbulletin", "Virus Bulletin", "Virus Bulletin", "cyber", "cyber",
  "https://www.virusbulletin.com/rss", "en", "[\"cyber\",\"malware\",\"threat-intel\"]", 3600,
  "Virus Bulletin — malware threat-intelligence feed");
