/* collectors/osint/sources/crypto_onchain.c — on-chain crypto OSINT, ported
 * from OSINTsaas whale_monitor.c (handle_defi_tracker / handle_exchange_flow)
 * onto the JapanOSINT source ABI. Pivots on an Ethereum address via Etherscan.
 *
 *   DEFI_TRACKER  — address balance + recent on-chain activity summary
 *   EXCHANGE_FLOW — recent transfers in/out (one item per tx)
 *
 * Etherscan needs a key → gates on ETHERSCAN_API_KEY, honest-empty without it.
 * Real chain data only. */
#include "../../lib/jocore.h"
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
  if (!is_eth_addr(addr)) {
    fprintf(stderr, "[DEFI_TRACKER] entity must be a 0x… Ethereum address\n");
    return 0;
  }
  const char *key = getenv("ETHERSCAN_API_KEY");
  /* Say it out loud: a silent 0 rows is indistinguishable from "looked and
   * found nothing", and every audit sweep misfiles this as EMPTY. */
  if (!key || !*key) {
    fprintf(stderr, "[DEFI_TRACKER] gated (no ETHERSCAN_API_KEY)\n");
    return 0;
  }

  cJSON *bal = etherscan(ctx->http, "balance", addr, "&tag=latest", key);
  const char *wei = bal ? jo_str(bal, "result") : NULL;
  cJSON *tx = etherscan(ctx->http, "txlist", addr, "&sort=desc&page=1&offset=10", key);
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
  int fetched = (bal != NULL) || (tx != NULL);
  if (bal) cJSON_Delete(bal);
  if (tx) cJSON_Delete(tx);
  fprintf(stderr, "[DEFI_TRACKER] emitted %d\n", e);
  /* run() is a STATUS code, not a row count (core/scheduler.c reads rc!=0 as
   * an errored run). Returning the emit count marked a successful lookup as an
   * error; only a dead Etherscan endpoint is one. */
  return fetched ? 0 : -1;
}

static int flow_run(const source_ctx *ctx, intel_sink *sink) {
  const char *addr = ctx->entity;
  if (!is_eth_addr(addr)) {
    fprintf(stderr, "[EXCHANGE_FLOW] entity must be a 0x… Ethereum address\n");
    return 0;
  }
  const char *key = getenv("ETHERSCAN_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[EXCHANGE_FLOW] gated (no ETHERSCAN_API_KEY)\n");
    return 0;
  }

  cJSON *tx = etherscan(ctx->http, "txlist", addr, "&sort=desc&page=1&offset=15", key);
  cJSON *txr = tx ? cJSON_GetObjectItem(tx, "result") : NULL;
  int emitted = 0, n = (txr && cJSON_IsArray(txr)) ? cJSON_GetArraySize(txr) : 0;
  for (int i = 0; i < n; i++) {
    cJSON *t = cJSON_GetArrayItem(txr, i);
    const char *hash = jo_str(t, "hash"), *from = jo_str(t, "from"), *to = jo_str(t, "to");
    const char *val = jo_str(t, "value"), *ts = jo_str(t, "timeStamp");
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
  int fetched = tx != NULL;
  if (tx) cJSON_Delete(tx);
  fprintf(stderr, "[EXCHANGE_FLOW] emitted %d\n", emitted);
  /* STATUS code, not a row count: an address with no recent transfers is an
   * honest empty. */
  return fetched ? 0 : -1;
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
