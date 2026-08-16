/* Google News per-country topic sections (Technology/Science/Health) — real RSS editions for priority countries, via rss_collect. */
#include "source.h"
#include "lib/rss_atom.h"

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

RSSX(gn_us_technology, "gnews-us-technology", "Google News Technology — United States", "Google Technology — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"technology\",\"united-states\",\"google-news\"]", 7560,
  "Google News Technology section for United States");

RSSX(gn_us_science, "gnews-us-science", "Google News Science — United States", "Google Science — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"science\",\"united-states\",\"google-news\"]", 7561,
  "Google News Science section for United States");

RSSX(gn_us_health, "gnews-us-health", "Google News Health — United States", "Google Health — アメリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-US&gl=US&ceid=US:en", "en", "[\"news\",\"health\",\"united-states\",\"google-news\"]", 7562,
  "Google News Health section for United States");

RSSX(gn_gb_technology, "gnews-gb-technology", "Google News Technology — United Kingdom", "Google Technology — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"technology\",\"united-kingdom\",\"google-news\"]", 7563,
  "Google News Technology section for United Kingdom");

RSSX(gn_gb_science, "gnews-gb-science", "Google News Science — United Kingdom", "Google Science — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"science\",\"united-kingdom\",\"google-news\"]", 7564,
  "Google News Science section for United Kingdom");

RSSX(gn_gb_health, "gnews-gb-health", "Google News Health — United Kingdom", "Google Health — イギリス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-GB&gl=GB&ceid=GB:en", "en", "[\"news\",\"health\",\"united-kingdom\",\"google-news\"]", 7565,
  "Google News Health section for United Kingdom");

RSSX(gn_fr_technology, "gnews-fr-technology", "Google News Technology — France", "Google Technology — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"technology\",\"france\",\"google-news\"]", 7566,
  "Google News Technology section for France");

RSSX(gn_fr_science, "gnews-fr-science", "Google News Science — France", "Google Science — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"science\",\"france\",\"google-news\"]", 7567,
  "Google News Science section for France");

RSSX(gn_fr_health, "gnews-fr-health", "Google News Health — France", "Google Health — フランス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=fr&gl=FR&ceid=FR:fr", "fr", "[\"news\",\"health\",\"france\",\"google-news\"]", 7568,
  "Google News Health section for France");

RSSX(gn_de_technology, "gnews-de-technology", "Google News Technology — Germany", "Google Technology — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"technology\",\"germany\",\"google-news\"]", 7569,
  "Google News Technology section for Germany");

RSSX(gn_de_science, "gnews-de-science", "Google News Science — Germany", "Google Science — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"science\",\"germany\",\"google-news\"]", 7570,
  "Google News Science section for Germany");

RSSX(gn_de_health, "gnews-de-health", "Google News Health — Germany", "Google Health — ドイツ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=de&gl=DE&ceid=DE:de", "de", "[\"news\",\"health\",\"germany\",\"google-news\"]", 7571,
  "Google News Health section for Germany");

RSSX(gn_it_technology, "gnews-it-technology", "Google News Technology — Italy", "Google Technology — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"technology\",\"italy\",\"google-news\"]", 7572,
  "Google News Technology section for Italy");

RSSX(gn_it_science, "gnews-it-science", "Google News Science — Italy", "Google Science — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"science\",\"italy\",\"google-news\"]", 7573,
  "Google News Science section for Italy");

RSSX(gn_it_health, "gnews-it-health", "Google News Health — Italy", "Google Health — イタリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=it&gl=IT&ceid=IT:it", "it", "[\"news\",\"health\",\"italy\",\"google-news\"]", 7574,
  "Google News Health section for Italy");

RSSX(gn_es_technology, "gnews-es-technology", "Google News Technology — Spain", "Google Technology — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"technology\",\"spain\",\"google-news\"]", 7575,
  "Google News Technology section for Spain");

RSSX(gn_es_science, "gnews-es-science", "Google News Science — Spain", "Google Science — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"science\",\"spain\",\"google-news\"]", 7576,
  "Google News Science section for Spain");

RSSX(gn_es_health, "gnews-es-health", "Google News Health — Spain", "Google Health — スペイン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es&gl=ES&ceid=ES:es", "es", "[\"news\",\"health\",\"spain\",\"google-news\"]", 7577,
  "Google News Health section for Spain");

RSSX(gn_jp_technology, "gnews-jp-technology", "Google News Technology — Japan", "Google Technology — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"technology\",\"japan\",\"google-news\"]", 7578,
  "Google News Technology section for Japan");

RSSX(gn_jp_science, "gnews-jp-science", "Google News Science — Japan", "Google Science — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"science\",\"japan\",\"google-news\"]", 7579,
  "Google News Science section for Japan");

