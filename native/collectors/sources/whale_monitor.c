/* collectors/osint/sources/whale_monitor.c
 * OSINT service — port of OSINTsaas osint_tools/whale_monitor.c
 * (whale_monitor → handle_whale_alert). Canonical SERVICE = WHALE_ALERT
 * (osint_dispatcher.c:281 {SERVICE_WALLET_ANALYZER, handle_whale_alert,
 * "WHALE_ALERT", true}; that handler calls whale_monitor(entities[0]) when an
 * entity is present). On-demand (interval 0); ctx->entity = a wallet address or
 * a generic query. WHALE_ALERT_API_KEY / ETHERSCAN_API_KEY are optional.
 *
 * PER-RECORD EMIT: instead of one summary blob, this explodes the real fetched
 * data into distinct intel_items:
 *   - one item per RECENT TX (Etherscan txlist, or Whale Alert tx feed)
 *       → remote_key "tx:<hash>"
 *   - one item for the ADDRESS BALANCE (Etherscan balance)
 *       → remote_key "addr:<address>"  (carries known-whale label classifier)
 *   - one item per DeFiLlama TOP protocol
 *       → remote_key "defi:<protocol-slug>"  body = name/tvl/chain
 *   - one item for the Bitcoin network stats (blockchain.info/stats)
 *       → remote_key "btcnet:stats"
 * Whale Alert tx feed is queried ONLY when WHALE_ALERT_API_KEY is present (else
 * skipped — no fabricated row). The KNOWN-whale table is used purely as a
 * classifier input on the address item. All values are real values fetched
 * from the live endpoints; nothing is fabricated. Nothing fetched → NOTHING
 * emitted and run() returns 0. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

typedef struct { const char *addr, *label, *chain; } known_t;
static const known_t KNOWN[] = {
  {"bc1qgdjqv0av3q56jvd82tkdjpy7gdp9ut8tlqmgrpmv24sq90ecnvqqjwvw97","Bitfinex Cold Wallet","BTC"},
  {"34xp4vRoCGJym3xR7yCVPFHoCNxv4Twseo","Binance Cold Wallet","BTC"},
  {"1LQoWist8KkaUXSPKZHNvEyfrEkPHzSsCd","Kraken Cold Wallet","BTC"},
  {"3M219KR5vEneNb47ewrPfWyb5jQ2DjxRP6","FTX/Alameda Wallet","BTC"},
  {"0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2","WETH Contract","ETH"},
  {"0x28C6c06298d514Db089934071355E5743bf21d60","Binance Hot Wallet","ETH"},
  {"0xDa9dF53a5f3A15D2D29F64f6a7C0aE0d9567be57","Unknown Whale 1","ETH"},
  {"0x00000000219ab540356cBB839Cbe05303d7705Fa","ETH 2.0 Deposit Contract","ETH"},
  {NULL,NULL,NULL}
};

static const known_t *identify_whale(const char *a) {
  if (!a) return NULL;
  for (int i = 0; KNOWN[i].addr; i++)
    if (strcasecmp(a, KNOWN[i].addr) == 0) return &KNOWN[i];
  return NULL;
}

static cJSON *get_json(http_client *http, const char *url) {
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  return j;
}

/* Lowercase + slugify a protocol name into [a-z0-9-] for a stable remote_key. */
static void slugify(const char *in, char *out, size_t cap) {
  size_t o = 0; int prev_dash = 0;
  for (size_t i = 0; in && in[i] && o + 1 < cap; i++) {
    unsigned char c = (unsigned char)in[i];
    if (isalnum(c)) { out[o++] = (char)tolower(c); prev_dash = 0; }
    else if (!prev_dash && o > 0) { out[o++] = '-'; prev_dash = 1; }
  }
  while (o > 0 && out[o-1] == '-') o--;
  out[o] = '\0';
  if (o == 0) snprintf(out, cap, "protocol");
}

/* Emit one Etherscan / Whale-Alert recent transaction. Returns 1 if emitted. */
static int emit_eth_tx(intel_sink *sink, const char *addr, cJSON *tx) {
  const cJSON *h = cJSON_GetObjectItem(tx, "hash");
  if (!h || !cJSON_IsString(h)) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "hash", h->valuestring);
  cJSON_AddStringToObject(data, "watched_address", addr);
  const cJSON *v  = cJSON_GetObjectItem(tx, "value");
  const cJSON *ts = cJSON_GetObjectItem(tx, "timeStamp");
  const cJSON *fr = cJSON_GetObjectItem(tx, "from");
  const cJSON *to = cJSON_GetObjectItem(tx, "to");
  char iso[40] = {0};
  if (v && cJSON_IsString(v)) cJSON_AddNumberToObject(data, "value_eth", atof(v->valuestring) / 1e18);
  if (fr && cJSON_IsString(fr)) cJSON_AddStringToObject(data, "from", fr->valuestring);
  if (to && cJSON_IsString(to)) cJSON_AddStringToObject(data, "to", to->valuestring);
  if (ts && cJSON_IsString(ts)) {
    time_t t = (time_t)atol(ts->valuestring);
    strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    cJSON_AddStringToObject(data, "time", iso);
  }
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WHALE_ALERT");
  cJSON_AddStringToObject(props, "record", "transaction");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[160], title[200];
  snprintf(rk, sizeof rk, "tx:%s", h->valuestring);
  snprintf(title, sizeof title, "tx %.16s…", h->valuestring);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "whale transaction";
  it.published_at    = iso[0] ? iso : NULL;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHALE_ALERT\",\"transaction\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit one Whale Alert feed tx (different schema). Returns 1 if emitted. */
