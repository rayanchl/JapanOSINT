#!/usr/bin/env python3
# Generator for 1000 world OSINT collectors -> native/collectors/sources/*.c
# Every source is a real RSS/Atom endpoint via the shared rss_collect toolkit.
import os, re

OUT = "/home/user/JapanOSINT/native/collectors/sources"
existing_ids = set(
    l.strip() for l in open(
        "/tmp/claude-0/-home-user-JapanOSINT/e003d5fe-3fcc-5b3c-b73d-86ef8af090a5/scratchpad/existing_ids.txt"))
# also reserve the 100 ids added in the previous batch
for l in open("/tmp/claude-0/-home-user-JapanOSINT/e003d5fe-3fcc-5b3c-b73d-86ef8af090a5/scratchpad/new_ids.txt"):
    existing_ids.add(l.strip())

used_ids = set()
used_syms = set()
def uid(base):
    """unique, sanitized id"""
    base = re.sub(r'[^a-z0-9-]', '-', base.lower()).strip('-')
    base = re.sub(r'-+', '-', base)
    i, cand = 1, base
    while cand in used_ids or cand in existing_ids:
        i += 1; cand = f"{base}-{i}"
    used_ids.add(cand); return cand
def usym(base):
    base = re.sub(r'[^a-z0-9_]', '_', base.lower())
    base = re.sub(r'_+', '_', base).strip('_')
    i, cand = 1, base
    while cand in used_syms:
        i += 1; cand = f"{base}_{i}"
    used_syms.add(cand); return cand
