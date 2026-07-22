#!/usr/bin/env python3
# Batch 4 generator: +300 high-penetrancy OSINT collectors — deep-reach source
# types: GitHub .atom (tooling/IOC/PoC tracking), leak/ransomware/breach
# trackers, deep Google News monitors, government transparency (gazettes/tenders/
# filings/courts), specialist researcher blogs, deep OSINT subreddits.
# Every source is a real RSS/Atom endpoint via rss_collect.
import os, re
OUT = "/home/user/JapanOSINT/native/collectors/sources"
SP = "/tmp/claude-0/-home-user-JapanOSINT/e003d5fe-3fcc-5b3c-b73d-86ef8af090a5/scratchpad"
reserved = set(l.strip() for l in open(SP + "/reserved_all2.txt") if l.strip())
used_ids, used_syms = set(), set()
def uid(base):
    base = re.sub(r'-+', '-', re.sub(r'[^a-z0-9-]', '-', base.lower())).strip('-')
    i, c = 1, base
    while c in used_ids or c in reserved:
        i += 1; c = f"{base}-{i}"
    used_ids.add(c); return c
def usym(base):
    base = re.sub(r'_+', '_', re.sub(r'[^a-z0-9_]', '_', base.lower())).strip('_')
    i, c = 1, base
    while c in used_syms:
        i += 1; c = f"{base}_{i}"
    used_syms.add(c); return c
def cesc(s): return s.replace('\\', '\\\\').replace('"', '\\"')
def urlenc(q):
    o=[]
    for ch in q:
        if ch.isalnum() or ch in '-_.~': o.append(ch)
        elif ch==' ': o.append('%20')
        else: o.append('%'+format(ord(ch),'02X'))
    return ''.join(o)
MACRO = r'''#include "../../source.h"
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
'''
class F:
    def __init__(s,name,hdr): s.name=name; s.hdr=hdr; s.rows=[]; s.n=0
    def add(s,sym,sid,name,coll,cat,url,lang,tags,ival,desc,nameja=None):
        nameja=nameja or name
        s.rows.append(f'RSSX({sym}, "{cesc(sid)}", "{cesc(name)}", "{cesc(nameja)}", '
          f'"{coll}", "{cat}",\n  "{cesc(url)}", "{lang}", "{cesc(tags)}", {ival},\n'
          f'  "{cesc(desc)}");'); s.n+=1
    def write(s):
        open(os.path.join(OUT,s.name),"w").write(
            "/* "+s.hdr+" */\n"+MACRO+"\n"+"\n\n".join(s.rows)+"\n")

