/* Google News national top-headlines editions (real per-country RSS). One high-yield OSINT news monitor per country worldwide, via rss_collect. */
#include "../../source.h"
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

RSSX(gn_top_us, "gnews-top-us", "Google News — United States", "Google ニュース — アメリカ", "osint", "news",
  "https://news.google.com/rss?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"united-states\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for United States — national news monitor");

RSSX(gn_top_gb, "gnews-top-gb", "Google News — United Kingdom", "Google ニュース — イギリス", "osint", "news",
  "https://news.google.com/rss?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"united-kingdom\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for United Kingdom — national news monitor");

RSSX(gn_top_fr, "gnews-top-fr", "Google News — France", "Google ニュース — フランス", "osint", "news",
  "https://news.google.com/rss?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"france\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for France — national news monitor");

RSSX(gn_top_de, "gnews-top-de", "Google News — Germany", "Google ニュース — ドイツ", "osint", "news",
  "https://news.google.com/rss?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"germany\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Germany — national news monitor");

RSSX(gn_top_it, "gnews-top-it", "Google News — Italy", "Google ニュース — イタリア", "osint", "news",
  "https://news.google.com/rss?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"italy\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Italy — national news monitor");

RSSX(gn_top_es, "gnews-top-es", "Google News — Spain", "Google ニュース — スペイン", "osint", "news",
  "https://news.google.com/rss?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"spain\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Spain — national news monitor");

RSSX(gn_top_jp, "gnews-top-jp", "Google News — Japan", "Google ニュース — 日本", "osint", "news",
  "https://news.google.com/rss?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"japan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Japan — national news monitor");

RSSX(gn_top_kr, "gnews-top-kr", "Google News — South Korea", "Google ニュース — 韓国", "osint", "news",
  "https://news.google.com/rss?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"south-korea\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for South Korea — national news monitor");

RSSX(gn_top_cn, "gnews-top-cn", "Google News — China", "Google ニュース — 中国", "osint", "news",
  "https://news.google.com/rss?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"china\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for China — national news monitor");

RSSX(gn_top_tw, "gnews-top-tw", "Google News — Taiwan", "Google ニュース — 台湾", "osint", "news",
  "https://news.google.com/rss?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"taiwan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Taiwan — national news monitor");

RSSX(gn_top_hk, "gnews-top-hk", "Google News — Hong Kong", "Google ニュース — 香港", "osint", "news",
  "https://news.google.com/rss?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"hong-kong\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Hong Kong — national news monitor");

RSSX(gn_top_in, "gnews-top-in", "Google News — India", "Google ニュース — インド", "osint", "news",
  "https://news.google.com/rss?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"india\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for India — national news monitor");

RSSX(gn_top_ru, "gnews-top-ru", "Google News — Russia", "Google ニュース — ロシア", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"russia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Russia — national news monitor");

RSSX(gn_top_ua, "gnews-top-ua", "Google News — Ukraine", "Google ニュース — ウクライナ", "osint", "news",
  "https://news.google.com/rss?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"ukraine\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Ukraine — national news monitor");

RSSX(gn_top_br, "gnews-top-br", "Google News — Brazil", "Google ニュース — ブラジル", "osint", "news",
  "https://news.google.com/rss?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"brazil\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Brazil — national news monitor");

RSSX(gn_top_mx, "gnews-top-mx", "Google News — Mexico", "Google ニュース — メキシコ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"mexico\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Mexico — national news monitor");

RSSX(gn_top_ar, "gnews-top-ar", "Google News — Argentina", "Google ニュース — アルゼンチン", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"argentina\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Argentina — national news monitor");

RSSX(gn_top_ca, "gnews-top-ca", "Google News — Canada", "Google ニュース — カナダ", "osint", "news",
  "https://news.google.com/rss?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"canada\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Canada — national news monitor");

RSSX(gn_top_au, "gnews-top-au", "Google News — Australia", "Google ニュース — オーストラリア", "osint", "news",
  "https://news.google.com/rss?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"australia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Australia — national news monitor");

RSSX(gn_top_nz, "gnews-top-nz", "Google News — New Zealand", "Google ニュース — ニュージーランド", "osint", "news",
  "https://news.google.com/rss?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"new-zealand\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for New Zealand — national news monitor");

RSSX(gn_top_tr, "gnews-top-tr", "Google News — Turkey", "Google ニュース — トルコ", "osint", "news",
  "https://news.google.com/rss?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"turkey\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Turkey — national news monitor");

RSSX(gn_top_il, "gnews-top-il", "Google News — Israel", "Google ニュース — イスラエル", "osint", "news",
  "https://news.google.com/rss?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"israel\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Israel — national news monitor");

RSSX(gn_top_sa, "gnews-top-sa", "Google News — Saudi Arabia", "Google ニュース — サウジアラビア", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"saudi-arabia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Saudi Arabia — national news monitor");

RSSX(gn_top_ae, "gnews-top-ae", "Google News — United Arab Emirates", "Google ニュース — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"united-arab-emirates\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for United Arab Emirates — national news monitor");

RSSX(gn_top_eg, "gnews-top-eg", "Google News — Egypt", "Google ニュース — エジプト", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"egypt\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Egypt — national news monitor");

RSSX(gn_top_ir, "gnews-top-ir", "Google News — Iran", "Google ニュース — イラン", "osint", "news",
  "https://news.google.com/rss?hl=fa&gl=IR&ceid=IR:fa", "fa", "[\"news\",\"iran\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Iran — national news monitor");

RSSX(gn_top_pk, "gnews-top-pk", "Google News — Pakistan", "Google ニュース — パキスタン", "osint", "news",
  "https://news.google.com/rss?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"pakistan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Pakistan — national news monitor");

RSSX(gn_top_id, "gnews-top-id", "Google News — Indonesia", "Google ニュース — インドネシア", "osint", "news",
  "https://news.google.com/rss?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"indonesia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Indonesia — national news monitor");

RSSX(gn_top_th, "gnews-top-th", "Google News — Thailand", "Google ニュース — タイ", "osint", "news",
  "https://news.google.com/rss?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"thailand\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Thailand — national news monitor");

RSSX(gn_top_vn, "gnews-top-vn", "Google News — Vietnam", "Google ニュース — ベトナム", "osint", "news",
  "https://news.google.com/rss?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"vietnam\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Vietnam — national news monitor");

RSSX(gn_top_ph, "gnews-top-ph", "Google News — Philippines", "Google ニュース — フィリピン", "osint", "news",
  "https://news.google.com/rss?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"philippines\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Philippines — national news monitor");

RSSX(gn_top_sg, "gnews-top-sg", "Google News — Singapore", "Google ニュース — シンガポール", "osint", "news",
  "https://news.google.com/rss?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"singapore\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Singapore — national news monitor");

RSSX(gn_top_my, "gnews-top-my", "Google News — Malaysia", "Google ニュース — マレーシア", "osint", "news",
  "https://news.google.com/rss?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"malaysia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Malaysia — national news monitor");

RSSX(gn_top_nl, "gnews-top-nl", "Google News — Netherlands", "Google ニュース — オランダ", "osint", "news",
  "https://news.google.com/rss?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"netherlands\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Netherlands — national news monitor");

RSSX(gn_top_be, "gnews-top-be", "Google News — Belgium", "Google ニュース — ベルギー", "osint", "news",
  "https://news.google.com/rss?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"belgium\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Belgium — national news monitor");

RSSX(gn_top_ch, "gnews-top-ch", "Google News — Switzerland", "Google ニュース — スイス", "osint", "news",
  "https://news.google.com/rss?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"switzerland\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Switzerland — national news monitor");

RSSX(gn_top_at, "gnews-top-at", "Google News — Austria", "Google ニュース — オーストリア", "osint", "news",
  "https://news.google.com/rss?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"austria\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Austria — national news monitor");

RSSX(gn_top_se, "gnews-top-se", "Google News — Sweden", "Google ニュース — スウェーデン", "osint", "news",
  "https://news.google.com/rss?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"sweden\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Sweden — national news monitor");

RSSX(gn_top_no, "gnews-top-no", "Google News — Norway", "Google ニュース — ノルウェー", "osint", "news",
  "https://news.google.com/rss?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"norway\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Norway — national news monitor");

RSSX(gn_top_dk, "gnews-top-dk", "Google News — Denmark", "Google ニュース — デンマーク", "osint", "news",
  "https://news.google.com/rss?hl=da&gl=DK&ceid=DK:da", "da", "[\"news\",\"denmark\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Denmark — national news monitor");

RSSX(gn_top_fi, "gnews-top-fi", "Google News — Finland", "Google ニュース — フィンランド", "osint", "news",
  "https://news.google.com/rss?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"finland\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Finland — national news monitor");

RSSX(gn_top_pl, "gnews-top-pl", "Google News — Poland", "Google ニュース — ポーランド", "osint", "news",
  "https://news.google.com/rss?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"poland\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Poland — national news monitor");

RSSX(gn_top_cz, "gnews-top-cz", "Google News — Czechia", "Google ニュース — チェコ", "osint", "news",
  "https://news.google.com/rss?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"czechia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Czechia — national news monitor");

RSSX(gn_top_gr, "gnews-top-gr", "Google News — Greece", "Google ニュース — ギリシャ", "osint", "news",
  "https://news.google.com/rss?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"greece\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Greece — national news monitor");

RSSX(gn_top_pt, "gnews-top-pt", "Google News — Portugal", "Google ニュース — ポルトガル", "osint", "news",
  "https://news.google.com/rss?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"portugal\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Portugal — national news monitor");

RSSX(gn_top_ie, "gnews-top-ie", "Google News — Ireland", "Google ニュース — アイルランド", "osint", "news",
  "https://news.google.com/rss?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"ireland\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Ireland — national news monitor");

RSSX(gn_top_za, "gnews-top-za", "Google News — South Africa", "Google ニュース — 南アフリカ", "osint", "news",
  "https://news.google.com/rss?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"south-africa\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for South Africa — national news monitor");

RSSX(gn_top_ng, "gnews-top-ng", "Google News — Nigeria", "Google ニュース — ナイジェリア", "osint", "news",
  "https://news.google.com/rss?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"nigeria\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Nigeria — national news monitor");

RSSX(gn_top_ke, "gnews-top-ke", "Google News — Kenya", "Google ニュース — ケニア", "osint", "news",
  "https://news.google.com/rss?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"kenya\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Kenya — national news monitor");

RSSX(gn_top_co, "gnews-top-co", "Google News — Colombia", "Google ニュース — コロンビア", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"colombia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Colombia — national news monitor");

RSSX(gn_top_cl, "gnews-top-cl", "Google News — Chile", "Google ニュース — チリ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"chile\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Chile — national news monitor");

RSSX(gn_top_pe, "gnews-top-pe", "Google News — Peru", "Google ニュース — ペルー", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"peru\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Peru — national news monitor");

RSSX(gn_top_ro, "gnews-top-ro", "Google News — Romania", "Google ニュース — ルーマニア", "osint", "news",
  "https://news.google.com/rss?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"romania\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Romania — national news monitor");

RSSX(gn_top_hu, "gnews-top-hu", "Google News — Hungary", "Google ニュース — ハンガリー", "osint", "news",
  "https://news.google.com/rss?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"hungary\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Hungary — national news monitor");

RSSX(gn_top_bg, "gnews-top-bg", "Google News — Bulgaria", "Google ニュース — ブルガリア", "osint", "news",
  "https://news.google.com/rss?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"bulgaria\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Bulgaria — national news monitor");

RSSX(gn_top_rs, "gnews-top-rs", "Google News — Serbia", "Google ニュース — セルビア", "osint", "news",
  "https://news.google.com/rss?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"serbia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Serbia — national news monitor");

RSSX(gn_top_hr, "gnews-top-hr", "Google News — Croatia", "Google ニュース — クロアチア", "osint", "news",
  "https://news.google.com/rss?hl=hr&gl=HR&ceid=HR:hr", "hr", "[\"news\",\"croatia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Croatia — national news monitor");

RSSX(gn_top_sk, "gnews-top-sk", "Google News — Slovakia", "Google ニュース — スロバキア", "osint", "news",
  "https://news.google.com/rss?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"slovakia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Slovakia — national news monitor");

RSSX(gn_top_si, "gnews-top-si", "Google News — Slovenia", "Google ニュース — スロベニア", "osint", "news",
  "https://news.google.com/rss?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"slovenia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Slovenia — national news monitor");

RSSX(gn_top_lt, "gnews-top-lt", "Google News — Lithuania", "Google ニュース — リトアニア", "osint", "news",
  "https://news.google.com/rss?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"lithuania\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Lithuania — national news monitor");

RSSX(gn_top_lv, "gnews-top-lv", "Google News — Latvia", "Google ニュース — ラトビア", "osint", "news",
  "https://news.google.com/rss?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"latvia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Latvia — national news monitor");

RSSX(gn_top_ee, "gnews-top-ee", "Google News — Estonia", "Google ニュース — エストニア", "osint", "news",
  "https://news.google.com/rss?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"estonia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Estonia — national news monitor");

RSSX(gn_top_is, "gnews-top-is", "Google News — Iceland", "Google ニュース — アイスランド", "osint", "news",
  "https://news.google.com/rss?hl=is&gl=IS&ceid=IS:is", "is", "[\"news\",\"iceland\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Iceland — national news monitor");

RSSX(gn_top_bd, "gnews-top-bd", "Google News — Bangladesh", "Google ニュース — バングラデシュ", "osint", "news",
  "https://news.google.com/rss?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"bangladesh\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Bangladesh — national news monitor");

RSSX(gn_top_lk, "gnews-top-lk", "Google News — Sri Lanka", "Google ニュース — スリランカ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=LK&ceid=LK:en", "en", "[\"news\",\"sri-lanka\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Sri Lanka — national news monitor");

RSSX(gn_top_np, "gnews-top-np", "Google News — Nepal", "Google ニュース — ネパール", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=NP&ceid=NP:en", "en", "[\"news\",\"nepal\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Nepal — national news monitor");

RSSX(gn_top_mm, "gnews-top-mm", "Google News — Myanmar", "Google ニュース — ミャンマー", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=MM&ceid=MM:en", "en", "[\"news\",\"myanmar\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Myanmar — national news monitor");

RSSX(gn_top_kh, "gnews-top-kh", "Google News — Cambodia", "Google ニュース — カンボジア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=KH&ceid=KH:en", "en", "[\"news\",\"cambodia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Cambodia — national news monitor");

RSSX(gn_top_qa, "gnews-top-qa", "Google News — Qatar", "Google ニュース — カタール", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=QA&ceid=QA:ar", "ar", "[\"news\",\"qatar\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Qatar — national news monitor");

RSSX(gn_top_kw, "gnews-top-kw", "Google News — Kuwait", "Google ニュース — クウェート", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=KW&ceid=KW:ar", "ar", "[\"news\",\"kuwait\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Kuwait — national news monitor");

RSSX(gn_top_iq, "gnews-top-iq", "Google News — Iraq", "Google ニュース — イラク", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=IQ&ceid=IQ:ar", "ar", "[\"news\",\"iraq\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Iraq — national news monitor");

RSSX(gn_top_jo, "gnews-top-jo", "Google News — Jordan", "Google ニュース — ヨルダン", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=JO&ceid=JO:ar", "ar", "[\"news\",\"jordan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Jordan — national news monitor");

RSSX(gn_top_lb, "gnews-top-lb", "Google News — Lebanon", "Google ニュース — レバノン", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"lebanon\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Lebanon — national news monitor");

RSSX(gn_top_ma, "gnews-top-ma", "Google News — Morocco", "Google ニュース — モロッコ", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=MA&ceid=MA:ar", "ar", "[\"news\",\"morocco\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Morocco — national news monitor");

RSSX(gn_top_dz, "gnews-top-dz", "Google News — Algeria", "Google ニュース — アルジェリア", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=DZ&ceid=DZ:ar", "ar", "[\"news\",\"algeria\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Algeria — national news monitor");

RSSX(gn_top_tn, "gnews-top-tn", "Google News — Tunisia", "Google ニュース — チュニジア", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=TN&ceid=TN:ar", "ar", "[\"news\",\"tunisia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Tunisia — national news monitor");

RSSX(gn_top_ve, "gnews-top-ve", "Google News — Venezuela", "Google ニュース — ベネズエラ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"venezuela\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Venezuela — national news monitor");

RSSX(gn_top_ec, "gnews-top-ec", "Google News — Ecuador", "Google ニュース — エクアドル", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=EC&ceid=EC:es", "es", "[\"news\",\"ecuador\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Ecuador — national news monitor");

RSSX(gn_top_bo, "gnews-top-bo", "Google News — Bolivia", "Google ニュース — ボリビア", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"bolivia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Bolivia — national news monitor");

RSSX(gn_top_py, "gnews-top-py", "Google News — Paraguay", "Google ニュース — パラグアイ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=PY&ceid=PY:es", "es", "[\"news\",\"paraguay\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Paraguay — national news monitor");

RSSX(gn_top_uy, "gnews-top-uy", "Google News — Uruguay", "Google ニュース — ウルグアイ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=UY&ceid=UY:es", "es", "[\"news\",\"uruguay\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Uruguay — national news monitor");

RSSX(gn_top_cr, "gnews-top-cr", "Google News — Costa Rica", "Google ニュース — コスタリカ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=CR&ceid=CR:es", "es", "[\"news\",\"costa-rica\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Costa Rica — national news monitor");

RSSX(gn_top_pa, "gnews-top-pa", "Google News — Panama", "Google ニュース — パナマ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=PA&ceid=PA:es", "es", "[\"news\",\"panama\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Panama — national news monitor");

RSSX(gn_top_gt, "gnews-top-gt", "Google News — Guatemala", "Google ニュース — グアテマラ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=GT&ceid=GT:es", "es", "[\"news\",\"guatemala\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Guatemala — national news monitor");

RSSX(gn_top_cu, "gnews-top-cu", "Google News — Cuba", "Google ニュース — キューバ", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"cuba\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Cuba — national news monitor");

RSSX(gn_top_do, "gnews-top-do", "Google News — Dominican Republic", "Google ニュース — ドミニカ共和国", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=DO&ceid=DO:es", "es", "[\"news\",\"dominican-republic\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Dominican Republic — national news monitor");

RSSX(gn_top_kz, "gnews-top-kz", "Google News — Kazakhstan", "Google ニュース — カザフスタン", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=KZ&ceid=KZ:ru", "ru", "[\"news\",\"kazakhstan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Kazakhstan — national news monitor");

RSSX(gn_top_uz, "gnews-top-uz", "Google News — Uzbekistan", "Google ニュース — ウズベキスタン", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=UZ&ceid=UZ:ru", "ru", "[\"news\",\"uzbekistan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Uzbekistan — national news monitor");

RSSX(gn_top_az, "gnews-top-az", "Google News — Azerbaijan", "Google ニュース — アゼルバイジャン", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=AZ&ceid=AZ:ru", "ru", "[\"news\",\"azerbaijan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Azerbaijan — national news monitor");

RSSX(gn_top_am, "gnews-top-am", "Google News — Armenia", "Google ニュース — アルメニア", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=AM&ceid=AM:ru", "ru", "[\"news\",\"armenia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Armenia — national news monitor");

RSSX(gn_top_ge, "gnews-top-ge", "Google News — Georgia", "Google ニュース — ジョージア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=GE&ceid=GE:en", "en", "[\"news\",\"georgia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Georgia — national news monitor");

RSSX(gn_top_by, "gnews-top-by", "Google News — Belarus", "Google ニュース — ベラルーシ", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=BY&ceid=BY:ru", "ru", "[\"news\",\"belarus\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Belarus — national news monitor");

RSSX(gn_top_md, "gnews-top-md", "Google News — Moldova", "Google ニュース — モルドバ", "osint", "news",
  "https://news.google.com/rss?hl=ro&gl=MD&ceid=MD:ro", "ro", "[\"news\",\"moldova\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Moldova — national news monitor");

RSSX(gn_top_mn, "gnews-top-mn", "Google News — Mongolia", "Google ニュース — モンゴル", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=MN&ceid=MN:en", "en", "[\"news\",\"mongolia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Mongolia — national news monitor");

RSSX(gn_top_gh, "gnews-top-gh", "Google News — Ghana", "Google ニュース — ガーナ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=GH&ceid=GH:en", "en", "[\"news\",\"ghana\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Ghana — national news monitor");

RSSX(gn_top_et, "gnews-top-et", "Google News — Ethiopia", "Google ニュース — エチオピア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=ET&ceid=ET:en", "en", "[\"news\",\"ethiopia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Ethiopia — national news monitor");

RSSX(gn_top_tz, "gnews-top-tz", "Google News — Tanzania", "Google ニュース — タンザニア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=TZ&ceid=TZ:en", "en", "[\"news\",\"tanzania\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Tanzania — national news monitor");

RSSX(gn_top_ug, "gnews-top-ug", "Google News — Uganda", "Google ニュース — ウガンダ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=UG&ceid=UG:en", "en", "[\"news\",\"uganda\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Uganda — national news monitor");

RSSX(gn_top_zw, "gnews-top-zw", "Google News — Zimbabwe", "Google ニュース — ジンバブエ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=ZW&ceid=ZW:en", "en", "[\"news\",\"zimbabwe\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Zimbabwe — national news monitor");

RSSX(gn_top_zm, "gnews-top-zm", "Google News — Zambia", "Google ニュース — ザンビア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=ZM&ceid=ZM:en", "en", "[\"news\",\"zambia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Zambia — national news monitor");

RSSX(gn_top_sn, "gnews-top-sn", "Google News — Senegal", "Google ニュース — セネガル", "osint", "news",
  "https://news.google.com/rss?hl=fr&gl=SN&ceid=SN:fr", "fr", "[\"news\",\"senegal\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Senegal — national news monitor");

RSSX(gn_top_ci, "gnews-top-ci", "Google News — Ivory Coast", "Google ニュース — コートジボワール", "osint", "news",
  "https://news.google.com/rss?hl=fr&gl=CI&ceid=CI:fr", "fr", "[\"news\",\"ivory-coast\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Ivory Coast — national news monitor");

RSSX(gn_top_cm, "gnews-top-cm", "Google News — Cameroon", "Google ニュース — カメルーン", "osint", "news",
  "https://news.google.com/rss?hl=fr&gl=CM&ceid=CM:fr", "fr", "[\"news\",\"cameroon\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Cameroon — national news monitor");

RSSX(gn_top_cd, "gnews-top-cd", "Google News — DR Congo", "Google ニュース — コンゴ民主共和国", "osint", "news",
  "https://news.google.com/rss?hl=fr&gl=CD&ceid=CD:fr", "fr", "[\"news\",\"dr-congo\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for DR Congo — national news monitor");

RSSX(gn_top_ao, "gnews-top-ao", "Google News — Angola", "Google ニュース — アンゴラ", "osint", "news",
  "https://news.google.com/rss?hl=pt-PT&gl=AO&ceid=AO:pt", "pt", "[\"news\",\"angola\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Angola — national news monitor");

RSSX(gn_top_mz, "gnews-top-mz", "Google News — Mozambique", "Google ニュース — モザンビーク", "osint", "news",
  "https://news.google.com/rss?hl=pt-PT&gl=MZ&ceid=MZ:pt", "pt", "[\"news\",\"mozambique\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Mozambique — national news monitor");

RSSX(gn_top_rw, "gnews-top-rw", "Google News — Rwanda", "Google ニュース — ルワンダ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=RW&ceid=RW:en", "en", "[\"news\",\"rwanda\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Rwanda — national news monitor");

RSSX(gn_top_bw, "gnews-top-bw", "Google News — Botswana", "Google ニュース — ボツワナ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=BW&ceid=BW:en", "en", "[\"news\",\"botswana\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Botswana — national news monitor");

RSSX(gn_top_na, "gnews-top-na", "Google News — Namibia", "Google ニュース — ナミビア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=NA&ceid=NA:en", "en", "[\"news\",\"namibia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Namibia — national news monitor");

RSSX(gn_top_mw, "gnews-top-mw", "Google News — Malawi", "Google ニュース — マラウイ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=MW&ceid=MW:en", "en", "[\"news\",\"malawi\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Malawi — national news monitor");

RSSX(gn_top_sd, "gnews-top-sd", "Google News — Sudan", "Google ニュース — スーダン", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=SD&ceid=SD:ar", "ar", "[\"news\",\"sudan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Sudan — national news monitor");

RSSX(gn_top_so, "gnews-top-so", "Google News — Somalia", "Google ニュース — ソマリア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=SO&ceid=SO:en", "en", "[\"news\",\"somalia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Somalia — national news monitor");

RSSX(gn_top_ly, "gnews-top-ly", "Google News — Libya", "Google ニュース — リビア", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=LY&ceid=LY:ar", "ar", "[\"news\",\"libya\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Libya — national news monitor");

RSSX(gn_top_ye, "gnews-top-ye", "Google News — Yemen", "Google ニュース — イエメン", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=YE&ceid=YE:ar", "ar", "[\"news\",\"yemen\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Yemen — national news monitor");

RSSX(gn_top_om, "gnews-top-om", "Google News — Oman", "Google ニュース — オマーン", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=OM&ceid=OM:ar", "ar", "[\"news\",\"oman\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Oman — national news monitor");

RSSX(gn_top_bh, "gnews-top-bh", "Google News — Bahrain", "Google ニュース — バーレーン", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=BH&ceid=BH:ar", "ar", "[\"news\",\"bahrain\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Bahrain — national news monitor");

RSSX(gn_top_sy, "gnews-top-sy", "Google News — Syria", "Google ニュース — シリア", "osint", "news",
  "https://news.google.com/rss?hl=ar&gl=SY&ceid=SY:ar", "ar", "[\"news\",\"syria\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Syria — national news monitor");

RSSX(gn_top_af, "gnews-top-af", "Google News — Afghanistan", "Google ニュース — アフガニスタン", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=AF&ceid=AF:en", "en", "[\"news\",\"afghanistan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Afghanistan — national news monitor");

RSSX(gn_top_kg, "gnews-top-kg", "Google News — Kyrgyzstan", "Google ニュース — キルギス", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=KG&ceid=KG:ru", "ru", "[\"news\",\"kyrgyzstan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Kyrgyzstan — national news monitor");

RSSX(gn_top_tj, "gnews-top-tj", "Google News — Tajikistan", "Google ニュース — タジキスタン", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=TJ&ceid=TJ:ru", "ru", "[\"news\",\"tajikistan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Tajikistan — national news monitor");

RSSX(gn_top_tm, "gnews-top-tm", "Google News — Turkmenistan", "Google ニュース — トルクメニスタン", "osint", "news",
  "https://news.google.com/rss?hl=ru&gl=TM&ceid=TM:ru", "ru", "[\"news\",\"turkmenistan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Turkmenistan — national news monitor");

RSSX(gn_top_la, "gnews-top-la", "Google News — Laos", "Google ニュース — ラオス", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=LA&ceid=LA:en", "en", "[\"news\",\"laos\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Laos — national news monitor");

RSSX(gn_top_bn, "gnews-top-bn", "Google News — Brunei", "Google ニュース — ブルネイ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=BN&ceid=BN:en", "en", "[\"news\",\"brunei\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Brunei — national news monitor");

RSSX(gn_top_mv, "gnews-top-mv", "Google News — Maldives", "Google ニュース — モルディブ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=MV&ceid=MV:en", "en", "[\"news\",\"maldives\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Maldives — national news monitor");

RSSX(gn_top_bt, "gnews-top-bt", "Google News — Bhutan", "Google ニュース — ブータン", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=BT&ceid=BT:en", "en", "[\"news\",\"bhutan\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Bhutan — national news monitor");

RSSX(gn_top_mo, "gnews-top-mo", "Google News — Macau", "Google ニュース — マカオ", "osint", "news",
  "https://news.google.com/rss?hl=zh-HK&gl=MO&ceid=MO:zh", "zh", "[\"news\",\"macau\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Macau — national news monitor");

RSSX(gn_top_fj, "gnews-top-fj", "Google News — Fiji", "Google ニュース — フィジー", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=FJ&ceid=FJ:en", "en", "[\"news\",\"fiji\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Fiji — national news monitor");

RSSX(gn_top_pg, "gnews-top-pg", "Google News — Papua New Guinea", "Google ニュース — パプアニューギニア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=PG&ceid=PG:en", "en", "[\"news\",\"papua-new-guinea\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Papua New Guinea — national news monitor");

RSSX(gn_top_hn, "gnews-top-hn", "Google News — Honduras", "Google ニュース — ホンジュラス", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=HN&ceid=HN:es", "es", "[\"news\",\"honduras\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Honduras — national news monitor");

RSSX(gn_top_ni, "gnews-top-ni", "Google News — Nicaragua", "Google ニュース — ニカラグア", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=NI&ceid=NI:es", "es", "[\"news\",\"nicaragua\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Nicaragua — national news monitor");

RSSX(gn_top_sv, "gnews-top-sv", "Google News — El Salvador", "Google ニュース — エルサルバドル", "osint", "news",
  "https://news.google.com/rss?hl=es-419&gl=SV&ceid=SV:es", "es", "[\"news\",\"el-salvador\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for El Salvador — national news monitor");

RSSX(gn_top_jm, "gnews-top-jm", "Google News — Jamaica", "Google ニュース — ジャマイカ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=JM&ceid=JM:en", "en", "[\"news\",\"jamaica\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Jamaica — national news monitor");

RSSX(gn_top_tt, "gnews-top-tt", "Google News — Trinidad and Tobago", "Google ニュース — トリニダード・トバゴ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=TT&ceid=TT:en", "en", "[\"news\",\"trinidad-and-tobago\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Trinidad and Tobago — national news monitor");

RSSX(gn_top_bs, "gnews-top-bs", "Google News — Bahamas", "Google ニュース — バハマ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=BS&ceid=BS:en", "en", "[\"news\",\"bahamas\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Bahamas — national news monitor");

RSSX(gn_top_lu, "gnews-top-lu", "Google News — Luxembourg", "Google ニュース — ルクセンブルク", "osint", "news",
  "https://news.google.com/rss?hl=fr&gl=LU&ceid=LU:fr", "fr", "[\"news\",\"luxembourg\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Luxembourg — national news monitor");

RSSX(gn_top_mt, "gnews-top-mt", "Google News — Malta", "Google ニュース — マルタ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=MT&ceid=MT:en", "en", "[\"news\",\"malta\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Malta — national news monitor");

RSSX(gn_top_cy, "gnews-top-cy", "Google News — Cyprus", "Google ニュース — キプロス", "osint", "news",
  "https://news.google.com/rss?hl=el&gl=CY&ceid=CY:el", "el", "[\"news\",\"cyprus\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Cyprus — national news monitor");

RSSX(gn_top_al, "gnews-top-al", "Google News — Albania", "Google ニュース — アルバニア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=AL&ceid=AL:en", "en", "[\"news\",\"albania\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Albania — national news monitor");

RSSX(gn_top_mk, "gnews-top-mk", "Google News — North Macedonia", "Google ニュース — 北マケドニア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=MK&ceid=MK:en", "en", "[\"news\",\"north-macedonia\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for North Macedonia — national news monitor");

RSSX(gn_top_ba, "gnews-top-ba", "Google News — Bosnia and Herzegovina", "Google ニュース — ボスニア", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=BA&ceid=BA:en", "en", "[\"news\",\"bosnia-and-herzegovina\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Bosnia and Herzegovina — national news monitor");

RSSX(gn_top_me, "gnews-top-me", "Google News — Montenegro", "Google ニュース — モンテネグロ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=ME&ceid=ME:en", "en", "[\"news\",\"montenegro\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Montenegro — national news monitor");

RSSX(gn_top_xk, "gnews-top-xk", "Google News — Kosovo", "Google ニュース — コソボ", "osint", "news",
  "https://news.google.com/rss?hl=en&gl=XK&ceid=XK:en", "en", "[\"news\",\"kosovo\",\"google-news\",\"country-edition\"]", 3600,
  "Google News top headlines edition for Kosovo — national news monitor");
