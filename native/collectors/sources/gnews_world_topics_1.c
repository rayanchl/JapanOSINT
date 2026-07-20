/* Google News per-country topic sections (World/National/Business) — real RSS editions for priority countries, via rss_collect. */
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

RSSX(gn_us_world, "gnews-us-world", "Google News World — United States", "Google World — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"world\",\"united-states\",\"google-news\"]", 3600,
  "Google News World section for United States");

RSSX(gn_us_nation, "gnews-us-nation", "Google News National — United States", "Google National — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"nation\",\"united-states\",\"google-news\"]", 3600,
  "Google News National section for United States");

RSSX(gn_us_business, "gnews-us-business", "Google News Business — United States", "Google Business — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"business\",\"united-states\",\"google-news\"]", 3600,
  "Google News Business section for United States");

RSSX(gn_gb_world, "gnews-gb-world", "Google News World — United Kingdom", "Google World — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"world\",\"united-kingdom\",\"google-news\"]", 3600,
  "Google News World section for United Kingdom");

RSSX(gn_gb_nation, "gnews-gb-nation", "Google News National — United Kingdom", "Google National — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"nation\",\"united-kingdom\",\"google-news\"]", 3600,
  "Google News National section for United Kingdom");

RSSX(gn_gb_business, "gnews-gb-business", "Google News Business — United Kingdom", "Google Business — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"business\",\"united-kingdom\",\"google-news\"]", 3600,
  "Google News Business section for United Kingdom");

RSSX(gn_fr_world, "gnews-fr-world", "Google News World — France", "Google World — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"world\",\"france\",\"google-news\"]", 3600,
  "Google News World section for France");

RSSX(gn_fr_nation, "gnews-fr-nation", "Google News National — France", "Google National — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"nation\",\"france\",\"google-news\"]", 3600,
  "Google News National section for France");

RSSX(gn_fr_business, "gnews-fr-business", "Google News Business — France", "Google Business — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"business\",\"france\",\"google-news\"]", 3600,
  "Google News Business section for France");

RSSX(gn_de_world, "gnews-de-world", "Google News World — Germany", "Google World — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"world\",\"germany\",\"google-news\"]", 3600,
  "Google News World section for Germany");

RSSX(gn_de_nation, "gnews-de-nation", "Google News National — Germany", "Google National — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"nation\",\"germany\",\"google-news\"]", 3600,
  "Google News National section for Germany");

RSSX(gn_de_business, "gnews-de-business", "Google News Business — Germany", "Google Business — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"business\",\"germany\",\"google-news\"]", 3600,
  "Google News Business section for Germany");

RSSX(gn_it_world, "gnews-it-world", "Google News World — Italy", "Google World — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"world\",\"italy\",\"google-news\"]", 3600,
  "Google News World section for Italy");

RSSX(gn_it_nation, "gnews-it-nation", "Google News National — Italy", "Google National — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"nation\",\"italy\",\"google-news\"]", 3600,
  "Google News National section for Italy");

RSSX(gn_it_business, "gnews-it-business", "Google News Business — Italy", "Google Business — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"business\",\"italy\",\"google-news\"]", 3600,
  "Google News Business section for Italy");

RSSX(gn_es_world, "gnews-es-world", "Google News World — Spain", "Google World — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"world\",\"spain\",\"google-news\"]", 3600,
  "Google News World section for Spain");

RSSX(gn_es_nation, "gnews-es-nation", "Google News National — Spain", "Google National — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"nation\",\"spain\",\"google-news\"]", 3600,
  "Google News National section for Spain");

RSSX(gn_es_business, "gnews-es-business", "Google News Business — Spain", "Google Business — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"business\",\"spain\",\"google-news\"]", 3600,
  "Google News Business section for Spain");

RSSX(gn_jp_world, "gnews-jp-world", "Google News World — Japan", "Google World — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"world\",\"japan\",\"google-news\"]", 3600,
  "Google News World section for Japan");

RSSX(gn_jp_nation, "gnews-jp-nation", "Google News National — Japan", "Google National — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"nation\",\"japan\",\"google-news\"]", 3600,
  "Google News National section for Japan");

RSSX(gn_jp_business, "gnews-jp-business", "Google News Business — Japan", "Google Business — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"business\",\"japan\",\"google-news\"]", 3600,
  "Google News Business section for Japan");

RSSX(gn_kr_world, "gnews-kr-world", "Google News World — South Korea", "Google World — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"world\",\"south-korea\",\"google-news\"]", 3600,
  "Google News World section for South Korea");

RSSX(gn_kr_nation, "gnews-kr-nation", "Google News National — South Korea", "Google National — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"nation\",\"south-korea\",\"google-news\"]", 3600,
  "Google News National section for South Korea");

RSSX(gn_kr_business, "gnews-kr-business", "Google News Business — South Korea", "Google Business — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"business\",\"south-korea\",\"google-news\"]", 3600,
  "Google News Business section for South Korea");

RSSX(gn_cn_world, "gnews-cn-world", "Google News World — China", "Google World — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"world\",\"china\",\"google-news\"]", 3600,
  "Google News World section for China");

RSSX(gn_cn_nation, "gnews-cn-nation", "Google News National — China", "Google National — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"nation\",\"china\",\"google-news\"]", 3600,
  "Google News National section for China");

RSSX(gn_cn_business, "gnews-cn-business", "Google News Business — China", "Google Business — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"business\",\"china\",\"google-news\"]", 3600,
  "Google News Business section for China");

RSSX(gn_tw_world, "gnews-tw-world", "Google News World — Taiwan", "Google World — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"world\",\"taiwan\",\"google-news\"]", 3600,
  "Google News World section for Taiwan");

RSSX(gn_tw_nation, "gnews-tw-nation", "Google News National — Taiwan", "Google National — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"nation\",\"taiwan\",\"google-news\"]", 3600,
  "Google News National section for Taiwan");

RSSX(gn_tw_business, "gnews-tw-business", "Google News Business — Taiwan", "Google Business — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"business\",\"taiwan\",\"google-news\"]", 3600,
  "Google News Business section for Taiwan");

RSSX(gn_hk_world, "gnews-hk-world", "Google News World — Hong Kong", "Google World — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"world\",\"hong-kong\",\"google-news\"]", 3600,
  "Google News World section for Hong Kong");

RSSX(gn_hk_nation, "gnews-hk-nation", "Google News National — Hong Kong", "Google National — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"nation\",\"hong-kong\",\"google-news\"]", 3600,
  "Google News National section for Hong Kong");

RSSX(gn_hk_business, "gnews-hk-business", "Google News Business — Hong Kong", "Google Business — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"business\",\"hong-kong\",\"google-news\"]", 3600,
  "Google News Business section for Hong Kong");

RSSX(gn_in_world, "gnews-in-world", "Google News World — India", "Google World — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"world\",\"india\",\"google-news\"]", 3600,
  "Google News World section for India");

RSSX(gn_in_nation, "gnews-in-nation", "Google News National — India", "Google National — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"nation\",\"india\",\"google-news\"]", 3600,
  "Google News National section for India");

RSSX(gn_in_business, "gnews-in-business", "Google News Business — India", "Google Business — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"business\",\"india\",\"google-news\"]", 3600,
  "Google News Business section for India");

RSSX(gn_ru_world, "gnews-ru-world", "Google News World — Russia", "Google World — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"world\",\"russia\",\"google-news\"]", 3600,
  "Google News World section for Russia");

RSSX(gn_ru_nation, "gnews-ru-nation", "Google News National — Russia", "Google National — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"nation\",\"russia\",\"google-news\"]", 3600,
  "Google News National section for Russia");

