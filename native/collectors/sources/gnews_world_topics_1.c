/* Google News per-country topic sections (World/National/Business) — real RSS editions for priority countries, via rss_collect. */
#include "../../source.h"
#include "../../lib/rss_atom.h"

/* AUDIT NOTE (2026-08-09) — why the interval is ~7200 and not 3600/1800.
 *
 * Every source in gnews_world_top.c (142), gnews_world_topics_1.c (218),
 * gnews_world_topics_2.c (185) and gnews_osint_monitors.c (68) hits ONE host,
 * news.google.com. At the previous intervals (3600 s for the three country
 * files, 1800 s for the monitors) the scheduler wanted 681 requests per hour
 * from one IP — one every 5.3 s.
 *
 * reddit_world_geo.c is the written post-mortem of exactly this failure on a
 * different host: 142 feeds at 1800 s (one per 12.7 s) had 135 of them return
 * rc=-1 with zero rows, because rss_collect() maps any non-2xx — including a
 * transient 429 — to -1, which anomaly_detect turns into status_bad and
 * quarantines. Google News is served through the same RSSX body at ~2.4x that
 * request rate, and throttling is host-correlated, so the whole ~24% of the
 * fleet that lives on news.google.com trips together on one trigger.
 *
 * 7200 s puts the cohort at ~11.7 s average spacing (306 req/h, less than half
 * the old rate). The interval is additionally staggered by one second per
 * source (7200 + cohort index, so 7200..7812): identical intervals preserve
 * whatever burst the boot ramp created forever, whereas a 1 s spread de-phases
 * the 613 sources across a full window inside a day and keeps them spread.
 *
 * As in the reddit case this is a mitigation, not a cure. The cure is a shared
 * per-host rate limiter (core/hostgate.h exists but is not consulted here) and
 * a distinct "throttled" outcome in lib/rss_atom.c so a 429 stops counting as
 * a collector failure. Both live in shared code and are reported, not patched
 * here.
 */

/* SYM, id, name, name_ja, collector, category, url, lang, tags_json, interval, description */
#include "_source_macros.inc"

RSSX(gn_us_world, "gnews-us-world", "Google News World — United States", "Google World — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"world\",\"united-states\",\"google-news\"]", 7342,
  "Google News World section for United States");

RSSX(gn_us_nation, "gnews-us-nation", "Google News National — United States", "Google National — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"nation\",\"united-states\",\"google-news\"]", 7343,
  "Google News National section for United States");

RSSX(gn_us_business, "gnews-us-business", "Google News Business — United States", "Google Business — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"business\",\"united-states\",\"google-news\"]", 7344,
  "Google News Business section for United States");

RSSX(gn_gb_world, "gnews-gb-world", "Google News World — United Kingdom", "Google World — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"world\",\"united-kingdom\",\"google-news\"]", 7345,
  "Google News World section for United Kingdom");

RSSX(gn_gb_nation, "gnews-gb-nation", "Google News National — United Kingdom", "Google National — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"nation\",\"united-kingdom\",\"google-news\"]", 7346,
  "Google News National section for United Kingdom");

RSSX(gn_gb_business, "gnews-gb-business", "Google News Business — United Kingdom", "Google Business — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"business\",\"united-kingdom\",\"google-news\"]", 7347,
  "Google News Business section for United Kingdom");

RSSX(gn_fr_world, "gnews-fr-world", "Google News World — France", "Google World — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"world\",\"france\",\"google-news\"]", 7348,
  "Google News World section for France");

RSSX(gn_fr_nation, "gnews-fr-nation", "Google News National — France", "Google National — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"nation\",\"france\",\"google-news\"]", 7349,
  "Google News National section for France");

RSSX(gn_fr_business, "gnews-fr-business", "Google News Business — France", "Google Business — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"business\",\"france\",\"google-news\"]", 7350,
  "Google News Business section for France");

RSSX(gn_de_world, "gnews-de-world", "Google News World — Germany", "Google World — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"world\",\"germany\",\"google-news\"]", 7351,
  "Google News World section for Germany");

RSSX(gn_de_nation, "gnews-de-nation", "Google News National — Germany", "Google National — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"nation\",\"germany\",\"google-news\"]", 7352,
  "Google News National section for Germany");

RSSX(gn_de_business, "gnews-de-business", "Google News Business — Germany", "Google Business — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"business\",\"germany\",\"google-news\"]", 7353,
  "Google News Business section for Germany");

RSSX(gn_it_world, "gnews-it-world", "Google News World — Italy", "Google World — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"world\",\"italy\",\"google-news\"]", 7354,
  "Google News World section for Italy");

RSSX(gn_it_nation, "gnews-it-nation", "Google News National — Italy", "Google National — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"nation\",\"italy\",\"google-news\"]", 7355,
  "Google News National section for Italy");

RSSX(gn_it_business, "gnews-it-business", "Google News Business — Italy", "Google Business — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"business\",\"italy\",\"google-news\"]", 7356,
  "Google News Business section for Italy");

RSSX(gn_es_world, "gnews-es-world", "Google News World — Spain", "Google World — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"world\",\"spain\",\"google-news\"]", 7357,
  "Google News World section for Spain");

RSSX(gn_es_nation, "gnews-es-nation", "Google News National — Spain", "Google National — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"nation\",\"spain\",\"google-news\"]", 7358,
  "Google News National section for Spain");

RSSX(gn_es_business, "gnews-es-business", "Google News Business — Spain", "Google Business — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"business\",\"spain\",\"google-news\"]", 7359,
  "Google News Business section for Spain");

RSSX(gn_jp_world, "gnews-jp-world", "Google News World — Japan", "Google World — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"world\",\"japan\",\"google-news\"]", 7360,
  "Google News World section for Japan");

RSSX(gn_jp_nation, "gnews-jp-nation", "Google News National — Japan", "Google National — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"nation\",\"japan\",\"google-news\"]", 7361,
  "Google News National section for Japan");

RSSX(gn_jp_business, "gnews-jp-business", "Google News Business — Japan", "Google Business — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"business\",\"japan\",\"google-news\"]", 7362,
  "Google News Business section for Japan");

RSSX(gn_kr_world, "gnews-kr-world", "Google News World — South Korea", "Google World — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"world\",\"south-korea\",\"google-news\"]", 7363,
  "Google News World section for South Korea");

RSSX(gn_kr_nation, "gnews-kr-nation", "Google News National — South Korea", "Google National — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"nation\",\"south-korea\",\"google-news\"]", 7364,
  "Google News National section for South Korea");

RSSX(gn_kr_business, "gnews-kr-business", "Google News Business — South Korea", "Google Business — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"business\",\"south-korea\",\"google-news\"]", 7365,
  "Google News Business section for South Korea");

RSSX(gn_cn_world, "gnews-cn-world", "Google News World — China", "Google World — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"world\",\"china\",\"google-news\"]", 7366,
  "Google News World section for China");

