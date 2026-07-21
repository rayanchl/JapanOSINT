#!/usr/bin/env python3
# Batch 3 generator: +200 diverse world OSINT collectors (research, government,
# central banks, think tanks, finance, science/space, geohazard, rights NGOs,
# regional outlets). Every source is a real RSS/Atom endpoint via rss_collect.
import os, re
OUT = "/home/user/JapanOSINT/native/collectors/sources"
SP = "/tmp/claude-0/-home-user-JapanOSINT/e003d5fe-3fcc-5b3c-b73d-86ef8af090a5/scratchpad"
reserved = set(l.strip() for l in open(SP + "/reserved_all.txt") if l.strip())
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
    def __init__(s, name, hdr): s.name=name; s.hdr=hdr; s.rows=[]; s.n=0
    def add(s, sym, sid, name, coll, cat, url, lang, tags, ival, desc, nameja=None):
        nameja = nameja or name
        s.rows.append(f'RSSX({sym}, "{cesc(sid)}", "{cesc(name)}", "{cesc(nameja)}", '
          f'"{coll}", "{cat}",\n  "{cesc(url)}", "{lang}", "{cesc(tags)}", {ival},\n'
          f'  "{cesc(desc)}");'); s.n+=1
    def write(s):
        open(os.path.join(OUT, s.name), "w").write(
            "/* " + s.hdr + " */\n" + MACRO + "\n" + "\n\n".join(s.rows) + "\n")

# ---------------- arXiv research categories (deterministic real RSS) ----------
ARXIV = [
 ("cs.CR","Cryptography and Security"),("cs.AI","Artificial Intelligence"),
 ("cs.LG","Machine Learning"),("cs.CL","Computation and Language (NLP)"),
 ("cs.CV","Computer Vision"),("cs.NI","Networking and Internet Architecture"),
 ("cs.CY","Computers and Society"),("cs.DC","Distributed and Cluster Computing"),
 ("cs.SE","Software Engineering"),("cs.RO","Robotics"),
 ("cs.DB","Databases"),("cs.IR","Information Retrieval"),
 ("cs.GT","Computer Science and Game Theory"),("cs.MA","Multiagent Systems"),
 ("cs.OS","Operating Systems"),("cs.PL","Programming Languages"),
 ("cs.AR","Hardware Architecture"),("cs.NE","Neural and Evolutionary Computing"),
 ("cs.SI","Social and Information Networks"),("cs.ET","Emerging Technologies"),
 ("cs.DS","Data Structures and Algorithms"),("cs.HC","Human-Computer Interaction"),
 ("eess.SP","Signal Processing"),("eess.SY","Systems and Control"),
 ("stat.ML","Statistics - Machine Learning"),("quant-ph","Quantum Physics"),
 ("physics.soc-ph","Physics and Society"),("math.OC","Optimization and Control"),
 ("econ.GN","General Economics"),("q-fin.GN","General Finance"),
 ("astro-ph.EP","Earth and Planetary Astrophysics"),("astro-ph.IM","Astrophysics Instrumentation"),
 ("physics.geo-ph","Geophysics"),("physics.space-ph","Space Physics"),
 ("physics.ao-ph","Atmospheric and Oceanic Physics"),("physics.med-ph","Medical Physics"),
 ("q-bio.PE","Populations and Evolution"),("nucl-ex","Nuclear Experiment"),
 ("cond-mat.mtrl-sci","Materials Science"),("physics.optics","Optics"),
]
f_arxiv = F("arxiv_feeds.c",
  "arXiv research-category feeds (deterministic real RSS at rss.arxiv.org). "
  "Early-signal scientific/technical intelligence, via rss_collect.")
for cat, name in ARXIV:
    f_arxiv.add(usym("arxiv_"+cat), uid("arxiv-"+cat), f"arXiv {cat} — {name}",
      "osint", "news", f"https://rss.arxiv.org/rss/{cat}", "en",
      f'["research","arxiv","preprint","{cat.lower()}"]', 21600,
      f"arXiv {cat} new submissions — {name}", nameja=f"arXiv {cat}")

