/* GitHub .atom watch feeds (deterministic) for high-penetrancy OSINT/threat-intel tooling, detections, IOCs and PoCs — new commits/releases as intel rows. */
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

RSSX(gh_projectdiscovery_nuclei_templates, "gh-projectdiscovery-nuclei-templates-commits", "GitHub: Nuclei Templates", "GitHub: Nuclei Templates", "cyber", "cyber",
  "https://github.com/projectdiscovery/nuclei-templates/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Nuclei Templates (projectdiscovery/nuclei-templates) commits — new vulnerability detection templates");

RSSX(gh_sigmahq_sigma, "gh-sigmahq-sigma-commits", "GitHub: Sigma Rules", "GitHub: Sigma Rules", "cyber", "cyber",
  "https://github.com/SigmaHQ/sigma/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Sigma Rules (SigmaHQ/sigma) commits — new SIEM detection rules");

RSSX(gh_elastic_detection_rules, "gh-elastic-detection-rules-commits", "GitHub: Elastic Detection Rules", "GitHub: Elastic Detection Rules", "cyber", "cyber",
  "https://github.com/elastic/detection-rules/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Elastic Detection Rules (elastic/detection-rules) commits — new EDR detection logic");

RSSX(gh_neo23x0_signature_base, "gh-neo23x0-signature-base-commits", "GitHub: Florian Roth signature-base", "GitHub: Florian Roth signature-base", "cyber", "cyber",
  "https://github.com/Neo23x0/signature-base/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Florian Roth signature-base (Neo23x0/signature-base) commits — new YARA/IOC signatures");