RSSX(gn_cn_nation, "gnews-cn-nation", "Google News National — China", "Google National — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"nation\",\"china\",\"google-news\"]", 7367,
  "Google News National section for China");

RSSX(gn_cn_business, "gnews-cn-business", "Google News Business — China", "Google Business — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"business\",\"china\",\"google-news\"]", 7368,
  "Google News Business section for China");

RSSX(gn_tw_world, "gnews-tw-world", "Google News World — Taiwan", "Google World — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"world\",\"taiwan\",\"google-news\"]", 7369,
  "Google News World section for Taiwan");

RSSX(gn_tw_nation, "gnews-tw-nation", "Google News National — Taiwan", "Google National — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"nation\",\"taiwan\",\"google-news\"]", 7370,
  "Google News National section for Taiwan");

RSSX(gn_tw_business, "gnews-tw-business", "Google News Business — Taiwan", "Google Business — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"business\",\"taiwan\",\"google-news\"]", 7371,
  "Google News Business section for Taiwan");

RSSX(gn_hk_world, "gnews-hk-world", "Google News World — Hong Kong", "Google World — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"world\",\"hong-kong\",\"google-news\"]", 7372,
  "Google News World section for Hong Kong");

RSSX(gn_hk_nation, "gnews-hk-nation", "Google News National — Hong Kong", "Google National — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"nation\",\"hong-kong\",\"google-news\"]", 7373,
  "Google News National section for Hong Kong");

RSSX(gn_hk_business, "gnews-hk-business", "Google News Business — Hong Kong", "Google Business — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"business\",\"hong-kong\",\"google-news\"]", 7374,
  "Google News Business section for Hong Kong");

RSSX(gn_in_world, "gnews-in-world", "Google News World — India", "Google World — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"world\",\"india\",\"google-news\"]", 7375,
  "Google News World section for India");

RSSX(gn_in_nation, "gnews-in-nation", "Google News National — India", "Google National — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"nation\",\"india\",\"google-news\"]", 7376,
  "Google News National section for India");

RSSX(gn_in_business, "gnews-in-business", "Google News Business — India", "Google Business — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"business\",\"india\",\"google-news\"]", 7377,
  "Google News Business section for India");

RSSX(gn_ru_world, "gnews-ru-world", "Google News World — Russia", "Google World — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"world\",\"russia\",\"google-news\"]", 7378,
  "Google News World section for Russia");

RSSX(gn_ru_nation, "gnews-ru-nation", "Google News National — Russia", "Google National — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"nation\",\"russia\",\"google-news\"]", 7379,
  "Google News National section for Russia");

RSSX(gn_ru_business, "gnews-ru-business", "Google News Business — Russia", "Google Business — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"business\",\"russia\",\"google-news\"]", 7380,
  "Google News Business section for Russia");

RSSX(gn_ua_world, "gnews-ua-world", "Google News World — Ukraine", "Google World — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"world\",\"ukraine\",\"google-news\"]", 7381,
  "Google News World section for Ukraine");

RSSX(gn_ua_nation, "gnews-ua-nation", "Google News National — Ukraine", "Google National — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"nation\",\"ukraine\",\"google-news\"]", 7382,
  "Google News National section for Ukraine");

RSSX(gn_ua_business, "gnews-ua-business", "Google News Business — Ukraine", "Google Business — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"business\",\"ukraine\",\"google-news\"]", 7383,
  "Google News Business section for Ukraine");

RSSX(gn_br_world, "gnews-br-world", "Google News World — Brazil", "Google World — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"world\",\"brazil\",\"google-news\"]", 7384,
  "Google News World section for Brazil");

RSSX(gn_br_nation, "gnews-br-nation", "Google News National — Brazil", "Google National — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"nation\",\"brazil\",\"google-news\"]", 7385,
  "Google News National section for Brazil");

RSSX(gn_br_business, "gnews-br-business", "Google News Business — Brazil", "Google Business — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"business\",\"brazil\",\"google-news\"]", 7386,
  "Google News Business section for Brazil");

RSSX(gn_mx_world, "gnews-mx-world", "Google News World — Mexico", "Google World — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"world\",\"mexico\",\"google-news\"]", 7387,
  "Google News World section for Mexico — Latin American Spanish (es-419) regional edition");

RSSX(gn_mx_nation, "gnews-mx-nation", "Google News National — Mexico", "Google National — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"nation\",\"mexico\",\"google-news\"]", 7388,
  "Google News National section for Mexico");

RSSX(gn_mx_business, "gnews-mx-business", "Google News Business — Mexico", "Google Business — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"business\",\"mexico\",\"google-news\"]", 7389,
  "Google News Business section for Mexico — Latin American Spanish (es-419) regional edition");

RSSX(gn_ar_world, "gnews-ar-world", "Google News World — Argentina", "Google World — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"world\",\"argentina\",\"google-news\"]", 7390,
  "Google News World section for Argentina — Latin American Spanish (es-419) regional edition");

RSSX(gn_ar_nation, "gnews-ar-nation", "Google News National — Argentina", "Google National — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"nation\",\"argentina\",\"google-news\"]", 7391,
  "Google News National section for Argentina");

RSSX(gn_ar_business, "gnews-ar-business", "Google News Business — Argentina", "Google Business — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"business\",\"argentina\",\"google-news\"]", 7392,
  "Google News Business section for Argentina — Latin American Spanish (es-419) regional edition");

RSSX(gn_ca_world, "gnews-ca-world", "Google News World — Canada", "Google World — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"world\",\"canada\",\"google-news\"]", 7393,
  "Google News World section for Canada");

RSSX(gn_ca_nation, "gnews-ca-nation", "Google News National — Canada", "Google National — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"nation\",\"canada\",\"google-news\"]", 7394,
  "Google News National section for Canada");

RSSX(gn_ca_business, "gnews-ca-business", "Google News Business — Canada", "Google Business — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"business\",\"canada\",\"google-news\"]", 7395,
  "Google News Business section for Canada");

RSSX(gn_au_world, "gnews-au-world", "Google News World — Australia", "Google World — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"world\",\"australia\",\"google-news\"]", 7396,
  "Google News World section for Australia");

RSSX(gn_au_nation, "gnews-au-nation", "Google News National — Australia", "Google National — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"nation\",\"australia\",\"google-news\"]", 7397,
  "Google News National section for Australia");

RSSX(gn_au_business, "gnews-au-business", "Google News Business — Australia", "Google Business — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"business\",\"australia\",\"google-news\"]", 7398,
  "Google News Business section for Australia");

RSSX(gn_nz_world, "gnews-nz-world", "Google News World — New Zealand", "Google World — ニュージーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"world\",\"new-zealand\",\"google-news\"]", 7399,
  "Google News World section for New Zealand");

RSSX(gn_nz_nation, "gnews-nz-nation", "Google News National — New Zealand", "Google National — ニュージーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"nation\",\"new-zealand\",\"google-news\"]", 7400,
  "Google News National section for New Zealand");

