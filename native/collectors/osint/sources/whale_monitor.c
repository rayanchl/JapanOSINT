/* collectors/osint/sources/whale_monitor.c
 * OSINT service — port of OSINTsaas osint_tools/whale_monitor.c
 * (whale_monitor → handle_whale_alert). Canonical SERVICE = WHALE_ALERT
 * (osint_dispatcher.c:281 {SERVICE_WALLET_ANALYZER, handle_whale_alert,
 * "WHALE_ALERT", true}; that handler calls whale_monitor(entities[0]) when an
 * entity is present). On-demand (interval 0); ctx->entity = a wallet address or
 * a generic query. WHALE_ALERT_API_KEY / ETHERSCAN_API_KEY are optional — the
 * C original degrades gracefully (Whale Alert returns an {error:"API key not
 * configured"} object; Etherscan still works key-less), so we do the same
 * rather than return 0. Faithfully reproduces identify_whale (built-in known
 * whale table), track_wallet (Etherscan balance + last 5 txs for 0x… addrs),
 * query_whale_alert (gated), get_exchange_flows (blockchain.info/stats),
 * get_defi_data (api.llama.fi/protocols top-20 + total TVL). ETH/BTC address
 * shape selects wallet_tracking; otherwise whale_alerts+exchange_flows+
 * defi_metrics. confidence 80. Emits ONE osint_service_result row like
 * dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
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

static cJSON *query_whale_alert(http_client *http, long minv, time_t since) {
  const char *key = getenv("WHALE_ALERT_API_KEY");
  if (!key || !*key) {
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "source", "Whale Alert");
    cJSON_AddStringToObject(r, "error", "API key not configured");
    cJSON_AddStringToObject(r, "note",
      "Set WHALE_ALERT_API_KEY environment variable for real-time whale tracking");
    return r;
  }
  char url[512];
  snprintf(url, sizeof url,
    "https://api.whale-alert.io/v1/transactions?api_key=%s&min_value=%ld&start=%ld",
    key, minv, (long)since);
  cJSON *j = get_json(http, url);
  if (!j) return NULL;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "Whale Alert");
  const cJSON *txs = cJSON_GetObjectItem(j, "transactions");
  if (txs && cJSON_IsArray(txs)) {
    int n = cJSON_GetArraySize(txs);
    cJSON_AddNumberToObject(r, "transaction_count", n);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n && i < 50; i++) {
      const cJSON *tx = cJSON_GetArrayItem(txs, i);
      cJSON *f = cJSON_CreateObject();
      const cJSON *bc = cJSON_GetObjectItem(tx, "blockchain");
      const cJSON *sy = cJSON_GetObjectItem(tx, "symbol");
      const cJSON *am = cJSON_GetObjectItem(tx, "amount");
      const cJSON *au = cJSON_GetObjectItem(tx, "amount_usd");
      const cJSON *hs = cJSON_GetObjectItem(tx, "hash");
      const cJSON *ts = cJSON_GetObjectItem(tx, "timestamp");
      if (bc && cJSON_IsString(bc)) cJSON_AddStringToObject(f, "blockchain", bc->valuestring);
      if (sy && cJSON_IsString(sy)) cJSON_AddStringToObject(f, "symbol", sy->valuestring);
      if (am && cJSON_IsNumber(am)) cJSON_AddNumberToObject(f, "amount", am->valuedouble);
      if (au && cJSON_IsNumber(au)) cJSON_AddNumberToObject(f, "amount_usd", au->valuedouble);
      if (hs && cJSON_IsString(hs)) cJSON_AddStringToObject(f, "tx_hash", hs->valuestring);
      if (ts && cJSON_IsNumber(ts)) cJSON_AddNumberToObject(f, "timestamp", ts->valuedouble);
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
          cJSON_AddItemToObject(f, side, si);
        }
      }
      cJSON_AddItemToArray(arr, f);
    }
    cJSON_AddItemToObject(r, "transactions", arr);
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *get_exchange_flows(http_client *http) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "metric", "exchange_flows");
  cJSON *s = get_json(http, "https://blockchain.info/stats?format=json");
  if (s) {
    cJSON *b = cJSON_CreateObject();
    const cJSON *mp = cJSON_GetObjectItem(s, "market_price_usd");
    const cJSON *tv = cJSON_GetObjectItem(s, "trade_volume_usd");
    const cJSON *nt = cJSON_GetObjectItem(s, "n_tx");
    const cJSON *bs = cJSON_GetObjectItem(s, "total_btc_sent");
    const cJSON *hr = cJSON_GetObjectItem(s, "hash_rate");
    if (mp && cJSON_IsNumber(mp)) cJSON_AddNumberToObject(b, "price_usd", mp->valuedouble);
    if (tv && cJSON_IsNumber(tv)) cJSON_AddNumberToObject(b, "24h_volume_usd", tv->valuedouble);
    if (nt && cJSON_IsNumber(nt)) cJSON_AddNumberToObject(b, "24h_transactions", nt->valuedouble);
    if (bs && cJSON_IsNumber(bs)) cJSON_AddNumberToObject(b, "24h_btc_sent", bs->valuedouble / 100000000.0);
    if (hr && cJSON_IsNumber(hr)) cJSON_AddNumberToObject(b, "hash_rate", hr->valuedouble);
    cJSON_AddItemToObject(r, "bitcoin_network", b);
    cJSON_Delete(s);
  }
  return r;
}

static cJSON *get_defi_data(http_client *http) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "DeFi Llama");
  cJSON *p = get_json(http, "https://api.llama.fi/protocols");
  if (!p || !cJSON_IsArray(p)) { if (p) cJSON_Delete(p); return r; }
  cJSON *top = cJSON_CreateArray();
  int n = cJSON_GetArraySize(p);
  for (int i = 0; i < n && i < 20; i++) {
    const cJSON *pr = cJSON_GetArrayItem(p, i);
    cJSON *info = cJSON_CreateObject();
    const cJSON *nm = cJSON_GetObjectItem(pr, "name");
    const cJSON *sy = cJSON_GetObjectItem(pr, "symbol");
    const cJSON *tv = cJSON_GetObjectItem(pr, "tvl");
    const cJSON *ch = cJSON_GetObjectItem(pr, "chain");
    const cJSON *ca = cJSON_GetObjectItem(pr, "category");
    const cJSON *c1 = cJSON_GetObjectItem(pr, "change_1h");
    const cJSON *cd = cJSON_GetObjectItem(pr, "change_1d");
    const cJSON *c7 = cJSON_GetObjectItem(pr, "change_7d");
    if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(info, "name", nm->valuestring);
    if (sy && cJSON_IsString(sy)) cJSON_AddStringToObject(info, "symbol", sy->valuestring);
    if (tv && cJSON_IsNumber(tv)) cJSON_AddNumberToObject(info, "tvl_usd", tv->valuedouble);
    if (ch && cJSON_IsString(ch)) cJSON_AddStringToObject(info, "chain", ch->valuestring);
    if (ca && cJSON_IsString(ca)) cJSON_AddStringToObject(info, "category", ca->valuestring);
    if (c1 && cJSON_IsNumber(c1)) cJSON_AddNumberToObject(info, "change_1h_pct", c1->valuedouble);
    if (cd && cJSON_IsNumber(cd)) cJSON_AddNumberToObject(info, "change_24h_pct", cd->valuedouble);
    if (c7 && cJSON_IsNumber(c7)) cJSON_AddNumberToObject(info, "change_7d_pct", c7->valuedouble);
    cJSON_AddItemToArray(top, info);
  }
  cJSON_AddItemToObject(r, "top_protocols", top);
  cJSON_AddNumberToObject(r, "total_protocols", n);
  double tot = 0;
  for (int i = 0; i < n; i++) {
    const cJSON *pr = cJSON_GetArrayItem(p, i);
    const cJSON *tv = cJSON_GetObjectItem(pr, "tvl");
    if (tv && cJSON_IsNumber(tv)) tot += tv->valuedouble;
  }
  cJSON_AddNumberToObject(r, "total_tvl_usd", tot);
  cJSON_Delete(p);
  return r;
}

static cJSON *track_wallet(http_client *http, const char *addr) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "address", addr);
  const known_t *w = identify_whale(addr);
  if (w) {
    cJSON_AddStringToObject(r, "known_entity", w->label);
    cJSON_AddStringToObject(r, "blockchain", w->chain);
    cJSON_AddBoolToObject(r, "is_known_whale", 1);
  } else cJSON_AddBoolToObject(r, "is_known_whale", 0);

  if (strncmp(addr, "0x", 2) == 0 && strlen(addr) == 42) {
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
    if (j) {
      const cJSON *b = cJSON_GetObjectItem(j, "result");
      if (b && cJSON_IsString(b))
        cJSON_AddNumberToObject(r, "balance_eth", atof(b->valuestring) / 1e18);
      cJSON_Delete(j);
    }
    if (ek && *ek)
      snprintf(url, sizeof url,
        "https://api.etherscan.io/api?module=account&action=txlist&address=%s&startblock=0&endblock=99999999&page=1&offset=5&sort=desc&apikey=%s",
        addr, ek);
    else
      snprintf(url, sizeof url,
        "https://api.etherscan.io/api?module=account&action=txlist&address=%s&startblock=0&endblock=99999999&page=1&offset=5&sort=desc",
        addr);
    cJSON *j2 = get_json(http, url);
    if (j2) {
      const cJSON *txs = cJSON_GetObjectItem(j2, "result");
      if (txs && cJSON_IsArray(txs)) {
        cJSON *rt = cJSON_CreateArray();
        int n = cJSON_GetArraySize(txs);
        for (int i = 0; i < n && i < 5; i++) {
          const cJSON *tx = cJSON_GetArrayItem(txs, i);
          cJSON *ti = cJSON_CreateObject();
          const cJSON *h = cJSON_GetObjectItem(tx, "hash");
          const cJSON *v = cJSON_GetObjectItem(tx, "value");
          const cJSON *ts = cJSON_GetObjectItem(tx, "timeStamp");
          const cJSON *fr = cJSON_GetObjectItem(tx, "from");
          const cJSON *to = cJSON_GetObjectItem(tx, "to");
          if (h && cJSON_IsString(h)) cJSON_AddStringToObject(ti, "hash", h->valuestring);
          if (v && cJSON_IsString(v)) cJSON_AddNumberToObject(ti, "value_eth", atof(v->valuestring) / 1e18);
          if (ts && cJSON_IsString(ts)) cJSON_AddNumberToObject(ti, "timestamp", (double)atol(ts->valuestring));
          if (fr && cJSON_IsString(fr)) cJSON_AddStringToObject(ti, "from", fr->valuestring);
          if (to && cJSON_IsString(to)) cJSON_AddStringToObject(ti, "to", to->valuestring);
          cJSON_AddItemToArray(rt, ti);
        }
        cJSON_AddItemToObject(r, "recent_transactions", rt);
      }
      cJSON_Delete(j2);
    }
  }
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", q);
  time_t now = time(NULL);
  cJSON_AddNumberToObject(root, "timestamp", (double)now);

  size_t len = strlen(q);
  int is_eth = (strncmp(q, "0x", 2) == 0 && len == 42);
  int is_btc = ((q[0] == '1' || q[0] == '3' || strncmp(q, "bc1", 3) == 0) &&
                len >= 26 && len <= 62);

  if (is_eth || is_btc) {
    cJSON_AddItemToObject(root, "wallet_tracking", track_wallet(ctx->http, q));
  } else {
    cJSON *wa = query_whale_alert(ctx->http, 10000000, now - 86400);
    if (wa) cJSON_AddItemToObject(root, "whale_alerts", wa);
    cJSON_AddItemToObject(root, "exchange_flows", get_exchange_flows(ctx->http));
    cJSON_AddItemToObject(root, "defi_metrics", get_defi_data(ctx->http));
  }

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 80);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "WHALE_ALERT");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "whale:%s", q);
  char title[320]; snprintf(title, sizeof title, "WHALE_ALERT — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = (is_eth || is_btc) ? "wallet tracking" : "whale monitoring";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"WHALE_ALERT\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def whale_monitor_def = {
  .id = "WHALE_ALERT", .collector = "osint",
  .name = "Crypto Whale Monitor", .name_ja = "暗号資産ホエール監視",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(whale_monitor_def)