RSSX(gh_reversinglabs_reversinglabs_yara_rules, "gh-reversinglabs-reversinglabs-yara-rules-commits", "GitHub: ReversingLabs YARA", "GitHub: ReversingLabs YARA", "cyber", "cyber",
  "https://github.com/reversinglabs/reversinglabs-yara-rules/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "ReversingLabs YARA (reversinglabs/reversinglabs-yara-rules) commits — malware YARA rules");

RSSX(gh_redcanaryco_atomic_red_team, "gh-redcanaryco-atomic-red-team-commits", "GitHub: Atomic Red Team", "GitHub: Atomic Red Team", "cyber", "cyber",
  "https://github.com/redcanaryco/atomic-red-team/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Atomic Red Team (redcanaryco/atomic-red-team) commits — new ATT&CK test procedures");

RSSX(gh_mitre_cti, "gh-mitre-cti-commits", "GitHub: MITRE ATT&CK CTI", "GitHub: MITRE ATT&CK CTI", "cyber", "cyber",
  "https://github.com/mitre/cti/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "MITRE ATT&CK CTI (mitre/cti) commits — ATT&CK knowledge-base updates");

RSSX(gh_center_for_threat_informed_defense_attack_flow, "gh-center-for-threat-informed-defense-attack-flow-commits", "GitHub: MITRE Attack Flow", "GitHub: MITRE Attack Flow", "cyber", "cyber",
  "https://github.com/center-for-threat-informed-defense/attack-flow/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "MITRE Attack Flow (center-for-threat-informed-defense/attack-flow) commits — attack-flow updates");

RSSX(gh_nomi_sec_poc_in_github, "gh-nomi-sec-poc-in-github-commits", "GitHub: PoC-in-GitHub", "GitHub: PoC-in-GitHub", "cyber", "cyber",
  "https://github.com/nomi-sec/PoC-in-GitHub/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "PoC-in-GitHub (nomi-sec/PoC-in-GitHub) commits — aggregated CVE proof-of-concept exploits");

RSSX(gh_cveproject_cvelistv5, "gh-cveproject-cvelistv5-commits", "GitHub: CVE List v5", "GitHub: CVE List v5", "cyber", "cyber",
  "https://github.com/CVEProject/cvelistV5/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "CVE List v5 (CVEProject/cvelistV5) commits — official CVE record additions");

RSSX(gh_github_advisory_database, "gh-github-advisory-database-commits", "GitHub: GitHub Advisory DB", "GitHub: GitHub Advisory DB", "cyber", "cyber",
  "https://github.com/github/advisory-database/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "GitHub Advisory DB (github/advisory-database) commits — GHSA security advisories");

RSSX(gh_ossf_malicious_packages, "gh-ossf-malicious-packages-commits", "GitHub: OpenSSF Malicious Packages", "GitHub: OpenSSF Malicious Packages", "cyber", "cyber",
  "https://github.com/ossf/malicious-packages/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "OpenSSF Malicious Packages (ossf/malicious-packages) commits — malicious OSS package reports");

RSSX(gh_stamparm_maltrail, "gh-stamparm-maltrail-commits", "GitHub: Maltrail", "GitHub: Maltrail", "cyber", "cyber",
  "https://github.com/stamparm/maltrail/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Maltrail (stamparm/maltrail) commits — malicious traffic indicator updates");

RSSX(gh_firehol_blocklist_ipsets, "gh-firehol-blocklist-ipsets-commits", "GitHub: FireHOL Blocklists", "GitHub: FireHOL Blocklists", "cyber", "cyber",
  "https://github.com/firehol/blocklist-ipsets/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "FireHOL Blocklists (firehol/blocklist-ipsets) commits — aggregated malicious IP blocklists");

RSSX(gh_stamparm_ipsum, "gh-stamparm-ipsum-commits", "GitHub: IPsum Threat List", "GitHub: IPsum Threat List", "cyber", "cyber",
  "https://github.com/stamparm/ipsum/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "IPsum Threat List (stamparm/ipsum) commits — aggregated suspicious IP list");

RSSX(gh_pan_unit42_iocs, "gh-pan-unit42-iocs-commits", "GitHub: Unit 42 IOCs", "GitHub: Unit 42 IOCs", "cyber", "cyber",
  "https://github.com/pan-unit42/iocs/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Unit 42 IOCs (pan-unit42/iocs) commits — Palo Alto Unit 42 indicators of compromise");

RSSX(gh_eset_malware_ioc, "gh-eset-malware-ioc-commits", "GitHub: ESET Malware IOC", "GitHub: ESET Malware IOC", "cyber", "cyber",
  "https://github.com/eset/malware-ioc/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "ESET Malware IOC (eset/malware-ioc) commits — ESET research indicators");

RSSX(gh_blackorbird_apt_report, "gh-blackorbird-apt-report-commits", "GitHub: APT_REPORT", "GitHub: APT_REPORT", "cyber", "cyber",
  "https://github.com/blackorbird/APT_REPORT/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "APT_REPORT (blackorbird/APT_REPORT) commits — collected APT threat reports");

RSSX(gh_kbandla_aptnotes, "gh-kbandla-aptnotes-commits", "GitHub: APTnotes", "GitHub: APTnotes", "cyber", "cyber",
  "https://github.com/kbandla/APTnotes/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "APTnotes (kbandla/APTnotes) commits — public APT report archive");

RSSX(gh_misp_misp_warninglists, "gh-misp-misp-warninglists-commits", "GitHub: MISP Warning Lists", "GitHub: MISP Warning Lists", "cyber", "cyber",
  "https://github.com/misp/misp-warninglists/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "MISP Warning Lists (misp/misp-warninglists) commits — false-positive/known-good indicator lists");

RSSX(gh_misp_misp, "gh-misp-misp-releases", "GitHub: MISP Platform", "GitHub: MISP Platform", "cyber", "cyber",
  "https://github.com/MISP/MISP/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "MISP Platform (MISP/MISP) releases — threat-intel sharing platform releases");

RSSX(gh_otrf_threathunter_playbook, "gh-otrf-threathunter-playbook-commits", "GitHub: Threat Hunter Playbook", "GitHub: Threat Hunter Playbook", "cyber", "cyber",
  "https://github.com/OTRF/ThreatHunter-Playbook/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Threat Hunter Playbook (OTRF/ThreatHunter-Playbook) commits — detection analytics updates");

RSSX(gh_cyb3rward0g_helk, "gh-cyb3rward0g-helk-commits", "GitHub: HELK", "GitHub: HELK", "cyber", "cyber",
  "https://github.com/Cyb3rWard0g/HELK/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "HELK (Cyb3rWard0g/HELK) commits — hunting ELK stack updates");

RSSX(gh_yara_rules_rules, "gh-yara-rules-rules-commits", "GitHub: Yara-Rules Community", "GitHub: Yara-Rules Community", "cyber", "cyber",
  "https://github.com/Yara-Rules/rules/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Yara-Rules Community (Yara-Rules/rules) commits — community YARA rule updates");

RSSX(gh_volatilityfoundation_volatility3, "gh-volatilityfoundation-volatility3-releases", "GitHub: Volatility 3", "GitHub: Volatility 3", "cyber", "cyber",
  "https://github.com/volatilityfoundation/volatility3/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Volatility 3 (volatilityfoundation/volatility3) releases — memory forensics releases");

RSSX(gh_sleuthkit_autopsy, "gh-sleuthkit-autopsy-releases", "GitHub: Autopsy", "GitHub: Autopsy", "cyber", "cyber",
  "https://github.com/sleuthkit/autopsy/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Autopsy (sleuthkit/autopsy) releases — digital forensics platform releases");

RSSX(gh_virustotal_yara, "gh-virustotal-yara-releases", "GitHub: YARA Engine", "GitHub: YARA Engine", "cyber", "cyber",
  "https://github.com/VirusTotal/yara/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "YARA Engine (VirusTotal/yara) releases — YARA engine releases");

RSSX(gh_gchq_cyberchef, "gh-gchq-cyberchef-releases", "GitHub: CyberChef", "GitHub: CyberChef", "cyber", "cyber",
  "https://github.com/gchq/CyberChef/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "CyberChef (gchq/CyberChef) releases — data analysis toolkit releases");

RSSX(gh_projectdiscovery_nuclei, "gh-projectdiscovery-nuclei-releases", "GitHub: Nuclei", "GitHub: Nuclei", "cyber", "cyber",
  "https://github.com/projectdiscovery/nuclei/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Nuclei (projectdiscovery/nuclei) releases — vulnerability scanner releases");

RSSX(gh_projectdiscovery_subfinder, "gh-projectdiscovery-subfinder-releases", "GitHub: Subfinder", "GitHub: Subfinder", "cyber", "cyber",
  "https://github.com/projectdiscovery/subfinder/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Subfinder (projectdiscovery/subfinder) releases — subdomain enumeration releases");

RSSX(gh_projectdiscovery_httpx, "gh-projectdiscovery-httpx-releases", "GitHub: httpx", "GitHub: httpx", "cyber", "cyber",
  "https://github.com/projectdiscovery/httpx/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "httpx (projectdiscovery/httpx) releases — HTTP toolkit releases");

RSSX(gh_projectdiscovery_katana, "gh-projectdiscovery-katana-releases", "GitHub: Katana", "GitHub: Katana", "cyber", "cyber",
  "https://github.com/projectdiscovery/katana/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Katana (projectdiscovery/katana) releases — web crawler releases");

RSSX(gh_projectdiscovery_naabu, "gh-projectdiscovery-naabu-releases", "GitHub: Naabu", "GitHub: Naabu", "cyber", "cyber",
  "https://github.com/projectdiscovery/naabu/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Naabu (projectdiscovery/naabu) releases — port scanner releases");

RSSX(gh_owasp_amass_amass, "gh-owasp-amass-amass-releases", "GitHub: OWASP Amass", "GitHub: OWASP Amass", "cyber", "cyber",
  "https://github.com/owasp-amass/amass/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "OWASP Amass (owasp-amass/amass) releases — attack-surface mapping releases");

RSSX(gh_smicallef_spiderfoot, "gh-smicallef-spiderfoot-releases", "GitHub: SpiderFoot", "GitHub: SpiderFoot", "cyber", "cyber",
  "https://github.com/smicallef/spiderfoot/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "SpiderFoot (smicallef/spiderfoot) releases — OSINT automation releases");

RSSX(gh_laramies_theharvester, "gh-laramies-theharvester-releases", "GitHub: theHarvester", "GitHub: theHarvester", "cyber", "cyber",
  "https://github.com/laramies/theHarvester/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "theHarvester (laramies/theHarvester) releases — email/subdomain recon releases");

RSSX(gh_sherlock_project_sherlock, "gh-sherlock-project-sherlock-releases", "GitHub: Sherlock", "GitHub: Sherlock", "cyber", "cyber",
  "https://github.com/sherlock-project/sherlock/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Sherlock (sherlock-project/sherlock) releases — username hunting releases");

RSSX(gh_soxoj_maigret, "gh-soxoj-maigret-releases", "GitHub: Maigret", "GitHub: Maigret", "cyber", "cyber",
  "https://github.com/soxoj/maigret/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Maigret (soxoj/maigret) releases — username OSINT releases");

RSSX(gh_megadose_holehe, "gh-megadose-holehe-releases", "GitHub: Holehe", "GitHub: Holehe", "cyber", "cyber",
  "https://github.com/megadose/holehe/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Holehe (megadose/holehe) releases — email account checker releases");

RSSX(gh_sundowndev_phoneinfoga, "gh-sundowndev-phoneinfoga-releases", "GitHub: PhoneInfoga", "GitHub: PhoneInfoga", "cyber", "cyber",
  "https://github.com/sundowndev/phoneinfoga/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "PhoneInfoga (sundowndev/phoneinfoga) releases — phone-number OSINT releases");

RSSX(gh_swisskyrepo_payloadsallthethings, "gh-swisskyrepo-payloadsallthethings-commits", "GitHub: PayloadsAllTheThings", "GitHub: PayloadsAllTheThings", "cyber", "cyber",
  "https://github.com/swisskyrepo/PayloadsAllTheThings/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "PayloadsAllTheThings (swisskyrepo/PayloadsAllTheThings) commits — offensive payload updates");

RSSX(gh_danielmiessler_seclists, "gh-danielmiessler-seclists-commits", "GitHub: SecLists", "GitHub: SecLists", "cyber", "cyber",
  "https://github.com/danielmiessler/SecLists/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "SecLists (danielmiessler/SecLists) commits — security testing wordlist updates");

RSSX(gh_carlospolop_peass_ng, "gh-carlospolop-peass-ng-releases", "GitHub: PEASS-ng", "GitHub: PEASS-ng", "cyber", "cyber",
  "https://github.com/carlospolop/PEASS-ng/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "PEASS-ng (carlospolop/PEASS-ng) releases — privilege-escalation suite releases");

RSSX(gh_bloodhoundad_bloodhound, "gh-bloodhoundad-bloodhound-releases", "GitHub: BloodHound", "GitHub: BloodHound", "cyber", "cyber",
  "https://github.com/BloodHoundAD/BloodHound/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "BloodHound (BloodHoundAD/BloodHound) releases — AD attack-path releases");

RSSX(gh_rapid7_metasploit_framework, "gh-rapid7-metasploit-framework-commits", "GitHub: Metasploit Framework", "GitHub: Metasploit Framework", "cyber", "cyber",
  "https://github.com/rapid7/metasploit-framework/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Metasploit Framework (rapid7/metasploit-framework) commits — exploit framework updates");

RSSX(gh_vulhub_vulhub, "gh-vulhub-vulhub-commits", "GitHub: Vulhub", "GitHub: Vulhub", "cyber", "cyber",
  "https://github.com/vulhub/vulhub/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Vulhub (vulhub/vulhub) commits — vulnerable environment additions");

RSSX(gh_the_art_of_hacking_h4cker, "gh-the-art-of-hacking-h4cker-commits", "GitHub: h4cker Resources", "GitHub: h4cker Resources", "cyber", "cyber",
  "https://github.com/The-Art-of-Hacking/h4cker/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "h4cker Resources (The-Art-of-Hacking/h4cker) commits — security resource updates");

RSSX(gh_trimstray_the_book_of_secret_knowledge, "gh-trimstray-the-book-of-secret-knowledge-commits", "GitHub: Book of Secret Knowledge", "GitHub: Book of Secret Knowledge", "cyber", "cyber",
  "https://github.com/trimstray/the-book-of-secret-knowledge/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Book of Secret Knowledge (trimstray/the-book-of-secret-knowledge) commits — tooling/resource updates");

RSSX(gh_jivoi_awesome_osint, "gh-jivoi-awesome-osint-commits", "GitHub: Awesome OSINT", "GitHub: Awesome OSINT", "cyber", "cyber",
  "https://github.com/jivoi/awesome-osint/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Awesome OSINT (jivoi/awesome-osint) commits — curated OSINT resource updates");

RSSX(gh_cipher387_osint_stuff_tool_collection, "gh-cipher387-osint-stuff-tool-collection-commits", "GitHub: OSINT Tool Collection", "GitHub: OSINT Tool Collection", "cyber", "cyber",
  "https://github.com/cipher387/osint_stuff_tool_collection/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "OSINT Tool Collection (cipher387/osint_stuff_tool_collection) commits — OSINT tool additions");

RSSX(gh_arpsyndicate_awesome_intelligence, "gh-arpsyndicate-awesome-intelligence-commits", "GitHub: Awesome Intelligence", "GitHub: Awesome Intelligence", "cyber", "cyber",
  "https://github.com/ARPSyndicate/awesome-intelligence/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Awesome Intelligence (ARPSyndicate/awesome-intelligence) commits — intelligence resource updates");

RSSX(gh_hslatman_awesome_threat_intelligence, "gh-hslatman-awesome-threat-intelligence-commits", "GitHub: Awesome Threat Intelligence", "GitHub: Awesome Threat Intelligence", "cyber", "cyber",
  "https://github.com/hslatman/awesome-threat-intelligence/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Awesome Threat Intelligence (hslatman/awesome-threat-intelligence) commits — threat-intel source updates");

RSSX(gh_fabacab_awesome_cybersecurity_blueteam, "gh-fabacab-awesome-cybersecurity-blueteam-commits", "GitHub: Awesome Blue Team", "GitHub: Awesome Blue Team", "cyber", "cyber",
  "https://github.com/fabacab/awesome-cybersecurity-blueteam/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Awesome Blue Team (fabacab/awesome-cybersecurity-blueteam) commits — defensive tooling updates");

RSSX(gh_infosecn1nja_red_teaming_toolkit, "gh-infosecn1nja-red-teaming-toolkit-commits", "GitHub: Red Teaming Toolkit", "GitHub: Red Teaming Toolkit", "cyber", "cyber",
  "https://github.com/infosecn1nja/Red-Teaming-Toolkit/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "Red Teaming Toolkit (infosecn1nja/Red-Teaming-Toolkit) commits — offensive tooling updates");

RSSX(gh_a_poc_redteam_tools, "gh-a-poc-redteam-tools-commits", "GitHub: RedTeam Tools", "GitHub: RedTeam Tools", "cyber", "cyber",
  "https://github.com/A-poc/RedTeam-Tools/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "RedTeam Tools (A-poc/RedTeam-Tools) commits — red-team tool updates");

RSSX(gh_random_robbie_keywords_to_search_for, "gh-random-robbie-keywords-to-search-for-commits", "GitHub: GitHub Dork Keywords", "GitHub: GitHub Dork Keywords", "cyber", "cyber",
  "https://github.com/random-robbie/keywords-to-search-for/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "GitHub Dork Keywords (random-robbie/keywords-to-search-for) commits — secret-hunting keyword updates");

RSSX(gh_streaak_keyhacks, "gh-streaak-keyhacks-commits", "GitHub: KeyHacks", "GitHub: KeyHacks", "cyber", "cyber",
  "https://github.com/streaak/keyhacks/commits.atom", "en", "[\"cyber\",\"github\",\"commits\",\"tooling\",\"threat-intel\"]", 10800,
  "KeyHacks (streaak/keyhacks) commits — leaked-key validation updates");

RSSX(gh_gitleaks_gitleaks, "gh-gitleaks-gitleaks-releases", "GitHub: Gitleaks", "GitHub: Gitleaks", "cyber", "cyber",
  "https://github.com/gitleaks/gitleaks/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Gitleaks (gitleaks/gitleaks) releases — secret-scanning releases");

RSSX(gh_trufflesecurity_trufflehog, "gh-trufflesecurity-trufflehog-releases", "GitHub: TruffleHog", "GitHub: TruffleHog", "cyber", "cyber",
  "https://github.com/trufflesecurity/trufflehog/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "TruffleHog (trufflesecurity/trufflehog) releases — credential-scanning releases");

RSSX(gh_yeti_platform_yeti, "gh-yeti-platform-yeti-releases", "GitHub: YETI", "GitHub: YETI", "cyber", "cyber",
  "https://github.com/Yeti-Platform/yeti/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "YETI (Yeti-Platform/yeti) releases — threat-intel repository releases");

RSSX(gh_misp_pymisp, "gh-misp-pymisp-releases", "GitHub: PyMISP", "GitHub: PyMISP", "cyber", "cyber",
  "https://github.com/MISP/PyMISP/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "PyMISP (MISP/PyMISP) releases — MISP client releases");

RSSX(gh_certtools_intelmq, "gh-certtools-intelmq-releases", "GitHub: IntelMQ", "GitHub: IntelMQ", "cyber", "cyber",
  "https://github.com/certtools/intelmq/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "IntelMQ (certtools/intelmq) releases — IOC processing pipeline releases");

RSSX(gh_exein_io_pulsar, "gh-exein-io-pulsar-releases", "GitHub: Pulsar", "GitHub: Pulsar", "cyber", "cyber",
  "https://github.com/Exein-io/pulsar/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Pulsar (Exein-io/pulsar) releases — runtime security releases");

RSSX(gh_wazuh_wazuh, "gh-wazuh-wazuh-releases", "GitHub: Wazuh", "GitHub: Wazuh", "cyber", "cyber",
  "https://github.com/wazuh/wazuh/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Wazuh (wazuh/wazuh) releases — XDR/SIEM platform releases");

RSSX(gh_velocidex_velociraptor, "gh-velocidex-velociraptor-releases", "GitHub: Velociraptor", "GitHub: Velociraptor", "cyber", "cyber",
  "https://github.com/Velocidex/velociraptor/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Velociraptor (Velocidex/velociraptor) releases — endpoint DFIR releases");

RSSX(gh_google_grr, "gh-google-grr-releases", "GitHub: GRR Rapid Response", "GitHub: GRR Rapid Response", "cyber", "cyber",
  "https://github.com/google/grr/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "GRR Rapid Response (google/grr) releases — incident-response releases");

RSSX(gh_thehive_project_cortex, "gh-thehive-project-cortex-releases", "GitHub: Cortex", "GitHub: Cortex", "cyber", "cyber",
  "https://github.com/thehive-project/Cortex/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "Cortex (thehive-project/Cortex) releases — observable analysis releases");

RSSX(gh_thehive_project_thehive, "gh-thehive-project-thehive-releases", "GitHub: TheHive", "GitHub: TheHive", "cyber", "cyber",
  "https://github.com/TheHive-Project/TheHive/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "TheHive (TheHive-Project/TheHive) releases — incident-response releases");

RSSX(gh_opencti_platform_opencti, "gh-opencti-platform-opencti-releases", "GitHub: OpenCTI", "GitHub: OpenCTI", "cyber", "cyber",
  "https://github.com/OpenCTI-Platform/opencti/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "OpenCTI (OpenCTI-Platform/opencti) releases — cyber-threat-intel platform releases");

RSSX(gh_ail_project_ail_framework, "gh-ail-project-ail-framework-releases", "GitHub: AIL Framework", "GitHub: AIL Framework", "cyber", "cyber",
  "https://github.com/ail-project/ail-framework/releases.atom", "en", "[\"cyber\",\"github\",\"releases\",\"tooling\",\"threat-intel\"]", 10800,
  "AIL Framework (ail-project/ail-framework) releases — leak/paste analysis releases");