# ---------------- Government / agency press feeds -----------------------------
GOV = [
 ("us-state-press","US State Dept Press Releases","https://www.state.gov/rss-feed/press-releases/feed/","en","usa"),
 ("us-dod-news","US Dept of Defense News","https://www.defense.gov/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=945&max=25","en","usa"),
 ("us-dhs-news","US DHS News Releases","https://www.dhs.gov/news-releases/rss.xml","en","usa"),
 ("us-doj-news","US Dept of Justice News","https://www.justice.gov/feeds/opa/justice-news.xml","en","usa"),
 ("us-sec-press","US SEC Press Releases","https://www.sec.gov/news/pressreleases.rss","en","usa"),
 ("us-ftc-press","US FTC Press Releases","https://www.ftc.gov/feeds/press-release.xml","en","usa"),
 ("us-gao-reports","US GAO Reports","https://www.gao.gov/rss/reports.xml","en","usa"),
 ("us-treasury-press","US Treasury Press","https://home.treasury.gov/rss/press.xml","en","usa"),
 ("us-nhc-atlantic","NOAA NHC Atlantic","https://www.nhc.noaa.gov/index-at.xml","en","usa"),
 ("us-nhc-epac","NOAA NHC East Pacific","https://www.nhc.noaa.gov/index-ep.xml","en","usa"),
 ("uk-gov-news","UK Government Announcements","https://www.gov.uk/search/news-and-communications.atom","en","uk"),
 ("uk-fcdo-news","UK FCDO News","https://www.gov.uk/government/organisations/foreign-commonwealth-development-office.atom","en","uk"),
 ("uk-mod-news","UK Ministry of Defence","https://www.gov.uk/government/organisations/ministry-of-defence.atom","en","uk"),
 ("ec-presscorner","European Commission Press","https://ec.europa.eu/commission/presscorner/api/rss?language=en","en","eu"),
 ("eeas-news","EU EEAS News","https://www.eeas.europa.eu/eeas/rss_en","en","eu"),
 ("enisa-news","ENISA News","https://www.enisa.europa.eu/media/news-items/news-wires/RSS","en","eu"),
 ("ema-news","European Medicines Agency","https://www.ema.europa.eu/en/rss.xml","en","eu"),
 ("frontex-news","Frontex News","https://www.frontex.europa.eu/rss/news/","en","eu"),
 ("europarl-news","European Parliament News","https://www.europarl.europa.eu/rss/doc/top-stories/en.xml","en","eu"),
 ("osce-news","OSCE News","https://www.osce.org/rss","en","europe"),
 ("iaea-topnews","IAEA Top News","https://www.iaea.org/feeds/topnews.rss","en","un"),
 ("un-news-2","UN News Global Perspective","https://news.un.org/feed/subscribe/en/news/all/rss.xml","en","un"),
 ("who-news","WHO News","https://www.who.int/rss-feeds/news-english.xml","en","un"),
 ("unhcr-news","UNHCR News","https://www.unhcr.org/rss/news.xml","en","un"),
 ("wfp-news","World Food Programme News","https://www.wfp.org/rss.xml","en","un"),
 ("iom-news","IOM Migration News","https://www.iom.int/rss.xml","en","un"),
 ("unodc-news","UNODC News","https://www.unodc.org/unodc/en/frontpage/rss.xml","en","un"),
 ("itu-news","ITU News","https://www.itu.int/en/rss/Pages/default.aspx","en","un"),
 ("opcw-news","OPCW News","https://www.opcw.org/rss.xml","en","un"),
 ("worldbank-news","World Bank News","https://www.worldbank.org/en/news/all.rss","en","un"),
 ("imf-news","IMF News","https://www.imf.org/en/News/rss?language=eng","en","un"),
 ("interpol-news","INTERPOL News","https://www.interpol.int/en/rss","en","global"),
 ("ru-mid","Russia MFA Statements","https://mid.ru/en/rss/","en","russia"),
 ("cn-mofa","China MFA Press","https://www.fmprc.gov.cn/mfa_eng/xwfw_665399/s2510_665401/rss.xml","en","china"),
 ("in-pib","India Press Info Bureau","https://pib.gov.in/RssMain.aspx?ModId=6&Lang=1&Regid=3","en","india"),
 ("au-dfat","Australia DFAT Media","https://www.dfat.gov.au/rss/media-releases.xml","en","australia"),
 ("ca-gov-news","Canada Government News","https://api.io.canada.ca/io-server/gc/news/en/v2?sort=publishedDate&orderBy=desc&format=atom","en","canada"),
 ("nato-opinions","NATO Opinions & Speeches","https://www.nato.int/cps/en/natohq/opinions.rss","en","global"),
 ("cisa-blog","CISA Blog","https://www.cisa.gov/blog.xml","en","usa"),
 ("fema-news","US FEMA News","https://www.fema.gov/feeds/news.rss","en","usa"),
 ("epa-news","US EPA News","https://www.epa.gov/newsreleases/search/rss","en","usa"),
 ("faa-news","US FAA News","https://www.faa.gov/newsroom/rss","en","usa"),
 ("ntsb-news","US NTSB News","https://www.ntsb.gov/news/press-releases/Pages/rss.aspx","en","usa"),
 ("noaa-swpc","NOAA Space Weather Alerts","https://www.swpc.noaa.gov/rss.xml","en","usa"),
]
f_gov = F("gov_agencies_world.c",
  "Government / agency / IGO press and advisory feeds worldwide (best-effort real "
  "RSS). Official primary-source intel, via rss_collect.")