RSSX(gn_ru_business, "gnews-ru-business", "Google News Business — Russia", "Google Business — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"business\",\"russia\",\"google-news\"]", 3600,
  "Google News Business section for Russia");

RSSX(gn_ua_world, "gnews-ua-world", "Google News World — Ukraine", "Google World — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"world\",\"ukraine\",\"google-news\"]", 3600,
  "Google News World section for Ukraine");

RSSX(gn_ua_nation, "gnews-ua-nation", "Google News National — Ukraine", "Google National — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"nation\",\"ukraine\",\"google-news\"]", 3600,
  "Google News National section for Ukraine");

RSSX(gn_ua_business, "gnews-ua-business", "Google News Business — Ukraine", "Google Business — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"business\",\"ukraine\",\"google-news\"]", 3600,
  "Google News Business section for Ukraine");

RSSX(gn_br_world, "gnews-br-world", "Google News World — Brazil", "Google World — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"world\",\"brazil\",\"google-news\"]", 3600,
  "Google News World section for Brazil");

RSSX(gn_br_nation, "gnews-br-nation", "Google News National — Brazil", "Google National — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"nation\",\"brazil\",\"google-news\"]", 3600,
  "Google News National section for Brazil");

RSSX(gn_br_business, "gnews-br-business", "Google News Business — Brazil", "Google Business — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"business\",\"brazil\",\"google-news\"]", 3600,
  "Google News Business section for Brazil");

RSSX(gn_mx_world, "gnews-mx-world", "Google News World — Mexico", "Google World — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"world\",\"mexico\",\"google-news\"]", 3600,
  "Google News World section for Mexico");

RSSX(gn_mx_nation, "gnews-mx-nation", "Google News National — Mexico", "Google National — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"nation\",\"mexico\",\"google-news\"]", 3600,
  "Google News National section for Mexico");

RSSX(gn_mx_business, "gnews-mx-business", "Google News Business — Mexico", "Google Business — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"business\",\"mexico\",\"google-news\"]", 3600,
  "Google News Business section for Mexico");

RSSX(gn_ar_world, "gnews-ar-world", "Google News World — Argentina", "Google World — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"world\",\"argentina\",\"google-news\"]", 3600,
  "Google News World section for Argentina");

RSSX(gn_ar_nation, "gnews-ar-nation", "Google News National — Argentina", "Google National — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"nation\",\"argentina\",\"google-news\"]", 3600,
  "Google News National section for Argentina");

RSSX(gn_ar_business, "gnews-ar-business", "Google News Business — Argentina", "Google Business — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"business\",\"argentina\",\"google-news\"]", 3600,
  "Google News Business section for Argentina");

RSSX(gn_ca_world, "gnews-ca-world", "Google News World — Canada", "Google World — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"world\",\"canada\",\"google-news\"]", 3600,
  "Google News World section for Canada");

RSSX(gn_ca_nation, "gnews-ca-nation", "Google News National — Canada", "Google National — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"nation\",\"canada\",\"google-news\"]", 3600,
  "Google News National section for Canada");

RSSX(gn_ca_business, "gnews-ca-business", "Google News Business — Canada", "Google Business — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"business\",\"canada\",\"google-news\"]", 3600,
  "Google News Business section for Canada");

RSSX(gn_au_world, "gnews-au-world", "Google News World — Australia", "Google World — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"world\",\"australia\",\"google-news\"]", 3600,
  "Google News World section for Australia");

RSSX(gn_au_nation, "gnews-au-nation", "Google News National — Australia", "Google National — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"nation\",\"australia\",\"google-news\"]", 3600,
  "Google News National section for Australia");

RSSX(gn_au_business, "gnews-au-business", "Google News Business — Australia", "Google Business — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"business\",\"australia\",\"google-news\"]", 3600,
  "Google News Business section for Australia");

RSSX(gn_nz_world, "gnews-nz-world", "Google News World — New Zealand", "Google World — ニュージーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"world\",\"new-zealand\",\"google-news\"]", 3600,
  "Google News World section for New Zealand");

RSSX(gn_nz_nation, "gnews-nz-nation", "Google News National — New Zealand", "Google National — ニュージーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"nation\",\"new-zealand\",\"google-news\"]", 3600,
  "Google News National section for New Zealand");

RSSX(gn_nz_business, "gnews-nz-business", "Google News Business — New Zealand", "Google Business — ニュージーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"business\",\"new-zealand\",\"google-news\"]", 3600,
  "Google News Business section for New Zealand");

RSSX(gn_tr_world, "gnews-tr-world", "Google News World — Turkey", "Google World — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"world\",\"turkey\",\"google-news\"]", 3600,
  "Google News World section for Turkey");

RSSX(gn_tr_nation, "gnews-tr-nation", "Google News National — Turkey", "Google National — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"nation\",\"turkey\",\"google-news\"]", 3600,
  "Google News National section for Turkey");

RSSX(gn_tr_business, "gnews-tr-business", "Google News Business — Turkey", "Google Business — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"business\",\"turkey\",\"google-news\"]", 3600,
  "Google News Business section for Turkey");

RSSX(gn_il_world, "gnews-il-world", "Google News World — Israel", "Google World — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"world\",\"israel\",\"google-news\"]", 3600,
  "Google News World section for Israel");

RSSX(gn_il_nation, "gnews-il-nation", "Google News National — Israel", "Google National — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"nation\",\"israel\",\"google-news\"]", 3600,
  "Google News National section for Israel");

RSSX(gn_il_business, "gnews-il-business", "Google News Business — Israel", "Google Business — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"business\",\"israel\",\"google-news\"]", 3600,
  "Google News Business section for Israel");

RSSX(gn_sa_world, "gnews-sa-world", "Google News World — Saudi Arabia", "Google World — サウジアラビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"world\",\"saudi-arabia\",\"google-news\"]", 3600,
  "Google News World section for Saudi Arabia");

RSSX(gn_sa_nation, "gnews-sa-nation", "Google News National — Saudi Arabia", "Google National — サウジアラビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"nation\",\"saudi-arabia\",\"google-news\"]", 3600,
  "Google News National section for Saudi Arabia");

RSSX(gn_sa_business, "gnews-sa-business", "Google News Business — Saudi Arabia", "Google Business — サウジアラビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"business\",\"saudi-arabia\",\"google-news\"]", 3600,
  "Google News Business section for Saudi Arabia");

RSSX(gn_ae_world, "gnews-ae-world", "Google News World — United Arab Emirates", "Google World — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"world\",\"united-arab-emirates\",\"google-news\"]", 3600,
  "Google News World section for United Arab Emirates");

RSSX(gn_ae_nation, "gnews-ae-nation", "Google News National — United Arab Emirates", "Google National — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"nation\",\"united-arab-emirates\",\"google-news\"]", 3600,
  "Google News National section for United Arab Emirates");

RSSX(gn_ae_business, "gnews-ae-business", "Google News Business — United Arab Emirates", "Google Business — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"business\",\"united-arab-emirates\",\"google-news\"]", 3600,
  "Google News Business section for United Arab Emirates");

RSSX(gn_eg_world, "gnews-eg-world", "Google News World — Egypt", "Google World — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"world\",\"egypt\",\"google-news\"]", 3600,
  "Google News World section for Egypt");

RSSX(gn_eg_nation, "gnews-eg-nation", "Google News National — Egypt", "Google National — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"nation\",\"egypt\",\"google-news\"]", 3600,
  "Google News National section for Egypt");

RSSX(gn_eg_business, "gnews-eg-business", "Google News Business — Egypt", "Google Business — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"business\",\"egypt\",\"google-news\"]", 3600,
  "Google News Business section for Egypt");

