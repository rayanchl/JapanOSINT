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
 * Upstream wraps results in result_builder (query=<addr>,
 * query_type="crypto_address"); its finalized JSON is faithfully rebuilt:
 *   {query,query_type:"crypto_address",
 *    results:[{source,found,confidence(90|0),detection_method:"direct",
 *              url?,data:{…,address,crypto_type},llama_analysis:null}],
 *    summary:{total_sources,found_count,detection_breakdown:{direct,llama:0}},
 *    _meta:{execution_time_ms,llama_fallback_used:false,timestamp}}
 * success = (a balance fetch succeeded); confidence 85 if success else 0.
 * Emits one osint_service_result row (body = {success,confidence,data}). */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
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

/* result_builder_add_item shape (CONFIDENCE_HIGH=90, NONE=0,
 * DETECTION_DIRECT="direct"). data gets address+crypto_type appended. */
static void add_item(cJSON *results, const char *src, int found,
                     const char *addr, const char *ctype,
                     cJSON *data /*owned*/, const char *url) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "source", src);
  cJSON_AddBoolToObject(o, "found", found);
  cJSON_AddNumberToObject(o, "confidence", found ? 90 : 0);
  cJSON_AddStringToObject(o, "detection_method", "direct");
  if (url) cJSON_AddStringToObject(o, "url", url);
  if (data) {
    cJSON_AddStringToObject(data, "address", addr);
    cJSON_AddStringToObject(data, "crypto_type", ctype);
    cJSON_AddItemToObject(o, "data", data);
  } else {
    cJSON_AddNullToObject(o, "data");
  }
  cJSON_AddNullToObject(o, "llama_analysis");
  cJSON_AddItemToArray(results, o);
}

static cJSON *http_json(http_client *h, const char *url) {
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  return j;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *addr = ctx->entity;
  if (!addr || !*addr) return -1;

  ctype_t t = detect_type(addr);
  const char *ts = type_str(t);
  time_t start = time(NULL);

  cJSON *results = cJSON_CreateArray();
  int success = 0, found_count = 0, total = 0;
  char url[512];

  if (t == C_BTC) {
    snprintf(url, sizeof url, "https://blockchain.info/rawaddr/%s?limit=10", addr);
    cJSON *j = http_json(ctx->http, url);
    if (j) {
      cJSON *data = cJSON_CreateObject();
      cJSON *bal = cJSON_GetObjectItem(j, "final_balance");
      cJSON *ntx = cJSON_GetObjectItem(j, "n_tx");
      if (bal && cJSON_IsNumber(bal))
        cJSON_AddNumberToObject(data, "balance_btc", bal->valuedouble / 1e8);
      if (ntx && cJSON_IsNumber(ntx))
        cJSON_AddNumberToObject(data, "transaction_count", ntx->valuedouble);
      add_item(results, "Blockchain.info", 1, addr, ts, data,
               "https://blockchain.info/");
      success = 1; found_count++; total++;
      cJSON_Delete(j);
    }
    if (!success) {
      snprintf(url, sizeof url,
        "https://api.blockcypher.com/v1/btc/main/addrs/%s", addr);
      cJSON *j2 = http_json(ctx->http, url);
      if (j2) {
        cJSON *data = cJSON_CreateObject();
        cJSON *bal = cJSON_GetObjectItem(j2, "balance");
        if (bal && cJSON_IsNumber(bal))
          cJSON_AddNumberToObject(data, "balance_btc", bal->valuedouble / 1e8);
        add_item(results, "BlockCypher", 1, addr, ts, data,
                 "https://blockcypher.com/");
        success = 1; found_count++; total++;
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
        cJSON_AddNumberToObject(data, "balance_eth", wei / 1e18);
        cJSON_AddNumberToObject(data, "balance_wei", wei);
        add_item(results, "Etherscan", 1, addr, ts, data,
                 "https://etherscan.io/");
        success = 1; found_count++; total++;
      }
      cJSON_Delete(j);
    }
  } else if (t == C_LTC || t == C_DOGE || t == C_DASH) {
    const char *coin = (t == C_LTC) ? "ltc" : (t == C_DOGE) ? "doge" : "dash";
    const char *bk = (t == C_LTC) ? "balance_ltc"
                    : (t == C_DOGE) ? "balance_doge" : "balance_dash";
    snprintf(url, sizeof url,
      "https://api.blockcypher.com/v1/%s/main/addrs/%s", coin, addr);
    cJSON *j = http_json(ctx->http, url);
    if (j) {
      cJSON *bal = cJSON_GetObjectItem(j, "balance");
      if (bal && cJSON_IsNumber(bal)) {
        cJSON *data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, bk, bal->valuedouble / 1e8);
        add_item(results, "BlockCypher", 1, addr, ts, data,
                 "https://blockcypher.com/");
        success = 1; found_count++; total++;
      }
      cJSON_Delete(j);
    }
  } else {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "error", "Unknown cryptocurrency type");
    add_item(results, "Detection", 0, addr, ts, data, NULL);
    total++;
  }

  /* Rebuild result_builder_finalize() JSON exactly. */
  cJSON *rb = cJSON_CreateObject();
  cJSON_AddStringToObject(rb, "query", addr);
  cJSON_AddStringToObject(rb, "query_type", "crypto_address");
  cJSON_AddItemToObject(rb, "results", results);
  cJSON *summ = cJSON_CreateObject();
  cJSON_AddNumberToObject(summ, "total_sources", total);
  cJSON_AddNumberToObject(summ, "found_count", found_count);
  cJSON *bd = cJSON_CreateObject();
  cJSON_AddNumberToObject(bd, "direct", total);
  cJSON_AddNumberToObject(bd, "llama", 0);
  cJSON_AddItemToObject(summ, "detection_breakdown", bd);
  cJSON_AddItemToObject(rb, "summary", summ);
  cJSON *meta = cJSON_CreateObject();
  time_t end = time(NULL);
  cJSON_AddNumberToObject(meta, "execution_time_ms",
                          (int)((end - start) * 1000));
  cJSON_AddBoolToObject(meta, "llama_fallback_used", 0);
  cJSON_AddNumberToObject(meta, "timestamp", (double)end);
  cJSON_AddItemToObject(rb, "_meta", meta);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", success ? 85 : 0);
  cJSON_AddItemToObject(env, "data", rb);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CRYPTO_TRACKER");
  cJSON_AddStringToObject(props, "entity", addr);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", success ? 85 : 0);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "crypto:%s", addr);
  snprintf(title, sizeof title, "CRYPTO_TRACKER — %s", addr);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "crypto address tracked"
                               : "crypto address not resolved";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CRYPTO_TRACKER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def crypto_tracker_def = {
  .id = "CRYPTO_TRACKER", .collector = "osint",
  .name = "Crypto Tracker", .name_ja = "暗号資産追跡",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(crypto_tracker_def)