for gid, name, url, lang, country in GOV:
    f_gov.add(usym("gov_"+gid.replace('-','_')), uid(gid), name, "government",
      "government", url, lang, f'["government","official","{country}"]', 3600,
      f"{name} — official government/agency feed ({country})")

# ---------------- Central banks / monetary authorities ------------------------
CBANK = [
 ("fed-press","US Federal Reserve Press","https://www.federalreserve.gov/feeds/press_all.xml","en"),
 ("ecb-press","European Central Bank Press","https://www.ecb.europa.eu/rss/press.html","en"),
 ("boe-news","Bank of England News","https://www.bankofengland.co.uk/rss/news","en"),
 ("boj-news","Bank of Japan Releases","https://www.boj.or.jp/en/rss/whatsnew.xml","en"),
 ("rba-media","Reserve Bank of Australia","https://www.rba.gov.au/rss/rss-cb-media-releases.xml","en"),
 ("boc-press","Bank of Canada Press","https://www.bankofcanada.ca/content_type/press-releases/feed/","en"),
 ("bis-press","Bank for Intl Settlements","https://www.bis.org/list/press_releases/rss.xml","en"),
 ("rbi-press","Reserve Bank of India","https://www.rbi.org.in/Scripts/Rss.aspx?Id=2","en"),
 ("snb-press","Swiss National Bank","https://www.snb.ch/public/en/rss/news","en"),
 ("riksbank","Sweden Riksbank","https://www.riksbank.se/en-gb/rss/press-releases/","en"),
 ("norgesbank","Norges Bank","https://www.norges-bank.no/en/rss/","en"),
 ("bcb-brazil","Banco Central do Brasil","https://www.bcb.gov.br/rss/ultimasnoticias","pt"),
 ("banxico","Bank of Mexico","https://www.banxico.org.mx/rss/rss.xml","es"),
 ("cbr-russia","Bank of Russia","https://www.cbr.ru/rss/eventrss","ru"),
 (" readme-imf","IMF Country Focus","https://www.imf.org/en/News/rss?language=eng&series=News+Article","en"),
 ("ecb-blog","ECB Blog & Research","https://www.ecb.europa.eu/rss/blog.html","en"),
]
f_cb = F("central_banks_world.c",
  "Central-bank / monetary-authority press feeds (best-effort real RSS) — "
  "macro-financial and policy signal, via rss_collect.")
for cid, name, url, lang in CBANK:
    cid = cid.strip()
    f_cb.add(usym("cb_"+cid.replace('-','_')), uid(cid), name, "economy",
      "economy", url, lang, '["economy","central-bank","monetary-policy"]', 7200,
      f"{name} — central-bank press and policy feed")