RSSX(gn_ir_world, "gnews-ir-world", "Google News World — Iran", "Google World — イラン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=fa&gl=IR&ceid=IR:fa", "fa", "[\"news\",\"world\",\"iran\",\"google-news\"]", 3600,
  "Google News World section for Iran");

RSSX(gn_ir_nation, "gnews-ir-nation", "Google News National — Iran", "Google National — イラン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=fa&gl=IR&ceid=IR:fa", "fa", "[\"news\",\"nation\",\"iran\",\"google-news\"]", 3600,
  "Google News National section for Iran");

RSSX(gn_ir_business, "gnews-ir-business", "Google News Business — Iran", "Google Business — イラン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=fa&gl=IR&ceid=IR:fa", "fa", "[\"news\",\"business\",\"iran\",\"google-news\"]", 3600,
  "Google News Business section for Iran");

RSSX(gn_pk_world, "gnews-pk-world", "Google News World — Pakistan", "Google World — パキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"world\",\"pakistan\",\"google-news\"]", 3600,
  "Google News World section for Pakistan");

RSSX(gn_pk_nation, "gnews-pk-nation", "Google News National — Pakistan", "Google National — パキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"nation\",\"pakistan\",\"google-news\"]", 3600,
  "Google News National section for Pakistan");

RSSX(gn_pk_business, "gnews-pk-business", "Google News Business — Pakistan", "Google Business — パキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"business\",\"pakistan\",\"google-news\"]", 3600,
  "Google News Business section for Pakistan");

RSSX(gn_id_world, "gnews-id-world", "Google News World — Indonesia", "Google World — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"world\",\"indonesia\",\"google-news\"]", 3600,
  "Google News World section for Indonesia");

RSSX(gn_id_nation, "gnews-id-nation", "Google News National — Indonesia", "Google National — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"nation\",\"indonesia\",\"google-news\"]", 3600,
  "Google News National section for Indonesia");

RSSX(gn_id_business, "gnews-id-business", "Google News Business — Indonesia", "Google Business — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"business\",\"indonesia\",\"google-news\"]", 3600,
  "Google News Business section for Indonesia");

RSSX(gn_th_world, "gnews-th-world", "Google News World — Thailand", "Google World — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"world\",\"thailand\",\"google-news\"]", 3600,
  "Google News World section for Thailand");

RSSX(gn_th_nation, "gnews-th-nation", "Google News National — Thailand", "Google National — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"nation\",\"thailand\",\"google-news\"]", 3600,
  "Google News National section for Thailand");

RSSX(gn_th_business, "gnews-th-business", "Google News Business — Thailand", "Google Business — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"business\",\"thailand\",\"google-news\"]", 3600,
  "Google News Business section for Thailand");

RSSX(gn_vn_world, "gnews-vn-world", "Google News World — Vietnam", "Google World — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"world\",\"vietnam\",\"google-news\"]", 3600,
  "Google News World section for Vietnam");

RSSX(gn_vn_nation, "gnews-vn-nation", "Google News National — Vietnam", "Google National — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"nation\",\"vietnam\",\"google-news\"]", 3600,
  "Google News National section for Vietnam");

RSSX(gn_vn_business, "gnews-vn-business", "Google News Business — Vietnam", "Google Business — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"business\",\"vietnam\",\"google-news\"]", 3600,
  "Google News Business section for Vietnam");

RSSX(gn_ph_world, "gnews-ph-world", "Google News World — Philippines", "Google World — フィリピン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"world\",\"philippines\",\"google-news\"]", 3600,
  "Google News World section for Philippines");

RSSX(gn_ph_nation, "gnews-ph-nation", "Google News National — Philippines", "Google National — フィリピン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"nation\",\"philippines\",\"google-news\"]", 3600,
  "Google News National section for Philippines");

RSSX(gn_ph_business, "gnews-ph-business", "Google News Business — Philippines", "Google Business — フィリピン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"business\",\"philippines\",\"google-news\"]", 3600,
  "Google News Business section for Philippines");

RSSX(gn_sg_world, "gnews-sg-world", "Google News World — Singapore", "Google World — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"world\",\"singapore\",\"google-news\"]", 3600,
  "Google News World section for Singapore");

RSSX(gn_sg_nation, "gnews-sg-nation", "Google News National — Singapore", "Google National — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"nation\",\"singapore\",\"google-news\"]", 3600,
  "Google News National section for Singapore");

RSSX(gn_sg_business, "gnews-sg-business", "Google News Business — Singapore", "Google Business — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"business\",\"singapore\",\"google-news\"]", 3600,
  "Google News Business section for Singapore");

RSSX(gn_my_world, "gnews-my-world", "Google News World — Malaysia", "Google World — マレーシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"world\",\"malaysia\",\"google-news\"]", 3600,
  "Google News World section for Malaysia");

RSSX(gn_my_nation, "gnews-my-nation", "Google News National — Malaysia", "Google National — マレーシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"nation\",\"malaysia\",\"google-news\"]", 3600,
  "Google News National section for Malaysia");

RSSX(gn_my_business, "gnews-my-business", "Google News Business — Malaysia", "Google Business — マレーシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"business\",\"malaysia\",\"google-news\"]", 3600,
  "Google News Business section for Malaysia");

RSSX(gn_nl_world, "gnews-nl-world", "Google News World — Netherlands", "Google World — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"world\",\"netherlands\",\"google-news\"]", 3600,
  "Google News World section for Netherlands");

RSSX(gn_nl_nation, "gnews-nl-nation", "Google News National — Netherlands", "Google National — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"nation\",\"netherlands\",\"google-news\"]", 3600,
  "Google News National section for Netherlands");

RSSX(gn_nl_business, "gnews-nl-business", "Google News Business — Netherlands", "Google Business — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"business\",\"netherlands\",\"google-news\"]", 3600,
  "Google News Business section for Netherlands");

RSSX(gn_be_world, "gnews-be-world", "Google News World — Belgium", "Google World — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"world\",\"belgium\",\"google-news\"]", 3600,
  "Google News World section for Belgium");

RSSX(gn_be_nation, "gnews-be-nation", "Google News National — Belgium", "Google National — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"nation\",\"belgium\",\"google-news\"]", 3600,
  "Google News National section for Belgium");

RSSX(gn_be_business, "gnews-be-business", "Google News Business — Belgium", "Google Business — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"business\",\"belgium\",\"google-news\"]", 3600,
  "Google News Business section for Belgium");

RSSX(gn_ch_world, "gnews-ch-world", "Google News World — Switzerland", "Google World — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"world\",\"switzerland\",\"google-news\"]", 3600,
  "Google News World section for Switzerland");

RSSX(gn_ch_nation, "gnews-ch-nation", "Google News National — Switzerland", "Google National — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"nation\",\"switzerland\",\"google-news\"]", 3600,
  "Google News National section for Switzerland");

RSSX(gn_ch_business, "gnews-ch-business", "Google News Business — Switzerland", "Google Business — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"business\",\"switzerland\",\"google-news\"]", 3600,
  "Google News Business section for Switzerland");

RSSX(gn_at_world, "gnews-at-world", "Google News World — Austria", "Google World — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"world\",\"austria\",\"google-news\"]", 3600,
  "Google News World section for Austria");

RSSX(gn_at_nation, "gnews-at-nation", "Google News National — Austria", "Google National — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"nation\",\"austria\",\"google-news\"]", 3600,
  "Google News National section for Austria");

RSSX(gn_at_business, "gnews-at-business", "Google News Business — Austria", "Google Business — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"business\",\"austria\",\"google-news\"]", 3600,
  "Google News Business section for Austria");

RSSX(gn_se_world, "gnews-se-world", "Google News World — Sweden", "Google World — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"world\",\"sweden\",\"google-news\"]", 3600,
  "Google News World section for Sweden");

