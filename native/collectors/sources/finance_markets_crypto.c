/* Finance / markets / crypto feeds (real RSS) — economic and digital-asset signal for follow-the-money OSINT, via rss_collect. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/rss_atom.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "_source_macros.inc"

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

/* AUDIT 2026-07-31: every published Kitco RSS path (/rss/KitcoNews.xml,
 * /rss/, /news/feed, feedburner/KitcoNews) now returns the site's 404 page,
 * and www.kitco.com/news exposes no <link rel=alternate> feed. The feed is
 * gone upstream; the URL is left in place so the failure stays visible
 * rather than being papered over with fabricated rows. */
RSSX(fin_kitco_news, "kitco-news", "Kitco Metals News", "Kitco Metals News", "osint", "economy",
  "https://www.kitco.com/rss/KitcoNews.xml", "en", "[\"economy\",\"commodities\",\"markets\"]", 3600,
  "Kitco Metals News — commodities market intelligence feed (RSS retired upstream)");

RSSX(fin_oilprice_2, "oilprice-2", "OilPrice Energy News", "OilPrice Energy News", "osint", "economy",
  "https://oilprice.com/rss/main", "en", "[\"economy\",\"energy\",\"markets\"]", 3600,
  "OilPrice Energy News — energy market intelligence feed");

/* --- Trading Economics -------------------------------------------------
 * /rss/news.aspx answers 403 to every non-browser client. The site's own
 * news stream endpoint (ws/stream.ashx) is open and returns richer JSON than
 * the RSS ever did (country, category, importance, author). */
#define TE_URL "https://tradingeconomics.com/ws/stream.ashx"

/* `start` is a real offset into the stream, not a cursor into one page:
 * start=0/5/100/1000 all return distinct, older items, and start=1000 still
 * answers with content. So `?start=0&size=100` was a single page of an archive
 * that keeps going backwards indefinitely — a silent slice.
 *
 * A news stream has no end, so "fetch everything" is not a thing this source
 * can do, and the bound is a real one. It is therefore an EXPLICIT bound: the
 * walk takes TE_PAGES pages of TE_PAGE_SIZE, and when it stops because the
 * backlog was still producing items it says so with a
 * collector-truncation-notice naming how far back it reached. */
#define TE_PAGE_SIZE 100
#define TE_PAGES 5   /* exhaustive-ok: bound on an unbounded news backlog; a full walk emits a collector-truncation-notice */

/* One stream item. Returns 1 when the sink took it. */
static int te_emit(intel_sink *s, cJSON *e) {
    const char *title = jo_sv(e, "title");
    if (!title) return 0;
    const char *desc = jo_sv(e, "description");
    const char *rel  = jo_sv(e, "url");
    const char *date = jo_sv(e, "date");
    const char *auth = jo_sv(e, "author");
    const char *ctry = jo_sv(e, "country");
    const char *cat  = jo_sv(e, "category");
    cJSON *idv = cJSON_GetObjectItem(e, "ID");
    cJSON *imp = cJSON_GetObjectItem(e, "importance");

    char rk[64], link[512];
    if (idv && cJSON_IsNumber(idv))
      snprintf(rk, sizeof rk, "%lld", (long long)cJSON_GetNumberValue(idv));
    else
      snprintf(rk, sizeof rk, "%.60s", title);
    if (rel)
      snprintf(link, sizeof link, "https://tradingeconomics.com%s%s",
               rel[0] == '/' ? "" : "/", rel);
    else
      snprintf(link, sizeof link, "https://tradingeconomics.com/stream");

    cJSON *p = cJSON_CreateObject();
    if (idv && cJSON_IsNumber(idv))
      cJSON_AddNumberToObject(p, "te_id", cJSON_GetNumberValue(idv));
    if (ctry) cJSON_AddStringToObject(p, "country", ctry);
    if (cat)  cJSON_AddStringToObject(p, "category", cat);
    if (imp && cJSON_IsNumber(imp))
      cJSON_AddNumberToObject(p, "importance", cJSON_GetNumberValue(imp));
    cJSON_AddStringToObject(p, "source", "tradingeconomics.com");
    char *pj = cJSON_PrintUnformatted(p);

    intel_item it = {0};
    it.remote_key = rk;
    it.title = title;
    it.body = desc;
    it.summary = desc;
    it.link = link;
    it.author = auth;
    it.lang = "en";
    it.published_at = date;
    it.record_type = "article";
    it.properties_json = pj;
    it.tags_json = "[\"economy\",\"markets\"]";
    int rc = s->emit(s, &it);
    free(pj); cJSON_Delete(p);
    return rc >= 0 ? 1 : 0;
}

static int run_fin_trading_econ(const source_ctx *c, intel_sink *s) {
  int pages = TE_PAGES;
  const char *penv = getenv("JO_TRADINGECON_PAGES");
  if (penv && *penv) { int v = atoi(penv); if (v > 0) pages = v; }

  int n = 0, start = 0, got_pages = 0, more_pending = 0;
  char url[160];
  for (int page = 0; page < pages; page++) {
    snprintf(url, sizeof url, "%s?start=%d&size=%d", TE_URL, start,
             TE_PAGE_SIZE);
    cJSON *arr = feed_get_json(c->http, url, 15000);
    if (!cJSON_IsArray(arr)) {
      if (arr) cJSON_Delete(arr);
      if (page == 0) { fprintf(stderr, "[trading-econ] fetch failed\n"); return -1; }
      fprintf(stderr, "[trading-econ] page at start=%d failed after %d rows\n",
              start, n);
      jo_trunc_notice(s, "trading-econ", TE_URL, n, -1,
                      "the stream walk stopped when a page failed to fetch or "
                      "parse; older items were not read",
                      "re-run the collector; the walk restarts at start=0");
      return 0;
    }
    int here = 0;
    cJSON *e;
    cJSON_ArrayForEach(e, arr) { here++; n += te_emit(s, e); }
    cJSON_Delete(arr);
    got_pages++;
    start += here;
    if (here < TE_PAGE_SIZE) break;      /* short page = end of the stream */
    if (page + 1 == pages) more_pending = 1;
  }

  fprintf(stderr, "[trading-econ] emitted %d over %d page(s), back to "
                  "start=%d\n", n, got_pages, start);
  if (more_pending) {
    /* The backlog has no declared size, so records_available is honestly
     * unknown; what IS known is how deep this run went. */
    char reason[256];
    snprintf(reason, sizeof reason,
             "the Trading Economics stream is an open-ended backlog; this run "
             "read %d item(s) back to offset %d and stopped at its own page "
             "bound, with older items still available", n, start);
    jo_trunc_notice(s, "trading-econ", TE_URL, n, -1, reason,
                    "raise $JO_TRADINGECON_PAGES to walk further back");
  }
  return 0;
}

static const source_def fin_trading_econ = {
  .id = "trading-econ", .collector = "osint",
  .name = "Trading Economics Stream", .name_ja = "Trading Economics Stream",
  .update_interval_sec = 3600, .run = run_fin_trading_econ,
  .category = "economy", .type = "api", .url = TE_URL,
  .description = "Trading Economics news stream — markets/economic headlines "
                 "with country, category and importance.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(fin_trading_econ)