# ---------------- Think tanks / research institutes ---------------------------
TANK = [
 ("csis","CSIS Analysis","https://www.csis.org/analysis/feed","en"),
 ("brookings","Brookings Institution","https://www.brookings.edu/feed/","en"),
 ("atlantic-council","Atlantic Council","https://www.atlanticcouncil.org/feed/","en"),
 ("carnegie-endow","Carnegie Endowment","https://carnegieendowment.org/posts/rss/","en"),
 ("chatham-house","Chatham House","https://www.chathamhouse.org/rss/publications.xml","en"),
 ("rusi","RUSI","https://rusi.org/rss.xml","en"),
 ("iiss","IISS","https://www.iiss.org/rss","en"),
 ("sipri","SIPRI","https://www.sipri.org/rss.xml","en"),
 ("fpri","Foreign Policy Research Institute","https://www.fpri.org/feed/","en"),
 ("wilson-center","Wilson Center","https://www.wilsoncenter.org/rss.xml","en"),
 ("cnas","Center for a New American Security","https://www.cnas.org/rss","en"),
 ("ecfr","European Council on Foreign Relations","https://ecfr.eu/feed/","en"),
 ("merics","MERICS (China)","https://merics.org/en/rss.xml","en"),
 ("lowy-interpreter","Lowy Institute Interpreter","https://www.lowyinstitute.org/the-interpreter/rss.xml","en"),
 ("stimson","Stimson Center","https://www.stimson.org/feed/","en"),
 ("jamestown","Jamestown Foundation","https://jamestown.org/feed/","en"),
 ("cepa","Center for European Policy Analysis","https://cepa.org/feed/","en"),
 ("hudson","Hudson Institute","https://www.hudson.org/rss","en"),
 ("heritage","Heritage Foundation","https://www.heritage.org/rss.xml","en"),
 ("rand-pubs","RAND Corporation","https://www.rand.org/pubs/new.xml","en"),
 ("csis-china","CSIS ChinaPower","https://chinapower.csis.org/feed/","en"),
 ("mei","Middle East Institute","https://www.mei.edu/rss.xml","en"),
 ("carnegie-china","Carnegie China","https://carnegieendowment.org/china/rss/","en"),
 ("gmfus","German Marshall Fund","https://www.gmfus.org/rss.xml","en"),
]
f_tank = F("thinktanks_world.c",
  "Think-tank / research-institute analysis feeds (best-effort real RSS) — "
  "geopolitical and security analysis, via rss_collect.")
for tid, name, url, lang in TANK:
    f_tank.add(usym("tank_"+tid.replace('-','_')), uid(tid), name, "osint",
      "news", url, lang, '["osint","think-tank","analysis"]', 10800,
      f"{name} — geopolitical/security research and analysis")

# ---------------- Finance / markets / crypto ----------------------------------
FIN = [
 ("coindesk","CoinDesk","https://www.coindesk.com/arc/outboundfeeds/rss/","crypto"),
 ("cointelegraph","Cointelegraph","https://cointelegraph.com/rss","crypto"),
 ("the-block","The Block","https://www.theblock.co/rss.xml","crypto"),
 ("decrypt","Decrypt","https://decrypt.co/feed","crypto"),
 ("bitcoin-mag","Bitcoin Magazine","https://bitcoinmagazine.com/feed","crypto"),
 ("cnbc-top","CNBC Top News","https://www.cnbc.com/id/100003114/device/rss/rss.html","markets"),
 ("cnbc-world","CNBC World","https://www.cnbc.com/id/100727362/device/rss/rss.html","markets"),
 ("cnbc-finance","CNBC Finance","https://www.cnbc.com/id/10000664/device/rss/rss.html","markets"),
 ("investing-news","Investing.com News","https://www.investing.com/rss/news.rss","markets"),
 ("zerohedge","ZeroHedge","https://feeds.feedburner.com/zerohedge/feed","markets"),
 ("ft-home","Financial Times Home","https://www.ft.com/rss/home","markets"),
 ("seeking-alpha","Seeking Alpha Market News","https://seekingalpha.com/market_currents.xml","markets"),
 ("marketwatch-top","MarketWatch Top Stories","http://feeds.marketwatch.com/marketwatch/topstories/","markets"),
 ("kitco-news","Kitco Metals News","https://www.kitco.com/rss/KitcoNews.xml","commodities"),
 ("oilprice-2","OilPrice Energy News","https://oilprice.com/rss/main","energy"),
 ("trading-econ","Trading Economics Stream","https://tradingeconomics.com/rss/news.aspx","markets"),
]
f_fin = F("finance_markets_crypto.c",
  "Finance / markets / crypto feeds (real RSS) — economic and digital-asset "
  "signal for follow-the-money OSINT, via rss_collect.")
for fid, name, url, tag in FIN:
    f_fin.add(usym("fin_"+fid.replace('-','_')), uid(fid), name, "osint",
      "economy", url, "en", f'["economy","{tag}","markets"]', 3600,
      f"{name} — {tag} market intelligence feed")