static int emit_wa_tx(intel_sink *sink, cJSON *tx) {
  const cJSON *hs = cJSON_GetObjectItem(tx, "hash");
  if (!hs || !cJSON_IsString(hs)) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "hash", hs->valuestring);
  const cJSON *bc = cJSON_GetObjectItem(tx, "blockchain");
  const cJSON *sy = cJSON_GetObjectItem(tx, "symbol");
  const cJSON *am = cJSON_GetObjectItem(tx, "amount");
  const cJSON *au = cJSON_GetObjectItem(tx, "amount_usd");
  const cJSON *tm = cJSON_GetObjectItem(tx, "timestamp");
  char iso[40] = {0};
  if (bc && cJSON_IsString(bc)) cJSON_AddStringToObject(data, "blockchain", bc->valuestring);
  if (sy && cJSON_IsString(sy)) cJSON_AddStringToObject(data, "symbol", sy->valuestring);
  if (am && cJSON_IsNumber(am)) cJSON_AddNumberToObject(data, "amount", am->valuedouble);
  if (au && cJSON_IsNumber(au)) cJSON_AddNumberToObject(data, "amount_usd", au->valuedouble);
  if (tm && cJSON_IsNumber(tm)) {
    time_t t = (time_t)tm->valuedouble;
    strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    cJSON_AddStringToObject(data, "time", iso);
  }
  for (int k = 0; k < 2; k++) {
    const char *side = k ? "to" : "from";
    const cJSON *s = cJSON_GetObjectItem(tx, side);
    if (s) {
      cJSON *si = cJSON_CreateObject();
      const cJSON *ad = cJSON_GetObjectItem(s, "address");
      const cJSON *ow = cJSON_GetObjectItem(s, "owner");
      const cJSON *ot = cJSON_GetObjectItem(s, "owner_type");
      if (ad && cJSON_IsString(ad)) cJSON_AddStringToObject(si, "address", ad->valuestring);
      if (ow && cJSON_IsString(ow)) cJSON_AddStringToObject(si, "label", ow->valuestring);
      if (ot && cJSON_IsString(ot)) cJSON_AddStringToObject(si, "type", ot->valuestring);
      cJSON_AddItemToObject(data, side, si);
    }
  }
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WHALE_ALERT");
  cJSON_AddStringToObject(props, "record", "transaction");
  cJSON_AddStringToObject(props, "feed", "whale_alert");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[160], title[200];
  snprintf(rk, sizeof rk, "tx:%s", hs->valuestring);
  if (sy && cJSON_IsString(sy) && au && cJSON_IsNumber(au))
    snprintf(title, sizeof title, "%s whale tx $%.0f", sy->valuestring, au->valuedouble);
  else
    snprintf(title, sizeof title, "whale tx %.16s…", hs->valuestring);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "whale transaction";
  it.published_at    = iso[0] ? iso : NULL;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHALE_ALERT\",\"transaction\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit one DeFiLlama protocol item. Returns 1 if emitted. */