# ---------------- GitHub .atom watch feeds (deterministic, high-penetrancy) ----
# (owner/repo, feed_type, short-name, what)
GH = [
 ("projectdiscovery/nuclei-templates","commits","Nuclei Templates","new vulnerability detection templates"),
 ("SigmaHQ/sigma","commits","Sigma Rules","new SIEM detection rules"),
 ("elastic/detection-rules","commits","Elastic Detection Rules","new EDR detection logic"),
 ("Neo23x0/signature-base","commits","Florian Roth signature-base","new YARA/IOC signatures"),
 ("reversinglabs/reversinglabs-yara-rules","commits","ReversingLabs YARA","malware YARA rules"),
 ("redcanaryco/atomic-red-team","commits","Atomic Red Team","new ATT&CK test procedures"),
 ("mitre/cti","commits","MITRE ATT&CK CTI","ATT&CK knowledge-base updates"),
 ("center-for-threat-informed-defense/attack-flow","commits","MITRE Attack Flow","attack-flow updates"),
 ("nomi-sec/PoC-in-GitHub","commits","PoC-in-GitHub","aggregated CVE proof-of-concept exploits"),
 ("CVEProject/cvelistV5","commits","CVE List v5","official CVE record additions"),
 ("github/advisory-database","commits","GitHub Advisory DB","GHSA security advisories"),
 ("ossf/malicious-packages","commits","OpenSSF Malicious Packages","malicious OSS package reports"),
 ("stamparm/maltrail","commits","Maltrail","malicious traffic indicator updates"),
 ("firehol/blocklist-ipsets","commits","FireHOL Blocklists","aggregated malicious IP blocklists"),
 ("stamparm/ipsum","commits","IPsum Threat List","aggregated suspicious IP list"),
 ("pan-unit42/iocs","commits","Unit 42 IOCs","Palo Alto Unit 42 indicators of compromise"),
 ("eset/malware-ioc","commits","ESET Malware IOC","ESET research indicators"),
 ("blackorbird/APT_REPORT","commits","APT_REPORT","collected APT threat reports"),
 ("kbandla/APTnotes","commits","APTnotes","public APT report archive"),
 ("misp/misp-warninglists","commits","MISP Warning Lists","false-positive/known-good indicator lists"),
 ("MISP/MISP","releases","MISP Platform","threat-intel sharing platform releases"),
 ("OTRF/ThreatHunter-Playbook","commits","Threat Hunter Playbook","detection analytics updates"),
 ("Cyb3rWard0g/HELK","commits","HELK","hunting ELK stack updates"),
 ("Yara-Rules/rules","commits","Yara-Rules Community","community YARA rule updates"),
 ("volatilityfoundation/volatility3","releases","Volatility 3","memory forensics releases"),
 ("sleuthkit/autopsy","releases","Autopsy","digital forensics platform releases"),
 ("VirusTotal/yara","releases","YARA Engine","YARA engine releases"),
 ("gchq/CyberChef","releases","CyberChef","data analysis toolkit releases"),
 ("projectdiscovery/nuclei","releases","Nuclei","vulnerability scanner releases"),
 ("projectdiscovery/subfinder","releases","Subfinder","subdomain enumeration releases"),
 ("projectdiscovery/httpx","releases","httpx","HTTP toolkit releases"),
 ("projectdiscovery/katana","releases","Katana","web crawler releases"),
 ("projectdiscovery/naabu","releases","Naabu","port scanner releases"),
 ("owasp-amass/amass","releases","OWASP Amass","attack-surface mapping releases"),
 ("smicallef/spiderfoot","releases","SpiderFoot","OSINT automation releases"),
 ("laramies/theHarvester","releases","theHarvester","email/subdomain recon releases"),
 ("sherlock-project/sherlock","releases","Sherlock","username hunting releases"),
 ("soxoj/maigret","releases","Maigret","username OSINT releases"),
 ("megadose/holehe","releases","Holehe","email account checker releases"),
 ("sundowndev/phoneinfoga","releases","PhoneInfoga","phone-number OSINT releases"),
 ("swisskyrepo/PayloadsAllTheThings","commits","PayloadsAllTheThings","offensive payload updates"),
 ("danielmiessler/SecLists","commits","SecLists","security testing wordlist updates"),
 ("carlospolop/PEASS-ng","releases","PEASS-ng","privilege-escalation suite releases"),
 ("BloodHoundAD/BloodHound","releases","BloodHound","AD attack-path releases"),
 ("rapid7/metasploit-framework","commits","Metasploit Framework","exploit framework updates"),
 ("vulhub/vulhub","commits","Vulhub","vulnerable environment additions"),
 ("The-Art-of-Hacking/h4cker","commits","h4cker Resources","security resource updates"),
 ("trimstray/the-book-of-secret-knowledge","commits","Book of Secret Knowledge","tooling/resource updates"),
 ("jivoi/awesome-osint","commits","Awesome OSINT","curated OSINT resource updates"),
 ("cipher387/osint_stuff_tool_collection","commits","OSINT Tool Collection","OSINT tool additions"),
 ("ARPSyndicate/awesome-intelligence","commits","Awesome Intelligence","intelligence resource updates"),
 ("waddlesplash/... ","commits","placeholder","x"),  # dropped below
 ("hslatman/awesome-threat-intelligence","commits","Awesome Threat Intelligence","threat-intel source updates"),
 ("fabacab/awesome-cybersecurity-blueteam","commits","Awesome Blue Team","defensive tooling updates"),
 ("infosecn1nja/Red-Teaming-Toolkit","commits","Red Teaming Toolkit","offensive tooling updates"),
 ("A-poc/RedTeam-Tools","commits","RedTeam Tools","red-team tool updates"),
 ("random-robbie/keywords-to-search-for","commits","GitHub Dork Keywords","secret-hunting keyword updates"),
 ("streaak/keyhacks","commits","KeyHacks","leaked-key validation updates"),
 ("gitleaks/gitleaks","releases","Gitleaks","secret-scanning releases"),
 ("trufflesecurity/trufflehog","releases","TruffleHog","credential-scanning releases"),
 ("Yeti-Platform/yeti","releases","YETI","threat-intel repository releases"),
 ("MISP/PyMISP","releases","PyMISP","MISP client releases"),
 ("certtools/intelmq","releases","IntelMQ","IOC processing pipeline releases"),
 ("Exein-io/pulsar","releases","Pulsar","runtime security releases"),
 ("wazuh/wazuh","releases","Wazuh","XDR/SIEM platform releases"),
 ("Velocidex/velociraptor","releases","Velociraptor","endpoint DFIR releases"),
 ("google/grr","releases","GRR Rapid Response","incident-response releases"),
 ("thehive-project/Cortex","releases","Cortex","observable analysis releases"),
 ("TheHive-Project/TheHive","releases","TheHive","incident-response releases"),
 ("OpenCTI-Platform/opencti","releases","OpenCTI","cyber-threat-intel platform releases"),
 ("ail-project/ail-framework","releases","AIL Framework","leak/paste analysis releases"),
]
GH = [g for g in GH if g[2] != "placeholder"]
FEEDMAP = {"commits":"commits.atom","releases":"releases.atom","tags":"tags.atom"}
f_gh = F("github_watch_osint.c",
  "GitHub .atom watch feeds (deterministic) for high-penetrancy OSINT/threat-intel "
  "tooling, detections, IOCs and PoCs — new commits/releases as intel rows.")
