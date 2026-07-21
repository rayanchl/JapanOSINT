/* Central-bank / monetary-authority press feeds (best-effort real RSS) — macro-financial and policy signal, via rss_collect. */
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

RSSX(cb_fed_press, "fed-press", "US Federal Reserve Press", "US Federal Reserve Press", "economy", "economy",
  "https://www.federalreserve.gov/feeds/press_all.xml", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "US Federal Reserve Press — central-bank press and policy feed");

RSSX(cb_ecb_press, "ecb-press", "European Central Bank Press", "European Central Bank Press", "economy", "economy",
  "https://www.ecb.europa.eu/rss/press.html", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "European Central Bank Press — central-bank press and policy feed");

RSSX(cb_boe_news, "boe-news", "Bank of England News", "Bank of England News", "economy", "economy",
  "https://www.bankofengland.co.uk/rss/news", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Bank of England News — central-bank press and policy feed");

RSSX(cb_boj_news, "boj-news", "Bank of Japan Releases", "Bank of Japan Releases", "economy", "economy",
  "https://www.boj.or.jp/en/rss/whatsnew.xml", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Bank of Japan Releases — central-bank press and policy feed");

RSSX(cb_rba_media, "rba-media", "Reserve Bank of Australia", "Reserve Bank of Australia", "economy", "economy",
  "https://www.rba.gov.au/rss/rss-cb-media-releases.xml", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Reserve Bank of Australia — central-bank press and policy feed");

RSSX(cb_boc_press, "boc-press", "Bank of Canada Press", "Bank of Canada Press", "economy", "economy",
  "https://www.bankofcanada.ca/content_type/press-releases/feed/", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Bank of Canada Press — central-bank press and policy feed");

RSSX(cb_bis_press, "bis-press", "Bank for Intl Settlements", "Bank for Intl Settlements", "economy", "economy",
  "https://www.bis.org/list/press_releases/rss.xml", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Bank for Intl Settlements — central-bank press and policy feed");

RSSX(cb_rbi_press, "rbi-press", "Reserve Bank of India", "Reserve Bank of India", "economy", "economy",
  "https://www.rbi.org.in/Scripts/Rss.aspx?Id=2", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Reserve Bank of India — central-bank press and policy feed");

RSSX(cb_snb_press, "snb-press", "Swiss National Bank", "Swiss National Bank", "economy", "economy",
  "https://www.snb.ch/public/en/rss/news", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Swiss National Bank — central-bank press and policy feed");

RSSX(cb_riksbank, "riksbank", "Sweden Riksbank", "Sweden Riksbank", "economy", "economy",
  "https://www.riksbank.se/en-gb/rss/press-releases/", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Sweden Riksbank — central-bank press and policy feed");

RSSX(cb_norgesbank, "norgesbank", "Norges Bank", "Norges Bank", "economy", "economy",
  "https://www.norges-bank.no/en/rss/", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Norges Bank — central-bank press and policy feed");

RSSX(cb_bcb_brazil, "bcb-brazil", "Banco Central do Brasil", "Banco Central do Brasil", "economy", "economy",
  "https://www.bcb.gov.br/rss/ultimasnoticias", "pt", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Banco Central do Brasil — central-bank press and policy feed");

RSSX(cb_banxico, "banxico", "Bank of Mexico", "Bank of Mexico", "economy", "economy",
  "https://www.banxico.org.mx/rss/rss.xml", "es", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Bank of Mexico — central-bank press and policy feed");

RSSX(cb_cbr_russia, "cbr-russia", "Bank of Russia", "Bank of Russia", "economy", "economy",
  "https://www.cbr.ru/rss/eventrss", "ru", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "Bank of Russia — central-bank press and policy feed");

RSSX(cb_readme_imf, "readme-imf", "IMF Country Focus", "IMF Country Focus", "economy", "economy",
  "https://www.imf.org/en/News/rss?language=eng&series=News+Article", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "IMF Country Focus — central-bank press and policy feed");

RSSX(cb_ecb_blog, "ecb-blog", "ECB Blog & Research", "ECB Blog & Research", "economy", "economy",
  "https://www.ecb.europa.eu/rss/blog.html", "en", "[\"economy\",\"central-bank\",\"monetary-policy\"]", 7200,
  "ECB Blog & Research — central-bank press and policy feed");
