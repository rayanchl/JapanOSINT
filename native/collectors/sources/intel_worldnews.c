/* collectors/sources/intel_worldnews.c — global open-source news intelligence.
 * Wire-service / national-outlet world desks spanning every region, so an entity
 * pivot or scheduled sweep has broad worldwide press coverage to correlate
 * against the JP news collectors (nhk, kyodo, yahoo). Real RSS/Atom via
 * rss_collect; each article is one FTS-indexed intel row. Scheduled hourly. */
#include "source.h"
#include "lib/rss_atom.h"

#define COLL     "osint"
#define INTERVAL 3600

#include "_source_macros.inc"

RSS(wn_bbc, "bbc-world", "BBC News (World)",
  "BBC ワールド", "news",
  "https://feeds.bbci.co.uk/news/world/rss.xml", "en",
  "[\"news\",\"world\",\"osint\"]",
  "BBC News world desk — global breaking news");

RSS(wn_guardian, "guardian-world", "The Guardian (World)",
  "ガーディアン ワールド", "news",
  "https://www.theguardian.com/world/rss", "en",
  "[\"news\",\"world\",\"osint\"]",
  "The Guardian world coverage — international news and analysis");

RSS(wn_aljazeera, "aljazeera", "Al Jazeera English",
  "アルジャジーラ", "news",
  "https://www.aljazeera.com/xml/rss/all.xml", "en",
  "[\"news\",\"world\",\"mena\"]",
  "Al Jazeera English — Middle East and global news");

RSS(wn_dw, "deutsche-welle", "Deutsche Welle (English)",
  "ドイチェ・ヴェレ", "news",
  "https://rss.dw.com/rdf/rss-en-all", "en",
  "[\"news\",\"world\",\"europe\"]",
  "Deutsche Welle — German international broadcaster, world news");

RSS(wn_france24, "france24", "France 24 (English)",
  "フランス24", "news",
  "https://www.france24.com/en/rss", "en",
  "[\"news\",\"world\",\"europe\"]",
  "France 24 — French international broadcaster, global news");

RSS(wn_npr, "npr-world", "NPR (World)",
  "NPR ワールド", "news",
  "https://feeds.npr.org/1004/rss.xml", "en",
  "[\"news\",\"world\",\"usa\"]",
  "NPR world desk — international news and analysis");

RSS(wn_cnn, "cnn-world", "CNN (World)",
  "CNN ワールド", "news",
  "http://rss.cnn.com/rss/edition_world.rss", "en",
  "[\"news\",\"world\",\"usa\"]",
  "CNN International world edition");

RSS(wn_nyt, "nyt-world", "New York Times (World)",
  "ニューヨーク・タイムズ", "news",
  "https://rss.nytimes.com/services/xml/rss/nyt/World.xml", "en",
  "[\"news\",\"world\",\"usa\"]",
  "The New York Times world desk");

RSS(wn_ap_top, "ap-topnews", "AP News (Top)",
  "AP通信", "news",
  /* rsshub.app itself now 403s every datacentre IP (Cloudflare); AP publishes
   * no first-party RSS any more, so this uses another public RSSHub instance
   * running the same apnews route (verified: 136 <item>s). */
  "https://rsshub.rssforever.com/apnews/topics/apf-topnews", "en",
  "[\"news\",\"world\",\"wire\"]",
  "Associated Press top news (via RSSHub aggregation)");

RSS(wn_scmp_top, "scmp-topnews", "South China Morning Post (Top)",
  "SCMP トップ", "news",
  "https://www.scmp.com/rss/91/feed", "en",
  "[\"news\",\"china\",\"asia\"]",
  "South China Morning Post top stories");

RSS(wn_toi, "times-of-india", "Times of India (Top)",
  "タイムズ・オブ・インディア", "news",
  "https://timesofindia.indiatimes.com/rssfeedstopstories.cms", "en",
  "[\"news\",\"india\",\"asia\"]",
  "Times of India — India and South Asia top stories");

RSS(wn_hindu, "the-hindu-intl", "The Hindu (International)",
  "ザ・ヒンドゥー", "news",
  "https://www.thehindu.com/news/international/feeder/default.rss", "en",
  "[\"news\",\"india\",\"world\"]",
  "The Hindu international desk — South Asian perspective on world news");

RSS(wn_straits, "straits-times", "The Straits Times (World)",
  "ストレーツ・タイムズ", "news",
  "https://www.straitstimes.com/news/world/rss.xml", "en",
  "[\"news\",\"singapore\",\"asia\"]",
  "The Straits Times world desk — Southeast Asia perspective");

RSS(wn_koreaherald, "korea-herald", "The Korea Herald",
  "コリア・ヘラルド", "news",
  "http://www.koreaherald.com/rss/newsAll", "en",
  "[\"news\",\"korea\",\"asia\"]",
  "The Korea Herald — South Korea national and international news");

RSS(wn_jpost, "jerusalem-post", "The Jerusalem Post",
  "エルサレム・ポスト", "news",
  "https://www.jpost.com/rss/rssfeedsheadlines.aspx", "en",
  "[\"news\",\"israel\",\"mena\"]",
  "The Jerusalem Post — Israel and Middle East news");

RSS(wn_toisrael, "times-of-israel", "The Times of Israel",
  "タイムズ・オブ・イスラエル", "news",
  "https://www.timesofisrael.com/feed/", "en",
  "[\"news\",\"israel\",\"mena\"]",
  "The Times of Israel — Israel and regional coverage");

RSS(wn_africanews, "africanews", "Africanews",
  "アフリカニュース", "news",
  "https://www.africanews.com/feed/rss", "en",
  "[\"news\",\"africa\",\"world\"]",
  "Africanews — pan-African news coverage");

RSS(wn_mgafrica, "mail-guardian-africa", "The Mail & Guardian (Africa)",
  "メール&ガーディアン", "news",
  "https://mg.co.za/rss/", "en",
  "[\"news\",\"africa\",\"south-africa\"]",
  "Mail & Guardian — South African investigative and continental news");

RSS(wn_merco, "mercopress", "MercoPress (South Atlantic)",
  "メルコプレス", "news",
  "https://en.mercopress.com/rss/", "en",
  "[\"news\",\"south-america\",\"world\"]",
  "MercoPress — South Atlantic / Mercosur regional news in English");

RSS(wn_batimes, "buenos-aires-times", "Buenos Aires Times",
  "ブエノスアイレス・タイムズ", "news",
  "https://www.batimes.com.ar/feed", "en",
  "[\"news\",\"argentina\",\"latam\"]",
  "Buenos Aires Times — Argentina and Latin America in English");

RSS(wn_riotimes, "rio-times", "The Rio Times (Brazil)",
  "リオ・タイムズ", "news",
  "https://www.riotimesonline.com/feed/", "en",
  "[\"news\",\"brazil\",\"latam\"]",
  "The Rio Times — Brazil and Latin America English coverage");

RSS(wn_nikkei, "nikkei-asia", "Nikkei Asia",
  "日経アジア", "news",
  "https://asia.nikkei.com/rss/feed/nar", "en",
  "[\"news\",\"asia\",\"economy\"]",
  "Nikkei Asia — Asian business, economy and politics in English");