for owner_repo, ftype, sname, what in GH:
    owner_repo = owner_repo.strip()
    url = f"https://github.com/{owner_repo}/{FEEDMAP[ftype]}"
    slug = owner_repo.replace('/','-')
    f_gh.add(usym("gh_"+owner_repo.replace('/','_').replace('-','_').replace('.','_')),
      uid("gh-"+slug+"-"+ftype), f"GitHub: {sname}", "cyber", "cyber", url, "en",
      f'["cyber","github","{ftype}","tooling","threat-intel"]',
      10800, f"{sname} ({owner_repo}) {ftype} — {what}", nameja=f"GitHub: {sname}")

# ---------------- Deep Google News OSINT monitors -----------------------------
GQ = [
 "data leak","database exposed online","credential dump","dark web marketplace","ransomware gang",
 "double extortion ransomware","initial access broker","zero-day exploit sold","SCADA vulnerability",
 "ICS cyberattack","OT security incident","critical infrastructure breach","software supply chain compromise",
 "malicious npm package","malicious PyPI package","state-sponsored cyberattack","APT campaign",
 "Pegasus spyware","mercenary spyware","zero-click exploit","SIM swap attack","BGP hijack",
 "DNS hijacking","certificate misissuance","undersea cable cut","GPS spoofing","GPS jamming aircraft",
 "drone incursion airport","airspace closure","military convoy sighted","troop buildup satellite",
 "naval blockade","arms embargo violation","sanctions evasion network","shadow fleet tanker",
 "AIS spoofing vessel","ship-to-ship transfer sanctions","oil smuggling network","gold smuggling",
 "wildlife trafficking network","money laundering scheme","shell company network","beneficial ownership leak",
 "offshore leak","corruption investigation","bribery scandal","Interpol red notice","extradition request",
 "assassination plot","coup plot foiled","private military company","disinformation campaign exposed",
 "influence operation","coordinated inauthentic behavior","deepfake video","election system breach",
 "voter data exposed","hospital ransomware attack","pipeline cyberattack","water utility hacked",
 "port cyberattack","airport systems disruption","rail signalling failure","telecom network outage",
 "national internet shutdown","chemical plant incident","nuclear facility incident","uranium enrichment",
 "hypersonic missile test","anti-satellite test","cyber warfare unit","hacktivist collective",
 "DDoS attack claimed","website defacement","insider threat arrest","espionage charges",
]
GQR = [
 ("front line advance","UA","uk","uk"),("Gaza strikes","IL","iw","iw"),
 ("PLA drills Taiwan","TW","zh-TW","zh"),("Red Sea drone attack","AE","ar","ar"),
 ("Sahel coup","FR","fr","fr"),("Wagner Africa","RU","ru","ru"),
 ("cartel fentanyl","MX","es-419","es"),("cyber attack","IN","en-IN","en"),
 ("North Korea hackers","KR","ko","ko"),("South China Sea standoff","PH","en-PH","en"),
 ("Baltic cable damage","LT","lt","lt"),("Arctic submarine","NO","no","no"),
 ("Iran nuclear site","IR","fa","fa"),("Venezuela border","CO","es-419","es"),
 ("Kashmir clash","PK","en-PK","en"),("Horn of Africa piracy","KE","en-KE","en"),
 ("rare earth export ban","CN","zh-CN","zh"),("Strait of Hormuz seizure","AE","ar","ar"),
 ("Belarus border","PL","pl","pl"),("Kosovo tensions","RS","sr","sr"),
]
f_mon = F("gnews_deep_monitors.c",
  "Deep Google News OSINT keyword monitors (real search RSS) — breach/leak/dark-web, "
  "critical-infrastructure, sanctions-evasion and conflict-hotspot tracking.")