static int emit_defi(intel_sink *sink, cJSON *pr) {
  const cJSON *nm = cJSON_GetObjectItem(pr, "name");
  if (!nm || !cJSON_IsString(nm)) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", nm->valuestring);
  const cJSON *sy = cJSON_GetObjectItem(pr, "symbol");
  const cJSON *tv = cJSON_GetObjectItem(pr, "tvl");
  const cJSON *ch = cJSON_GetObjectItem(pr, "chain");
  const cJSON *ca = cJSON_GetObjectItem(pr, "category");
  const cJSON *c1 = cJSON_GetObjectItem(pr, "change_1h");
  const cJSON *cd = cJSON_GetObjectItem(pr, "change_1d");
  const cJSON *c7 = cJSON_GetObjectItem(pr, "change_7d");
  double tvl = 0;
  if (sy && cJSON_IsString(sy)) cJSON_AddStringToObject(data, "symbol", sy->valuestring);
  if (tv && cJSON_IsNumber(tv)) { tvl = tv->valuedouble; cJSON_AddNumberToObject(data, "tvl_usd", tvl); }
  if (ch && cJSON_IsString(ch)) cJSON_AddStringToObject(data, "chain", ch->valuestring);
  if (ca && cJSON_IsString(ca)) cJSON_AddStringToObject(data, "category", ca->valuestring);
  if (c1 && cJSON_IsNumber(c1)) cJSON_AddNumberToObject(data, "change_1h_pct", c1->valuedouble);
  if (cd && cJSON_IsNumber(cd)) cJSON_AddNumberToObject(data, "change_24h_pct", cd->valuedouble);
  if (c7 && cJSON_IsNumber(c7)) cJSON_AddNumberToObject(data, "change_7d_pct", c7->valuedouble);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WHALE_ALERT");
  cJSON_AddStringToObject(props, "record", "defi_protocol");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char slug[96], rk[128], title[200];
  slugify(nm->valuestring, slug, sizeof slug);
  snprintf(rk, sizeof rk, "defi:%s", slug);
  snprintf(title, sizeof title, "%s — TVL $%.0f", nm->valuestring, tvl);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "DeFi protocol";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHALE_ALERT\",\"defi\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit the address-balance item (Etherscan). Returns 1 if emitted. */
static int emit_addr_balance(intel_sink *sink, http_client *http, const char *addr) {
  const char *ek = getenv("ETHERSCAN_API_KEY");
  char url[512];
  if (ek && *ek)
    snprintf(url, sizeof url,
      "https://api.etherscan.io/api?module=account&action=balance&address=%s&tag=latest&apikey=%s",
      addr, ek);
  else
    snprintf(url, sizeof url,
      "https://api.etherscan.io/api?module=account&action=balance&address=%s&tag=latest",
      addr);
  cJSON *j = get_json(http, url);
  if (!j) return 0;
  const cJSON *st = cJSON_GetObjectItem(j, "status");
  const cJSON *b  = cJSON_GetObjectItem(j, "result");
  if (!(st && cJSON_IsString(st) && !strcmp(st->valuestring, "1") &&
        b && cJSON_IsString(b))) { cJSON_Delete(j); return 0; }

  double eth = atof(b->valuestring) / 1e18;
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "address", addr);
  cJSON_AddNumberToObject(data, "balance_eth", eth);
  const known_t *w = identify_whale(addr);
  if (w) {
    cJSON_AddStringToObject(data, "known_entity", w->label);
    cJSON_AddStringToObject(data, "blockchain", w->chain);
    cJSON_AddBoolToObject(data, "is_known_whale", 1);
  } else cJSON_AddBoolToObject(data, "is_known_whale", 0);
  cJSON_Delete(j);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WHALE_ALERT");
  cJSON_AddStringToObject(props, "record", "address");
  cJSON_AddStringToObject(props, "entity", addr);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[160], title[256];
  snprintf(rk, sizeof rk, "addr:%s", addr);
  if (w) snprintf(title, sizeof title, "%s — %.4f ETH", w->label, eth);
  else   snprintf(title, sizeof title, "%.20s… — %.4f ETH", addr, eth);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "wallet balance";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHALE_ALERT\",\"address\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit Etherscan transactions for an ETH address. Returns count emitted.
 *
 * This used to request offset=5 and then re-cap the loop at 5 — a whale wallet
 * with thousands of transfers reported five of them. Full pages are requested
 * and walked until the upstream runs short (docs/SOURCE_EXHAUSTIVENESS.md). */
static int emit_eth_txs(intel_sink *sink, http_client *http, const char *addr) {
  const char *ek = getenv("ETHERSCAN_API_KEY");
  int emitted = 0;
  for (int page = 1; page <= 20; page++) {
    char url[600];
    if (ek && *ek)
      snprintf(url, sizeof url,
        "https://api.etherscan.io/api?module=account&action=txlist&address=%s"
        "&startblock=0&endblock=99999999&page=%d&offset=100&sort=desc&apikey=%s",
        addr, page, ek);
    else
      snprintf(url, sizeof url,
        "https://api.etherscan.io/api?module=account&action=txlist&address=%s"
        "&startblock=0&endblock=99999999&page=%d&offset=100&sort=desc",
        addr, page);
    cJSON *j = get_json(http, url);
    if (!j) break;
    const cJSON *txs = cJSON_GetObjectItem(j, "result");
    int n = (txs && cJSON_IsArray(txs)) ? cJSON_GetArraySize(txs) : 0;
    for (int i = 0; i < n; i++)
      emitted += emit_eth_tx(sink, addr, cJSON_GetArrayItem(txs, i));
    cJSON_Delete(j);
    if (n < 100) break;               /* short page = upstream exhausted */
  }
  return emitted;
}

/* Emit the Bitcoin-network stats item (blockchain.info/stats). Returns 1. */
static int emit_btc_stats(intel_sink *sink, http_client *http) {
  cJSON *s = get_json(http, "https://blockchain.info/stats?format=json");
  if (!s) return 0;
  cJSON *data = cJSON_CreateObject();
  const cJSON *mp = cJSON_GetObjectItem(s, "market_price_usd");
  const cJSON *tv = cJSON_GetObjectItem(s, "trade_volume_usd");
  const cJSON *nt = cJSON_GetObjectItem(s, "n_tx");
  const cJSON *bs = cJSON_GetObjectItem(s, "total_btc_sent");
  const cJSON *hr = cJSON_GetObjectItem(s, "hash_rate");
  int have = 0;
  if (mp && cJSON_IsNumber(mp)) { cJSON_AddNumberToObject(data, "price_usd", mp->valuedouble); have = 1; }
  if (tv && cJSON_IsNumber(tv)) { cJSON_AddNumberToObject(data, "24h_volume_usd", tv->valuedouble); have = 1; }
  if (nt && cJSON_IsNumber(nt)) { cJSON_AddNumberToObject(data, "24h_transactions", nt->valuedouble); have = 1; }
  if (bs && cJSON_IsNumber(bs)) { cJSON_AddNumberToObject(data, "24h_btc_sent", bs->valuedouble / 1e8); have = 1; }
  if (hr && cJSON_IsNumber(hr)) { cJSON_AddNumberToObject(data, "hash_rate", hr->valuedouble); have = 1; }
  cJSON_Delete(s);
  if (!have) { cJSON_Delete(data); return 0; }
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WHALE_ALERT");
  cJSON_AddStringToObject(props, "record", "btc_network");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = "btcnet:stats";
  it.title           = "Bitcoin network stats";
  it.body            = bj;
  it.summary         = "exchange flows / network";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHALE_ALERT\",\"network\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit Whale Alert feed txs (key required). Returns count emitted. */
static int emit_whale_alert_feed(intel_sink *sink, http_client *http,
                                 long minv, time_t since) {
  const char *key = getenv("WHALE_ALERT_API_KEY");
  if (!key || !*key) return 0;     /* gated: no key → skip, no fabricated row */
  char url[512];
  snprintf(url, sizeof url,
    "https://api.whale-alert.io/v1/transactions?api_key=%s&min_value=%ld&start=%ld",
    key, minv, (long)since);
  cJSON *j = get_json(http, url);
  if (!j) return 0;
  int emitted = 0;
  const cJSON *txs = cJSON_GetObjectItem(j, "transactions");
  if (txs && cJSON_IsArray(txs)) {
    int n = cJSON_GetArraySize(txs);
    for (int i = 0; i < n && i < 50; i++)
      emitted += emit_wa_tx(sink, cJSON_GetArrayItem(txs, i));
  }
  cJSON_Delete(j);
  return emitted;
}

/* Emit top-20 DeFiLlama protocols. Returns count emitted. */
static int emit_defi_protocols(intel_sink *sink, http_client *http) {
  cJSON *p = get_json(http, "https://api.llama.fi/protocols");
  if (!p || !cJSON_IsArray(p)) { if (p) cJSON_Delete(p); return 0; }
  int emitted = 0, n = cJSON_GetArraySize(p);
  for (int i = 0; i < n && i < 20; i++)
    emitted += emit_defi(sink, cJSON_GetArrayItem(p, i));
  cJSON_Delete(p);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  time_t now = time(NULL);
  size_t len = strlen(q);
  int is_eth = (strncmp(q, "0x", 2) == 0 && len == 42);
  int is_btc = ((q[0] == '1' || q[0] == '3' || strncmp(q, "bc1", 3) == 0) &&
                len >= 26 && len <= 62);

  if (is_eth) {
    emit_addr_balance(sink, ctx->http, q);
    emit_eth_txs(sink, ctx->http, q);
  } else if (is_btc) {
    /* BTC address: known-whale classifier carried via balance item is ETH-only
     * upstream; surface BTC network context (real fetched stats). */
    emit_btc_stats(sink, ctx->http);
  } else {
    emit_whale_alert_feed(sink, ctx->http, 10000000, now - 86400);
    emit_btc_stats(sink, ctx->http);
    emit_defi_protocols(sink, ctx->http);
  }
  return 0;                                 /* honest empty is not an error */
}

static const source_def whale_monitor_def = {
  .id = "WHALE_ALERT", .collector = "osint",
  .name = "Crypto Whale Monitor", .name_ja = "暗号資産ホエール監視",
  .update_interval_sec = 0, .run = run,
  .category = "economy", .type = "api",
  .url = "internal://osint/whale-alert",
  .description = "Track large crypto transactions for an address.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(whale_monitor_def)