RSSX(gn_nz_business, "gnews-nz-business", "Google News Business — New Zealand", "Google Business — ニュージーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"business\",\"new-zealand\",\"google-news\"]", 7401,
  "Google News Business section for New Zealand");

RSSX(gn_tr_world, "gnews-tr-world", "Google News World — Turkey", "Google World — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"world\",\"turkey\",\"google-news\"]", 7402,
  "Google News World section for Turkey");

RSSX(gn_tr_nation, "gnews-tr-nation", "Google News National — Turkey", "Google National — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"nation\",\"turkey\",\"google-news\"]", 7403,
  "Google News National section for Turkey");

RSSX(gn_tr_business, "gnews-tr-business", "Google News Business — Turkey", "Google Business — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"business\",\"turkey\",\"google-news\"]", 7404,
  "Google News Business section for Turkey");

RSSX(gn_il_world, "gnews-il-world", "Google News World — Israel", "Google World — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"world\",\"israel\",\"google-news\"]", 7405,
  "Google News World section for Israel");

RSSX(gn_il_nation, "gnews-il-nation", "Google News National — Israel", "Google National — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"nation\",\"israel\",\"google-news\"]", 7406,
  "Google News National section for Israel");

RSSX(gn_il_business, "gnews-il-business", "Google News Business — Israel", "Google Business — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"business\",\"israel\",\"google-news\"]", 7407,
  "Google News Business section for Israel");

RSSX(gn_sa_world, "gnews-sa-world", "Google News World — Saudi Arabia", "Google World — サウジアラビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"world\",\"saudi-arabia\",\"google-news\"]", 7408,
  "Google News World section for Saudi Arabia");

RSSX(gn_sa_nation, "gnews-sa-nation", "Google News National — Saudi Arabia", "Google National — サウジアラビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"nation\",\"saudi-arabia\",\"google-news\"]", 7409,
  "Google News National section for Saudi Arabia");

RSSX(gn_sa_business, "gnews-sa-business", "Google News Business — Saudi Arabia", "Google Business — サウジアラビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"business\",\"saudi-arabia\",\"google-news\"]", 7410,
  "Google News Business section for Saudi Arabia");

RSSX(gn_ae_world, "gnews-ae-world", "Google News World — United Arab Emirates", "Google World — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"world\",\"united-arab-emirates\",\"google-news\"]", 7411,
  "Google News World section for United Arab Emirates");

RSSX(gn_ae_nation, "gnews-ae-nation", "Google News National — United Arab Emirates", "Google National — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"nation\",\"united-arab-emirates\",\"google-news\"]", 7412,
  "Google News National section for United Arab Emirates");

RSSX(gn_ae_business, "gnews-ae-business", "Google News Business — United Arab Emirates", "Google Business — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"business\",\"united-arab-emirates\",\"google-news\"]", 7413,
  "Google News Business section for United Arab Emirates");

RSSX(gn_eg_world, "gnews-eg-world", "Google News World — Egypt", "Google World — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"world\",\"egypt\",\"google-news\"]", 7414,
  "Google News World section for Egypt");

RSSX(gn_eg_nation, "gnews-eg-nation", "Google News National — Egypt", "Google National — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"nation\",\"egypt\",\"google-news\"]", 7415,
  "Google News National section for Egypt");

RSSX(gn_eg_business, "gnews-eg-business", "Google News Business — Egypt", "Google Business — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"business\",\"egypt\",\"google-news\"]", 7416,
  "Google News Business section for Egypt");