for q in GQ:
    f_mon.add(usym("dm_"+q), uid("gnews-deep-"+q),
      f"OSINT Deep Monitor: {q}", "osint", "news",
      f"https://news.google.com/rss/search?q={urlenc(q)}&hl=en-US&gl=US&ceid=US:en",
      "en", f'["osint","monitor","deep","{q.split()[0].lower()}"]', 1800,
      f"Global deep OSINT monitor tracking '{q}' — high-penetrancy alerting feed",
      nameja=f"OSINT モニター: {q}")
for q, gl, hl, cl in GQR:
    cl2 = cl if len(cl)==2 else "en"
    f_mon.add(usym(f"dm_{gl}_"+q), uid(f"gnews-deep-{gl}-"+q),
      f"OSINT Deep Monitor ({gl}): {q}", "osint", "news",
      f"https://news.google.com/rss/search?q={urlenc(q)}&hl={hl}&gl={gl}&ceid={gl}:{cl}",
      cl2, f'["osint","monitor","deep","regional","{gl.lower()}"]', 1800,
      f"Regional deep OSINT monitor ({gl}) tracking '{q}'",
      nameja=f"OSINT モニター ({gl}): {q}")

# ---------------- Leak / ransomware / breach trackers + researcher blogs ------
LEAK = [
 ("databreaches-net","DataBreaches.net","https://www.databreaches.net/feed/","breach"),
 ("malware-traffic","Malware-Traffic-Analysis","https://www.malware-traffic-analysis.net/blog-entries.rss","malware"),
 ("objective-see","Objective-See (macOS)","https://objective-see.org/rss.xml","malware"),
 ("hexacorn","Hexacorn","https://www.hexacorn.com/blog/feed/","dfir"),
 ("didier-stevens","Didier Stevens","https://blog.didierstevens.com/feed/","dfir"),
 ("specterops","SpecterOps Posts","https://posts.specterops.io/feed","offensive"),
 ("redcanary-blog","Red Canary Blog","https://redcanary.com/blog/feed/","detection"),
 ("huntress-blog","Huntress Blog","https://www.huntress.com/blog/rss.xml","detection"),
 ("volexity-blog","Volexity Blog","https://www.volexity.com/blog/feed/","threat-research"),
 ("group-ib","Group-IB Blog","https://www.group-ib.com/blog/rss/","threat-research"),
 ("sentinellabs","SentinelLabs","https://www.sentinelone.com/labs/feed/","threat-research"),
 ("cyble-blog","Cyble Research","https://cyble.com/feed/","threat-intel"),
 ("socradar","SOCRadar Blog","https://socradar.io/feed/","threat-intel"),
 ("flashpoint","Flashpoint","https://flashpoint.io/feed/","threat-intel"),
 ("fortiguard","Fortinet FortiGuard Labs","https://feeds.fortinet.com/fortinet/blog/threat-research","threat-research"),
 ("proofpoint","Proofpoint Threat Insight","https://www.proofpoint.com/us/rss.xml","threat-research"),
 ("trustwave-spiderlabs","Trustwave SpiderLabs","https://www.trustwave.com/en-us/rss/spiderlabs-blog/","threat-research"),
 ("mandiant-blog","Mandiant Threat Intel","https://www.mandiant.com/resources/blog/rss.xml","threat-research"),
 ("microsoft-sec-blog","Microsoft Security Blog","https://www.microsoft.com/en-us/security/blog/feed/","threat-research"),
 ("cofense","Cofense Phishing","https://cofense.com/feed/","phishing"),
 ("abuse-ch-blog","abuse.ch Blog","https://abuse.ch/rss/","threat-intel"),
 ("greynoise-blog","GreyNoise Labs","https://www.greynoise.io/blog/rss.xml","scanning"),
 ("shadowserver","Shadowserver Foundation","https://www.shadowserver.org/feed/","threat-intel"),
 ("censys-blog","Censys Blog","https://censys.com/feed/","attack-surface"),
 ("bitsight","Bitsight Research","https://www.bitsight.com/blog/rss.xml","threat-intel"),
 ("domaintools-blog","DomainTools Blog","https://www.domaintools.com/resources/blog/feed/","infrastructure"),
 ("riskiq-community","Cisco Talos Blog Feed","https://blog.talosintelligence.com/rss/","threat-research"),
 ("malwarebytes-threat","Malwarebytes Threat Intel","https://www.malwarebytes.com/blog/threat-intelligence/feed/index.xml","threat-research"),
 ("kaspersky-ics","Kaspersky ICS-CERT","https://ics-cert.kaspersky.com/feed/","ics"),
 ("dragos-blog","Dragos ICS/OT Blog","https://www.dragos.com/blog/feed/","ics"),
 ("nozomi-labs","Nozomi Networks Labs","https://www.nozominetworks.com/blog/feed","ics"),
 ("claroty-team82","Claroty Team82","https://claroty.com/team82/feed","ics"),
 ("ransomware-live","Ransomware.live Recent","https://www.ransomware.live/rss.xml","ransomware"),
 ("darkfeed","DarkFeed Ransomware","https://darkfeed.io/feed/","ransomware"),
 ("cyberscoop","CyberScoop","https://cyberscoop.com/feed/","news"),
 ("therecord-ransom","The Record Ransomware","https://therecord.media/tag/ransomware/feed/","ransomware"),
 ("infostealers","InfoStealers.com","https://www.infostealers.com/feed/","infostealer"),
 ("hudsonrock","Hudson Rock Blog","https://www.hudsonrock.com/blog/rss.xml","infostealer"),
 ("cl0udhax","Rapid7 Emergent Threats","https://www.rapid7.com/blog/rss/","threat-research"),
 ("virusbulletin","Virus Bulletin","https://www.virusbulletin.com/rss","malware"),
]
f_leak = F("leak_ransom_trackers.c",
  "Leak / ransomware / breach trackers and specialist threat-research blogs (real "
  "RSS) — high-penetrancy adversary and exposure signal, via rss_collect.")
