/* Finance / markets / crypto feeds (real RSS) — economic and digital-asset signal for follow-the-money OSINT, via rss_collect. */
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

RSSX(fin_coindesk, "coindesk", "CoinDesk", "CoinDesk", "osint", "economy",
  "https://www.coindesk.com/arc/outboundfeeds/rss/", "en", "[\"economy\",\"crypto\",\"markets\"]", 3600,
  "CoinDesk — crypto market intelligence feed");

RSSX(fin_cointelegraph, "cointelegraph", "Cointelegraph", "Cointelegraph", "osint", "economy",
  "https://cointelegraph.com/rss", "en", "[\"economy\",\"crypto\",\"markets\"]", 3600,
  "Cointelegraph — crypto market intelligence feed");

RSSX(fin_the_block, "the-block", "The Block", "The Block", "osint", "economy",
  "https://www.theblock.co/rss.xml", "en", "[\"economy\",\"crypto\",\"markets\"]", 3600,
  "The Block — crypto market intelligence feed");

RSSX(fin_decrypt, "decrypt", "Decrypt", "Decrypt", "osint", "economy",
  "https://decrypt.co/feed", "en", "[\"economy\",\"crypto\",\"markets\"]", 3600,
  "Decrypt — crypto market intelligence feed");

RSSX(fin_bitcoin_mag, "bitcoin-mag", "Bitcoin Magazine", "Bitcoin Magazine", "osint", "economy",
  "https://bitcoinmagazine.com/feed", "en", "[\"economy\",\"crypto\",\"markets\"]", 3600,
  "Bitcoin Magazine — crypto market intelligence feed");

RSSX(fin_cnbc_top, "cnbc-top", "CNBC Top News", "CNBC Top News", "osint", "economy",
  "https://www.cnbc.com/id/100003114/device/rss/rss.html", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "CNBC Top News — markets market intelligence feed");

RSSX(fin_cnbc_world, "cnbc-world", "CNBC World", "CNBC World", "osint", "economy",
  "https://www.cnbc.com/id/100727362/device/rss/rss.html", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "CNBC World — markets market intelligence feed");

RSSX(fin_cnbc_finance, "cnbc-finance", "CNBC Finance", "CNBC Finance", "osint", "economy",
  "https://www.cnbc.com/id/10000664/device/rss/rss.html", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "CNBC Finance — markets market intelligence feed");

RSSX(fin_investing_news, "investing-news", "Investing.com News", "Investing.com News", "osint", "economy",
  "https://www.investing.com/rss/news.rss", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "Investing.com News — markets market intelligence feed");

RSSX(fin_zerohedge, "zerohedge", "ZeroHedge", "ZeroHedge", "osint", "economy",
  "https://feeds.feedburner.com/zerohedge/feed", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "ZeroHedge — markets market intelligence feed");

RSSX(fin_ft_home, "ft-home", "Financial Times Home", "Financial Times Home", "osint", "economy",
  "https://www.ft.com/rss/home", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "Financial Times Home — markets market intelligence feed");

RSSX(fin_seeking_alpha, "seeking-alpha", "Seeking Alpha Market News", "Seeking Alpha Market News", "osint", "economy",
  "https://seekingalpha.com/market_currents.xml", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "Seeking Alpha Market News — markets market intelligence feed");

RSSX(fin_marketwatch_top, "marketwatch-top", "MarketWatch Top Stories", "MarketWatch Top Stories", "osint", "economy",
  "http://feeds.marketwatch.com/marketwatch/topstories/", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "MarketWatch Top Stories — markets market intelligence feed");

RSSX(fin_kitco_news, "kitco-news", "Kitco Metals News", "Kitco Metals News", "osint", "economy",
  "https://www.kitco.com/rss/KitcoNews.xml", "en", "[\"economy\",\"commodities\",\"markets\"]", 3600,
  "Kitco Metals News — commodities market intelligence feed");

RSSX(fin_oilprice_2, "oilprice-2", "OilPrice Energy News", "OilPrice Energy News", "osint", "economy",
  "https://oilprice.com/rss/main", "en", "[\"economy\",\"energy\",\"markets\"]", 3600,
  "OilPrice Energy News — energy market intelligence feed");

RSSX(fin_trading_econ, "trading-econ", "Trading Economics Stream", "Trading Economics Stream", "osint", "economy",
  "https://tradingeconomics.com/rss/news.aspx", "en", "[\"economy\",\"markets\",\"markets\"]", 3600,
  "Trading Economics Stream — markets market intelligence feed");