RSSX(gn_jp_health, "gnews-jp-health", "Google News Health — Japan", "Google Health — 日本", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=ja&gl=JP&ceid=JP:ja", "ja", "[\"news\",\"health\",\"japan\",\"google-news\"]", 7580,
  "Google News Health section for Japan");

RSSX(gn_kr_technology, "gnews-kr-technology", "Google News Technology — South Korea", "Google Technology — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"technology\",\"south-korea\",\"google-news\"]", 7581,
  "Google News Technology section for South Korea");

RSSX(gn_kr_science, "gnews-kr-science", "Google News Science — South Korea", "Google Science — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"science\",\"south-korea\",\"google-news\"]", 7582,
  "Google News Science section for South Korea");

RSSX(gn_kr_health, "gnews-kr-health", "Google News Health — South Korea", "Google Health — 韓国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=ko&gl=KR&ceid=KR:ko", "ko", "[\"news\",\"health\",\"south-korea\",\"google-news\"]", 7583,
  "Google News Health section for South Korea");

RSSX(gn_cn_technology, "gnews-cn-technology", "Google News Technology — China", "Google Technology — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"technology\",\"china\",\"google-news\"]", 7584,
  "Google News Technology section for China");

RSSX(gn_cn_science, "gnews-cn-science", "Google News Science — China", "Google Science — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"science\",\"china\",\"google-news\"]", 7585,
  "Google News Science section for China");

RSSX(gn_cn_health, "gnews-cn-health", "Google News Health — China", "Google Health — 中国", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=zh-CN&gl=CN&ceid=CN:zh", "zh", "[\"news\",\"health\",\"china\",\"google-news\"]", 7586,
  "Google News Health section for China");

RSSX(gn_tw_technology, "gnews-tw-technology", "Google News Technology — Taiwan", "Google Technology — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"technology\",\"taiwan\",\"google-news\"]", 7587,
  "Google News Technology section for Taiwan");

RSSX(gn_tw_science, "gnews-tw-science", "Google News Science — Taiwan", "Google Science — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"science\",\"taiwan\",\"google-news\"]", 7588,
  "Google News Science section for Taiwan");

RSSX(gn_tw_health, "gnews-tw-health", "Google News Health — Taiwan", "Google Health — 台湾", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=zh-TW&gl=TW&ceid=TW:zh", "zh", "[\"news\",\"health\",\"taiwan\",\"google-news\"]", 7589,
  "Google News Health section for Taiwan");

RSSX(gn_hk_technology, "gnews-hk-technology", "Google News Technology — Hong Kong", "Google Technology — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"technology\",\"hong-kong\",\"google-news\"]", 7590,
  "Google News Technology section for Hong Kong");

RSSX(gn_hk_science, "gnews-hk-science", "Google News Science — Hong Kong", "Google Science — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"science\",\"hong-kong\",\"google-news\"]", 7591,
  "Google News Science section for Hong Kong");

RSSX(gn_hk_health, "gnews-hk-health", "Google News Health — Hong Kong", "Google Health — 香港", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=zh-HK&gl=HK&ceid=HK:zh", "zh", "[\"news\",\"health\",\"hong-kong\",\"google-news\"]", 7592,
  "Google News Health section for Hong Kong");

RSSX(gn_in_technology, "gnews-in-technology", "Google News Technology — India", "Google Technology — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"technology\",\"india\",\"google-news\"]", 7593,
  "Google News Technology section for India");

RSSX(gn_in_science, "gnews-in-science", "Google News Science — India", "Google Science — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"science\",\"india\",\"google-news\"]", 7594,
  "Google News Science section for India");

RSSX(gn_in_health, "gnews-in-health", "Google News Health — India", "Google Health — インド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-IN&gl=IN&ceid=IN:en", "en", "[\"news\",\"health\",\"india\",\"google-news\"]", 7595,
  "Google News Health section for India");

RSSX(gn_ru_technology, "gnews-ru-technology", "Google News Technology — Russia", "Google Technology — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"technology\",\"russia\",\"google-news\"]", 7596,
  "Google News Technology section for Russia");

RSSX(gn_ru_science, "gnews-ru-science", "Google News Science — Russia", "Google Science — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"science\",\"russia\",\"google-news\"]", 7597,
  "Google News Science section for Russia");

RSSX(gn_ru_health, "gnews-ru-health", "Google News Health — Russia", "Google Health — ロシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=ru&gl=RU&ceid=RU:ru", "ru", "[\"news\",\"health\",\"russia\",\"google-news\"]", 7598,
  "Google News Health section for Russia");

RSSX(gn_ua_technology, "gnews-ua-technology", "Google News Technology — Ukraine", "Google Technology — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"technology\",\"ukraine\",\"google-news\"]", 7599,
  "Google News Technology section for Ukraine");

RSSX(gn_ua_science, "gnews-ua-science", "Google News Science — Ukraine", "Google Science — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"science\",\"ukraine\",\"google-news\"]", 7600,
  "Google News Science section for Ukraine");

RSSX(gn_ua_health, "gnews-ua-health", "Google News Health — Ukraine", "Google Health — ウクライナ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=uk&gl=UA&ceid=UA:uk", "uk", "[\"news\",\"health\",\"ukraine\",\"google-news\"]", 7601,
  "Google News Health section for Ukraine");

RSSX(gn_br_technology, "gnews-br-technology", "Google News Technology — Brazil", "Google Technology — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"technology\",\"brazil\",\"google-news\"]", 7602,
  "Google News Technology section for Brazil");

RSSX(gn_br_science, "gnews-br-science", "Google News Science — Brazil", "Google Science — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"science\",\"brazil\",\"google-news\"]", 7603,
  "Google News Science section for Brazil");

RSSX(gn_br_health, "gnews-br-health", "Google News Health — Brazil", "Google Health — ブラジル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=pt-BR&gl=BR&ceid=BR:pt", "pt", "[\"news\",\"health\",\"brazil\",\"google-news\"]", 7604,
  "Google News Health section for Brazil");

RSSX(gn_mx_technology, "gnews-mx-technology", "Google News Technology — Mexico", "Google Technology — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"technology\",\"mexico\",\"google-news\"]", 7605,
  "Google News Technology section for Mexico — Latin American Spanish (es-419) regional edition");

RSSX(gn_mx_science, "gnews-mx-science", "Google News Science — Mexico", "Google Science — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"science\",\"mexico\",\"google-news\"]", 7606,
  "Google News Science section for Mexico — Latin American Spanish (es-419) regional edition");

RSSX(gn_mx_health, "gnews-mx-health", "Google News Health — Mexico", "Google Health — メキシコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es-419&gl=MX&ceid=MX:es", "es", "[\"news\",\"health\",\"mexico\",\"google-news\"]", 7607,
  "Google News Health section for Mexico — Latin American Spanish (es-419) regional edition");

RSSX(gn_ar_technology, "gnews-ar-technology", "Google News Technology — Argentina", "Google Technology — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"technology\",\"argentina\",\"google-news\"]", 7608,
  "Google News Technology section for Argentina — Latin American Spanish (es-419) regional edition");

RSSX(gn_ar_science, "gnews-ar-science", "Google News Science — Argentina", "Google Science — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"science\",\"argentina\",\"google-news\"]", 7609,
  "Google News Science section for Argentina — Latin American Spanish (es-419) regional edition");

RSSX(gn_ar_health, "gnews-ar-health", "Google News Health — Argentina", "Google Health — アルゼンチン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es-419&gl=AR&ceid=AR:es", "es", "[\"news\",\"health\",\"argentina\",\"google-news\"]", 7610,
  "Google News Health section for Argentina — Latin American Spanish (es-419) regional edition");

RSSX(gn_ca_technology, "gnews-ca-technology", "Google News Technology — Canada", "Google Technology — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"technology\",\"canada\",\"google-news\"]", 7611,
  "Google News Technology section for Canada");

RSSX(gn_ca_science, "gnews-ca-science", "Google News Science — Canada", "Google Science — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"science\",\"canada\",\"google-news\"]", 7612,
  "Google News Science section for Canada");

RSSX(gn_ca_health, "gnews-ca-health", "Google News Health — Canada", "Google Health — カナダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-CA&gl=CA&ceid=CA:en", "en", "[\"news\",\"health\",\"canada\",\"google-news\"]", 7613,
  "Google News Health section for Canada");

RSSX(gn_au_technology, "gnews-au-technology", "Google News Technology — Australia", "Google Technology — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"technology\",\"australia\",\"google-news\"]", 7614,
  "Google News Technology section for Australia");

RSSX(gn_au_science, "gnews-au-science", "Google News Science — Australia", "Google Science — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"science\",\"australia\",\"google-news\"]", 7615,
  "Google News Science section for Australia");

RSSX(gn_au_health, "gnews-au-health", "Google News Health — Australia", "Google Health — オーストラリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-AU&gl=AU&ceid=AU:en", "en", "[\"news\",\"health\",\"australia\",\"google-news\"]", 7616,
  "Google News Health section for Australia");

RSSX(gn_nz_technology, "gnews-nz-technology", "Google News Technology — New Zealand", "Google Technology — ニュージーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"technology\",\"new-zealand\",\"google-news\"]", 7617,
  "Google News Technology section for New Zealand");

/* gnews-nz-science retired: duplicate of gnews-ie-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_nz_health, "gnews-nz-health", "Google News Health — New Zealand", "Google Health — ニュージーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-NZ&gl=NZ&ceid=NZ:en", "en", "[\"news\",\"health\",\"new-zealand\",\"google-news\"]", 7618,
  "Google News Health section for New Zealand");

RSSX(gn_tr_technology, "gnews-tr-technology", "Google News Technology — Turkey", "Google Technology — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"technology\",\"turkey\",\"google-news\"]", 7619,
  "Google News Technology section for Turkey");

RSSX(gn_tr_science, "gnews-tr-science", "Google News Science — Turkey", "Google Science — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"science\",\"turkey\",\"google-news\"]", 7620,
  "Google News Science section for Turkey");

RSSX(gn_tr_health, "gnews-tr-health", "Google News Health — Turkey", "Google Health — トルコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=tr&gl=TR&ceid=TR:tr", "tr", "[\"news\",\"health\",\"turkey\",\"google-news\"]", 7621,
  "Google News Health section for Turkey");

RSSX(gn_il_technology, "gnews-il-technology", "Google News Technology — Israel", "Google Technology — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"technology\",\"israel\",\"google-news\"]", 7622,
  "Google News Technology section for Israel");

RSSX(gn_il_science, "gnews-il-science", "Google News Science — Israel", "Google Science — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"science\",\"israel\",\"google-news\"]", 7623,
  "Google News Science section for Israel");

RSSX(gn_il_health, "gnews-il-health", "Google News Health — Israel", "Google Health — イスラエル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=iw&gl=IL&ceid=IL:iw", "iw", "[\"news\",\"health\",\"israel\",\"google-news\"]", 7624,
  "Google News Health section for Israel");

RSSX(gn_sa_technology, "gnews-sa-technology", "Google News Technology — Saudi Arabia", "Google Technology — サウジアラビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"technology\",\"saudi-arabia\",\"google-news\"]", 7625,
  "Google News Technology section for Saudi Arabia");

/* gnews-sa-science retired: duplicate of gnews-ae-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_sa_health, "gnews-sa-health", "Google News Health — Saudi Arabia", "Google Health — サウジアラビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=ar&gl=SA&ceid=SA:ar", "ar", "[\"news\",\"health\",\"saudi-arabia\",\"google-news\"]", 7626,
  "Google News Health section for Saudi Arabia");

RSSX(gn_ae_technology, "gnews-ae-technology", "Google News Technology — United Arab Emirates", "Google Technology — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"technology\",\"united-arab-emirates\",\"google-news\"]", 7627,
  "Google News Technology section for United Arab Emirates");

RSSX(gn_ae_science, "gnews-ae-science", "Google News Science — United Arab Emirates", "Google Science — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"science\",\"united-arab-emirates\",\"google-news\"]", 7628,
  "Google News Science section for United Arab Emirates");

RSSX(gn_ae_health, "gnews-ae-health", "Google News Health — United Arab Emirates", "Google Health — アラブ首長国連邦", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=ar&gl=AE&ceid=AE:ar", "ar", "[\"news\",\"health\",\"united-arab-emirates\",\"google-news\"]", 7629,
  "Google News Health section for United Arab Emirates");

RSSX(gn_eg_technology, "gnews-eg-technology", "Google News Technology — Egypt", "Google Technology — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"technology\",\"egypt\",\"google-news\"]", 7630,
  "Google News Technology section for Egypt");

RSSX(gn_eg_science, "gnews-eg-science", "Google News Science — Egypt", "Google Science — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"science\",\"egypt\",\"google-news\"]", 7631,
  "Google News Science section for Egypt");

RSSX(gn_eg_health, "gnews-eg-health", "Google News Health — Egypt", "Google Health — エジプト", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=ar&gl=EG&ceid=EG:ar", "ar", "[\"news\",\"health\",\"egypt\",\"google-news\"]", 7632,
  "Google News Health section for Egypt");

/* gnews-ir-technology retired: duplicate of gnews-us-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-ir-science retired: duplicate of gnews-us-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-ir-health retired: duplicate of gnews-hr-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_pk_technology, "gnews-pk-technology", "Google News Technology — Pakistan", "Google Technology — パキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"technology\",\"pakistan\",\"google-news\"]", 7633,
  "Google News Technology section for Pakistan");

/* gnews-pk-science retired: duplicate of gnews-ie-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_pk_health, "gnews-pk-health", "Google News Health — Pakistan", "Google Health — パキスタン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-PK&gl=PK&ceid=PK:en", "en", "[\"news\",\"health\",\"pakistan\",\"google-news\"]", 7634,
  "Google News Health section for Pakistan");

RSSX(gn_id_technology, "gnews-id-technology", "Google News Technology — Indonesia", "Google Technology — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"technology\",\"indonesia\",\"google-news\"]", 7635,
  "Google News Technology section for Indonesia");

RSSX(gn_id_science, "gnews-id-science", "Google News Science — Indonesia", "Google Science — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"science\",\"indonesia\",\"google-news\"]", 7636,
  "Google News Science section for Indonesia");

RSSX(gn_id_health, "gnews-id-health", "Google News Health — Indonesia", "Google Health — インドネシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=id&gl=ID&ceid=ID:id", "id", "[\"news\",\"health\",\"indonesia\",\"google-news\"]", 7637,
  "Google News Health section for Indonesia");

RSSX(gn_th_technology, "gnews-th-technology", "Google News Technology — Thailand", "Google Technology — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"technology\",\"thailand\",\"google-news\"]", 7638,
  "Google News Technology section for Thailand");

RSSX(gn_th_science, "gnews-th-science", "Google News Science — Thailand", "Google Science — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"science\",\"thailand\",\"google-news\"]", 7639,
  "Google News Science section for Thailand");

RSSX(gn_th_health, "gnews-th-health", "Google News Health — Thailand", "Google Health — タイ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=th&gl=TH&ceid=TH:th", "th", "[\"news\",\"health\",\"thailand\",\"google-news\"]", 7640,
  "Google News Health section for Thailand");

RSSX(gn_vn_technology, "gnews-vn-technology", "Google News Technology — Vietnam", "Google Technology — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"technology\",\"vietnam\",\"google-news\"]", 7641,
  "Google News Technology section for Vietnam");

RSSX(gn_vn_science, "gnews-vn-science", "Google News Science — Vietnam", "Google Science — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"science\",\"vietnam\",\"google-news\"]", 7642,
  "Google News Science section for Vietnam");

RSSX(gn_vn_health, "gnews-vn-health", "Google News Health — Vietnam", "Google Health — ベトナム", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=vi&gl=VN&ceid=VN:vi", "vi", "[\"news\",\"health\",\"vietnam\",\"google-news\"]", 7643,
  "Google News Health section for Vietnam");

RSSX(gn_ph_technology, "gnews-ph-technology", "Google News Technology — Philippines", "Google Technology — フィリピン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"technology\",\"philippines\",\"google-news\"]", 7644,
  "Google News Technology section for Philippines");

/* gnews-ph-science retired: duplicate of gnews-ie-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_ph_health, "gnews-ph-health", "Google News Health — Philippines", "Google Health — フィリピン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-PH&gl=PH&ceid=PH:en", "en", "[\"news\",\"health\",\"philippines\",\"google-news\"]", 7645,
  "Google News Health section for Philippines");

RSSX(gn_sg_technology, "gnews-sg-technology", "Google News Technology — Singapore", "Google Technology — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"technology\",\"singapore\",\"google-news\"]", 7646,
  "Google News Technology section for Singapore");

RSSX(gn_sg_science, "gnews-sg-science", "Google News Science — Singapore", "Google Science — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"science\",\"singapore\",\"google-news\"]", 7647,
  "Google News Science section for Singapore");

RSSX(gn_sg_health, "gnews-sg-health", "Google News Health — Singapore", "Google Health — シンガポール", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-SG&gl=SG&ceid=SG:en", "en", "[\"news\",\"health\",\"singapore\",\"google-news\"]", 7648,
  "Google News Health section for Singapore");

RSSX(gn_my_technology, "gnews-my-technology", "Google News Technology — Malaysia", "Google Technology — マレーシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"technology\",\"malaysia\",\"google-news\"]", 7649,
  "Google News Technology section for Malaysia");

/* gnews-my-science retired: duplicate of gnews-ke-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_my_health, "gnews-my-health", "Google News Health — Malaysia", "Google Health — マレーシア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-MY&gl=MY&ceid=MY:en", "en", "[\"news\",\"health\",\"malaysia\",\"google-news\"]", 7650,
  "Google News Health section for Malaysia");

RSSX(gn_nl_technology, "gnews-nl-technology", "Google News Technology — Netherlands", "Google Technology — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"technology\",\"netherlands\",\"google-news\"]", 7651,
  "Google News Technology section for Netherlands");

RSSX(gn_nl_science, "gnews-nl-science", "Google News Science — Netherlands", "Google Science — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"science\",\"netherlands\",\"google-news\"]", 7652,
  "Google News Science section for Netherlands");

RSSX(gn_nl_health, "gnews-nl-health", "Google News Health — Netherlands", "Google Health — オランダ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=nl&gl=NL&ceid=NL:nl", "nl", "[\"news\",\"health\",\"netherlands\",\"google-news\"]", 7653,
  "Google News Health section for Netherlands");

RSSX(gn_be_technology, "gnews-be-technology", "Google News Technology — Belgium", "Google Technology — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"technology\",\"belgium\",\"google-news\"]", 7654,
  "Google News Technology section for Belgium");

RSSX(gn_be_science, "gnews-be-science", "Google News Science — Belgium", "Google Science — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"science\",\"belgium\",\"google-news\"]", 7655,
  "Google News Science section for Belgium");

RSSX(gn_be_health, "gnews-be-health", "Google News Health — Belgium", "Google Health — ベルギー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=fr&gl=BE&ceid=BE:fr", "fr", "[\"news\",\"health\",\"belgium\",\"google-news\"]", 7656,
  "Google News Health section for Belgium");

RSSX(gn_ch_technology, "gnews-ch-technology", "Google News Technology — Switzerland", "Google Technology — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"technology\",\"switzerland\",\"google-news\"]", 7657,
  "Google News Technology section for Switzerland");

RSSX(gn_ch_science, "gnews-ch-science", "Google News Science — Switzerland", "Google Science — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"science\",\"switzerland\",\"google-news\"]", 7658,
  "Google News Science section for Switzerland");

RSSX(gn_ch_health, "gnews-ch-health", "Google News Health — Switzerland", "Google Health — スイス", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=de&gl=CH&ceid=CH:de", "de", "[\"news\",\"health\",\"switzerland\",\"google-news\"]", 7659,
  "Google News Health section for Switzerland");

RSSX(gn_at_technology, "gnews-at-technology", "Google News Technology — Austria", "Google Technology — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"technology\",\"austria\",\"google-news\"]", 7660,
  "Google News Technology section for Austria");

RSSX(gn_at_science, "gnews-at-science", "Google News Science — Austria", "Google Science — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"science\",\"austria\",\"google-news\"]", 7661,
  "Google News Science section for Austria");

RSSX(gn_at_health, "gnews-at-health", "Google News Health — Austria", "Google Health — オーストリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=de&gl=AT&ceid=AT:de", "de", "[\"news\",\"health\",\"austria\",\"google-news\"]", 7662,
  "Google News Health section for Austria");

RSSX(gn_se_technology, "gnews-se-technology", "Google News Technology — Sweden", "Google Technology — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"technology\",\"sweden\",\"google-news\"]", 7663,
  "Google News Technology section for Sweden");

RSSX(gn_se_science, "gnews-se-science", "Google News Science — Sweden", "Google Science — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"science\",\"sweden\",\"google-news\"]", 7664,
  "Google News Science section for Sweden");

RSSX(gn_se_health, "gnews-se-health", "Google News Health — Sweden", "Google Health — スウェーデン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=sv&gl=SE&ceid=SE:sv", "sv", "[\"news\",\"health\",\"sweden\",\"google-news\"]", 7665,
  "Google News Health section for Sweden");

RSSX(gn_no_technology, "gnews-no-technology", "Google News Technology — Norway", "Google Technology — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"technology\",\"norway\",\"google-news\"]", 7666,
  "Google News Technology section for Norway");

RSSX(gn_no_science, "gnews-no-science", "Google News Science — Norway", "Google Science — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"science\",\"norway\",\"google-news\"]", 7667,
  "Google News Science section for Norway");

RSSX(gn_no_health, "gnews-no-health", "Google News Health — Norway", "Google Health — ノルウェー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=no&gl=NO&ceid=NO:no", "no", "[\"news\",\"health\",\"norway\",\"google-news\"]", 7668,
  "Google News Health section for Norway");

/* gnews-dk-technology retired: duplicate of gnews-no-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-dk-science retired: duplicate of gnews-no-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-dk-health retired: duplicate of gnews-no-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_fi_technology, "gnews-fi-technology", "Google News Technology — Finland", "Google Technology — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"technology\",\"finland\",\"google-news\"]", 7669,
  "Google News Technology section for Finland");

RSSX(gn_fi_science, "gnews-fi-science", "Google News Science — Finland", "Google Science — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"science\",\"finland\",\"google-news\"]", 7670,
  "Google News Science section for Finland");

RSSX(gn_fi_health, "gnews-fi-health", "Google News Health — Finland", "Google Health — フィンランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=fi&gl=FI&ceid=FI:fi", "fi", "[\"news\",\"health\",\"finland\",\"google-news\"]", 7671,
  "Google News Health section for Finland");

RSSX(gn_pl_technology, "gnews-pl-technology", "Google News Technology — Poland", "Google Technology — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"technology\",\"poland\",\"google-news\"]", 7672,
  "Google News Technology section for Poland");

RSSX(gn_pl_science, "gnews-pl-science", "Google News Science — Poland", "Google Science — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"science\",\"poland\",\"google-news\"]", 7673,
  "Google News Science section for Poland");

RSSX(gn_pl_health, "gnews-pl-health", "Google News Health — Poland", "Google Health — ポーランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=pl&gl=PL&ceid=PL:pl", "pl", "[\"news\",\"health\",\"poland\",\"google-news\"]", 7674,
  "Google News Health section for Poland");

RSSX(gn_cz_technology, "gnews-cz-technology", "Google News Technology — Czechia", "Google Technology — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"technology\",\"czechia\",\"google-news\"]", 7675,
  "Google News Technology section for Czechia");

RSSX(gn_cz_science, "gnews-cz-science", "Google News Science — Czechia", "Google Science — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"science\",\"czechia\",\"google-news\"]", 7676,
  "Google News Science section for Czechia");

RSSX(gn_cz_health, "gnews-cz-health", "Google News Health — Czechia", "Google Health — チェコ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=cs&gl=CZ&ceid=CZ:cs", "cs", "[\"news\",\"health\",\"czechia\",\"google-news\"]", 7677,
  "Google News Health section for Czechia");

RSSX(gn_gr_technology, "gnews-gr-technology", "Google News Technology — Greece", "Google Technology — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"technology\",\"greece\",\"google-news\"]", 7678,
  "Google News Technology section for Greece");

RSSX(gn_gr_science, "gnews-gr-science", "Google News Science — Greece", "Google Science — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"science\",\"greece\",\"google-news\"]", 7679,
  "Google News Science section for Greece");

RSSX(gn_gr_health, "gnews-gr-health", "Google News Health — Greece", "Google Health — ギリシャ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=el&gl=GR&ceid=GR:el", "el", "[\"news\",\"health\",\"greece\",\"google-news\"]", 7680,
  "Google News Health section for Greece");

RSSX(gn_pt_technology, "gnews-pt-technology", "Google News Technology — Portugal", "Google Technology — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"technology\",\"portugal\",\"google-news\"]", 7681,
  "Google News Technology section for Portugal");

RSSX(gn_pt_science, "gnews-pt-science", "Google News Science — Portugal", "Google Science — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"science\",\"portugal\",\"google-news\"]", 7682,
  "Google News Science section for Portugal");

RSSX(gn_pt_health, "gnews-pt-health", "Google News Health — Portugal", "Google Health — ポルトガル", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=pt-PT&gl=PT&ceid=PT:pt", "pt", "[\"news\",\"health\",\"portugal\",\"google-news\"]", 7683,
  "Google News Health section for Portugal");

RSSX(gn_ie_technology, "gnews-ie-technology", "Google News Technology — Ireland", "Google Technology — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"technology\",\"ireland\",\"google-news\"]", 7684,
  "Google News Technology section for Ireland");

RSSX(gn_ie_science, "gnews-ie-science", "Google News Science — Ireland", "Google Science — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"science\",\"ireland\",\"google-news\"]", 7685,
  "Google News Science section for Ireland");

RSSX(gn_ie_health, "gnews-ie-health", "Google News Health — Ireland", "Google Health — アイルランド", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-IE&gl=IE&ceid=IE:en", "en", "[\"news\",\"health\",\"ireland\",\"google-news\"]", 7686,
  "Google News Health section for Ireland");

RSSX(gn_za_technology, "gnews-za-technology", "Google News Technology — South Africa", "Google Technology — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"technology\",\"south-africa\",\"google-news\"]", 7687,
  "Google News Technology section for South Africa");

RSSX(gn_za_science, "gnews-za-science", "Google News Science — South Africa", "Google Science — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"science\",\"south-africa\",\"google-news\"]", 7688,
  "Google News Science section for South Africa");

RSSX(gn_za_health, "gnews-za-health", "Google News Health — South Africa", "Google Health — 南アフリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-ZA&gl=ZA&ceid=ZA:en", "en", "[\"news\",\"health\",\"south-africa\",\"google-news\"]", 7689,
  "Google News Health section for South Africa");

RSSX(gn_ng_technology, "gnews-ng-technology", "Google News Technology — Nigeria", "Google Technology — ナイジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"technology\",\"nigeria\",\"google-news\"]", 7690,
  "Google News Technology section for Nigeria");

/* gnews-ng-science retired: duplicate of gnews-ke-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_ng_health, "gnews-ng-health", "Google News Health — Nigeria", "Google Health — ナイジェリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-NG&gl=NG&ceid=NG:en", "en", "[\"news\",\"health\",\"nigeria\",\"google-news\"]", 7691,
  "Google News Health section for Nigeria");

RSSX(gn_ke_technology, "gnews-ke-technology", "Google News Technology — Kenya", "Google Technology — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"technology\",\"kenya\",\"google-news\"]", 7692,
  "Google News Technology section for Kenya");

RSSX(gn_ke_science, "gnews-ke-science", "Google News Science — Kenya", "Google Science — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"science\",\"kenya\",\"google-news\"]", 7693,
  "Google News Science section for Kenya");

RSSX(gn_ke_health, "gnews-ke-health", "Google News Health — Kenya", "Google Health — ケニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=en-KE&gl=KE&ceid=KE:en", "en", "[\"news\",\"health\",\"kenya\",\"google-news\"]", 7694,
  "Google News Health section for Kenya");

RSSX(gn_co_technology, "gnews-co-technology", "Google News Technology — Colombia", "Google Technology — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"technology\",\"colombia\",\"google-news\"]", 7695,
  "Google News Technology section for Colombia — Latin American Spanish (es-419) regional edition");

RSSX(gn_co_science, "gnews-co-science", "Google News Science — Colombia", "Google Science — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"science\",\"colombia\",\"google-news\"]", 7696,
  "Google News Science section for Colombia — Latin American Spanish (es-419) regional edition");

RSSX(gn_co_health, "gnews-co-health", "Google News Health — Colombia", "Google Health — コロンビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es-419&gl=CO&ceid=CO:es", "es", "[\"news\",\"health\",\"colombia\",\"google-news\"]", 7697,
  "Google News Health section for Colombia — Latin American Spanish (es-419) regional edition");

RSSX(gn_cl_technology, "gnews-cl-technology", "Google News Technology — Chile", "Google Technology — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"technology\",\"chile\",\"google-news\"]", 7698,
  "Google News Technology section for Chile — Latin American Spanish (es-419) regional edition");

RSSX(gn_cl_science, "gnews-cl-science", "Google News Science — Chile", "Google Science — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"science\",\"chile\",\"google-news\"]", 7699,
  "Google News Science section for Chile — Latin American Spanish (es-419) regional edition");

RSSX(gn_cl_health, "gnews-cl-health", "Google News Health — Chile", "Google Health — チリ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es-419&gl=CL&ceid=CL:es", "es", "[\"news\",\"health\",\"chile\",\"google-news\"]", 7700,
  "Google News Health section for Chile — Latin American Spanish (es-419) regional edition");

RSSX(gn_pe_technology, "gnews-pe-technology", "Google News Technology — Peru", "Google Technology — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"technology\",\"peru\",\"google-news\"]", 7701,
  "Google News Technology section for Peru — Latin American Spanish (es-419) regional edition");

RSSX(gn_pe_science, "gnews-pe-science", "Google News Science — Peru", "Google Science — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"science\",\"peru\",\"google-news\"]", 7702,
  "Google News Science section for Peru — Latin American Spanish (es-419) regional edition");

RSSX(gn_pe_health, "gnews-pe-health", "Google News Health — Peru", "Google Health — ペルー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es-419&gl=PE&ceid=PE:es", "es", "[\"news\",\"health\",\"peru\",\"google-news\"]", 7703,
  "Google News Health section for Peru — Latin American Spanish (es-419) regional edition");

RSSX(gn_ro_technology, "gnews-ro-technology", "Google News Technology — Romania", "Google Technology — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"technology\",\"romania\",\"google-news\"]", 7704,
  "Google News Technology section for Romania");

RSSX(gn_ro_science, "gnews-ro-science", "Google News Science — Romania", "Google Science — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"science\",\"romania\",\"google-news\"]", 7705,
  "Google News Science section for Romania");

RSSX(gn_ro_health, "gnews-ro-health", "Google News Health — Romania", "Google Health — ルーマニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=ro&gl=RO&ceid=RO:ro", "ro", "[\"news\",\"health\",\"romania\",\"google-news\"]", 7706,
  "Google News Health section for Romania");

RSSX(gn_hu_technology, "gnews-hu-technology", "Google News Technology — Hungary", "Google Technology — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"technology\",\"hungary\",\"google-news\"]", 7707,
  "Google News Technology section for Hungary");

RSSX(gn_hu_science, "gnews-hu-science", "Google News Science — Hungary", "Google Science — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"science\",\"hungary\",\"google-news\"]", 7708,
  "Google News Science section for Hungary");

RSSX(gn_hu_health, "gnews-hu-health", "Google News Health — Hungary", "Google Health — ハンガリー", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=hu&gl=HU&ceid=HU:hu", "hu", "[\"news\",\"health\",\"hungary\",\"google-news\"]", 7709,
  "Google News Health section for Hungary");

RSSX(gn_bg_technology, "gnews-bg-technology", "Google News Technology — Bulgaria", "Google Technology — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"technology\",\"bulgaria\",\"google-news\"]", 7710,
  "Google News Technology section for Bulgaria");

RSSX(gn_bg_science, "gnews-bg-science", "Google News Science — Bulgaria", "Google Science — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"science\",\"bulgaria\",\"google-news\"]", 7711,
  "Google News Science section for Bulgaria");

RSSX(gn_bg_health, "gnews-bg-health", "Google News Health — Bulgaria", "Google Health — ブルガリア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=bg&gl=BG&ceid=BG:bg", "bg", "[\"news\",\"health\",\"bulgaria\",\"google-news\"]", 7712,
  "Google News Health section for Bulgaria");

RSSX(gn_rs_technology, "gnews-rs-technology", "Google News Technology — Serbia", "Google Technology — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"technology\",\"serbia\",\"google-news\"]", 7713,
  "Google News Technology section for Serbia");

RSSX(gn_rs_science, "gnews-rs-science", "Google News Science — Serbia", "Google Science — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"science\",\"serbia\",\"google-news\"]", 7714,
  "Google News Science section for Serbia");

RSSX(gn_rs_health, "gnews-rs-health", "Google News Health — Serbia", "Google Health — セルビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=sr&gl=RS&ceid=RS:sr", "sr", "[\"news\",\"health\",\"serbia\",\"google-news\"]", 7715,
  "Google News Health section for Serbia");

/* gnews-hr-technology retired: duplicate of gnews-us-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-hr-science retired: duplicate of gnews-us-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-hr-health retired: serves the us edition, already collected by gnews-us-health (jaccard 1.00). See tests/audit/gnews_mislabel_actions.json. */

RSSX(gn_sk_technology, "gnews-sk-technology", "Google News Technology — Slovakia", "Google Technology — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"technology\",\"slovakia\",\"google-news\"]", 7716,
  "Google News Technology section for Slovakia");

RSSX(gn_sk_science, "gnews-sk-science", "Google News Science — Slovakia", "Google Science — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"science\",\"slovakia\",\"google-news\"]", 7717,
  "Google News Science section for Slovakia");

RSSX(gn_sk_health, "gnews-sk-health", "Google News Health — Slovakia", "Google Health — スロバキア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=sk&gl=SK&ceid=SK:sk", "sk", "[\"news\",\"health\",\"slovakia\",\"google-news\"]", 7718,
  "Google News Health section for Slovakia");

RSSX(gn_si_technology, "gnews-si-technology", "Google News Technology — Slovenia", "Google Technology — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"technology\",\"slovenia\",\"google-news\"]", 7719,
  "Google News Technology section for Slovenia");

RSSX(gn_si_science, "gnews-si-science", "Google News Science — Slovenia", "Google Science — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"science\",\"slovenia\",\"google-news\"]", 7720,
  "Google News Science section for Slovenia");

RSSX(gn_si_health, "gnews-si-health", "Google News Health — Slovenia", "Google Health — スロベニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=sl&gl=SI&ceid=SI:sl", "sl", "[\"news\",\"health\",\"slovenia\",\"google-news\"]", 7721,
  "Google News Health section for Slovenia");

RSSX(gn_lt_technology, "gnews-lt-technology", "Google News Technology — Lithuania", "Google Technology — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"technology\",\"lithuania\",\"google-news\"]", 7722,
  "Google News Technology section for Lithuania");

RSSX(gn_lt_science, "gnews-lt-science", "Google News Science — Lithuania", "Google Science — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"science\",\"lithuania\",\"google-news\"]", 7723,
  "Google News Science section for Lithuania");

RSSX(gn_lt_health, "gnews-lt-health", "Google News Health — Lithuania", "Google Health — リトアニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=lt&gl=LT&ceid=LT:lt", "lt", "[\"news\",\"health\",\"lithuania\",\"google-news\"]", 7724,
  "Google News Health section for Lithuania");

RSSX(gn_lv_technology, "gnews-lv-technology", "Google News Technology — Latvia", "Google Technology — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"technology\",\"latvia\",\"google-news\"]", 7725,
  "Google News Technology section for Latvia");

RSSX(gn_lv_science, "gnews-lv-science", "Google News Science — Latvia", "Google Science — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"science\",\"latvia\",\"google-news\"]", 7726,
  "Google News Science section for Latvia");

RSSX(gn_lv_health, "gnews-lv-health", "Google News Health — Latvia", "Google Health — ラトビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=lv&gl=LV&ceid=LV:lv", "lv", "[\"news\",\"health\",\"latvia\",\"google-news\"]", 7727,
  "Google News Health section for Latvia");

RSSX(gn_ee_technology, "gnews-ee-technology", "Google News Technology — Estonia", "Google Technology — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"technology\",\"estonia\",\"google-news\"]", 7728,
  "Google News Technology section for Estonia");

RSSX(gn_ee_science, "gnews-ee-science", "Google News Science — Estonia", "Google Science — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"science\",\"estonia\",\"google-news\"]", 7729,
  "Google News Science section for Estonia");

RSSX(gn_ee_health, "gnews-ee-health", "Google News Health — Estonia", "Google Health — エストニア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=et&gl=EE&ceid=EE:et", "et", "[\"news\",\"health\",\"estonia\",\"google-news\"]", 7730,
  "Google News Health section for Estonia");

/* gnews-is-technology retired: duplicate of gnews-us-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-is-science retired: duplicate of gnews-us-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-is-health retired: duplicate of gnews-hr-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_bd_technology, "gnews-bd-technology", "Google News Technology — Bangladesh", "Google Technology — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"technology\",\"bangladesh\",\"google-news\"]", 7731,
  "Google News Technology section for Bangladesh");

RSSX(gn_bd_science, "gnews-bd-science", "Google News Science — Bangladesh", "Google Science — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"science\",\"bangladesh\",\"google-news\"]", 7732,
  "Google News Science section for Bangladesh");

RSSX(gn_bd_health, "gnews-bd-health", "Google News Health — Bangladesh", "Google Health — バングラデシュ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=bn&gl=BD&ceid=BD:bn", "bn", "[\"news\",\"health\",\"bangladesh\",\"google-news\"]", 7733,
  "Google News Health section for Bangladesh");

/* gnews-lk-technology retired: duplicate of gnews-us-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-lk-science retired: duplicate of gnews-us-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-lk-health retired: duplicate of gnews-hr-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-np-technology retired: duplicate of gnews-us-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-np-science retired: duplicate of gnews-us-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-np-health retired: duplicate of gnews-hr-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-mm-technology retired: duplicate of gnews-us-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-mm-science retired: duplicate of gnews-us-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-mm-health retired: duplicate of gnews-hr-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kh-technology retired: duplicate of gnews-us-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kh-science retired: duplicate of gnews-us-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kh-health retired: duplicate of gnews-hr-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-qa-technology retired: duplicate of gnews-eg-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-qa-science retired: duplicate of gnews-eg-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-qa-health retired: duplicate of gnews-eg-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kw-technology retired: duplicate of gnews-eg-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kw-science retired: duplicate of gnews-eg-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kw-health retired: duplicate of gnews-eg-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-iq-technology retired: duplicate of gnews-eg-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-iq-science retired: duplicate of gnews-eg-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-iq-health retired: duplicate of gnews-eg-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-jo-technology retired: duplicate of gnews-eg-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-jo-science retired: duplicate of gnews-eg-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-jo-health retired: duplicate of gnews-eg-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_lb_technology, "gnews-lb-technology", "Google News Technology — Lebanon", "Google Technology — レバノン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"technology\",\"lebanon\",\"google-news\"]", 7734,
  "Google News Technology section for Lebanon");

RSSX(gn_lb_science, "gnews-lb-science", "Google News Science — Lebanon", "Google Science — レバノン", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=ar&gl=LB&ceid=LB:ar", "ar", "[\"news\",\"science\",\"lebanon\",\"google-news\"]", 7735,
  "Google News Science section for Lebanon");

/* gnews-lb-health retired: duplicate of gnews-ae-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-ma-technology retired: duplicate of gnews-eg-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-ma-science retired: duplicate of gnews-eg-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-ma-health retired: duplicate of gnews-eg-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-dz-technology retired: duplicate of gnews-eg-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-dz-science retired: duplicate of gnews-eg-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-dz-health retired: duplicate of gnews-eg-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-tn-technology retired: duplicate of gnews-eg-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-tn-science retired: duplicate of gnews-eg-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-tn-health retired: duplicate of gnews-eg-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_ve_technology, "gnews-ve-technology", "Google News Technology — Venezuela", "Google Technology — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"technology\",\"venezuela\",\"google-news\"]", 7736,
  "Google News Technology section for Venezuela — Latin American Spanish (es-419) regional edition");

RSSX(gn_ve_science, "gnews-ve-science", "Google News Science — Venezuela", "Google Science — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"science\",\"venezuela\",\"google-news\"]", 7737,
  "Google News Science section for Venezuela — Latin American Spanish (es-419) regional edition");

RSSX(gn_ve_health, "gnews-ve-health", "Google News Health — Venezuela", "Google Health — ベネズエラ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es-419&gl=VE&ceid=VE:es", "es", "[\"news\",\"health\",\"venezuela\",\"google-news\"]", 7738,
  "Google News Health section for Venezuela — Latin American Spanish (es-419) regional edition");

/* gnews-ec-technology retired: duplicate of gnews-bo-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-ec-science retired: duplicate of gnews-cr-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-ec-health retired: duplicate of gnews-bo-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_es419_technology, "gnews-es419-technology", "Google News Technology — Bolivia", "Google Technology — ボリビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"technology\",\"bolivia\",\"google-news\"]", 7739,
  "Google News Technology section for Bolivia — Latin American Spanish (es-419) regional edition");

/* gnews-bo-science retired: duplicate of gnews-es419-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_es419_health, "gnews-es419-health", "Google News Health — Bolivia", "Google Health — ボリビア", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es-419&gl=BO&ceid=BO:es", "es", "[\"news\",\"health\",\"bolivia\",\"google-news\"]", 7740,
  "Google News Health section for Bolivia — Latin American Spanish (es-419) regional edition");

/* gnews-py-technology retired: duplicate of gnews-bo-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-py-science retired: duplicate of gnews-cr-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-py-health retired: duplicate of gnews-bo-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-uy-technology retired: duplicate of gnews-bo-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-uy-science retired: duplicate of gnews-cr-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-uy-health retired: duplicate of gnews-bo-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-cr-technology retired: duplicate of gnews-bo-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_es419_science, "gnews-es419-science", "Google News Science — Costa Rica", "Google Science — コスタリカ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es-419&gl=CR&ceid=CR:es", "es", "[\"news\",\"science\",\"costa-rica\",\"google-news\"]", 7741,
  "Google News Science section for Costa Rica — Latin American Spanish (es-419) regional edition");

/* gnews-cr-health retired: duplicate of gnews-bo-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-pa-technology retired: duplicate of gnews-bo-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-pa-science retired: duplicate of gnews-cr-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-pa-health retired: duplicate of gnews-bo-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-gt-technology retired: duplicate of gnews-bo-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-gt-science retired: duplicate of gnews-cr-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-gt-health retired: duplicate of gnews-bo-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

RSSX(gn_cu_technology, "gnews-cu-technology", "Google News Technology — Cuba", "Google Technology — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"technology\",\"cuba\",\"google-news\"]", 7742,
  "Google News Technology section for Cuba — Latin American Spanish (es-419) regional edition");

RSSX(gn_cu_science, "gnews-cu-science", "Google News Science — Cuba", "Google Science — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"science\",\"cuba\",\"google-news\"]", 7743,
  "Google News Science section for Cuba — Latin American Spanish (es-419) regional edition");

RSSX(gn_cu_health, "gnews-cu-health", "Google News Health — Cuba", "Google Health — キューバ", "osint", "news",
  "https://news.google.com/rss/headlines/section/topic/HEALTH?hl=es-419&gl=CU&ceid=CU:es", "es", "[\"news\",\"health\",\"cuba\",\"google-news\"]", 7744,
  "Google News Health section for Cuba — Latin American Spanish (es-419) regional edition");

/* gnews-do-technology retired: duplicate of gnews-bo-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-do-science retired: duplicate of gnews-cr-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-do-health retired: duplicate of gnews-bo-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kz-technology retired: duplicate of gnews-am-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kz-science retired: duplicate of gnews-ru-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-kz-health retired: duplicate of gnews-ru-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-uz-technology retired: duplicate of gnews-am-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-uz-science retired: duplicate of gnews-am-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-uz-health retired: duplicate of gnews-ru-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-az-technology retired: duplicate of gnews-am-technology (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-az-science retired: duplicate of gnews-am-science (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-az-health retired: duplicate of gnews-ru-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */

/* gnews-am-technology retired: serves the ru edition, already collected by gnews-ru-technology (jaccard 1.00). See tests/audit/gnews_mislabel_actions.json. */

/* gnews-am-science retired: serves the ru edition, already collected by gnews-ru-science (jaccard 1.00). See tests/audit/gnews_mislabel_actions.json. */

/* gnews-am-health retired: duplicate of gnews-ru-health (identical guid set; Google serves one generic edition for locales it does not publish). See tests/audit/gnews_dupes.json. */