RSSX(gn_se_nation, "gnews-se-nation", "Google News National — Sweden", "Google National — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"nation\",\"sweden\",\"google-news\"]", 3600,
  "Google News National section for Sweden");

RSSX(gn_se_business, "gnews-se-business", "Google News Business — Sweden", "Google Business — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"business\",\"sweden\",\"google-news\"]", 3600,
  "Google News Business section for Sweden");

RSSX(gn_no_world, "gnews-no-world", "Google News World — Norway", "Google World — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"world\",\"norway\",\"google-news\"]", 3600,
  "Google News World section for Norway");

RSSX(gn_no_nation, "gnews-no-nation", "Google News National — Norway", "Google National — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"nation\",\"norway\",\"google-news\"]", 3600,
  "Google News National section for Norway");

RSSX(gn_no_business, "gnews-no-business", "Google News Business — Norway", "Google Business — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"business\",\"norway\",\"google-news\"]", 3600,
  "Google News Business section for Norway");

RSSX(gn_dk_world, "gnews-dk-world", "Google News World — Denmark", "Google World — デンマーク", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=da&gl=DK&ceid=DK:da", "da", "[\"news\",\"world\",\"denmark\",\"google-news\"]", 3600,
  "Google News World section for Denmark");

RSSX(gn_dk_nation, "gnews-dk-nation", "Google News National — Denmark", "Google National — デンマーク", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=da&gl=DK&ceid=DK:da", "da", "[\"news\",\"nation\",\"denmark\",\"google-news\"]", 3600,
  "Google News National section for Denmark");

RSSX(gn_dk_business, "gnews-dk-business", "Google News Business — Denmark", "Google Business — デンマーク", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=da&gl=DK&ceid=DK:da", "da", "[\"news\",\"business\",\"denmark\",\"google-news\"]", 3600,
  "Google News Business section for Denmark");

RSSX(gn_fi_world, "gnews-fi-world", "Google News World — Finland", "Google World — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"world\",\"finland\",\"google-news\"]", 3600,
  "Google News World section for Finland");

RSSX(gn_fi_nation, "gnews-fi-nation", "Google News National — Finland", "Google National — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"nation\",\"finland\",\"google-news\"]", 3600,
  "Google News National section for Finland");

RSSX(gn_fi_business, "gnews-fi-business", "Google News Business — Finland", "Google Business — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"business\",\"finland\",\"google-news\"]", 3600,
  "Google News Business section for Finland");

RSSX(gn_pl_world, "gnews-pl-world", "Google News World — Poland", "Google World — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"world\",\"poland\",\"google-news\"]", 3600,
  "Google News World section for Poland");

RSSX(gn_pl_nation, "gnews-pl-nation", "Google News National — Poland", "Google National — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"nation\",\"poland\",\"google-news\"]", 3600,
  "Google News National section for Poland");

RSSX(gn_pl_business, "gnews-pl-business", "Google News Business — Poland", "Google Business — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"business\",\"poland\",\"google-news\"]", 3600,
  "Google News Business section for Poland");

RSSX(gn_cz_world, "gnews-cz-world", "Google News World — Czechia", "Google World — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"world\",\"czechia\",\"google-news\"]", 3600,
  "Google News World section for Czechia");

RSSX(gn_cz_nation, "gnews-cz-nation", "Google News National — Czechia", "Google National — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"nation\",\"czechia\",\"google-news\"]", 3600,
  "Google News National section for Czechia");

RSSX(gn_cz_business, "gnews-cz-business", "Google News Business — Czechia", "Google Business — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"business\",\"czechia\",\"google-news\"]", 3600,
  "Google News Business section for Czechia");

RSSX(gn_gr_world, "gnews-gr-world", "Google News World — Greece", "Google World — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"world\",\"greece\",\"google-news\"]", 3600,
  "Google News World section for Greece");

RSSX(gn_gr_nation, "gnews-gr-nation", "Google News National — Greece", "Google National — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"nation\",\"greece\",\"google-news\"]", 3600,
  "Google News National section for Greece");

RSSX(gn_gr_business, "gnews-gr-business", "Google News Business — Greece", "Google Business — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"business\",\"greece\",\"google-news\"]", 3600,
  "Google News Business section for Greece");

RSSX(gn_pt_world, "gnews-pt-world", "Google News World — Portugal", "Google World — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"world\",\"portugal\",\"google-news\"]", 3600,
  "Google News World section for Portugal");

RSSX(gn_pt_nation, "gnews-pt-nation", "Google News National — Portugal", "Google National — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"nation\",\"portugal\",\"google-news\"]", 3600,
  "Google News National section for Portugal");

RSSX(gn_pt_business, "gnews-pt-business", "Google News Business — Portugal", "Google Business — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"business\",\"portugal\",\"google-news\"]", 3600,
  "Google News Business section for Portugal");

RSSX(gn_ie_world, "gnews-ie-world", "Google News World — Ireland", "Google World — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"world\",\"ireland\",\"google-news\"]", 3600,
  "Google News World section for Ireland");

RSSX(gn_ie_nation, "gnews-ie-nation", "Google News National — Ireland", "Google National — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"nation\",\"ireland\",\"google-news\"]", 3600,
  "Google News National section for Ireland");

RSSX(gn_ie_business, "gnews-ie-business", "Google News Business — Ireland", "Google Business — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"business\",\"ireland\",\"google-news\"]", 3600,
  "Google News Business section for Ireland");

RSSX(gn_za_world, "gnews-za-world", "Google News World — South Africa", "Google World — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"world\",\"south-africa\",\"google-news\"]", 3600,
  "Google News World section for South Africa");

RSSX(gn_za_nation, "gnews-za-nation", "Google News National — South Africa", "Google National — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"nation\",\"south-africa\",\"google-news\"]", 3600,
  "Google News National section for South Africa");

RSSX(gn_za_business, "gnews-za-business", "Google News Business — South Africa", "Google Business — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"business\",\"south-africa\",\"google-news\"]", 3600,
  "Google News Business section for South Africa");

RSSX(gn_ng_world, "gnews-ng-world", "Google News World — Nigeria", "Google World — ナイジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"world\",\"nigeria\",\"google-news\"]", 3600,
  "Google News World section for Nigeria");

RSSX(gn_ng_nation, "gnews-ng-nation", "Google News National — Nigeria", "Google National — ナイジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"nation\",\"nigeria\",\"google-news\"]", 3600,
  "Google News National section for Nigeria");

RSSX(gn_ng_business, "gnews-ng-business", "Google News Business — Nigeria", "Google Business — ナイジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"business\",\"nigeria\",\"google-news\"]", 3600,
  "Google News Business section for Nigeria");

RSSX(gn_ke_world, "gnews-ke-world", "Google News World — Kenya", "Google World — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"world\",\"kenya\",\"google-news\"]", 3600,
  "Google News World section for Kenya");

RSSX(gn_ke_nation, "gnews-ke-nation", "Google News National — Kenya", "Google National — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"nation\",\"kenya\",\"google-news\"]", 3600,
  "Google News National section for Kenya");

RSSX(gn_ke_business, "gnews-ke-business", "Google News Business — Kenya", "Google Business — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"business\",\"kenya\",\"google-news\"]", 3600,
  "Google News Business section for Kenya");

RSSX(gn_co_world, "gnews-co-world", "Google News World — Colombia", "Google World — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"world\",\"colombia\",\"google-news\"]", 3600,
  "Google News World section for Colombia");

RSSX(gn_co_nation, "gnews-co-nation", "Google News National — Colombia", "Google National — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"nation\",\"colombia\",\"google-news\"]", 3600,
  "Google News National section for Colombia");

RSSX(gn_co_business, "gnews-co-business", "Google News Business — Colombia", "Google Business — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"business\",\"colombia\",\"google-news\"]", 3600,
  "Google News Business section for Colombia");

RSSX(gn_cl_world, "gnews-cl-world", "Google News World — Chile", "Google World — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"world\",\"chile\",\"google-news\"]", 3600,
  "Google News World section for Chile");

RSSX(gn_cl_nation, "gnews-cl-nation", "Google News National — Chile", "Google National — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"nation\",\"chile\",\"google-news\"]", 3600,
  "Google News National section for Chile");

RSSX(gn_cl_business, "gnews-cl-business", "Google News Business — Chile", "Google Business — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"business\",\"chile\",\"google-news\"]", 3600,
  "Google News Business section for Chile");

RSSX(gn_pe_world, "gnews-pe-world", "Google News World — Peru", "Google World — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"world\",\"peru\",\"google-news\"]", 3600,
  "Google News World section for Peru");

RSSX(gn_pe_nation, "gnews-pe-nation", "Google News National — Peru", "Google National — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"nation\",\"peru\",\"google-news\"]", 3600,
  "Google News National section for Peru");

RSSX(gn_pe_business, "gnews-pe-business", "Google News Business — Peru", "Google Business — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"business\",\"peru\",\"google-news\"]", 3600,
  "Google News Business section for Peru");

RSSX(gn_ro_world, "gnews-ro-world", "Google News World — Romania", "Google World — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"world\",\"romania\",\"google-news\"]", 3600,
  "Google News World section for Romania");

RSSX(gn_ro_nation, "gnews-ro-nation", "Google News National — Romania", "Google National — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"nation\",\"romania\",\"google-news\"]", 3600,
  "Google News National section for Romania");

RSSX(gn_ro_business, "gnews-ro-business", "Google News Business — Romania", "Google Business — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"business\",\"romania\",\"google-news\"]", 3600,
  "Google News Business section for Romania");

RSSX(gn_hu_world, "gnews-hu-world", "Google News World — Hungary", "Google World — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"world\",\"hungary\",\"google-news\"]", 3600,
  "Google News World section for Hungary");

RSSX(gn_hu_nation, "gnews-hu-nation", "Google News National — Hungary", "Google National — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"nation\",\"hungary\",\"google-news\"]", 3600,
  "Google News National section for Hungary");

RSSX(gn_hu_business, "gnews-hu-business", "Google News Business — Hungary", "Google Business — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"business\",\"hungary\",\"google-news\"]", 3600,
  "Google News Business section for Hungary");

RSSX(gn_bg_world, "gnews-bg-world", "Google News World — Bulgaria", "Google World — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"world\",\"bulgaria\",\"google-news\"]", 3600,
  "Google News World section for Bulgaria");

RSSX(gn_bg_nation, "gnews-bg-nation", "Google News National — Bulgaria", "Google National — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"nation\",\"bulgaria\",\"google-news\"]", 3600,
  "Google News National section for Bulgaria");

RSSX(gn_bg_business, "gnews-bg-business", "Google News Business — Bulgaria", "Google Business — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"business\",\"bulgaria\",\"google-news\"]", 3600,
  "Google News Business section for Bulgaria");

RSSX(gn_rs_world, "gnews-rs-world", "Google News World — Serbia", "Google World — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"world\",\"serbia\",\"google-news\"]", 3600,
  "Google News World section for Serbia");

RSSX(gn_rs_nation, "gnews-rs-nation", "Google News National — Serbia", "Google National — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"nation\",\"serbia\",\"google-news\"]", 3600,
  "Google News National section for Serbia");

RSSX(gn_rs_business, "gnews-rs-business", "Google News Business — Serbia", "Google Business — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"business\",\"serbia\",\"google-news\"]", 3600,
  "Google News Business section for Serbia");

RSSX(gn_hr_world, "gnews-hr-world", "Google News World — Croatia", "Google World — クロアチア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=hr&gl=HR&ceid=HR:hr", "hr", "[\"news\",\"world\",\"croatia\",\"google-news\"]", 3600,
  "Google News World section for Croatia");

RSSX(gn_hr_nation, "gnews-hr-nation", "Google News National — Croatia", "Google National — クロアチア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=hr&gl=HR&ceid=HR:hr", "hr", "[\"news\",\"nation\",\"croatia\",\"google-news\"]", 3600,
  "Google News National section for Croatia");

RSSX(gn_hr_business, "gnews-hr-business", "Google News Business — Croatia", "Google Business — クロアチア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=hr&gl=HR&ceid=HR:hr", "hr", "[\"news\",\"business\",\"croatia\",\"google-news\"]", 3600,
  "Google News Business section for Croatia");

RSSX(gn_sk_world, "gnews-sk-world", "Google News World — Slovakia", "Google World — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"world\",\"slovakia\",\"google-news\"]", 3600,
  "Google News World section for Slovakia");

RSSX(gn_sk_nation, "gnews-sk-nation", "Google News National — Slovakia", "Google National — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"nation\",\"slovakia\",\"google-news\"]", 3600,
  "Google News National section for Slovakia");

RSSX(gn_sk_business, "gnews-sk-business", "Google News Business — Slovakia", "Google Business — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"business\",\"slovakia\",\"google-news\"]", 3600,
  "Google News Business section for Slovakia");

RSSX(gn_si_world, "gnews-si-world", "Google News World — Slovenia", "Google World — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"world\",\"slovenia\",\"google-news\"]", 3600,
  "Google News World section for Slovenia");

RSSX(gn_si_nation, "gnews-si-nation", "Google News National — Slovenia", "Google National — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"nation\",\"slovenia\",\"google-news\"]", 3600,
  "Google News National section for Slovenia");

RSSX(gn_si_business, "gnews-si-business", "Google News Business — Slovenia", "Google Business — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"business\",\"slovenia\",\"google-news\"]", 3600,
  "Google News Business section for Slovenia");

RSSX(gn_lt_world, "gnews-lt-world", "Google News World — Lithuania", "Google World — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"world\",\"lithuania\",\"google-news\"]", 3600,
  "Google News World section for Lithuania");

RSSX(gn_lt_nation, "gnews-lt-nation", "Google News National — Lithuania", "Google National — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"nation\",\"lithuania\",\"google-news\"]", 3600,
  "Google News National section for Lithuania");

RSSX(gn_lt_business, "gnews-lt-business", "Google News Business — Lithuania", "Google Business — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"business\",\"lithuania\",\"google-news\"]", 3600,
  "Google News Business section for Lithuania");

RSSX(gn_lv_world, "gnews-lv-world", "Google News World — Latvia", "Google World — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"world\",\"latvia\",\"google-news\"]", 3600,
  "Google News World section for Latvia");

RSSX(gn_lv_nation, "gnews-lv-nation", "Google News National — Latvia", "Google National — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"nation\",\"latvia\",\"google-news\"]", 3600,
  "Google News National section for Latvia");

RSSX(gn_lv_business, "gnews-lv-business", "Google News Business — Latvia", "Google Business — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"business\",\"latvia\",\"google-news\"]", 3600,
  "Google News Business section for Latvia");

RSSX(gn_ee_world, "gnews-ee-world", "Google News World — Estonia", "Google World — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"world\",\"estonia\",\"google-news\"]", 3600,
  "Google News World section for Estonia");

RSSX(gn_ee_nation, "gnews-ee-nation", "Google News National — Estonia", "Google National — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"nation\",\"estonia\",\"google-news\"]", 3600,
  "Google News National section for Estonia");

RSSX(gn_ee_business, "gnews-ee-business", "Google News Business — Estonia", "Google Business — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"business\",\"estonia\",\"google-news\"]", 3600,
  "Google News Business section for Estonia");

RSSX(gn_is_world, "gnews-is-world", "Google News World — Iceland", "Google World — アイスランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=is&gl=IS&ceid=IS:is", "is", "[\"news\",\"world\",\"iceland\",\"google-news\"]", 3600,
  "Google News World section for Iceland");

RSSX(gn_is_nation, "gnews-is-nation", "Google News National — Iceland", "Google National — アイスランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=is&gl=IS&ceid=IS:is", "is", "[\"news\",\"nation\",\"iceland\",\"google-news\"]", 3600,
  "Google News National section for Iceland");

RSSX(gn_is_business, "gnews-is-business", "Google News Business — Iceland", "Google Business — アイスランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=is&gl=IS&ceid=IS:is", "is", "[\"news\",\"business\",\"iceland\",\"google-news\"]", 3600,
  "Google News Business section for Iceland");

RSSX(gn_bd_world, "gnews-bd-world", "Google News World — Bangladesh", "Google World — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"world\",\"bangladesh\",\"google-news\"]", 3600,
  "Google News World section for Bangladesh");

RSSX(gn_bd_nation, "gnews-bd-nation", "Google News National — Bangladesh", "Google National — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"nation\",\"bangladesh\",\"google-news\"]", 3600,
  "Google News National section for Bangladesh");

RSSX(gn_bd_business, "gnews-bd-business", "Google News Business — Bangladesh", "Google Business — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"business\",\"bangladesh\",\"google-news\"]", 3600,
  "Google News Business section for Bangladesh");

RSSX(gn_lk_world, "gnews-lk-world", "Google News World — Sri Lanka", "Google World — スリランカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en&gl=LK&ceid=LK:en", "en", "[\"news\",\"world\",\"sri-lanka\",\"google-news\"]", 3600,
  "Google News World section for Sri Lanka");

RSSX(gn_lk_nation, "gnews-lk-nation", "Google News National — Sri Lanka", "Google National — スリランカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en&gl=LK&ceid=LK:en", "en", "[\"news\",\"nation\",\"sri-lanka\",\"google-news\"]", 3600,
  "Google News National section for Sri Lanka");

RSSX(gn_lk_business, "gnews-lk-business", "Google News Business — Sri Lanka", "Google Business — スリランカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en&gl=LK&ceid=LK:en", "en", "[\"news\",\"business\",\"sri-lanka\",\"google-news\"]", 3600,
  "Google News Business section for Sri Lanka");

RSSX(gn_np_world, "gnews-np-world", "Google News World — Nepal", "Google World — ネパール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en&gl=NP&ceid=NP:en", "en", "[\"news\",\"world\",\"nepal\",\"google-news\"]", 3600,
  "Google News World section for Nepal");

RSSX(gn_np_nation, "gnews-np-nation", "Google News National — Nepal", "Google National — ネパール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en&gl=NP&ceid=NP:en", "en", "[\"news\",\"nation\",\"nepal\",\"google-news\"]", 3600,
  "Google News National section for Nepal");

RSSX(gn_np_business, "gnews-np-business", "Google News Business — Nepal", "Google Business — ネパール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en&gl=NP&ceid=NP:en", "en", "[\"news\",\"business\",\"nepal\",\"google-news\"]", 3600,
  "Google News Business section for Nepal");

RSSX(gn_mm_world, "gnews-mm-world", "Google News World — Myanmar", "Google World — ミャンマー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en&gl=MM&ceid=MM:en", "en", "[\"news\",\"world\",\"myanmar\",\"google-news\"]", 3600,
  "Google News World section for Myanmar");

RSSX(gn_mm_nation, "gnews-mm-nation", "Google News National — Myanmar", "Google National — ミャンマー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en&gl=MM&ceid=MM:en", "en", "[\"news\",\"nation\",\"myanmar\",\"google-news\"]", 3600,
  "Google News National section for Myanmar");

RSSX(gn_mm_business, "gnews-mm-business", "Google News Business — Myanmar", "Google Business — ミャンマー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en&gl=MM&ceid=MM:en", "en", "[\"news\",\"business\",\"myanmar\",\"google-news\"]", 3600,
  "Google News Business section for Myanmar");

RSSX(gn_kh_world, "gnews-kh-world", "Google News World — Cambodia", "Google World — カンボジア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en&gl=KH&ceid=KH:en", "en", "[\"news\",\"world\",\"cambodia\",\"google-news\"]", 3600,
  "Google News World section for Cambodia");

RSSX(gn_kh_nation, "gnews-kh-nation", "Google News National — Cambodia", "Google National — カンボジア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en&gl=KH&ceid=KH:en", "en", "[\"news\",\"nation\",\"cambodia\",\"google-news\"]", 3600,
  "Google News National section for Cambodia");

RSSX(gn_kh_business, "gnews-kh-business", "Google News Business — Cambodia", "Google Business — カンボジア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en&gl=KH&ceid=KH:en", "en", "[\"news\",\"business\",\"cambodia\",\"google-news\"]", 3600,
  "Google News Business section for Cambodia");

RSSX(gn_qa_world, "gnews-qa-world", "Google News World — Qatar", "Google World — カタール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=QA&ceid=QA:ar", "ar", "[\"news\",\"world\",\"qatar\",\"google-news\"]", 3600,
  "Google News World section for Qatar");

RSSX(gn_qa_nation, "gnews-qa-nation", "Google News National — Qatar", "Google National — カタール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=QA&ceid=QA:ar", "ar", "[\"news\",\"nation\",\"qatar\",\"google-news\"]", 3600,
  "Google News National section for Qatar");

RSSX(gn_qa_business, "gnews-qa-business", "Google News Business — Qatar", "Google Business — カタール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=QA&ceid=QA:ar", "ar", "[\"news\",\"business\",\"qatar\",\"google-news\"]", 3600,
  "Google News Business section for Qatar");

RSSX(gn_kw_world, "gnews-kw-world", "Google News World — Kuwait", "Google World — クウェート", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=KW&ceid=KW:ar", "ar", "[\"news\",\"world\",\"kuwait\",\"google-news\"]", 3600,
  "Google News World section for Kuwait");

RSSX(gn_kw_nation, "gnews-kw-nation", "Google News National — Kuwait", "Google National — クウェート", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=KW&ceid=KW:ar", "ar", "[\"news\",\"nation\",\"kuwait\",\"google-news\"]", 3600,
  "Google News National section for Kuwait");

RSSX(gn_kw_business, "gnews-kw-business", "Google News Business — Kuwait", "Google Business — クウェート", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=KW&ceid=KW:ar", "ar", "[\"news\",\"business\",\"kuwait\",\"google-news\"]", 3600,
  "Google News Business section for Kuwait");

RSSX(gn_iq_world, "gnews-iq-world", "Google News World — Iraq", "Google World — イラク", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=IQ&ceid=IQ:ar", "ar", "[\"news\",\"world\",\"iraq\",\"google-news\"]", 3600,
  "Google News World section for Iraq");

RSSX(gn_iq_nation, "gnews-iq-nation", "Google News National — Iraq", "Google National — イラク", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=IQ&ceid=IQ:ar", "ar", "[\"news\",\"nation\",\"iraq\",\"google-news\"]", 3600,
  "Google News National section for Iraq");

RSSX(gn_iq_business, "gnews-iq-business", "Google News Business — Iraq", "Google Business — イラク", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=IQ&ceid=IQ:ar", "ar", "[\"news\",\"business\",\"iraq\",\"google-news\"]", 3600,
  "Google News Business section for Iraq");

RSSX(gn_jo_world, "gnews-jo-world", "Google News World — Jordan", "Google World — ヨルダン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=JO&ceid=JO:ar", "ar", "[\"news\",\"world\",\"jordan\",\"google-news\"]", 3600,
  "Google News World section for Jordan");

RSSX(gn_jo_nation, "gnews-jo-nation", "Google News National — Jordan", "Google National — ヨルダン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=JO&ceid=JO:ar", "ar", "[\"news\",\"nation\",\"jordan\",\"google-news\"]", 3600,
  "Google News National section for Jordan");

RSSX(gn_jo_business, "gnews-jo-business", "Google News Business — Jordan", "Google Business — ヨルダン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=JO&ceid=JO:ar", "ar", "[\"news\",\"business\",\"jordan\",\"google-news\"]", 3600,
  "Google News Business section for Jordan");

RSSX(gn_lb_world, "gnews-lb-world", "Google News World — Lebanon", "Google World — レバノン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"world\",\"lebanon\",\"google-news\"]", 3600,
  "Google News World section for Lebanon");

RSSX(gn_lb_nation, "gnews-lb-nation", "Google News National — Lebanon", "Google National — レバノン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"nation\",\"lebanon\",\"google-news\"]", 3600,
  "Google News National section for Lebanon");

RSSX(gn_lb_business, "gnews-lb-business", "Google News Business — Lebanon", "Google Business — レバノン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"business\",\"lebanon\",\"google-news\"]", 3600,
  "Google News Business section for Lebanon");

RSSX(gn_ma_world, "gnews-ma-world", "Google News World — Morocco", "Google World — モロッコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=MA&ceid=MA:ar", "ar", "[\"news\",\"world\",\"morocco\",\"google-news\"]", 3600,
  "Google News World section for Morocco");

RSSX(gn_ma_nation, "gnews-ma-nation", "Google News National — Morocco", "Google National — モロッコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=MA&ceid=MA:ar", "ar", "[\"news\",\"nation\",\"morocco\",\"google-news\"]", 3600,
  "Google News National section for Morocco");

RSSX(gn_ma_business, "gnews-ma-business", "Google News Business — Morocco", "Google Business — モロッコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=MA&ceid=MA:ar", "ar", "[\"news\",\"business\",\"morocco\",\"google-news\"]", 3600,
  "Google News Business section for Morocco");

RSSX(gn_dz_world, "gnews-dz-world", "Google News World — Algeria", "Google World — アルジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=DZ&ceid=DZ:ar", "ar", "[\"news\",\"world\",\"algeria\",\"google-news\"]", 3600,
  "Google News World section for Algeria");

RSSX(gn_dz_nation, "gnews-dz-nation", "Google News National — Algeria", "Google National — アルジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=DZ&ceid=DZ:ar", "ar", "[\"news\",\"nation\",\"algeria\",\"google-news\"]", 3600,
  "Google News National section for Algeria");

RSSX(gn_dz_business, "gnews-dz-business", "Google News Business — Algeria", "Google Business — アルジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=DZ&ceid=DZ:ar", "ar", "[\"news\",\"business\",\"algeria\",\"google-news\"]", 3600,
  "Google News Business section for Algeria");

RSSX(gn_tn_world, "gnews-tn-world", "Google News World — Tunisia", "Google World — チュニジア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=TN&ceid=TN:ar", "ar", "[\"news\",\"world\",\"tunisia\",\"google-news\"]", 3600,
  "Google News World section for Tunisia");

RSSX(gn_tn_nation, "gnews-tn-nation", "Google News National — Tunisia", "Google National — チュニジア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=TN&ceid=TN:ar", "ar", "[\"news\",\"nation\",\"tunisia\",\"google-news\"]", 3600,
  "Google News National section for Tunisia");

RSSX(gn_tn_business, "gnews-tn-business", "Google News Business — Tunisia", "Google Business — チュニジア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=TN&ceid=TN:ar", "ar", "[\"news\",\"business\",\"tunisia\",\"google-news\"]", 3600,
  "Google News Business section for Tunisia");

RSSX(gn_ve_world, "gnews-ve-world", "Google News World — Venezuela", "Google World — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"world\",\"venezuela\",\"google-news\"]", 3600,
  "Google News World section for Venezuela");

RSSX(gn_ve_nation, "gnews-ve-nation", "Google News National — Venezuela", "Google National — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"nation\",\"venezuela\",\"google-news\"]", 3600,
  "Google News National section for Venezuela");

RSSX(gn_ve_business, "gnews-ve-business", "Google News Business — Venezuela", "Google Business — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"business\",\"venezuela\",\"google-news\"]", 3600,
  "Google News Business section for Venezuela");

RSSX(gn_ec_world, "gnews-ec-world", "Google News World — Ecuador", "Google World — エクアドル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=EC&ceid=EC:es", "es", "[\"news\",\"world\",\"ecuador\",\"google-news\"]", 3600,
  "Google News World section for Ecuador");

RSSX(gn_ec_nation, "gnews-ec-nation", "Google News National — Ecuador", "Google National — エクアドル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=EC&ceid=EC:es", "es", "[\"news\",\"nation\",\"ecuador\",\"google-news\"]", 3600,
  "Google News National section for Ecuador");

RSSX(gn_ec_business, "gnews-ec-business", "Google News Business — Ecuador", "Google Business — エクアドル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=EC&ceid=EC:es", "es", "[\"news\",\"business\",\"ecuador\",\"google-news\"]", 3600,
  "Google News Business section for Ecuador");

RSSX(gn_bo_world, "gnews-bo-world", "Google News World — Bolivia", "Google World — ボリビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"world\",\"bolivia\",\"google-news\"]", 3600,
  "Google News World section for Bolivia");

RSSX(gn_bo_nation, "gnews-bo-nation", "Google News National — Bolivia", "Google National — ボリビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"nation\",\"bolivia\",\"google-news\"]", 3600,
  "Google News National section for Bolivia");

RSSX(gn_bo_business, "gnews-bo-business", "Google News Business — Bolivia", "Google Business — ボリビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"business\",\"bolivia\",\"google-news\"]", 3600,
  "Google News Business section for Bolivia");

RSSX(gn_py_world, "gnews-py-world", "Google News World — Paraguay", "Google World — パラグアイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=PY&ceid=PY:es", "es", "[\"news\",\"world\",\"paraguay\",\"google-news\"]", 3600,
  "Google News World section for Paraguay");

RSSX(gn_py_nation, "gnews-py-nation", "Google News National — Paraguay", "Google National — パラグアイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=PY&ceid=PY:es", "es", "[\"news\",\"nation\",\"paraguay\",\"google-news\"]", 3600,
  "Google News National section for Paraguay");

RSSX(gn_py_business, "gnews-py-business", "Google News Business — Paraguay", "Google Business — パラグアイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=PY&ceid=PY:es", "es", "[\"news\",\"business\",\"paraguay\",\"google-news\"]", 3600,
  "Google News Business section for Paraguay");

RSSX(gn_uy_world, "gnews-uy-world", "Google News World — Uruguay", "Google World — ウルグアイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=UY&ceid=UY:es", "es", "[\"news\",\"world\",\"uruguay\",\"google-news\"]", 3600,
  "Google News World section for Uruguay");

RSSX(gn_uy_nation, "gnews-uy-nation", "Google News National — Uruguay", "Google National — ウルグアイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=UY&ceid=UY:es", "es", "[\"news\",\"nation\",\"uruguay\",\"google-news\"]", 3600,
  "Google News National section for Uruguay");

RSSX(gn_uy_business, "gnews-uy-business", "Google News Business — Uruguay", "Google Business — ウルグアイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=UY&ceid=UY:es", "es", "[\"news\",\"business\",\"uruguay\",\"google-news\"]", 3600,
  "Google News Business section for Uruguay");

RSSX(gn_cr_world, "gnews-cr-world", "Google News World — Costa Rica", "Google World — コスタリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=CR&ceid=CR:es", "es", "[\"news\",\"world\",\"costa-rica\",\"google-news\"]", 3600,
  "Google News World section for Costa Rica");

RSSX(gn_cr_nation, "gnews-cr-nation", "Google News National — Costa Rica", "Google National — コスタリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=CR&ceid=CR:es", "es", "[\"news\",\"nation\",\"costa-rica\",\"google-news\"]", 3600,
  "Google News National section for Costa Rica");

RSSX(gn_cr_business, "gnews-cr-business", "Google News Business — Costa Rica", "Google Business — コスタリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=CR&ceid=CR:es", "es", "[\"news\",\"business\",\"costa-rica\",\"google-news\"]", 3600,
  "Google News Business section for Costa Rica");

RSSX(gn_pa_world, "gnews-pa-world", "Google News World — Panama", "Google World — パナマ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=PA&ceid=PA:es", "es", "[\"news\",\"world\",\"panama\",\"google-news\"]", 3600,
  "Google News World section for Panama");

RSSX(gn_pa_nation, "gnews-pa-nation", "Google News National — Panama", "Google National — パナマ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=PA&ceid=PA:es", "es", "[\"news\",\"nation\",\"panama\",\"google-news\"]", 3600,
  "Google News National section for Panama");

RSSX(gn_pa_business, "gnews-pa-business", "Google News Business — Panama", "Google Business — パナマ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=PA&ceid=PA:es", "es", "[\"news\",\"business\",\"panama\",\"google-news\"]", 3600,
  "Google News Business section for Panama");

RSSX(gn_gt_world, "gnews-gt-world", "Google News World — Guatemala", "Google World — グアテマラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=GT&ceid=GT:es", "es", "[\"news\",\"world\",\"guatemala\",\"google-news\"]", 3600,
  "Google News World section for Guatemala");

RSSX(gn_gt_nation, "gnews-gt-nation", "Google News National — Guatemala", "Google National — グアテマラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=GT&ceid=GT:es", "es", "[\"news\",\"nation\",\"guatemala\",\"google-news\"]", 3600,
  "Google News National section for Guatemala");

RSSX(gn_gt_business, "gnews-gt-business", "Google News Business — Guatemala", "Google Business — グアテマラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=GT&ceid=GT:es", "es", "[\"news\",\"business\",\"guatemala\",\"google-news\"]", 3600,
  "Google News Business section for Guatemala");

RSSX(gn_cu_world, "gnews-cu-world", "Google News World — Cuba", "Google World — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"world\",\"cuba\",\"google-news\"]", 3600,
  "Google News World section for Cuba");

RSSX(gn_cu_nation, "gnews-cu-nation", "Google News National — Cuba", "Google National — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"nation\",\"cuba\",\"google-news\"]", 3600,
  "Google News National section for Cuba");

RSSX(gn_cu_business, "gnews-cu-business", "Google News Business — Cuba", "Google Business — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"business\",\"cuba\",\"google-news\"]", 3600,
  "Google News Business section for Cuba");

RSSX(gn_do_world, "gnews-do-world", "Google News World — Dominican Republic", "Google World — ドミニカ共和国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=DO&ceid=DO:es", "es", "[\"news\",\"world\",\"dominican-republic\",\"google-news\"]", 3600,
  "Google News World section for Dominican Republic");

RSSX(gn_do_nation, "gnews-do-nation", "Google News National — Dominican Republic", "Google National — ドミニカ共和国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=DO&ceid=DO:es", "es", "[\"news\",\"nation\",\"dominican-republic\",\"google-news\"]", 3600,
  "Google News National section for Dominican Republic");

RSSX(gn_do_business, "gnews-do-business", "Google News Business — Dominican Republic", "Google Business — ドミニカ共和国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=DO&ceid=DO:es", "es", "[\"news\",\"business\",\"dominican-republic\",\"google-news\"]", 3600,
  "Google News Business section for Dominican Republic");

RSSX(gn_kz_world, "gnews-kz-world", "Google News World — Kazakhstan", "Google World — カザフスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ru&gl=KZ&ceid=KZ:ru", "ru", "[\"news\",\"world\",\"kazakhstan\",\"google-news\"]", 3600,
  "Google News World section for Kazakhstan");

RSSX(gn_kz_nation, "gnews-kz-nation", "Google News National — Kazakhstan", "Google National — カザフスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ru&gl=KZ&ceid=KZ:ru", "ru", "[\"news\",\"nation\",\"kazakhstan\",\"google-news\"]", 3600,
  "Google News National section for Kazakhstan");

RSSX(gn_kz_business, "gnews-kz-business", "Google News Business — Kazakhstan", "Google Business — カザフスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ru&gl=KZ&ceid=KZ:ru", "ru", "[\"news\",\"business\",\"kazakhstan\",\"google-news\"]", 3600,
  "Google News Business section for Kazakhstan");

RSSX(gn_uz_world, "gnews-uz-world", "Google News World — Uzbekistan", "Google World — ウズベキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ru&gl=UZ&ceid=UZ:ru", "ru", "[\"news\",\"world\",\"uzbekistan\",\"google-news\"]", 3600,
  "Google News World section for Uzbekistan");

RSSX(gn_uz_nation, "gnews-uz-nation", "Google News National — Uzbekistan", "Google National — ウズベキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ru&gl=UZ&ceid=UZ:ru", "ru", "[\"news\",\"nation\",\"uzbekistan\",\"google-news\"]", 3600,
  "Google News National section for Uzbekistan");

RSSX(gn_uz_business, "gnews-uz-business", "Google News Business — Uzbekistan", "Google Business — ウズベキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ru&gl=UZ&ceid=UZ:ru", "ru", "[\"news\",\"business\",\"uzbekistan\",\"google-news\"]", 3600,
  "Google News Business section for Uzbekistan");

RSSX(gn_az_world, "gnews-az-world", "Google News World — Azerbaijan", "Google World — アゼルバイジャン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ru&gl=AZ&ceid=AZ:ru", "ru", "[\"news\",\"world\",\"azerbaijan\",\"google-news\"]", 3600,
  "Google News World section for Azerbaijan");

RSSX(gn_az_nation, "gnews-az-nation", "Google News National — Azerbaijan", "Google National — アゼルバイジャン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ru&gl=AZ&ceid=AZ:ru", "ru", "[\"news\",\"nation\",\"azerbaijan\",\"google-news\"]", 3600,
  "Google News National section for Azerbaijan");

RSSX(gn_az_business, "gnews-az-business", "Google News Business — Azerbaijan", "Google Business — アゼルバイジャン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ru&gl=AZ&ceid=AZ:ru", "ru", "[\"news\",\"business\",\"azerbaijan\",\"google-news\"]", 3600,
  "Google News Business section for Azerbaijan");

RSSX(gn_am_world, "gnews-am-world", "Google News World — Armenia", "Google World — アルメニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ru&gl=AM&ceid=AM:ru", "ru", "[\"news\",\"world\",\"armenia\",\"google-news\"]", 3600,
  "Google News World section for Armenia");

RSSX(gn_am_nation, "gnews-am-nation", "Google News National — Armenia", "Google National — アルメニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ru&gl=AM&ceid=AM:ru", "ru", "[\"news\",\"nation\",\"armenia\",\"google-news\"]", 3600,
  "Google News National section for Armenia");

RSSX(gn_am_business, "gnews-am-business", "Google News Business — Armenia", "Google Business — アルメニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ru&gl=AM&ceid=AM:ru", "ru", "[\"news\",\"business\",\"armenia\",\"google-news\"]", 3600,
  "Google News Business section for Armenia");