# ---------------- Science / space / tech --------------------------------------
SCI = [
 ("esa-topnews","ESA Top News","https://www.esa.int/rssfeed/TopNews","space"),
 ("nature-news","Nature News","https://www.nature.com/nature.rss","science"),
 ("science-aaas","Science (AAAS) News","https://www.science.org/rss/news_current.xml","science"),
 ("mit-tech-review","MIT Technology Review","https://www.technologyreview.com/feed/","tech"),
 ("ieee-spectrum","IEEE Spectrum","https://spectrum.ieee.org/rss/fulltext","tech"),
 ("ars-technica","Ars Technica","https://feeds.arstechnica.com/arstechnica/index","tech"),
 ("wired","WIRED","https://www.wired.com/feed/rss","tech"),
 ("the-register","The Register","https://www.theregister.com/headlines.atom","tech"),
 ("phys-org","Phys.org","https://phys.org/rss-feed/","science"),
 ("sciencedaily","ScienceDaily","https://www.sciencedaily.com/rss/all.xml","science"),
 ("space-com","Space.com","https://www.space.com/feeds/all","space"),
 ("defense-one","Defense One","https://www.defenseone.com/rss/all/","defense"),
 ("mit-news","MIT News","https://news.mit.edu/rss/feed","science"),
 ("quanta","Quanta Magazine","https://www.quantamagazine.org/feed/","science"),
 ("hn-frontpage","Hacker News Front Page","https://hnrss.org/frontpage","tech"),
]
f_sci = F("science_space_tech.c",
  "Science / space / technology feeds (real RSS) — emerging-tech and dual-use "
  "research signal, via rss_collect.")
for sid, name, url, tag in SCI:
    f_sci.add(usym("sci_"+sid.replace('-','_')), uid(sid), name, "osint",
      "news", url, "en", f'["science","{tag}","research"]', 7200,
      f"{name} — {tag} science/technology feed")

# ---------------- Geohazard extra ---------------------------------------------
GEO = [
 ("emsc-quakes","EMSC Latest Earthquakes","https://www.emsc-csem.org/service/rss/rss.php?typ=emsc","seismic"),
 ("nhc-cpac","NOAA NHC Central Pacific","https://www.nhc.noaa.gov/index-cp.xml","cyclone"),
 ("volcanodiscovery","VolcanoDiscovery","https://www.volcanodiscovery.com/rss.xml","volcano"),
 ("bom-au-warnings","Australia BoM Warnings","http://www.bom.gov.au/rss/1.xml","weather"),
 ("metoffice-warnings","UK Met Office Warnings","https://www.metoffice.gov.uk/public/data/PWSCache/WarningsRSS/Region/UK","weather"),
 ("iceland-quakes","Iceland Met Office Quakes","https://en.vedur.is/earthquakes-and-volcanism/earthquakes/rss/","seismic"),
 ("copernicus-ems","Copernicus Emergency Mgmt","https://emergency.copernicus.eu/mapping/activations-rapid/rss.xml","disaster"),
 ("pdc-disasters","Pacific Disaster Center","https://www.pdc.org/feed/","disaster"),
]
f_geo = F("geohazard_extra.c",
  "Additional real-time geohazard feeds (seismic, cyclone, volcano, weather, "
  "disaster) — real RSS, via rss_collect.")
for gid, name, url, tag in GEO:
    f_geo.add(usym("geo_"+gid.replace('-','_')), uid(gid), name, "environment",
      "environment", url, "en", f'["hazard","{tag}","alert"]', 900,
      f"{name} — real-time {tag} hazard feed")

# ---------------- Rights / investigative NGOs ---------------------------------
NGO = [
 ("insight-crime","InSight Crime","https://insightcrime.org/feed/","organized-crime"),
 ("citizen-lab","Citizen Lab","https://citizenlab.ca/feed/","digital-rights"),
 ("eff-deeplinks","EFF Deeplinks","https://www.eff.org/rss/updates.xml","digital-rights"),
 ("cpj","Committee to Protect Journalists","https://cpj.org/feed/","press-freedom"),
 ("rsf","Reporters Without Borders","https://rsf.org/en/rss.xml","press-freedom"),
 ("global-witness","Global Witness","https://www.globalwitness.org/en/rss/","corruption"),
 ("transparency-intl","Transparency International","https://www.transparency.org/en/rss","corruption"),
 ("access-now","Access Now","https://www.accessnow.org/feed/","digital-rights"),
 ("article19","ARTICLE 19","https://www.article19.org/feed/","free-expression"),
 ("forbidden-stories","Forbidden Stories","https://forbiddenstories.org/feed/","investigation"),
 ("freedom-house","Freedom House","https://freedomhouse.org/rss.xml","democracy"),
 ("bellingcat-3","Bellingcat Long-Reads","https://www.bellingcat.com/category/resources/feed/","osint"),
 ("privacy-intl","Privacy International","https://privacyinternational.org/rss.xml","privacy"),
]
f_ngo = F("ngo_rights_extra.c",
  "Rights / investigative-NGO feeds (real RSS) — corruption, digital rights, "
  "press freedom, cross-border investigations, via rss_collect.")