/* gnews-ir-world retired: duplicate of gnews-hr-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_ir_nation, "gnews-ir-nation", "Google News National — Iran", "Google National — イラン", "osint", "news",
  "https://news.google.com/rss/search?q=%D8%A7%DB%8C%D8%B1%D8%A7%D9%86&hl=fa&gl=IR&ceid=IR:fa", "fa", "[\"news\",\"nation\",\"iran\",\"google-news\"]", 7417,
  "Google News national coverage for Iran (country-scoped feed; the NATION topic returns an HTML page, not RSS)");

/* gnews-ir-business retired: duplicate of gnews-us-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_pk_world, "gnews-pk-world", "Google News World — Pakistan", "Google World — パキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"world\",\"pakistan\",\"google-news\"]", 7418,
  "Google News World section for Pakistan");

RSSX(gn_pk_nation, "gnews-pk-nation", "Google News National — Pakistan", "Google National — パキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"nation\",\"pakistan\",\"google-news\"]", 7419,
  "Google News National section for Pakistan");

RSSX(gn_pk_business, "gnews-pk-business", "Google News Business — Pakistan", "Google Business — パキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"business\",\"pakistan\",\"google-news\"]", 7420,
  "Google News Business section for Pakistan");

RSSX(gn_id_world, "gnews-id-world", "Google News World — Indonesia", "Google World — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"world\",\"indonesia\",\"google-news\"]", 7421,
  "Google News World section for Indonesia");

RSSX(gn_id_nation, "gnews-id-nation", "Google News National — Indonesia", "Google National — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"nation\",\"indonesia\",\"google-news\"]", 7422,
  "Google News National section for Indonesia");

RSSX(gn_id_business, "gnews-id-business", "Google News Business — Indonesia", "Google Business — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"business\",\"indonesia\",\"google-news\"]", 7423,
  "Google News Business section for Indonesia");

RSSX(gn_th_world, "gnews-th-world", "Google News World — Thailand", "Google World — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"world\",\"thailand\",\"google-news\"]", 7424,
  "Google News World section for Thailand");

RSSX(gn_th_nation, "gnews-th-nation", "Google News National — Thailand", "Google National — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"nation\",\"thailand\",\"google-news\"]", 7425,
  "Google News National section for Thailand");

RSSX(gn_th_business, "gnews-th-business", "Google News Business — Thailand", "Google Business — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"business\",\"thailand\",\"google-news\"]", 7426,
  "Google News Business section for Thailand");

RSSX(gn_vn_world, "gnews-vn-world", "Google News World — Vietnam", "Google World — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"world\",\"vietnam\",\"google-news\"]", 7427,
  "Google News World section for Vietnam");

RSSX(gn_vn_nation, "gnews-vn-nation", "Google News National — Vietnam", "Google National — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"nation\",\"vietnam\",\"google-news\"]", 7428,
  "Google News National section for Vietnam");

RSSX(gn_vn_business, "gnews-vn-business", "Google News Business — Vietnam", "Google Business — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"business\",\"vietnam\",\"google-news\"]", 7429,
  "Google News Business section for Vietnam");

RSSX(gn_ph_world, "gnews-ph-world", "Google News World — Philippines", "Google World — フィリピン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"world\",\"philippines\",\"google-news\"]", 7430,
  "Google News World section for Philippines");

RSSX(gn_ph_nation, "gnews-ph-nation", "Google News National — Philippines", "Google National — フィリピン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"nation\",\"philippines\",\"google-news\"]", 7431,
  "Google News National section for Philippines");

RSSX(gn_ph_business, "gnews-ph-business", "Google News Business — Philippines", "Google Business — フィリピン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"business\",\"philippines\",\"google-news\"]", 7432,
  "Google News Business section for Philippines");

RSSX(gn_sg_world, "gnews-sg-world", "Google News World — Singapore", "Google World — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"world\",\"singapore\",\"google-news\"]", 7433,
  "Google News World section for Singapore");

RSSX(gn_sg_nation, "gnews-sg-nation", "Google News National — Singapore", "Google National — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"nation\",\"singapore\",\"google-news\"]", 7434,
  "Google News National section for Singapore");

RSSX(gn_sg_business, "gnews-sg-business", "Google News Business — Singapore", "Google Business — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"business\",\"singapore\",\"google-news\"]", 7435,
  "Google News Business section for Singapore");

RSSX(gn_my_world, "gnews-my-world", "Google News World — Malaysia", "Google World — マレーシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"world\",\"malaysia\",\"google-news\"]", 7436,
  "Google News World section for Malaysia");

RSSX(gn_my_nation, "gnews-my-nation", "Google News National — Malaysia", "Google National — マレーシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"nation\",\"malaysia\",\"google-news\"]", 7437,
  "Google News National section for Malaysia");

RSSX(gn_my_business, "gnews-my-business", "Google News Business — Malaysia", "Google Business — マレーシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"business\",\"malaysia\",\"google-news\"]", 7438,
  "Google News Business section for Malaysia");

RSSX(gn_nl_world, "gnews-nl-world", "Google News World — Netherlands", "Google World — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"world\",\"netherlands\",\"google-news\"]", 7439,
  "Google News World section for Netherlands");

RSSX(gn_nl_nation, "gnews-nl-nation", "Google News National — Netherlands", "Google National — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"nation\",\"netherlands\",\"google-news\"]", 7440,
  "Google News National section for Netherlands");

RSSX(gn_nl_business, "gnews-nl-business", "Google News Business — Netherlands", "Google Business — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"business\",\"netherlands\",\"google-news\"]", 7441,
  "Google News Business section for Netherlands");

RSSX(gn_be_world, "gnews-be-world", "Google News World — Belgium", "Google World — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"world\",\"belgium\",\"google-news\"]", 7442,
  "Google News World section for Belgium");

RSSX(gn_be_nation, "gnews-be-nation", "Google News National — Belgium", "Google National — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"nation\",\"belgium\",\"google-news\"]", 7443,
  "Google News National section for Belgium");

RSSX(gn_be_business, "gnews-be-business", "Google News Business — Belgium", "Google Business — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"business\",\"belgium\",\"google-news\"]", 7444,
  "Google News Business section for Belgium");

RSSX(gn_ch_world, "gnews-ch-world", "Google News World — Switzerland", "Google World — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"world\",\"switzerland\",\"google-news\"]", 7445,
  "Google News World section for Switzerland");

RSSX(gn_ch_nation, "gnews-ch-nation", "Google News National — Switzerland", "Google National — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"nation\",\"switzerland\",\"google-news\"]", 7446,
  "Google News National section for Switzerland");

RSSX(gn_ch_business, "gnews-ch-business", "Google News Business — Switzerland", "Google Business — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"business\",\"switzerland\",\"google-news\"]", 7447,
  "Google News Business section for Switzerland");

RSSX(gn_at_world, "gnews-at-world", "Google News World — Austria", "Google World — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"world\",\"austria\",\"google-news\"]", 7448,
  "Google News World section for Austria");

RSSX(gn_at_nation, "gnews-at-nation", "Google News National — Austria", "Google National — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"nation\",\"austria\",\"google-news\"]", 7449,
  "Google News National section for Austria");

RSSX(gn_at_business, "gnews-at-business", "Google News Business — Austria", "Google Business — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"business\",\"austria\",\"google-news\"]", 7450,
  "Google News Business section for Austria");

RSSX(gn_se_world, "gnews-se-world", "Google News World — Sweden", "Google World — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"world\",\"sweden\",\"google-news\"]", 7451,
  "Google News World section for Sweden");

RSSX(gn_se_nation, "gnews-se-nation", "Google News National — Sweden", "Google National — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"nation\",\"sweden\",\"google-news\"]", 7452,
  "Google News National section for Sweden");

RSSX(gn_se_business, "gnews-se-business", "Google News Business — Sweden", "Google Business — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"business\",\"sweden\",\"google-news\"]", 7453,
  "Google News Business section for Sweden");

RSSX(gn_no_world, "gnews-no-world", "Google News World — Norway", "Google World — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"world\",\"norway\",\"google-news\"]", 7454,
  "Google News World section for Norway");

RSSX(gn_no_nation, "gnews-no-nation", "Google News National — Norway", "Google National — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"nation\",\"norway\",\"google-news\"]", 7455,
  "Google News National section for Norway");

RSSX(gn_no_business, "gnews-no-business", "Google News Business — Norway", "Google Business — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"business\",\"norway\",\"google-news\"]", 7456,
  "Google News Business section for Norway");

/* gnews-dk-world retired: duplicate of gnews-no-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_dk_nation, "gnews-dk-nation", "Google News National — Denmark", "Google National — デンマーク", "osint", "news",
  "https://news.google.com/rss/search?q=Danmark&hl=da&gl=DK&ceid=DK:da", "da", "[\"news\",\"nation\",\"denmark\",\"google-news\"]", 7457,
  "Google News national coverage for Denmark (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-dk-business retired: duplicate of gnews-no-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_fi_world, "gnews-fi-world", "Google News World — Finland", "Google World — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"world\",\"finland\",\"google-news\"]", 7458,
  "Google News World section for Finland");

RSSX(gn_fi_nation, "gnews-fi-nation", "Google News National — Finland", "Google National — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"nation\",\"finland\",\"google-news\"]", 7459,
  "Google News National section for Finland");

RSSX(gn_fi_business, "gnews-fi-business", "Google News Business — Finland", "Google Business — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"business\",\"finland\",\"google-news\"]", 7460,
  "Google News Business section for Finland");

RSSX(gn_pl_world, "gnews-pl-world", "Google News World — Poland", "Google World — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"world\",\"poland\",\"google-news\"]", 7461,
  "Google News World section for Poland");

RSSX(gn_pl_nation, "gnews-pl-nation", "Google News National — Poland", "Google National — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"nation\",\"poland\",\"google-news\"]", 7462,
  "Google News National section for Poland");

RSSX(gn_pl_business, "gnews-pl-business", "Google News Business — Poland", "Google Business — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"business\",\"poland\",\"google-news\"]", 7463,
  "Google News Business section for Poland");

RSSX(gn_cz_world, "gnews-cz-world", "Google News World — Czechia", "Google World — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"world\",\"czechia\",\"google-news\"]", 7464,
  "Google News World section for Czechia");

RSSX(gn_cz_nation, "gnews-cz-nation", "Google News National — Czechia", "Google National — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"nation\",\"czechia\",\"google-news\"]", 7465,
  "Google News National section for Czechia");

RSSX(gn_cz_business, "gnews-cz-business", "Google News Business — Czechia", "Google Business — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"business\",\"czechia\",\"google-news\"]", 7466,
  "Google News Business section for Czechia");

RSSX(gn_gr_world, "gnews-gr-world", "Google News World — Greece", "Google World — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"world\",\"greece\",\"google-news\"]", 7467,
  "Google News World section for Greece");

RSSX(gn_gr_nation, "gnews-gr-nation", "Google News National — Greece", "Google National — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"nation\",\"greece\",\"google-news\"]", 7468,
  "Google News National section for Greece");

RSSX(gn_gr_business, "gnews-gr-business", "Google News Business — Greece", "Google Business — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"business\",\"greece\",\"google-news\"]", 7469,
  "Google News Business section for Greece");

RSSX(gn_pt_world, "gnews-pt-world", "Google News World — Portugal", "Google World — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"world\",\"portugal\",\"google-news\"]", 7470,
  "Google News World section for Portugal");

RSSX(gn_pt_nation, "gnews-pt-nation", "Google News National — Portugal", "Google National — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"nation\",\"portugal\",\"google-news\"]", 7471,
  "Google News National section for Portugal");

RSSX(gn_pt_business, "gnews-pt-business", "Google News Business — Portugal", "Google Business — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"business\",\"portugal\",\"google-news\"]", 7472,
  "Google News Business section for Portugal");

RSSX(gn_ie_world, "gnews-ie-world", "Google News World — Ireland", "Google World — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"world\",\"ireland\",\"google-news\"]", 7473,
  "Google News World section for Ireland");

RSSX(gn_ie_nation, "gnews-ie-nation", "Google News National — Ireland", "Google National — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"nation\",\"ireland\",\"google-news\"]", 7474,
  "Google News National section for Ireland");

RSSX(gn_ie_business, "gnews-ie-business", "Google News Business — Ireland", "Google Business — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"business\",\"ireland\",\"google-news\"]", 7475,
  "Google News Business section for Ireland");

RSSX(gn_za_world, "gnews-za-world", "Google News World — South Africa", "Google World — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"world\",\"south-africa\",\"google-news\"]", 7476,
  "Google News World section for South Africa");

RSSX(gn_za_nation, "gnews-za-nation", "Google News National — South Africa", "Google National — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"nation\",\"south-africa\",\"google-news\"]", 7477,
  "Google News National section for South Africa");

RSSX(gn_za_business, "gnews-za-business", "Google News Business — South Africa", "Google Business — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"business\",\"south-africa\",\"google-news\"]", 7478,
  "Google News Business section for South Africa");

RSSX(gn_ng_world, "gnews-ng-world", "Google News World — Nigeria", "Google World — ナイジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"world\",\"nigeria\",\"google-news\"]", 7479,
  "Google News World section for Nigeria");

RSSX(gn_ng_nation, "gnews-ng-nation", "Google News National — Nigeria", "Google National — ナイジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"nation\",\"nigeria\",\"google-news\"]", 7480,
  "Google News National section for Nigeria");

RSSX(gn_ng_business, "gnews-ng-business", "Google News Business — Nigeria", "Google Business — ナイジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"business\",\"nigeria\",\"google-news\"]", 7481,
  "Google News Business section for Nigeria");

RSSX(gn_ke_world, "gnews-ke-world", "Google News World — Kenya", "Google World — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"world\",\"kenya\",\"google-news\"]", 7482,
  "Google News World section for Kenya");

RSSX(gn_ke_nation, "gnews-ke-nation", "Google News National — Kenya", "Google National — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"nation\",\"kenya\",\"google-news\"]", 7483,
  "Google News National section for Kenya");

RSSX(gn_ke_business, "gnews-ke-business", "Google News Business — Kenya", "Google Business — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"business\",\"kenya\",\"google-news\"]", 7484,
  "Google News Business section for Kenya");

RSSX(gn_co_world, "gnews-co-world", "Google News World — Colombia", "Google World — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"world\",\"colombia\",\"google-news\"]", 7485,
  "Google News World section for Colombia — Latin American Spanish (es-419) regional edition");

RSSX(gn_co_nation, "gnews-co-nation", "Google News National — Colombia", "Google National — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"nation\",\"colombia\",\"google-news\"]", 7486,
  "Google News National section for Colombia");

RSSX(gn_co_business, "gnews-co-business", "Google News Business — Colombia", "Google Business — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"business\",\"colombia\",\"google-news\"]", 7487,
  "Google News Business section for Colombia — Latin American Spanish (es-419) regional edition");

RSSX(gn_cl_world, "gnews-cl-world", "Google News World — Chile", "Google World — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"world\",\"chile\",\"google-news\"]", 7488,
  "Google News World section for Chile — Latin American Spanish (es-419) regional edition");

RSSX(gn_cl_nation, "gnews-cl-nation", "Google News National — Chile", "Google National — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"nation\",\"chile\",\"google-news\"]", 7489,
  "Google News National section for Chile");

RSSX(gn_cl_business, "gnews-cl-business", "Google News Business — Chile", "Google Business — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"business\",\"chile\",\"google-news\"]", 7490,
  "Google News Business section for Chile — Latin American Spanish (es-419) regional edition");

RSSX(gn_pe_world, "gnews-pe-world", "Google News World — Peru", "Google World — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"world\",\"peru\",\"google-news\"]", 7491,
  "Google News World section for Peru — Latin American Spanish (es-419) regional edition");

RSSX(gn_pe_nation, "gnews-pe-nation", "Google News National — Peru", "Google National — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"nation\",\"peru\",\"google-news\"]", 7492,
  "Google News National section for Peru");

RSSX(gn_pe_business, "gnews-pe-business", "Google News Business — Peru", "Google Business — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"business\",\"peru\",\"google-news\"]", 7493,
  "Google News Business section for Peru — Latin American Spanish (es-419) regional edition");

RSSX(gn_ro_world, "gnews-ro-world", "Google News World — Romania", "Google World — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"world\",\"romania\",\"google-news\"]", 7494,
  "Google News World section for Romania");

RSSX(gn_ro_nation, "gnews-ro-nation", "Google News National — Romania", "Google National — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"nation\",\"romania\",\"google-news\"]", 7495,
  "Google News National section for Romania");

RSSX(gn_ro_business, "gnews-ro-business", "Google News Business — Romania", "Google Business — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"business\",\"romania\",\"google-news\"]", 7496,
  "Google News Business section for Romania");

RSSX(gn_hu_world, "gnews-hu-world", "Google News World — Hungary", "Google World — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"world\",\"hungary\",\"google-news\"]", 7497,
  "Google News World section for Hungary");

RSSX(gn_hu_nation, "gnews-hu-nation", "Google News National — Hungary", "Google National — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"nation\",\"hungary\",\"google-news\"]", 7498,
  "Google News National section for Hungary");

RSSX(gn_hu_business, "gnews-hu-business", "Google News Business — Hungary", "Google Business — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"business\",\"hungary\",\"google-news\"]", 7499,
  "Google News Business section for Hungary");

RSSX(gn_bg_world, "gnews-bg-world", "Google News World — Bulgaria", "Google World — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"world\",\"bulgaria\",\"google-news\"]", 7500,
  "Google News World section for Bulgaria");

RSSX(gn_bg_nation, "gnews-bg-nation", "Google News National — Bulgaria", "Google National — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"nation\",\"bulgaria\",\"google-news\"]", 7501,
  "Google News National section for Bulgaria");

RSSX(gn_bg_business, "gnews-bg-business", "Google News Business — Bulgaria", "Google Business — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"business\",\"bulgaria\",\"google-news\"]", 7502,
  "Google News Business section for Bulgaria");

RSSX(gn_rs_world, "gnews-rs-world", "Google News World — Serbia", "Google World — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"world\",\"serbia\",\"google-news\"]", 7503,
  "Google News World section for Serbia");

RSSX(gn_rs_nation, "gnews-rs-nation", "Google News National — Serbia", "Google National — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"nation\",\"serbia\",\"google-news\"]", 7504,
  "Google News National section for Serbia");

RSSX(gn_rs_business, "gnews-rs-business", "Google News Business — Serbia", "Google Business — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"business\",\"serbia\",\"google-news\"]", 7505,
  "Google News Business section for Serbia");

/* gnews-hr-world retired: serves the us edition, already collected by gnews-us-world (jaccard 1.00). See tests/audit/gnews_mislabel_actions.json. */