def cesc(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')
def urlenc(q):
    out = []
    for ch in q:
        if ch.isalnum() or ch in '-_.~':
            out.append(ch)
        elif ch == ' ':
            out.append('%20')
        else:
            out.append('%' + format(ord(ch), '02X'))
    return ''.join(out)

MACRO = r'''#include "../../source.h"
#include "../../lib/rss_atom.h"

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
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

class File:
    def __init__(self, name, header):
        self.name = name; self.lines = []; self.header = header; self.n = 0
    def add(self, sym, sid, name, nameja, coll, cat, url, lang, tags, ival, desc):
        self.lines.append(
            f'RSSX({sym}, "{cesc(sid)}", "{cesc(name)}", "{cesc(nameja)}", '
            f'"{coll}", "{cat}",\n  "{cesc(url)}", "{lang}", "{cesc(tags)}", {ival},\n'
            f'  "{cesc(desc)}");')
        self.n += 1
    def write(self):
        body = MACRO + "\n" + "\n\n".join(self.lines) + "\n"
        with open(os.path.join(OUT, self.name), "w") as f:
            f.write("/* " + self.header + " */\n" + body)

# ------------------------------------------------------------------ countries
# (gl, hl, clang, english, japanese)  -- priority order; topics apply to first N
C = [
 ("US","en-US","en","United States","アメリカ"),("GB","en-GB","en","United Kingdom","イギリス"),
 ("FR","fr","fr","France","フランス"),("DE","de","de","Germany","ドイツ"),
 ("IT","it","it","Italy","イタリア"),("ES","es","es","Spain","スペイン"),
 ("JP","ja","ja","Japan","日本"),("KR","ko","ko","South Korea","韓国"),
 ("CN","zh-CN","zh","China","中国"),("TW","zh-TW","zh","Taiwan","台湾"),
 ("HK","zh-HK","zh","Hong Kong","香港"),("IN","en-IN","en","India","インド"),
 ("RU","ru","ru","Russia","ロシア"),("UA","uk","uk","Ukraine","ウクライナ"),
 ("BR","pt-BR","pt","Brazil","ブラジル"),("MX","es-419","es","Mexico","メキシコ"),
 ("AR","es-419","es","Argentina","アルゼンチン"),("CA","en-CA","en","Canada","カナダ"),
 ("AU","en-AU","en","Australia","オーストラリア"),("NZ","en-NZ","en","New Zealand","ニュージーランド"),
 ("TR","tr","tr","Turkey","トルコ"),("IL","iw","iw","Israel","イスラエル"),
 ("SA","ar","ar","Saudi Arabia","サウジアラビア"),("AE","ar","ar","United Arab Emirates","アラブ首長国連邦"),
 ("EG","ar","ar","Egypt","エジプト"),("IR","fa","fa","Iran","イラン"),
 ("PK","en-PK","en","Pakistan","パキスタン"),("ID","id","id","Indonesia","インドネシア"),
 ("TH","th","th","Thailand","タイ"),("VN","vi","vi","Vietnam","ベトナム"),
 ("PH","en-PH","en","Philippines","フィリピン"),("SG","en-SG","en","Singapore","シンガポール"),
 ("MY","en-MY","en","Malaysia","マレーシア"),("NL","nl","nl","Netherlands","オランダ"),
 ("BE","fr","fr","Belgium","ベルギー"),("CH","de","de","Switzerland","スイス"),
 ("AT","de","de","Austria","オーストリア"),("SE","sv","sv","Sweden","スウェーデン"),
 ("NO","no","no","Norway","ノルウェー"),("DK","da","da","Denmark","デンマーク"),
 ("FI","fi","fi","Finland","フィンランド"),("PL","pl","pl","Poland","ポーランド"),
 ("CZ","cs","cs","Czechia","チェコ"),("GR","el","el","Greece","ギリシャ"),
 ("PT","pt-PT","pt","Portugal","ポルトガル"),("IE","en-IE","en","Ireland","アイルランド"),
 ("ZA","en-ZA","en","South Africa","南アフリカ"),("NG","en-NG","en","Nigeria","ナイジェリア"),
 ("KE","en-KE","en","Kenya","ケニア"),("CO","es-419","es","Colombia","コロンビア"),
 ("CL","es-419","es","Chile","チリ"),("PE","es-419","es","Peru","ペルー"),
 ("RO","ro","ro","Romania","ルーマニア"),("HU","hu","hu","Hungary","ハンガリー"),
 ("BG","bg","bg","Bulgaria","ブルガリア"),("RS","sr","sr","Serbia","セルビア"),
 ("HR","hr","hr","Croatia","クロアチア"),("SK","sk","sk","Slovakia","スロバキア"),
 ("SI","sl","sl","Slovenia","スロベニア"),("LT","lt","lt","Lithuania","リトアニア"),
 ("LV","lv","lv","Latvia","ラトビア"),("EE","et","et","Estonia","エストニア"),
 ("IS","is","is","Iceland","アイスランド"),("BD","bn","bn","Bangladesh","バングラデシュ"),
 ("LK","en","en","Sri Lanka","スリランカ"),("NP","en","en","Nepal","ネパール"),
 ("MM","en","en","Myanmar","ミャンマー"),("KH","en","en","Cambodia","カンボジア"),
 ("QA","ar","ar","Qatar","カタール"),("KW","ar","ar","Kuwait","クウェート"),
 ("IQ","ar","ar","Iraq","イラク"),("JO","ar","ar","Jordan","ヨルダン"),
 ("LB","ar","ar","Lebanon","レバノン"),("MA","ar","ar","Morocco","モロッコ"),
 ("DZ","ar","ar","Algeria","アルジェリア"),("TN","ar","ar","Tunisia","チュニジア"),
 ("VE","es-419","es","Venezuela","ベネズエラ"),("EC","es-419","es","Ecuador","エクアドル"),
 ("BO","es-419","es","Bolivia","ボリビア"),("PY","es-419","es","Paraguay","パラグアイ"),
 ("UY","es-419","es","Uruguay","ウルグアイ"),("CR","es-419","es","Costa Rica","コスタリカ"),
 ("PA","es-419","es","Panama","パナマ"),("GT","es-419","es","Guatemala","グアテマラ"),
 ("CU","es-419","es","Cuba","キューバ"),("DO","es-419","es","Dominican Republic","ドミニカ共和国"),
 ("KZ","ru","ru","Kazakhstan","カザフスタン"),("UZ","ru","ru","Uzbekistan","ウズベキスタン"),
 ("AZ","ru","ru","Azerbaijan","アゼルバイジャン"),("AM","ru","ru","Armenia","アルメニア"),
 ("GE","en","en","Georgia","ジョージア"),("BY","ru","ru","Belarus","ベラルーシ"),
 ("MD","ro","ro","Moldova","モルドバ"),("MN","en","en","Mongolia","モンゴル"),
 # long tail (top-headlines only)
 ("GH","en","en","Ghana","ガーナ"),("ET","en","en","Ethiopia","エチオピア"),
 ("TZ","en","en","Tanzania","タンザニア"),("UG","en","en","Uganda","ウガンダ"),
 ("ZW","en","en","Zimbabwe","ジンバブエ"),("ZM","en","en","Zambia","ザンビア"),
 ("SN","fr","fr","Senegal","セネガル"),("CI","fr","fr","Ivory Coast","コートジボワール"),
 ("CM","fr","fr","Cameroon","カメルーン"),("CD","fr","fr","DR Congo","コンゴ民主共和国"),
 ("AO","pt-PT","pt","Angola","アンゴラ"),("MZ","pt-PT","pt","Mozambique","モザンビーク"),
 ("RW","en","en","Rwanda","ルワンダ"),("BW","en","en","Botswana","ボツワナ"),
 ("NA","en","en","Namibia","ナミビア"),("MW","en","en","Malawi","マラウイ"),
 ("SD","ar","ar","Sudan","スーダン"),("SO","en","en","Somalia","ソマリア"),
 ("LY","ar","ar","Libya","リビア"),("YE","ar","ar","Yemen","イエメン"),
 ("OM","ar","ar","Oman","オマーン"),("BH","ar","ar","Bahrain","バーレーン"),
 ("SY","ar","ar","Syria","シリア"),("AF","en","en","Afghanistan","アフガニスタン"),
 ("KG","ru","ru","Kyrgyzstan","キルギス"),("TJ","ru","ru","Tajikistan","タジキスタン"),
 ("TM","ru","ru","Turkmenistan","トルクメニスタン"),("LA","en","en","Laos","ラオス"),
 ("BN","en","en","Brunei","ブルネイ"),("MV","en","en","Maldives","モルディブ"),
 ("BT","en","en","Bhutan","ブータン"),("MO","zh-HK","zh","Macau","マカオ"),
 ("FJ","en","en","Fiji","フィジー"),("PG","en","en","Papua New Guinea","パプアニューギニア"),
 ("HN","es-419","es","Honduras","ホンジュラス"),("NI","es-419","es","Nicaragua","ニカラグア"),
 ("SV","es-419","es","El Salvador","エルサルバドル"),("JM","en","en","Jamaica","ジャマイカ"),
 ("TT","en","en","Trinidad and Tobago","トリニダード・トバゴ"),("BS","en","en","Bahamas","バハマ"),
 ("LU","fr","fr","Luxembourg","ルクセンブルク"),("MT","en","en","Malta","マルタ"),
 ("CY","el","el","Cyprus","キプロス"),("AL","en","en","Albania","アルバニア"),
 ("MK","en","en","North Macedonia","北マケドニア"),("BA","en","en","Bosnia and Herzegovina","ボスニア"),
 ("ME","en","en","Montenegro","モンテネグロ"),("XK","en","en","Kosovo","コソボ"),
 ("UA2","uk","uk","Ukraine (regional)","ウクライナ地方"),  # placeholder removed below
]
# drop accidental placeholder
C = [c for c in C if c[0] != "UA2"]

def ceid(gl, clang):
    return f"{gl}:{clang}"
def gnews_top(gl, hl, clang):
    return f"https://news.google.com/rss?hl={hl}&gl={gl}&ceid={ceid(gl,clang)}"
def gnews_topic(topic, gl, hl, clang):
    return (f"https://news.google.com/rss/headlines/section/topic/{topic}"
            f"?hl={hl}&gl={gl}&ceid={ceid(gl,clang)}")
def gnews_search(q, gl, hl, clang):
    return (f"https://news.google.com/rss/search?q={urlenc(q)}"
            f"&hl={hl}&gl={gl}&ceid={ceid(gl,clang)}")

# ---- FILE: national top headlines
f_top = File("gnews_world_top.c",
  "Google News national top-headlines editions (real per-country RSS). One "
  "high-yield OSINT news monitor per country worldwide, via rss_collect.")
for gl, hl, clang, en, ja in C:
    sid = uid(f"gnews-top-{gl}")
    sym = usym(f"gn_top_{gl}")
    f_top.add(sym, sid, f"Google News — {en}", f"Google ニュース — {ja}",
      "osint", "news", gnews_top(gl, hl, clang), clang if len(clang)==2 else "en",
      f'["news","{en.lower().replace(" ","-")}","google-news","country-edition"]',
      3600, f"Google News top headlines edition for {en} — national news monitor")

# ---- FILES: topic sections for priority countries
TOPICS = [("WORLD","World"),("NATION","National"),("BUSINESS","Business"),
          ("TECHNOLOGY","Technology"),("SCIENCE","Science"),("HEALTH","Health")]
PRIORITY = C[:90]
f_t1 = File("gnews_world_topics_1.c",
  "Google News per-country topic sections (World/National/Business) — real RSS "
  "editions for priority countries, via rss_collect.")
f_t2 = File("gnews_world_topics_2.c",
  "Google News per-country topic sections (Technology/Science/Health) — real RSS "
  "editions for priority countries, via rss_collect.")
for gl, hl, clang, en, ja in PRIORITY:
    for topic, tl in TOPICS:
        sid = uid(f"gnews-{gl}-{topic.lower()}")
        sym = usym(f"gn_{gl}_{topic.lower()}")
        tgt = f_t1 if topic in ("WORLD","NATION","BUSINESS") else f_t2
        tgt.add(sym, sid, f"Google News {tl} — {en}",
          f"Google {tl} — {ja}", "osint", "news",
          gnews_topic(topic, gl, hl, clang), clang if len(clang)==2 else "en",
          f'["news","{topic.lower()}","{en.lower().replace(" ","-")}","google-news"]',
          3600, f"Google News {tl} section for {en}")

# ---- FILE: global + regional OSINT search monitors
GLOBAL_Q = [
 "cyberattack","data breach","ransomware","zero-day exploit","critical infrastructure attack",
 "power grid failure","undersea cable","GPS jamming","electronic warfare","military exercise",
 "missile test","nuclear weapon","troop movement","border clash","drone strike",
 "airspace violation","naval deployment","military coup","civil unrest","economic sanctions",
 "arms shipment","weapons smuggling","money laundering","oil tanker seizure","shadow fleet",
 "port disruption","supply chain disruption","semiconductor export controls","espionage arrest",
 "assassination","terror attack","hostage crisis","drug cartel","human trafficking",
 "wildfire","major earthquake","catastrophic flood","volcano eruption","chemical spill",
 "nuclear plant incident","pipeline explosion","aviation incident","cargo ship collision",
 "disease outbreak","election interference","disinformation campaign","state-sponsored hacking",
 "satellite launch","hypersonic missile","cyber espionage",
]
REGIONAL_Q = [
 ("front line","UA","uk","uk"),("Gaza","IL","iw","iw"),("PLA incursion","TW","zh-TW","zh"),
 ("South China Sea","PH","en-PH","en"),("Sahel militants","FR","fr","fr"),
 ("Red Sea shipping","AE","ar","ar"),("North Korea missile","KR","ko","ko"),
 ("Taiwan Strait","US","en-US","en"),("Wagner","RU","ru","ru"),("Kashmir","IN","en-IN","en"),
 ("cartel violence","MX","es-419","es"),("Amazon deforestation","BR","pt-BR","pt"),
 ("Strait of Hormuz","US","en-US","en"),("Baltic security","LT","lt","lt"),
 ("Arctic militarization","NO","no","no"),("Horn of Africa","KE","en-KE","en"),
 ("cross-strait","CN","zh-CN","zh"),("nuclear talks","IR","fa","fa"),
 ("coup attempt","FR","fr","fr"),("rare earth","CN","zh-CN","zh"),
]
f_mon = File("gnews_osint_monitors.c",
  "Google News OSINT keyword monitors — real search-feed RSS tracking high-yield "
  "security/conflict/infrastructure/disaster terms globally and per hotspot region.")
for q in GLOBAL_Q:
    sid = uid("gnews-mon-" + q)
    sym = usym("gn_mon_" + q)
    f_mon.add(sym, sid, f"OSINT Monitor: {q}", f"OSINT モニター: {q}",
      "osint", "news", gnews_search(q, "US", "en-US", "en"), "en",
      f'["osint","monitor","alert","{q.split()[0].lower()}"]', 1800,
      f"Global Google News monitor tracking '{q}' — real-time OSINT alerting feed")
for q, gl, hl, clang in REGIONAL_Q:
    sid = uid(f"gnews-mon-{gl}-" + q)
    sym = usym(f"gn_mon_{gl}_" + q)
    f_mon.add(sym, sid, f"OSINT Monitor ({gl}): {q}", f"OSINT モニター ({gl}): {q}",
      "osint", "news", gnews_search(q, gl, hl, clang), clang if len(clang)==2 else "en",
      f'["osint","monitor","regional","{gl.lower()}"]', 1800,
      f"Regional Google News monitor ({gl}) tracking '{q}' — hotspot OSINT feed")

# ---- FILE: Reddit geo / country / city / OSINT subreddits (.rss deterministic)
REDDIT = [
 # global + osint/defense
 ("worldnews","world"),("news","global"),("geopolitics","geopolitics"),("anime_titties","world-news-alt"),
 ("OSINT","osint"),("CredibleDefense","defense"),("LessCredibleDefence","defense"),
 ("WarCollege","military"),("CombatFootage","conflict"),("UkraineWarVideoReport","ukraine"),
 ("syriancivilwar","syria"),("europe","europe"),("MiddleEastNews","middle-east"),
 ("africa","africa"),("asia","asia"),("southasia","south-asia"),("latinamerica","latam"),
 ("neutralnews","news"),("qualitynews","news"),("Intelligence","intelligence"),
 ("NatSecPol","national-security"),("geopolitics2","geopolitics"),("YemeniCrisis","yemen"),
 ("IsraelPalestine","israel-palestine"),("China_Debate","china"),("aviation","aviation"),
 ("navy","navy"),("Military","military"),("TankPorn","military"),("hackernews","cyber"),
 ("cybersecurity","cyber"),("netsec","cyber"),("Malware","cyber"),("blackhat","cyber"),
 # countries
 ("unitedkingdom","uk"),("ukpolitics","uk"),("france","france"),("germany","germany"),
 ("italy","italy"),("spain","spain"),("portugal","portugal"),("netherlands","netherlands"),
 ("belgium","belgium"),("ireland","ireland"),("sweden","sweden"),("norway","norway"),
 ("denmark","denmark"),("Finland","finland"),("iceland","iceland"),("poland","poland"),
 ("czech","czechia"),("Slovakia","slovakia"),("hungary","hungary"),("Romania","romania"),
 ("bulgaria","bulgaria"),("greece","greece"),("croatia","croatia"),("serbia","serbia"),
 ("Austria","austria"),("switzerland","switzerland"),("russia","russia"),("ukraina","ukraine"),
 ("belarus","belarus"),("lithuania","lithuania"),("latvia","latvia"),("Eesti","estonia"),
 ("india","india"),("IndiaSpeaks","india"),("china","china"),("China_irl","china"),
 ("japan","japan"),("korea","korea"),("taiwan","taiwan"),("HongKong","hong-kong"),
 ("indonesia","indonesia"),("Thailand","thailand"),("VietNam","vietnam"),("Philippines","philippines"),
 ("malaysia","malaysia"),("singapore","singapore"),("pakistan","pakistan"),("bangladesh","bangladesh"),
 ("Nepal","nepal"),("srilanka","sri-lanka"),("myanmar","myanmar"),("australia","australia"),
 ("newzealand","new-zealand"),("canada","canada"),("mexico","mexico"),("brasil","brazil"),
 ("argentina","argentina"),("chile","chile"),("Colombia","colombia"),("PERU","peru"),
 ("vzla","venezuela"),("ecuador","ecuador"),("bolivia","bolivia"),("uruguay","uruguay"),
 ("southafrica","south-africa"),("Nigeria","nigeria"),("Kenya","kenya"),("egypt","egypt"),
 ("Morocco","morocco"),("ethiopia","ethiopia"),("ghana","ghana"),("turkey","turkey"),
 ("iran","iran"),("Israel","israel"),("saudiarabia","saudi-arabia"),("dubai","uae"),
 ("qatar","qatar"),("lebanon","lebanon"),("iraq","iraq"),("jordan","jordan"),
 ("Syria","syria"),("afghanistan","afghanistan"),("Kazakhstan","kazakhstan"),("uzbekistan","uzbekistan"),
 ("azerbaijan","azerbaijan"),("armenia","armenia"),("Sakartvelo","georgia"),("mongolia","mongolia"),
 # cities
 ("london","london"),("paris","paris"),("Tokyo","tokyo"),("berlin","berlin"),
 ("moscow","moscow"),("nyc","new-york"),("LosAngeles","los-angeles"),("toronto","toronto"),
 ("sydney","sydney"),("melbourne","melbourne"),("delhi","delhi"),("mumbai","mumbai"),
 ("shanghai","shanghai"),("beijing","beijing"),("istanbul","istanbul"),("bangkok","bangkok"),
 ("jakarta","jakarta"),("Seoul","seoul"),("hongkong","hong-kong-city"),("kyiv","kyiv"),
]
f_red = File("reddit_world_geo.c",
  "Reddit geo/country/city/OSINT subreddit feeds (deterministic /r/<sub>/.rss). "
  "Community signal + local reporting worldwide, via rss_collect.")
for sub, tag in REDDIT:
    sid = uid(f"reddit-{sub}")
    sym = usym(f"reddit_{sub}")
    f_red.add(sym, sid, f"Reddit r/{sub}", f"Reddit r/{sub}",
      "osint", "social", f"https://www.reddit.com/r/{sub}/.rss", "en",
      f'["reddit","social","{tag}","community"]', 1800,
      f"Reddit r/{sub} feed — community-sourced signal ({tag})")

# ---- FILE: curated national outlets / agencies (diverse providers, known RSS)
OUTLETS = [
 ("tass","TASS (Russia)","https://tass.com/rss/v2.xml","en","russia"),
 ("rt-news","RT (Russia)","https://www.rt.com/rss/","en","russia"),
 ("global-times","Global Times (China)","https://www.globaltimes.cn/rss/outbrain.xml","en","china"),
 ("china-daily","China Daily","http://www.chinadaily.com.cn/rss/china_rss.xml","en","china"),
 ("scmp-china2","SCMP China","https://www.scmp.com/rss/4/feed","en","china"),
 ("abc-au","ABC News (Australia)","https://www.abc.net.au/news/feed/51120/rss.xml","en","australia"),
 ("cbc-top","CBC News (Canada)","https://www.cbc.ca/webfeed/rss/rss-topstories","en","canada"),
 ("cbc-world","CBC World","https://www.cbc.ca/webfeed/rss/rss-world","en","canada"),
 ("lemonde-une","Le Monde (France)","https://www.lemonde.fr/rss/une.xml","fr","france"),
 ("lefigaro","Le Figaro (France)","https://www.lefigaro.fr/rss/figaro_actualites.xml","fr","france"),
 ("rfi-fr","RFI (France)","https://www.rfi.fr/fr/rss","fr","france"),
 ("spiegel-intl","Der Spiegel International","https://www.spiegel.de/international/index.rss","en","germany"),
 ("dw-top","Deutsche Welle Top","https://rss.dw.com/rdf/rss-en-top","en","germany"),
 ("elpais-port","El País (Spain)","https://feeds.elpais.com/mrss-s/pages/ep/site/elpais.com/portada","es","spain"),
 ("elmundo","El Mundo (Spain)","https://e00-elmundo.uecdn.es/elmundo/rss/portada.xml","es","spain"),
 ("ansa-it","ANSA (Italy)","https://www.ansa.it/sito/ansait_rss.xml","it","italy"),
 ("repubblica","La Repubblica (Italy)","https://www.repubblica.it/rss/homepage/rss2.0.xml","it","italy"),
 ("dawn-pk","Dawn (Pakistan)","https://www.dawn.com/feeds/home","en","pakistan"),
 ("kyiv-post","Kyiv Post (Ukraine)","https://www.kyivpost.com/feed","en","ukraine"),
 ("pravda-ua","Ukrainska Pravda","https://www.pravda.com.ua/rss/","uk","ukraine"),
 ("anadolu","Anadolu Agency (Turkey)","https://www.aa.com.tr/en/rss/default?cat=guncel","en","turkey"),
 ("hurriyet-en","Hürriyet Daily News","https://www.hurriyetdailynews.com/rss","en","turkey"),
 ("bangkok-post","Bangkok Post","https://www.bangkokpost.com/rss/data/topstories.xml","en","thailand"),
 ("jakarta-post","Jakarta Post","https://www.thejakartapost.com/feed","en","indonesia"),
 ("straits-top","Straits Times Top","https://www.straitstimes.com/news/singapore/rss.xml","en","singapore"),
 ("cna-sg","Channel NewsAsia","https://www.channelnewsasia.com/rssfeeds/8395986","en","singapore"),
 ("manila-times","Manila Times","https://www.manilatimes.net/feed","en","philippines"),
 ("hindustan-times","Hindustan Times (India)","https://www.hindustantimes.com/feeds/rss/world-news/rssfeed.xml","en","india"),
 ("the-hindu-nat","The Hindu National","https://www.thehindu.com/news/national/feeder/default.rss","en","india"),
 ("ndtv","NDTV (India)","https://feeds.feedburner.com/ndtvnews-top-stories","en","india"),
 ("dhaka-tribune","Dhaka Tribune","https://www.dhakatribune.com/feed","en","bangladesh"),
 ("korea-times","Korea Times","https://www.koreatimes.co.kr/www/rss/rss.xml","en","korea"),
 ("japan-times","The Japan Times","https://www.japantimes.co.jp/feed/","en","japan"),
 ("mainichi-en","Mainichi (Japan, EN)","https://mainichi.jp/rss/etc/mainichi-flash.rss","ja","japan"),
 ("taipei-times","Taipei Times","https://www.taipeitimes.com/xml/index.rss","en","taiwan"),
 ("folha-br","Folha de S.Paulo","https://feeds.folha.uol.com.br/emcimadahora/rss091.xml","pt","brazil"),
 ("g1-globo","G1 Globo (Brazil)","https://g1.globo.com/rss/g1/","pt","brazil"),
 ("clarin-ar","Clarín (Argentina)","https://www.clarin.com/rss/lo-ultimo/","es","argentina"),
 ("infobae","Infobae (Argentina)","https://www.infobae.com/feeds/rss/","es","argentina"),
 ("eluniversal-mx","El Universal (Mexico)","https://www.eluniversal.com.mx/rss.xml","es","mexico"),
 ("milenio","Milenio (Mexico)","https://www.milenio.com/rss","es","mexico"),
 ("eltiempo-co","El Tiempo (Colombia)","https://www.eltiempo.com/rss/mundo.xml","es","colombia"),
 ("emol-cl","Emol (Chile)","https://www.emol.com/rss/rss.asp?canal=nacional","es","chile"),
 ("news24-za","News24 (South Africa)","https://feeds.24.com/articles/news24/TopStories/rss","en","south-africa"),
 ("punch-ng","The Punch (Nigeria)","https://punchng.com/feed/","en","nigeria"),
 ("premium-times","Premium Times (Nigeria)","https://www.premiumtimesng.com/feed","en","nigeria"),
 ("nation-ke","Nation (Kenya)","https://nation.africa/kenya/rss","en","kenya"),
 ("ahram-eg","Al-Ahram (Egypt)","https://english.ahram.org.eg/rss/RssHome.aspx","en","egypt"),
 ("arab-news","Arab News (Saudi)","https://www.arabnews.com/rss.xml","en","saudi-arabia"),
 ("gulf-news","Gulf News (UAE)","https://gulfnews.com/rss?generatorType=Section&query=%2Fnews","en","uae"),
 ("the-national-ae","The National (UAE)","https://www.thenationalnews.com/arc/outboundfeeds/rss/","en","uae"),
 ("al-jazeera-ar","Al Jazeera Arabic","https://www.aljazeera.net/aljazeerarss/a7c186be-1baa-4bd4-9d80-a84db769f779/73d0e1b4-532f-45ef-b135-bfdff8b8cab9","ar","qatar"),
 ("times-israel-2","Times of Israel Ops","https://www.timesofisrael.com/feed/","en","israel"),
 ("haaretz-2","Haaretz Headlines","https://www.haaretz.com/srv/htz---all-articles","en","israel"),
 ("tehran-times","Tehran Times (Iran)","https://www.tehrantimes.com/rss","en","iran"),
 ("presstv","Press TV (Iran)","https://www.presstv.ir/rss.xml","en","iran"),
 ("moscow-times-2","Moscow Times Business","https://www.themoscowtimes.com/rss/news","en","russia"),
 ("kommersant","Kommersant (Russia)","https://www.kommersant.ru/RSS/news.xml","ru","russia"),
 ("interfax","Interfax (Russia)","https://www.interfax.ru/rss.asp","ru","russia"),
 ("belta-by","BelTA (Belarus)","https://eng.belta.by/rss","en","belarus"),
 ("astana-times","Astana Times (Kazakhstan)","https://astanatimes.com/feed/","en","kazakhstan"),
 ("rappler-ph","Rappler (Philippines)","https://www.rappler.com/feed/","en","philippines"),
 ("vnexpress","VnExpress (Vietnam)","https://vnexpress.net/rss/tin-moi-nhat.rss","vi","vietnam"),
 ("tuoitre","Tuoi Tre News (Vietnam)","https://tuoitrenews.vn/rss/homepage.rss","en","vietnam"),
 ("nzherald","NZ Herald","https://www.nzherald.co.nz/arc/outboundfeeds/rss/curated/78/","en","new-zealand"),
 ("guardian-au","Guardian Australia","https://www.theguardian.com/australia-news/rss","en","australia"),
 ("smh-au","Sydney Morning Herald","https://www.smh.com.au/rss/feed.xml","en","australia"),
 ("irish-times","The Irish Times","https://www.irishtimes.com/arc/outboundfeeds/feeds/newsdesk/?outputType=xml","en","ireland"),
 ("thelocal-eu","The Local (Europe)","https://www.thelocal.com/feeds/rss.php","en","europe"),
 ("euractiv","EURACTIV","https://www.euractiv.com/feed/","en","europe"),
 ("politico-eu","POLITICO Europe","https://www.politico.eu/feed/","en","europe"),
 ("balkan-insight","Balkan Insight","https://balkaninsight.com/feed/","en","balkans"),
 ("kyiv-independent-2","Kyiv Independent War","https://kyivindependent.com/feed/","en","ukraine"),
 ("notes-poland","Notes from Poland","https://notesfrompoland.com/feed/","en","poland"),
 ("baltic-times","The Baltic Times","https://www.baltictimes.com/rss/","en","baltics"),
 ("swissinfo","SWI swissinfo.ch","https://www.swissinfo.ch/eng/rss","en","switzerland"),
 ("dutch-news","DutchNews.nl","https://www.dutchnews.nl/feed/","en","netherlands"),
 ("brussels-times","The Brussels Times","https://www.brusselstimes.com/feed","en","belgium"),
 ("local-se","The Local Sweden","https://www.thelocal.se/feeds/rss.php","en","sweden"),
 ("ice-news","Iceland Monitor","https://icelandmonitor.mbl.is/rss/","en","iceland"),
 ("greek-reporter","Greek Reporter","https://greekreporter.com/feed/","en","greece"),
 ("daily-sabah","Daily Sabah (Turkey)","https://www.dailysabah.com/rssFeed/homepage","en","turkey"),
 ("caucasus-watch","Caucasus Watch","https://caucasuswatch.de/en/rss.xml","en","caucasus"),
 ("eurasianet","Eurasianet","https://eurasianet.org/rss.xml","en","central-asia"),
 ("the-diplomat-2","The Diplomat Security","https://thediplomat.com/feed/","en","asia"),
 ("nikkei-asia-2","Nikkei Asia Politics","https://asia.nikkei.com/rss/feed/nar","en","asia"),
 ("scmp-china3","SCMP China Diplomacy","https://www.scmp.com/rss/4/feed","en","china"),
 ("caixin","Caixin Global (China)","https://www.caixinglobal.com/rss/economics.xml","en","china"),
 ("indian-express","Indian Express","https://indianexpress.com/feed/","en","india"),
 ("dawn-world","Dawn World","https://www.dawn.com/feeds/world","en","pakistan"),
 ("al-monitor","Al-Monitor (Middle East)","https://www.al-monitor.com/rss","en","middle-east"),
 ("middle-east-eye","Middle East Eye","https://www.middleeasteye.net/rss","en","middle-east"),
 ("mees","Middle East Institute","https://www.mei.edu/rss.xml","en","middle-east"),
 ("africa-report","The Africa Report","https://www.theafricareport.com/feed/","en","africa"),
 ("mg-africa-2","Daily Maverick (SA)","https://www.dailymaverick.co.za/dmrss/","en","south-africa"),
 ("addis-standard","Addis Standard (Ethiopia)","https://addisstandard.com/feed/","en","ethiopia"),
 ("the-east-african","The East African","https://www.theeastafrican.co.ke/rss","en","east-africa"),
 ("mail-guardian-2","Mail & Guardian Africa","https://mg.co.za/feed/","en","south-africa"),
 ("semafor-africa","Semafor Africa","https://www.semafor.com/rss.xml","en","africa"),
 ("americas-quarterly","Americas Quarterly","https://americasquarterly.org/feed/","en","latam"),
 ("mercopress-2","MercoPress South Atlantic","https://en.mercopress.com/rss/","en","latam"),
 ("caracas-chronicles","Caracas Chronicles","https://www.caracaschronicles.com/feed/","en","venezuela"),
 ("tico-times","The Tico Times (Costa Rica)","https://ticotimes.net/feed","en","costa-rica"),
 ("jamaica-gleaner","Jamaica Gleaner","https://jamaica-gleaner.com/feed/rss.xml","en","jamaica"),
 ("thecable-ng","TheCable (Nigeria)","https://www.thecable.ng/feed","en","nigeria"),
 ("nation-thailand","The Nation (Thailand)","https://www.nationthailand.com/rss","en","thailand"),
 ("khmer-times","Khmer Times (Cambodia)","https://www.khmertimeskh.com/feed/","en","cambodia"),
 ("myanmar-now","Myanmar Now","https://myanmar-now.org/en/rss","en","myanmar"),
 ("irrawaddy","The Irrawaddy (Myanmar)","https://www.irrawaddy.com/feed","en","myanmar"),
 ("kathmandu-post","The Kathmandu Post (Nepal)","https://kathmandupost.com/rss","en","nepal"),
 ("daily-mirror-lk","Daily Mirror (Sri Lanka)","https://www.dailymirror.lk/rss","en","sri-lanka"),
 ("fiji-times","The Fiji Times","https://www.fijitimes.com.fj/feed/","en","fiji"),
 ("pina-pacific","Pacific Islands News","https://www.rnz.co.nz/rss/pacific.xml","en","pacific"),
 ("buenosaires-h","Buenos Aires Herald","https://buenosairesherald.com/feed","en","argentina"),
]
f_out1 = File("world_outlets_1.c",
  "Curated national news outlets / agencies worldwide (diverse providers, known "
  "RSS). Provider diversity beyond the aggregators, via rss_collect.")
f_out2 = File("world_outlets_2.c",
  "Curated national news outlets / agencies worldwide, part 2, via rss_collect.")
for i, (oid, name, url, lang, country) in enumerate(OUTLETS):
    sid = uid(oid)
    sym = usym("out_" + oid)
    tgt = f_out1 if i < len(OUTLETS)//2 else f_out2
    tgt.add(sym, sid, name, name, "osint", "news", url, lang,
      f'["news","{country}","national-outlet"]', 3600,
      f"{name} — national/regional press feed ({country})")

# ---- FILE: national CERTs / cyber advisory feeds
CERTS = [
 ("cert-fr","CERT-FR (ANSSI, France)","https://www.cert.ssi.gouv.fr/feed/","fr","france"),
 ("cert-se","CERT-SE (Sweden)","https://www.cert.se/feed","sv","sweden"),
 ("cert-be","CERT.be (Belgium)","https://cert.be/en/rss","en","belgium"),
 ("cert-pl","CERT Polska","https://cert.pl/en/posts/index.xml","en","poland"),
 ("ncsc-nl","NCSC-NL (Netherlands)","https://www.ncsc.nl/rss/actueel","nl","netherlands"),
 ("cert-in","CERT-In (India)","https://www.cert-in.org.in/RSS/CertIn.xml","en","india"),
 ("jpcert-en","JPCERT/CC (English)","https://www.jpcert.or.jp/english/rss/jpcert-en.rdf","en","japan"),
 ("cert-nz","CERT NZ","https://www.cert.govt.nz/individuals/rss/","en","new-zealand"),
 ("acsc-au","ACSC (Australia)","https://www.cyber.gov.au/rss/advisories","en","australia"),
 ("cccs-ca","Canadian Centre for Cyber Security","https://www.cyber.gc.ca/webservice/en/rss/alerts","en","canada"),
 ("cert-at","CERT.at (Austria)","https://www.cert.at/de/warnungen/warnungen.xml","de","austria"),
 ("circl-lu","CIRCL (Luxembourg)","https://www.circl.lu/rss.xml","en","luxembourg"),
 ("cert-ee","CERT-EE (Estonia)","https://www.ria.ee/en/rss.xml","en","estonia"),
 ("ncsc-ch","NCSC Switzerland","https://www.ncsc.admin.ch/ncsc/en/home.rss.html","en","switzerland"),
 ("cert-ee2","RIA Estonia Advisories","https://www.ria.ee/rss.xml","et","estonia"),
 ("cert-si","SI-CERT (Slovenia)","https://www.cert.si/feed/","sl","slovenia"),
 ("cert-fi","NCSC-FI (Finland)","https://www.kyberturvallisuuskeskus.fi/en/rss.xml","en","finland"),
 ("cert-br","CERT.br (Brazil)","https://www.cert.br/rss/certbr-rss.xml","pt","brazil"),
 ("cert-gov-uk","NCSC-UK Reports","https://www.ncsc.gov.uk/api/1/services/v1/report-rss-feed.xml","en","uk"),
 ("cisa-ics","CISA ICS Advisories","https://www.cisa.gov/cybersecurity-advisories/ics-advisories.xml","en","usa"),
 ("krcert","KrCERT/KISA (Korea)","https://www.krcert.or.kr/rss/security.do","ko","korea"),
 ("ncsc-ie","NCSC Ireland","https://www.ncsc.gov.ie/feed/","en","ireland"),
 ("csa-sg","CSA SingCERT","https://www.csa.gov.sg/rss/alerts-advisories","en","singapore"),
 ("cert-es","CCN-CERT (Spain)","https://www.ccn-cert.cni.es/component/obrss/rss-ultimas-vulnerabilidades.feed","es","spain"),
 ("cert-it","CSIRT Italia","https://www.csirt.gov.it/rss","it","italy"),
 ("cert-pt","CERT.PT (Portugal)","https://www.cncs.gov.pt/pt/feed/","pt","portugal"),
 ("cert-no","NSM/NCSC Norway","https://nsm.no/rss/","no","norway"),
 ("cert-dk","CFCS Denmark","https://www.cfcs.dk/da/rss/","da","denmark"),
 ("cert-lt","NKSC Lithuania","https://www.nksc.lt/rss.xml","lt","lithuania"),
 ("cert-ua","CERT-UA (Ukraine)","https://cert.gov.ua/api/articles/rss","uk","ukraine"),
]
f_cert = File("world_cert_advisories.c",
  "National CERT / CSIRT cyber-advisory feeds worldwide (real RSS), via "
  "rss_collect. Complements the JP jpcert/ipa collectors with global coverage.")
for cid, name, url, lang, country in CERTS:
    sid = uid(cid)
    sym = usym("cert_" + cid.replace('-','_'))
    f_cert.add(sym, sid, name, name, "cyber", "cyber", url, lang,
      f'["cyber","cert","advisory","{country}"]', 3600,
      f"{name} — national cyber-security advisory feed")

# ---- FILE: thematic global OSINT feeds (finance/energy/aviation/maritime/health/rights/space)
THEMATIC = [
 ("gcaptain","gCaptain (Maritime)","https://gcaptain.com/feed/","maritime","Global maritime shipping and incident reporting"),
 ("maritime-bulletin","Maritime Bulletin","https://maritimebulletin.net/feed/","maritime","Ship casualties, seizures and maritime security"),
 ("splash247","Splash 247 (Shipping)","https://splash247.com/feed/","maritime","Global shipping industry and vessel news"),
 ("avherald","The Aviation Herald","https://avherald.com/rss/","aviation","Aviation incidents and accidents worldwide"),
 ("flightglobal","FlightGlobal","https://www.flightglobal.com/rss/","aviation","Aviation industry and defense aerospace news"),
 ("spacenews","SpaceNews","https://spacenews.com/feed/","space","Space industry, launches and military space"),
 ("nasaspaceflight","NASASpaceflight","https://www.nasaspaceflight.com/feed/","space","Launch vehicle and orbital activity reporting"),
 ("oilprice","OilPrice.com (Energy)","https://oilprice.com/rss/main","energy","Global oil, gas and energy market intelligence"),
 ("rigzone","Rigzone (Oil & Gas)","https://www.rigzone.com/news/rss/rigzone_latest.aspx","energy","Upstream oil and gas industry news"),
 ("argus-energy","Reccessary Energy","https://www.reccessary.com/en/rss","energy","Asia energy transition and power markets"),
 ("statnews","STAT News (Health)","https://www.statnews.com/feed/","health","Global health, pharma and biotech intelligence"),
 ("cidrap","CIDRAP (Infectious Disease)","https://www.cidrap.umn.edu/rss.xml","health","Infectious-disease outbreak and biosecurity news"),
 ("hrw-news","Human Rights Watch","https://www.hrw.org/rss/news","rights","Human-rights investigations worldwide"),
 ("amnesty","Amnesty International","https://www.amnesty.org/en/rss/","rights","Human-rights abuse reporting and campaigns"),
 ("crisis-group","International Crisis Group","https://www.crisisgroup.org/rss.xml","conflict","Conflict analysis and early-warning briefings"),
 ("acled-blog","ACLED Analysis","https://acleddata.com/feed/","conflict","Armed conflict and political violence analysis"),
 ("small-wars","Small Wars Journal","https://smallwarsjournal.com/rss.xml","conflict","Irregular warfare and counterinsurgency analysis"),
 ("bellingcat-2","Bellingcat Investigations","https://www.bellingcat.com/feed/","osint","Open-source investigation and geolocation"),
 ("intelnews","IntelNews","https://intelnews.org/feed/","intelligence","Intelligence-community and espionage reporting"),
 ("lawfare","Lawfare","https://www.lawfaremedia.org/feed/rss.xml","security","National-security law and policy analysis"),
]
f_them = File("osint_thematic_global.c",
  "Thematic global OSINT feeds — maritime, aviation, space, energy, health, "
  "human-rights, conflict and intelligence domains, via rss_collect.")
for tid, name, url, tag, desc in THEMATIC:
    sid = uid(tid)
    sym = usym("them_" + tid.replace('-','_'))
    coll = "osint"
    f_them.add(sym, sid, name, name, coll, "news", url, "en",
      f'["osint","{tag}","thematic","global"]', 3600, desc)

files = [f_top, f_t1, f_t2, f_mon, f_red, f_out1, f_out2, f_cert, f_them]
total = 0
for f in files:
    f.write(); total += f.n
    print(f"{f.name}: {f.n}")
print("TOTAL:", total)
# dump ids for dedup verification
with open("/tmp/claude-0/-home-user-JapanOSINT/e003d5fe-3fcc-5b3c-b73d-86ef8af090a5/scratchpad/gen_ids.txt","w") as f:
    for i in sorted(used_ids): f.write(i+"\n")
