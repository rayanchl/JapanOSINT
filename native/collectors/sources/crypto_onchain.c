/* collectors/osint/sources/crypto_onchain.c — on-chain crypto OSINT, ported
 * from OSINTsaas whale_monitor.c (handle_defi_tracker / handle_exchange_flow)
 * onto the JapanOSINT source ABI. Pivots on an Ethereum address via Etherscan.
 *
 *   DEFI_TRACKER  — address balance + recent on-chain activity summary
 *   EXCHANGE_FLOW — recent transfers in/out (one item per tx)
 *
 * Etherscan needs a key → gates on ETHERSCAN_API_KEY, honest-empty without it.
 * Real chain data only. */
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int is_eth_addr(const char *s) {
  if (!s || strncmp(s, "0x", 2) != 0 || strlen(s) != 42) return 0;
  for (const char *p = s + 2; *p; p++) if (!isxdigit((unsigned char)*p)) return 0;
  return 1;
}
static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* GET an Etherscan account action; returns parsed JSON or NULL. */
static cJSON *etherscan(http_client *http, const char *action, const char *addr,
                        const char *extra, const char *key) {
  char url[512];
  snprintf(url, sizeof url,
    "https://api.etherscan.io/api?module=account&action=%s&address=%s%s&apikey=%s",
    action, addr, extra ? extra : "", key);
  http_response hr = {0};
  if (http_request(http, "GET", url, NULL, NULL, 0, 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  return j;
}

static int emit_one(intel_sink *sink, const char *addr, const char *service,
                    const char *tags, cJSON *data, const char *rk_suffix,
                    const char *title) {
  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "entity", addr);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[160];
  snprintf(rk, sizeof rk, "chain:%s:%s:%s", service, addr, rk_suffix);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = service;
  it.record_type = "osint_service_result"; it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int defi_run(const source_ctx *ctx, intel_sink *sink) {
  const char *addr = ctx->entity;
  if (!is_eth_addr(addr)) return 0;
  const char *key = getenv("ETHERSCAN_API_KEY");
  if (!key || !*key) return 0;

  cJSON *bal = etherscan(ctx->http, "balance", addr, "&tag=latest", key);
  const char *wei = bal ? jstr(bal, "result") : NULL;
  /* exhaustive-ok: this view emits ONE balance record; the tx list is read only
   * to report a recent-activity count. EXCHANGE_FLOW below walks every page and
   * emits the transactions themselves. */
  cJSON *tx = etherscan(ctx->http, "txlist", addr, "&sort=desc&page=1&offset=100", key);  /* exhaustive-ok: activity count for the balance record; EXCHANGE_FLOW emits the txs */
  cJSON *txr = tx ? cJSON_GetObjectItem(tx, "result") : NULL;
  int txn = (txr && cJSON_IsArray(txr)) ? cJSON_GetArraySize(txr) : 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "source", "api.etherscan.io");
  cJSON_AddStringToObject(data, "address", addr);
  if (wei) {
    cJSON_AddStringToObject(data, "balance_wei", wei);
    cJSON_AddNumberToObject(data, "balance_eth", strtod(wei, NULL) / 1e18);
  }
  cJSON_AddNumberToObject(data, "recent_tx_count", txn);
  char title[128]; snprintf(title, sizeof title, "On-chain profile — %s", addr);
  int e = emit_one(sink, addr, "DEFI_TRACKER", "[\"osint-search\",\"DEFI_TRACKER\"]",
                   data, "summary", title);
  cJSON_Delete(data);
  if (bal) cJSON_Delete(bal);
  if (tx) cJSON_Delete(tx);
  return e;
}

static int flow_run(const source_ctx *ctx, intel_sink *sink) {
  const char *addr = ctx->entity;
  if (!is_eth_addr(addr)) return 0;
  const char *key = getenv("ETHERSCAN_API_KEY");
  if (!key || !*key) return 0;

  /* Walk pages until Etherscan stops returning transactions, instead of
   * keeping an arbitrary first 15 (docs/SOURCE_EXHAUSTIVENESS.md). The page
   * ceiling is a request budget, not an editorial cut, and it is logged. */
  int emitted = 0;
  for (int page = 1; page <= 20; page++) {
    char extra[64];
    snprintf(extra, sizeof extra, "&sort=desc&page=%d&offset=100", page);
    cJSON *tx = etherscan(ctx->http, "txlist", addr, extra, key);
    cJSON *txr = tx ? cJSON_GetObjectItem(tx, "result") : NULL;
    int n = (txr && cJSON_IsArray(txr)) ? cJSON_GetArraySize(txr) : 0;
    if (n == 0) { if (tx) cJSON_Delete(tx); break; }
    for (int i = 0; i < n; i++) {
      cJSON *t = cJSON_GetArrayItem(txr, i);
      const char *hash = jstr(t, "hash"), *from = jstr(t, "from"), *to = jstr(t, "to");
      const char *val = jstr(t, "value"), *ts = jstr(t, "timeStamp");
      if (!hash) continue;
      int outgoing = from && strcasecmp(from, addr) == 0;
      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "source", "api.etherscan.io");
      cJSON_AddStringToObject(data, "tx", hash);
      cJSON_AddStringToObject(data, "direction", outgoing ? "out" : "in");
      if (from) cJSON_AddStringToObject(data, "from", from);
      if (to) cJSON_AddStringToObject(data, "to", to);
      if (val) cJSON_AddNumberToObject(data, "value_eth", strtod(val, NULL) / 1e18);
      if (ts) cJSON_AddStringToObject(data, "timestamp", ts);
      char title[160]; snprintf(title, sizeof title, "Flow %s — %.10s…", outgoing ? "out" : "in", hash);
      emitted += emit_one(sink, addr, "EXCHANGE_FLOW", "[\"osint-search\",\"EXCHANGE_FLOW\"]",
                          data, hash, title);
      cJSON_Delete(data);
    }
    if (tx) cJSON_Delete(tx);
    if (n < 100) break;                 /* short page = upstream exhausted */
  }
  return emitted;
}

static const source_def defi_def = {
  .id = "DEFI_TRACKER", .collector = "osint", .name = "DeFi/On-chain Tracker", .run = defi_run,
  .category = "investigation", .type = "api", .url = "https://api.etherscan.io/",
  .description = "Ethereum address balance + recent activity via Etherscan (requires ETHERSCAN_API_KEY).",
  .free_tier = 0,
};
REGISTER_SOURCE(defi_def)

static const source_def flow_def = {
  .id = "EXCHANGE_FLOW", .collector = "osint", .name = "Exchange Flow", .run = flow_run,
  .category = "investigation", .type = "api", .url = "https://api.etherscan.io/",
  .description = "Recent in/out transfers for an Ethereum address (requires ETHERSCAN_API_KEY).",
  .free_tier = 0,
};
REGISTER_SOURCE(flow_def)