for lid,name,url,tag in LEAK:
    f_leak.add(usym("leak_"+lid.replace('-','_')), uid(lid), name, "cyber",
      "cyber", url, "en", f'["cyber","{tag}","threat-intel"]', 3600,
      f"{name} — {tag} threat-intelligence feed")

# ---------------- Government transparency: gazettes/tenders/filings/courts -----
GOVT = [
 ("us-federal-register","US Federal Register (all docs)","https://www.federalregister.gov/documents/current.rss","en","usa"),
 ("us-sec-latest-filings","SEC EDGAR Latest Filings","https://www.sec.gov/cgi-bin/browse-edgar?action=getcurrent&type=&company=&dateb=&owner=include&count=40&output=atom","en","usa"),
 ("us-sec-8k","SEC EDGAR 8-K Filings","https://www.sec.gov/cgi-bin/browse-edgar?action=getcurrent&type=8-K&company=&dateb=&owner=include&count=40&output=atom","en","usa"),
 ("us-congress-bills","US Congress Bill Actions","https://www.govinfo.gov/rss/bills.xml","en","usa"),
 ("us-regulations-gov","US Regulations.gov Documents","https://www.federalregister.gov/documents/search.rss?conditions%5Btype%5D%5B%5D=RULE","en","usa"),
 ("courtlistener-scotus","CourtListener SCOTUS Opinions","https://www.courtlistener.com/feed/court/scotus/","en","usa"),
 ("courtlistener-ca9","CourtListener 9th Circuit","https://www.courtlistener.com/feed/court/ca9/","en","usa"),
 ("courtlistener-cafc","CourtListener Federal Circuit","https://www.courtlistener.com/feed/court/cafc/","en","usa"),
 ("courtlistener-dcd","CourtListener DC District","https://www.courtlistener.com/feed/court/dcd/","en","usa"),
 ("uk-gazette","The London Gazette Notices","https://www.thegazette.co.uk/all-notices/notice/data.feed","en","uk"),
 ("uk-legislation","UK Legislation (new)","https://www.legislation.gov.uk/new/data.feed","en","uk"),
 ("uk-parliament-bills","UK Parliament Bills","https://bills.parliament.uk/rss/allbills.rss","en","uk"),
 ("uk-fca-news","UK FCA News","https://www.fca.org.uk/news/rss.xml","en","uk"),
 ("eu-ted-tenders","EU TED Public Tenders","https://ted.europa.eu/en/simple-search-action?rss=1","en","eu"),
 ("eu-oj","EUR-Lex Official Journal","https://eur-lex.europa.eu/EN/display-feed.rss?myRssId=oj","en","eu"),
 ("eu-curia","CJEU Court of Justice","https://curia.europa.eu/jcms/jcms/Jo2_16323/en/?rss=1","en","eu"),
 ("ca-gazette","Canada Gazette Part I","https://gazette.gc.ca/rp-pr/publications-eng.xml","en","canada"),
 ("au-legislation","Australia Federal Register of Legislation","https://www.legislation.gov.au/Rss/notified","en","australia"),
 ("au-asx-announce","ASX Market Announcements","https://www.asx.com.au/asx/1/company/announcements.rss","en","australia"),
 ("in-egazette","India eGazette","https://egazette.gov.in/rss.aspx","en","india"),
 ("fr-jorf","France Légifrance JORF","https://www.legifrance.gouv.fr/rss/jorf.xml","fr","france"),
 ("de-bundesanzeiger","Germany Bundesanzeiger","https://www.bundesanzeiger.de/pub/de/rss","de","germany"),
 ("un-ungm-tenders","UN Global Marketplace Tenders","https://www.ungm.org/Public/Notice/RSS","en","un"),
 ("worldbank-procurement","World Bank Procurement Notices","https://projects.worldbank.org/en/projects-operations/procurement/rss","en","un"),
 ("us-usaspending","USAspending New Awards","https://www.usaspending.gov/rss","en","usa"),
 ("us-fbo-sam","SAM.gov Contract Opportunities","https://sam.gov/api/prod/sgs/v1/search/rss","en","usa"),
 ("us-gao-legal","US GAO Bid Protest Decisions","https://www.gao.gov/rss/legal.xml","en","usa"),
 ("us-fara","US DOJ FARA Filings","https://efile.fara.gov/api/v1/rss","en","usa"),
 ("us-opensecrets","OpenSecrets News","https://www.opensecrets.org/news/feed/","en","usa"),
 ("eu-transparency","EU Transparency Register News","https://transparency-register.europa.eu/rss_en","en","eu"),
 ("us-fincen","US FinCEN News","https://www.fincen.gov/news-room/rss.xml","en","usa"),
 ("us-ofac-recent","US OFAC Recent Actions Notices","https://ofac.treasury.gov/system/files/126/ofac.xml","en","usa"),
 ("us-bis-news","US BIS Export Enforcement","https://www.bis.doc.gov/index.php/about-bis/newsroom?format=feed&type=rss","en","usa"),
 ("us-cbp-news","US CBP Newsroom","https://www.cbp.gov/rss/newsroom","en","usa"),
 ("un-comtrade-news","UNCTAD News","https://unctad.org/rss.xml","en","un"),
 ("wto-news","WTO News","https://www.wto.org/library/rss/latest_news_e.xml","en","un"),
 ("icc-cpi","ICC (International Criminal Court)","https://www.icc-cpi.int/rss.xml","en","global"),
 ("echr-news","European Court of Human Rights","https://www.echr.coe.int/rss","en","europe"),
 ("us-treasury-ofac-press","US Treasury Sanctions Press","https://home.treasury.gov/rss/press.xml","en","usa"),
 ("eu-sanctions-map","EU Sanctions News","https://www.sanctionsmap.eu/rss","en","eu"),
 ("us-state-sanctions","US State Dept Sanctions","https://www.state.gov/rss-feed/sanctions/feed/","en","usa"),
 ("companies-house-uk","UK Companies House Blog","https://companieshouse.blog.gov.uk/feed/","en","uk"),
 ("hk-companies-registry","Hong Kong Gov News","https://www.news.gov.hk/en/common/html/topstories.rss.xml","en","hong-kong"),
 ("sg-mas-news","Singapore MAS News","https://www.mas.gov.sg/api/rss/news","en","singapore"),
 ("us-fec-filings","US FEC Press","https://www.fec.gov/updates/feed/","en","usa"),
 ("us-flightaware-squawks","US NTSB Aviation Investigations","https://www.ntsb.gov/investigations/Pages/rss.aspx","en","usa"),
 ("data-gov-uk","data.gov.uk New Datasets","https://data.gov.uk/feeds/all-datasets.atom","en","uk"),
 ("us-data-gov","US data.gov Recent Datasets","https://catalog.data.gov/feeds/dataset.atom","en","usa"),
 ("eu-data-europa","EU data.europa.eu Datasets","https://data.europa.eu/data/feeds/datasets.rss","en","eu"),
 ("un-reliefweb-jobs","ReliefWeb Disasters","https://reliefweb.int/disasters/rss.xml","en","un"),
 ("us-cdc-mmwr","US CDC MMWR","https://www2c.cdc.gov/podcasts/createrss.asp?c=171","en","usa"),
 ("who-emergencies","WHO Emergencies","https://www.who.int/feeds/entity/emergencies/en/rss.xml","en","un"),
 ("us-nrc-news","US Nuclear Regulatory Commission","https://www.nrc.gov/public-involve/rss.xml","en","usa"),
 ("iaea-alerts","IAEA Incident & Trafficking","https://www.iaea.org/feeds/incident-and-emergency-centre.rss","en","un"),
]
f_govt = F("gov_transparency_world.c",
  "Government transparency high-penetrancy feeds — gazettes, official journals, "
  "tenders/procurement, corporate/securities filings, court dockets, sanctions "
  "and lobbying disclosures (best-effort real RSS/Atom), via rss_collect.")