RSSX(gn_hr_nation, "gnews-hr-nation", "Google News National — Croatia", "Google National — クロアチア", "osint", "news",
  "https://news.google.com/rss/search?q=Hrvatska&hl=hr&gl=HR&ceid=HR:hr", "hr", "[\"news\",\"nation\",\"croatia\",\"google-news\"]", 7506,
  "Google News national coverage for Croatia (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-hr-business retired: duplicate of gnews-us-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_sk_world, "gnews-sk-world", "Google News World — Slovakia", "Google World — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"world\",\"slovakia\",\"google-news\"]", 7507,
  "Google News World section for Slovakia");

RSSX(gn_sk_nation, "gnews-sk-nation", "Google News National — Slovakia", "Google National — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"nation\",\"slovakia\",\"google-news\"]", 7508,
  "Google News National section for Slovakia");

RSSX(gn_sk_business, "gnews-sk-business", "Google News Business — Slovakia", "Google Business — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"business\",\"slovakia\",\"google-news\"]", 7509,
  "Google News Business section for Slovakia");

RSSX(gn_si_world, "gnews-si-world", "Google News World — Slovenia", "Google World — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"world\",\"slovenia\",\"google-news\"]", 7510,
  "Google News World section for Slovenia");

RSSX(gn_si_nation, "gnews-si-nation", "Google News National — Slovenia", "Google National — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"nation\",\"slovenia\",\"google-news\"]", 7511,
  "Google News National section for Slovenia");

