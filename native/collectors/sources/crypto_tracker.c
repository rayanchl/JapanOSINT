/* collectors/osint/sources/crypto_tracker.c
 * OSINT service — faithful port of OSINTsaas osint_tools/crypto_tracker.c
 * (crypto_track_address ← handle_crypto_tracker). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_CRYPTO_TRACKER, handle_crypto_tracker,
 * "CRYPTO_TRACKER", true} (aliases BLOCKCHAIN_EXPLORER /
 * CRYPTO_EXCHANGE_MONITOR share the same handler; primary = CRYPTO_TRACKER).
 * Entity = a crypto address (BTC/ETH/LTC/DOGE/DASH/… auto-detected via
 * crypto_detect_type, reproduced verbatim). Keyless public APIs:
 * blockchain.info, api.blockcypher.com, api.etherscan.io (free tier).
 *
 * PER-RECORD EMIT: instead of one summary blob, this explodes the real fetched
 * data into distinct intel_items:
 *   - one item per CHAIN that returned real balance data for the address
 *       → remote_key "crypto:<chain>:<address>"
 *         title       "<chain> balance <amount>"
 *         body        real balance / tx_count / wei fields for that chain
 *   - one item per recent TRANSACTION (BTC rawaddr returns a tx list)
 *       → remote_key "tx:<hash>"
 * All values are real values fetched from blockchain.info / BlockCypher /
 * Etherscan; nothing is fabricated. Unknown/unresolved address (no balance
 * fetched) → NOTHING is emitted and run() returns 0. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef enum { C_UNKNOWN, C_BTC, C_ETH, C_LTC, C_DOGE, C_DASH,
               C_BCH, C_MONERO } ctype_t;

/* Verbatim port of crypto_detect_type. */
static ctype_t detect_type(const char *a) {
  if (!a || !*a) return C_UNKNOWN;
  size_t len = strlen(a);
  if ((a[0] == '1' || a[0] == '3') && (len >= 26 && len <= 35)) return C_BTC;
  if (!strncmp(a, "bc1", 3) && len >= 42) return C_BTC;
  if (!strncmp(a, "0x", 2) && len == 42) {
    for (size_t i = 2; i < len; i++)
      if (!isxdigit((unsigned char)a[i])) return C_UNKNOWN;
    return C_ETH;
  }
  if ((a[0] == 'L' || a[0] == 'M') && (len >= 26 && len <= 35)) return C_LTC;
  if (!strncmp(a, "ltc1", 4)) return C_LTC;
  if (a[0] == 'D' && (len >= 26 && len <= 35)) return C_DOGE;
  if ((a[0] == 'q' || a[0] == 'p') && len == 42) return C_BCH;
  if (a[0] == 'X' && (len >= 26 && len <= 35)) return C_DASH;
  if ((a[0] == '4' || a[0] == '8') && (len >= 95 && len <= 106)) return C_MONERO;
  return C_UNKNOWN;
}

static const char *type_str(ctype_t t) {
  switch (t) {
    case C_BTC: return "bitcoin";
    case C_ETH: return "ethereum";
    case C_LTC: return "litecoin";
    case C_DOGE: return "dogecoin";
    case C_DASH: return "dash";
    case C_MONERO: return "monero";
    case C_BCH: return "bitcoin_cash";
    default: return "unknown";
  }
}

static cJSON *http_json(http_client *h, const char *url) {
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  return j;
}

/* Emit one per-chain balance item. `data` is owned and consumed here.
 * `amount_str` is a human balance string for the title (e.g. "0.5 BTC").
 * Returns 1 if emitted. */