for nid, name, url, tag in NGO:
    f_ngo.add(usym("ngo_"+nid.replace('-','_')), uid(nid), name, "osint",
      "news", url, "en", f'["osint","ngo","{tag}"]', 10800,
      f"{name} — {tag} investigations and reporting")

# ---------------- Regional outlets (gap regions) ------------------------------
REG = [
 ("allafrica","AllAfrica Headlines","https://allafrica.com/tools/headlines/rdf/latest/headlines.rdf","en","africa"),
 ("iss-africa","ISS Africa","https://issafrica.org/rss/recent.xml","en","africa"),
 ("the-continent","The Africa Report Politics","https://www.theafricareport.com/section/politics/feed/","en","africa"),
 ("semafor-africa-2","Semafor Africa Signals","https://www.semafor.com/vertical/africa/rss.xml","en","africa"),
 ("oc-media","OC Media (Caucasus)","https://oc-media.org/feed/","en","caucasus"),
 ("bne-intellinews","bne IntelliNews","https://www.intellinews.com/feed","en","emerging-europe"),
 ("emerging-europe","Emerging Europe","https://emerging-europe.com/feed/","en","emerging-europe"),
 ("novaya-europe","Novaya Gazeta Europe","https://novayagazeta.eu/feed/rss","en","russia"),
 ("meduza-2","Meduza Features","https://meduza.io/rss/en/all","en","russia"),
 ("benar-news","BenarNews (SE Asia)","https://www.benarnews.org/english/rss2.xml","en","southeast-asia"),
 ("frontier-myanmar","Frontier Myanmar","https://www.frontiermyanmar.net/en/feed/","en","myanmar"),
 ("the-wire-india","The Wire (India)","https://thewire.in/rss","en","india"),
 ("scroll-in","Scroll.in (India)","https://scroll.in/feeds/all.rss","en","india"),
 ("the-new-arab","The New Arab","https://www.newarab.com/rss","en","middle-east"),
 ("amwaj-media","Amwaj.media (Gulf/Iran)","https://amwaj.media/rss","en","middle-east"),
 ("insight-crime-2","InSight Crime Investigations","https://insightcrime.org/investigations/feed/","en","latam"),
 ("dialogo-americas","Diálogo Américas","https://dialogo-americas.com/feed/","en","latam"),
 ("caribbean-news","Caribbean National Weekly","https://www.caribbeannationalweekly.com/feed/","en","caribbean"),
 ("rnz-pacific","RNZ Pacific","https://www.rnz.co.nz/rss/pacific.xml","en","pacific"),
 ("islands-business","Islands Business (Pacific)","https://islandsbusiness.com/feed/","en","pacific"),
 ("kyiv-indep-3","Kyiv Independent Business","https://kyivindependent.com/tag/business/feed/","en","ukraine"),
 ("balkan-insight-2","BIRN Balkan Insight Investigations","https://balkaninsight.com/category/investigations/feed/","en","balkans"),
 ("hong-kong-fp","Hong Kong Free Press","https://hongkongfp.com/feed/","en","hong-kong"),
 ("the-print-india","ThePrint (India)","https://theprint.in/feed/","en","india"),
]
f_reg = F("regional_outlets_extra.c",
  "Regional OSINT outlets for gap regions (Africa, Caucasus, Central/Emerging "
  "Europe, SE Asia, Gulf, LatAm, Caribbean, Pacific) — real RSS, via rss_collect.")
for rid, name, url, lang, region in REG:
    f_reg.add(usym("reg_"+rid.replace('-','_')), uid(rid), name, "osint",
      "news", url, lang, f'["news","{region}","regional-outlet"]', 3600,
      f"{name} — regional coverage ({region})")

files = [f_arxiv, f_gov, f_cb, f_tank, f_fin, f_sci, f_geo, f_ngo, f_reg]
total = 0
for f in files:
    f.write(); total += f.n; print(f"{f.name}: {f.n}")
print("TOTAL:", total)
open(SP+"/gen_ids_b3.txt","w").write("\n".join(sorted(used_ids))+"\n")