RSSX(gn_si_business, "gnews-si-business", "Google News Business — Slovenia", "Google Business — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"business\",\"slovenia\",\"google-news\"]", 7512,
  "Google News Business section for Slovenia");

RSSX(gn_lt_world, "gnews-lt-world", "Google News World — Lithuania", "Google World — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"world\",\"lithuania\",\"google-news\"]", 7513,
  "Google News World section for Lithuania");

RSSX(gn_lt_nation, "gnews-lt-nation", "Google News National — Lithuania", "Google National — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"nation\",\"lithuania\",\"google-news\"]", 7514,
  "Google News National section for Lithuania");

RSSX(gn_lt_business, "gnews-lt-business", "Google News Business — Lithuania", "Google Business — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"business\",\"lithuania\",\"google-news\"]", 7515,
  "Google News Business section for Lithuania");

RSSX(gn_lv_world, "gnews-lv-world", "Google News World — Latvia", "Google World — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"world\",\"latvia\",\"google-news\"]", 7516,
  "Google News World section for Latvia");

RSSX(gn_lv_nation, "gnews-lv-nation", "Google News National — Latvia", "Google National — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"nation\",\"latvia\",\"google-news\"]", 7517,
  "Google News National section for Latvia");

RSSX(gn_lv_business, "gnews-lv-business", "Google News Business — Latvia", "Google Business — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"business\",\"latvia\",\"google-news\"]", 7518,
  "Google News Business section for Latvia");

RSSX(gn_ee_world, "gnews-ee-world", "Google News World — Estonia", "Google World — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"world\",\"estonia\",\"google-news\"]", 7519,
  "Google News World section for Estonia");

RSSX(gn_ee_nation, "gnews-ee-nation", "Google News National — Estonia", "Google National — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"nation\",\"estonia\",\"google-news\"]", 7520,
  "Google News National section for Estonia");

RSSX(gn_ee_business, "gnews-ee-business", "Google News Business — Estonia", "Google Business — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"business\",\"estonia\",\"google-news\"]", 7521,
  "Google News Business section for Estonia");