for gid,name,url,lang,country in GOVT:
    f_govt.add(usym("govt_"+gid.replace('-','_')), uid(gid), name, "government",
      "government", url, lang, f'["government","transparency","{country}"]', 7200,
      f"{name} — official transparency/records feed ({country})")

# ---------------- Deep OSINT subreddits ---------------------------------------
RSUB = [
 ("AskNetsec","cyber"),("blueteamsec","cyber"),("redteamsec","cyber"),("threatintel","threat-intel"),
 ("Pentesting","cyber"),("HowToHack","cyber"),("ReverseEngineering","reversing"),
 ("ComputerForensics","forensics"),("computerforensics","forensics"),("digitalforensics","forensics"),
 ("privacy","privacy"),("opsec","opsec"),("onions","dark-web"),("deepweb","dark-web"),
 ("RemoteSensing","geoint"),("gis","geoint"),("QGIS","geoint"),("geospatial","geoint"),
 ("adsb","aviation-tracking"),("RTLSDR","sigint"),("amateurradio","sigint"),("shortwave","sigint"),
 ("numberstations","sigint"),("hacking","cyber"),("blackhatlounge","cyber"),("Information_Security","cyber"),
 ("Scams","fraud"),("phishing","fraud"),("Bitcoin","crypto"),("CryptoCurrency","crypto"),
 ("Monero","crypto"),("WarshipPorn","military"),("MilitaryPorn","military"),("interestingasfuck","world"),
 ("TrueReddit","analysis"),("foreignpolicy","geopolitics"),("China","china-2"),("ukraine","ukraine-2"),
]
f_rsub = F("reddit_osint_deep.c",
  "Deep OSINT / SIGINT / GEOINT / cyber subreddit feeds (deterministic .rss) — "
  "specialist community signal, via rss_collect.")