static int emit_chain(intel_sink *sink, const char *chain, const char *addr,
                      const char *src, const char *url, const char *amount_str,
                      cJSON *data /*owned*/) {
  cJSON_AddStringToObject(data, "address", addr);
  cJSON_AddStringToObject(data, "crypto_type", chain);
  cJSON_AddStringToObject(data, "source", src);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CRYPTO_TRACKER");
  cJSON_AddStringToObject(props, "entity", addr);
  cJSON_AddStringToObject(props, "chain", chain);
  cJSON_AddStringToObject(props, "record", "balance");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320], title[360];
  snprintf(rk, sizeof rk, "crypto:%s:%s", chain, addr);
  snprintf(title, sizeof title, "%s balance %s", chain, amount_str);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "crypto balance";
  it.link            = url;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CRYPTO_TRACKER\",\"balance\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit one per-transaction item from a BTC rawaddr tx object. Returns 1. */
static int emit_tx(intel_sink *sink, const char *chain, const char *addr,
                   cJSON *tx) {
  cJSON *hash = cJSON_GetObjectItem(tx, "hash");
  if (!hash || !hash->valuestring) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "hash", hash->valuestring);
  cJSON_AddStringToObject(data, "address", addr);
  cJSON_AddStringToObject(data, "crypto_type", chain);
  cJSON *res  = cJSON_GetObjectItem(tx, "result");      /* net effect, satoshi */
  cJSON *ntime = cJSON_GetObjectItem(tx, "time");
  cJSON *fee  = cJSON_GetObjectItem(tx, "fee");
  char iso[40] = {0};
  if (res && cJSON_IsNumber(res))
    cJSON_AddNumberToObject(data, "value_btc", res->valuedouble / 1e8);
  if (fee && cJSON_IsNumber(fee))
    cJSON_AddNumberToObject(data, "fee_btc", fee->valuedouble / 1e8);
  if (ntime && cJSON_IsNumber(ntime)) {
    time_t t = (time_t)ntime->valuedouble;
    strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    cJSON_AddStringToObject(data, "time", iso);
  }
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CRYPTO_TRACKER");
  cJSON_AddStringToObject(props, "entity", addr);
  cJSON_AddStringToObject(props, "chain", chain);
  cJSON_AddStringToObject(props, "record", "transaction");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[160], title[200];
  snprintf(rk, sizeof rk, "tx:%s", hash->valuestring);
  snprintf(title, sizeof title, "%s tx %.16s…", chain, hash->valuestring);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "crypto transaction";
  it.published_at    = iso[0] ? iso : NULL;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CRYPTO_TRACKER\",\"transaction\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *addr = ctx->entity;
  if (!addr || !*addr) return -1;

  ctype_t t = detect_type(addr);
  const char *ts = type_str(t);
  char url[512], amt[64];
  int emitted = 0;

  if (t == C_BTC) {
    snprintf(url, sizeof url, "https://blockchain.info/rawaddr/%s?limit=10", addr);
    cJSON *j = http_json(ctx->http, url);
    if (j) {
      cJSON *bal = cJSON_GetObjectItem(j, "final_balance");
      cJSON *ntx = cJSON_GetObjectItem(j, "n_tx");
      cJSON *data = cJSON_CreateObject();
      double btc = 0;
      if (bal && cJSON_IsNumber(bal)) {
        btc = bal->valuedouble / 1e8;
        cJSON_AddNumberToObject(data, "balance_btc", btc);
      }
      if (ntx && cJSON_IsNumber(ntx))
        cJSON_AddNumberToObject(data, "transaction_count", ntx->valuedouble);
      snprintf(amt, sizeof amt, "%.8f BTC", btc);
      emitted += emit_chain(sink, ts, addr, "Blockchain.info",
                            "https://blockchain.info/", amt, data);
      /* one item per recent transaction */
      cJSON *txs = cJSON_GetObjectItem(j, "txs");
      if (txs && cJSON_IsArray(txs)) {
        int n = cJSON_GetArraySize(txs);
        for (int i = 0; i < n; i++)
          emitted += emit_tx(sink, ts, addr, cJSON_GetArrayItem(txs, i));
      }
      cJSON_Delete(j);
    }
    if (emitted == 0) {
      snprintf(url, sizeof url,
        "https://api.blockcypher.com/v1/btc/main/addrs/%s", addr);
      cJSON *j2 = http_json(ctx->http, url);
      if (j2) {
        cJSON *bal = cJSON_GetObjectItem(j2, "balance");
        if (bal && cJSON_IsNumber(bal)) {
          cJSON *data = cJSON_CreateObject();
          double btc = bal->valuedouble / 1e8;
          cJSON_AddNumberToObject(data, "balance_btc", btc);
          cJSON *ntx = cJSON_GetObjectItem(j2, "n_tx");
          if (ntx && cJSON_IsNumber(ntx))
            cJSON_AddNumberToObject(data, "transaction_count", ntx->valuedouble);
          snprintf(amt, sizeof amt, "%.8f BTC", btc);
          emitted += emit_chain(sink, ts, addr, "BlockCypher",
                                "https://blockcypher.com/", amt, data);
        }
        cJSON_Delete(j2);
      }
    }
  } else if (t == C_ETH) {
    snprintf(url, sizeof url,
      "https://api.etherscan.io/api?module=account&action=balance&address=%s&tag=latest",
      addr);
    cJSON *j = http_json(ctx->http, url);
    if (j) {
      cJSON *st = cJSON_GetObjectItem(j, "status");
      cJSON *rs = cJSON_GetObjectItem(j, "result");
      if (st && cJSON_IsString(st) && !strcmp(st->valuestring, "1") &&
          rs && cJSON_IsString(rs)) {
        cJSON *data = cJSON_CreateObject();
        double wei = atof(rs->valuestring);
        double eth = wei / 1e18;
        cJSON_AddNumberToObject(data, "balance_eth", eth);
        cJSON_AddNumberToObject(data, "balance_wei", wei);
        snprintf(amt, sizeof amt, "%.6f ETH", eth);
        emitted += emit_chain(sink, ts, addr, "Etherscan",
                              "https://etherscan.io/", amt, data);
      }
      cJSON_Delete(j);
    }
  } else if (t == C_LTC || t == C_DOGE || t == C_DASH) {
    const char *coin = (t == C_LTC) ? "ltc" : (t == C_DOGE) ? "doge" : "dash";
    const char *bk = (t == C_LTC) ? "balance_ltc"
                    : (t == C_DOGE) ? "balance_doge" : "balance_dash";
    const char *unit = (t == C_LTC) ? "LTC" : (t == C_DOGE) ? "DOGE" : "DASH";
    snprintf(url, sizeof url,
      "https://api.blockcypher.com/v1/%s/main/addrs/%s", coin, addr);
    cJSON *j = http_json(ctx->http, url);
    if (j) {
      cJSON *bal = cJSON_GetObjectItem(j, "balance");
      if (bal && cJSON_IsNumber(bal)) {
        cJSON *data = cJSON_CreateObject();
        double v = bal->valuedouble / 1e8;
        cJSON_AddNumberToObject(data, bk, v);
        cJSON *ntx = cJSON_GetObjectItem(j, "n_tx");
        if (ntx && cJSON_IsNumber(ntx))
          cJSON_AddNumberToObject(data, "transaction_count", ntx->valuedouble);
        snprintf(amt, sizeof amt, "%.8f %s", v, unit);
        emitted += emit_chain(sink, ts, addr, "BlockCypher",
                              "https://blockcypher.com/", amt, data);
      }
      cJSON_Delete(j);
    }
  }
  /* Unknown type or no balance fetched → emit nothing. */
  (void)emitted;
  return 0;                                 /* honest empty is not an error */
}

static const source_def crypto_tracker_def = {
  .id = "CRYPTO_TRACKER", .collector = "osint",
  .name = "Crypto Tracker", .name_ja = "暗号資産追跡",
  .update_interval_sec = 0, .run = run,
  .category = "economy", .type = "api",
  .url = "internal://osint/crypto-tracker",
  .description = "Looks up crypto address balances via blockchain.info/BlockCypher/Etherscan",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(crypto_tracker_def)