/* gnews-is-world retired: duplicate of gnews-hr-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_is_nation, "gnews-is-nation", "Google News National — Iceland", "Google National — アイスランド", "osint", "news",
  "https://news.google.com/rss/search?q=%C3%8Dsland&hl=is&gl=IS&ceid=IS:is", "is", "[\"news\",\"nation\",\"iceland\",\"google-news\"]", 7522,
  "Google News national coverage for Iceland (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-is-business retired: duplicate of gnews-us-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_bd_world, "gnews-bd-world", "Google News World — Bangladesh", "Google World — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"world\",\"bangladesh\",\"google-news\"]", 7523,
  "Google News World section for Bangladesh");

RSSX(gn_bd_nation, "gnews-bd-nation", "Google News National — Bangladesh", "Google National — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"nation\",\"bangladesh\",\"google-news\"]", 7524,
  "Google News National section for Bangladesh");

RSSX(gn_bd_business, "gnews-bd-business", "Google News Business — Bangladesh", "Google Business — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"business\",\"bangladesh\",\"google-news\"]", 7525,
  "Google News Business section for Bangladesh");

/* gnews-lk-world retired: duplicate of gnews-hr-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_lk_nation, "gnews-lk-nation", "Google News National — Sri Lanka", "Google National — スリランカ", "osint", "news",
  "https://news.google.com/rss/search?q=Sri%20Lanka&hl=en&gl=LK&ceid=LK:en", "en", "[\"news\",\"nation\",\"sri-lanka\",\"google-news\"]", 7526,
  "Google News national coverage for Sri Lanka (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-lk-business retired: duplicate of gnews-us-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-np-world retired: duplicate of gnews-hr-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_np_nation, "gnews-np-nation", "Google News National — Nepal", "Google National — ネパール", "osint", "news",
  "https://news.google.com/rss/search?q=Nepal&hl=en&gl=NP&ceid=NP:en", "en", "[\"news\",\"nation\",\"nepal\",\"google-news\"]", 7527,
  "Google News national coverage for Nepal (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-np-business retired: duplicate of gnews-us-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-mm-world retired: duplicate of gnews-hr-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_mm_nation, "gnews-mm-nation", "Google News National — Myanmar", "Google National — ミャンマー", "osint", "news",
  "https://news.google.com/rss/search?q=Myanmar&hl=en&gl=MM&ceid=MM:en", "en", "[\"news\",\"nation\",\"myanmar\",\"google-news\"]", 7528,
  "Google News national coverage for Myanmar (country-scoped feed; the NATION topic returns an HTML page, not RSS)");

/* gnews-mm-business retired: duplicate of gnews-us-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kh-world retired: duplicate of gnews-hr-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_kh_nation, "gnews-kh-nation", "Google News National — Cambodia", "Google National — カンボジア", "osint", "news",
  "https://news.google.com/rss/search?q=Cambodia&hl=en&gl=KH&ceid=KH:en", "en", "[\"news\",\"nation\",\"cambodia\",\"google-news\"]", 7529,
  "Google News national coverage for Cambodia (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-kh-business retired: duplicate of gnews-us-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-qa-world retired: duplicate of gnews-eg-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_qa_nation, "gnews-qa-nation", "Google News National — Qatar", "Google National — カタール", "osint", "news",
  "https://news.google.com/rss/search?q=%D9%82%D8%B7%D8%B1&hl=ar&gl=QA&ceid=QA:ar", "ar", "[\"news\",\"nation\",\"qatar\",\"google-news\"]", 7530,
  "Google News national coverage for Qatar (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-qa-business retired: duplicate of gnews-eg-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kw-world retired: duplicate of gnews-eg-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_kw_nation, "gnews-kw-nation", "Google News National — Kuwait", "Google National — クウェート", "osint", "news",
  "https://news.google.com/rss/search?q=%D8%A7%D9%84%D9%83%D9%88%D9%8A%D8%AA&hl=ar&gl=KW&ceid=KW:ar", "ar", "[\"news\",\"nation\",\"kuwait\",\"google-news\"]", 7531,
  "Google News national coverage for Kuwait (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-kw-business retired: duplicate of gnews-eg-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-iq-world retired: duplicate of gnews-eg-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_iq_nation, "gnews-iq-nation", "Google News National — Iraq", "Google National — イラク", "osint", "news",
  "https://news.google.com/rss/search?q=%D8%A7%D9%84%D8%B9%D8%B1%D8%A7%D9%82&hl=ar&gl=IQ&ceid=IQ:ar", "ar", "[\"news\",\"nation\",\"iraq\",\"google-news\"]", 7532,
  "Google News national coverage for Iraq (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-iq-business retired: duplicate of gnews-eg-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-jo-world retired: duplicate of gnews-eg-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_jo_nation, "gnews-jo-nation", "Google News National — Jordan", "Google National — ヨルダン", "osint", "news",
  "https://news.google.com/rss/search?q=%D8%A7%D9%84%D8%A3%D8%B1%D8%AF%D9%86&hl=ar&gl=JO&ceid=JO:ar", "ar", "[\"news\",\"nation\",\"jordan\",\"google-news\"]", 7533,
  "Google News national coverage for Jordan (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-jo-business retired: duplicate of gnews-eg-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_lb_world, "gnews-lb-world", "Google News World — Lebanon", "Google World — レバノン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"world\",\"lebanon\",\"google-news\"]", 7534,
  "Google News World section for Lebanon");

RSSX(gn_lb_nation, "gnews-lb-nation", "Google News National — Lebanon", "Google National — レバノン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"nation\",\"lebanon\",\"google-news\"]", 7535,
  "Google News National section for Lebanon");

RSSX(gn_lb_business, "gnews-lb-business", "Google News Business — Lebanon", "Google Business — レバノン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"business\",\"lebanon\",\"google-news\"]", 7536,
  "Google News Business section for Lebanon");

/* gnews-ma-world retired: duplicate of gnews-eg-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_ma_nation, "gnews-ma-nation", "Google News National — Morocco", "Google National — モロッコ", "osint", "news",
  "https://news.google.com/rss/search?q=%D8%A7%D9%84%D9%85%D8%BA%D8%B1%D8%A8&hl=ar&gl=MA&ceid=MA:ar", "ar", "[\"news\",\"nation\",\"morocco\",\"google-news\"]", 7537,
  "Google News national coverage for Morocco (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-ma-business retired: duplicate of gnews-eg-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-dz-world retired: duplicate of gnews-eg-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_dz_nation, "gnews-dz-nation", "Google News National — Algeria", "Google National — アルジェリア", "osint", "news",
  "https://news.google.com/rss/search?q=%D8%A7%D9%84%D8%AC%D8%B2%D8%A7%D8%A6%D8%B1&hl=ar&gl=DZ&ceid=DZ:ar", "ar", "[\"news\",\"nation\",\"algeria\",\"google-news\"]", 7538,
  "Google News national coverage for Algeria (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-dz-business retired: duplicate of gnews-eg-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-tn-world retired: duplicate of gnews-eg-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_tn_nation, "gnews-tn-nation", "Google News National — Tunisia", "Google National — チュニジア", "osint", "news",
  "https://news.google.com/rss/search?q=%D8%AA%D9%88%D9%86%D8%B3&hl=ar&gl=TN&ceid=TN:ar", "ar", "[\"news\",\"nation\",\"tunisia\",\"google-news\"]", 7539,
  "Google News national coverage for Tunisia (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-tn-business retired: duplicate of gnews-eg-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_ve_world, "gnews-ve-world", "Google News World — Venezuela", "Google World — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"world\",\"venezuela\",\"google-news\"]", 7540,
  "Google News World section for Venezuela — Latin American Spanish (es-419) regional edition");

RSSX(gn_ve_nation, "gnews-ve-nation", "Google News National — Venezuela", "Google National — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"nation\",\"venezuela\",\"google-news\"]", 7541,
  "Google News National section for Venezuela");

RSSX(gn_ve_business, "gnews-ve-business", "Google News Business — Venezuela", "Google Business — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"business\",\"venezuela\",\"google-news\"]", 7542,
  "Google News Business section for Venezuela — Latin American Spanish (es-419) regional edition");

/* gnews-ec-world retired: duplicate of gnews-bo-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_ec_nation, "gnews-ec-nation", "Google News National — Ecuador", "Google National — エクアドル", "osint", "news",
  "https://news.google.com/rss/search?q=Ecuador&hl=es-419&gl=EC&ceid=EC:es", "es", "[\"news\",\"nation\",\"ecuador\",\"google-news\"]", 7543,
  "Google News national coverage for Ecuador (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-ec-business retired: duplicate of gnews-bo-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_es419_world, "gnews-es419-world", "Google News World — Bolivia", "Google World — ボリビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"world\",\"bolivia\",\"google-news\"]", 7544,
  "Google News World section for Bolivia — Latin American Spanish (es-419) regional edition");

RSSX(gn_bo_nation, "gnews-bo-nation", "Google News National — Bolivia", "Google National — ボリビア", "osint", "news",
  "https://news.google.com/rss/search?q=Bolivia&hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"nation\",\"bolivia\",\"google-news\"]", 7545,
  "Google News national coverage for Bolivia (country-scoped feed; the NATION topic is empty upstream for this edition)");

RSSX(gn_es419_business, "gnews-es419-business", "Google News Business — Bolivia", "Google Business — ボリビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"business\",\"bolivia\",\"google-news\"]", 7546,
  "Google News Business section for Bolivia — Latin American Spanish (es-419) regional edition");

/* gnews-py-world retired: duplicate of gnews-bo-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_py_nation, "gnews-py-nation", "Google News National — Paraguay", "Google National — パラグアイ", "osint", "news",
  "https://news.google.com/rss/search?q=Paraguay&hl=es-419&gl=PY&ceid=PY:es", "es", "[\"news\",\"nation\",\"paraguay\",\"google-news\"]", 7547,
  "Google News national coverage for Paraguay (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-py-business retired: duplicate of gnews-bo-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-uy-world retired: duplicate of gnews-bo-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_uy_nation, "gnews-uy-nation", "Google News National — Uruguay", "Google National — ウルグアイ", "osint", "news",
  "https://news.google.com/rss/search?q=Uruguay&hl=es-419&gl=UY&ceid=UY:es", "es", "[\"news\",\"nation\",\"uruguay\",\"google-news\"]", 7548,
  "Google News national coverage for Uruguay (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-uy-business retired: duplicate of gnews-bo-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-cr-world retired: duplicate of gnews-bo-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_cr_nation, "gnews-cr-nation", "Google News National — Costa Rica", "Google National — コスタリカ", "osint", "news",
  "https://news.google.com/rss/search?q=Costa%20Rica&hl=es-419&gl=CR&ceid=CR:es", "es", "[\"news\",\"nation\",\"costa-rica\",\"google-news\"]", 7549,
  "Google News national coverage for Costa Rica (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-cr-business retired: duplicate of gnews-bo-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-pa-world retired: duplicate of gnews-bo-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_pa_nation, "gnews-pa-nation", "Google News National — Panama", "Google National — パナマ", "osint", "news",
  "https://news.google.com/rss/search?q=Panam%C3%A1&hl=es-419&gl=PA&ceid=PA:es", "es", "[\"news\",\"nation\",\"panama\",\"google-news\"]", 7550,
  "Google News national coverage for Panama (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-pa-business retired: duplicate of gnews-bo-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-gt-world retired: duplicate of gnews-bo-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_gt_nation, "gnews-gt-nation", "Google News National — Guatemala", "Google National — グアテマラ", "osint", "news",
  "https://news.google.com/rss/search?q=Guatemala&hl=es-419&gl=GT&ceid=GT:es", "es", "[\"news\",\"nation\",\"guatemala\",\"google-news\"]", 7551,
  "Google News national coverage for Guatemala (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-gt-business retired: duplicate of gnews-bo-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_cu_world, "gnews-cu-world", "Google News World — Cuba", "Google World — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/WORLD?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"world\",\"cuba\",\"google-news\"]", 7552,
  "Google News World section for Cuba — Latin American Spanish (es-419) regional edition");

RSSX(gn_cu_nation, "gnews-cu-nation", "Google News National — Cuba", "Google National — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/NATION?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"nation\",\"cuba\",\"google-news\"]", 7553,
  "Google News National section for Cuba");

RSSX(gn_cu_business, "gnews-cu-business", "Google News Business — Cuba", "Google Business — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"business\",\"cuba\",\"google-news\"]", 7554,
  "Google News Business section for Cuba — Latin American Spanish (es-419) regional edition");

/* gnews-do-world retired: duplicate of gnews-bo-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_do_nation, "gnews-do-nation", "Google News National — Dominican Republic", "Google National — ドミニカ共和国", "osint", "news",
  "https://news.google.com/rss/search?q=Rep%C3%BAblica%20Dominicana&hl=es-419&gl=DO&ceid=DO:es", "es", "[\"news\",\"nation\",\"dominican-republic\",\"google-news\"]", 7555,
  "Google News national coverage for Dominican Republic (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-do-business retired: duplicate of gnews-bo-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kz-world retired: duplicate of gnews-am-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_kz_nation, "gnews-kz-nation", "Google News National — Kazakhstan", "Google National — カザフスタン", "osint", "news",
  "https://news.google.com/rss/search?q=%D0%9A%D0%B0%D0%B7%D0%B0%D1%85%D1%81%D1%82%D0%B0%D0%BD&hl=ru&gl=KZ&ceid=KZ:ru", "ru", "[\"news\",\"nation\",\"kazakhstan\",\"google-news\"]", 7556,
  "Google News national coverage for Kazakhstan (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-kz-business retired: duplicate of gnews-am-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-uz-world retired: duplicate of gnews-am-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_uz_nation, "gnews-uz-nation", "Google News National — Uzbekistan", "Google National — ウズベキスタン", "osint", "news",
  "https://news.google.com/rss/search?q=%D0%A3%D0%B7%D0%B1%D0%B5%D0%BA%D0%B8%D1%81%D1%82%D0%B0%D0%BD&hl=ru&gl=UZ&ceid=UZ:ru", "ru", "[\"news\",\"nation\",\"uzbekistan\",\"google-news\"]", 7557,
  "Google News national coverage for Uzbekistan (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-uz-business retired: duplicate of gnews-am-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-az-world retired: duplicate of gnews-am-world (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_az_nation, "gnews-az-nation", "Google News National — Azerbaijan", "Google National — アゼルバイジャン", "osint", "news",
  "https://news.google.com/rss/search?q=%D0%90%D0%B7%D0%B5%D1%80%D0%B1%D0%B0%D0%B9%D0%B4%D0%B6%D0%B0%D0%BD&hl=ru&gl=AZ&ceid=AZ:ru", "ru", "[\"news\",\"nation\",\"azerbaijan\",\"google-news\"]", 7558,
  "Google News national coverage for Azerbaijan (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-az-business retired: duplicate of gnews-am-business (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-am-world retired: serves the ru edition, already collected by gnews-ru-world (jaccard 0.97). See tests/audit/gnews_mislabel_actions.json. */

RSSX(gn_am_nation, "gnews-am-nation", "Google News National — Armenia", "Google National — アルメニア", "osint", "news",
  "https://news.google.com/rss/search?q=%D0%90%D1%80%D0%BC%D0%B5%D0%BD%D0%B8%D1%8F&hl=ru&gl=AM&ceid=AM:ru", "ru", "[\"news\",\"nation\",\"armenia\",\"google-news\"]", 7559,
  "Google News national coverage for Armenia (country-scoped feed; the NATION topic is empty upstream for this edition)");

/* gnews-am-business retired: serves the ru edition, already collected by gnews-ru-business (jaccard 0.97). See tests/audit/gnews_mislabel_actions.json. */