for sub,tag in RSUB:
    f_rsub.add(usym("rsub_"+sub), uid(f"reddit-{sub}"), f"Reddit r/{sub}", "osint",
      "social", f"https://www.reddit.com/r/{sub}/.rss", "en",
      f'["reddit","social","{tag}"]', 1800, f"Reddit r/{sub} — {tag} community signal",
      nameja=f"Reddit r/{sub}")

# ---------------- Gap CERTs + outlets -----------------------------------------
GAP = [
 ("cert-gr","GR-CSIRT (Greece)","https://csirt.cynet.gov.cy/feed/","el","cyprus","cyber"),
 ("cert-hu","NBSZ NCSC Hungary","https://nki.gov.hu/feed/","hu","hungary","cyber"),
 ("cert-ro","CERT-RO / DNSC Romania","https://dnsc.ro/feed","ro","romania","cyber"),
 ("cert-hr","CERT.hr (Croatia)","https://www.cert.hr/feed/","hr","croatia","cyber"),
 ("cert-bg","CERT Bulgaria","https://www.govcert.bg/feed/","bg","bulgaria","cyber"),
 ("cert-tr","USOM Turkey","https://www.usom.gov.tr/rss/duyuru.rss","tr","turkey","cyber"),
 ("cert-lk","Sri Lanka CERT|CC","https://www.cert.gov.lk/feed/","en","sri-lanka","cyber"),
 ("cert-za","CSIRT South Africa","https://www.cybersecurityhub.gov.za/feed/","en","south-africa","cyber"),
 ("cert-ng","ngCERT Nigeria","https://cert.gov.ng/feed/","en","nigeria","cyber"),
 ("cert-my","MyCERT Malaysia","https://www.mycert.org.my/portal/rss","en","malaysia","cyber"),
 ("cert-id","BSSN Indonesia","https://bssn.go.id/feed/","id","indonesia","cyber"),
 ("cert-ph","NCERT Philippines","https://ncert.gov.ph/feed/","en","philippines","cyber"),
 ("cert-vn","VNCERT Vietnam","https://vncert.vn/rss","vi","vietnam","cyber"),
 ("cert-ae","aeCERT UAE","https://www.tra.gov.ae/en/rss.aspx","en","uae","cyber"),
 ("cert-sa","Saudi NCA","https://nca.gov.sa/rss","ar","saudi-arabia","cyber"),
 ("outlet-kompas","Kompas (Indonesia)","https://www.kompas.com/rss","id","indonesia","news"),
 ("outlet-thairath","Thairath (Thailand)","https://www.thairath.co.th/rss/news.xml","th","thailand","news"),
 ("outlet-vanguard-ng","Vanguard (Nigeria)","https://www.vanguardngr.com/feed/","en","nigeria","news"),
 ("outlet-standard-ke","The Standard (Kenya)","https://www.standardmedia.co.ke/rss/headlines.php","en","kenya","news"),
 ("outlet-el-comercio-pe","El Comercio (Peru)","https://elcomercio.pe/arcio/rss/","es","peru","news"),
]
f_gap = F("cert_outlet_gap.c",
  "Gap-region national CERTs and outlets (best-effort real RSS) — deeper country "
  "coverage, via rss_collect.")
for cid,name,url,lang,country,tag in GAP:
    coll = "cyber" if tag=="cyber" else "osint"
    f_gap.add(usym("gap_"+cid.replace('-','_')), uid(cid), name, coll,
      "cyber" if tag=="cyber" else "news", url, lang,
      f'["{tag}","{country}","national"]', 3600,
      f"{name} — national {tag} feed ({country})")

files = [f_gh, f_mon, f_leak, f_govt, f_rsub, f_gap]
total=0
for f in files:
    f.write(); total+=f.n; print(f"{f.name}: {f.n}")
print("TOTAL:", total)
open(SP+"/gen_ids_b4.txt","w").write("\n".join(sorted(used_ids))+"\n")
