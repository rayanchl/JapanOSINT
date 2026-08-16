/* collectors/sources/crypto_world3.c
 * OSINT services — net-new keyless on-chain address intelligence for Ethereum and
 * Bitcoin, pivoting on ctx->entity (an address). Shared run() switches on
 * ctx->source_id. Real fetch or honest-empty. Keyless (Ethplorer uses its public
 * "freekey"; mempool.space needs no key).
 *
 *   ETHPLORER_ADDR   api.ethplorer.io/getAddressInfo/<addr>?apiKey=freekey (JSON)
 *   MEMPOOL_ADDR     mempool.space/api/address/<addr>                       (JSON) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CX_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_ethplorer(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.ethplorer.io/getAddressInfo/%s?apiKey=freekey", enc);
  const char *hdrs[] = { "Accept: application/json", CX_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "ETHPLORER_ADDR");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *addr = jo_sv(root, "address");
  if (!addr) { cJSON_Delete(root); return 0; }
  const cJSON *eth = cJSON_GetObjectItem(root, "ETH");
  const cJSON *ethbal = eth ? cJSON_GetObjectItem(eth, "balance") : NULL;
  const cJSON *ntx = cJSON_GetObjectItem(root, "countTxs");
  const cJSON *tokens = cJSON_GetObjectItem(root, "tokens");
  int tokcount = cJSON_IsArray(tokens) ? cJSON_GetArraySize(tokens) : 0;

  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "address", addr);
  if (ethbal && cJSON_IsNumber(ethbal)) cJSON_AddNumberToObject(d, "eth_balance", ethbal->valuedouble);
  if (ntx && cJSON_IsNumber(ntx)) cJSON_AddNumberToObject(d, "count_txs", ntx->valuedouble);
  cJSON_AddNumberToObject(d, "token_count", tokcount);
  /* list up to 10 token symbols held */
  if (tokcount) {
    cJSON *tarr = cJSON_CreateArray();
    int k = 0; const cJSON *t;
    cJSON_ArrayForEach(t, tokens) {
      const cJSON *ti = cJSON_GetObjectItem(t, "tokenInfo");
      const char *sym = ti ? jo_sv(ti, "symbol") : NULL;
      if (sym) cJSON_AddItemToArray(tarr, cJSON_CreateString(sym));
      if (++k >= 10) break;
    }
    cJSON_AddItemToObject(d, "tokens", tarr);
  }
  cJSON_AddStringToObject(d, "source", "Ethplorer");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "ETHPLORER_ADDR");
  cJSON_AddStringToObject(p, "chain", "eth");
  cJSON_AddStringToObject(p, "address", addr);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char rk[300], link[320], title[300];
  snprintf(rk, sizeof rk, "eth:%s", addr);
  snprintf(link, sizeof link, "https://etherscan.io/address/%s", addr);
  double b = (ethbal && cJSON_IsNumber(ethbal)) ? ethbal->valuedouble : 0;
  snprintf(title, sizeof title, "ETH %s (%.6f ETH, %d tokens)", addr, b, tokcount);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.summary = "ETH address";
  it.body = bj; it.link = link; it.lang = "en";
  it.record_type = "crypto-address";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"ETHPLORER_ADDR\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run_mempool(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://mempool.space/api/address/%s", enc);
  const char *hdrs[] = { "Accept: application/json", CX_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "MEMPOOL_ADDR");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *addr = jo_sv(root, "address");
  if (!addr) { cJSON_Delete(root); return 0; }
  const cJSON *cs = cJSON_GetObjectItem(root, "chain_stats");
  double funded = 0, spent = 0; int txc = 0;
  if (cs) {
    const cJSON *f = cJSON_GetObjectItem(cs, "funded_txo_sum");
    const cJSON *s = cJSON_GetObjectItem(cs, "spent_txo_sum");
    const cJSON *tc = cJSON_GetObjectItem(cs, "tx_count");
    if (cJSON_IsNumber(f)) funded = f->valuedouble;
    if (cJSON_IsNumber(s)) spent = s->valuedouble;
    if (cJSON_IsNumber(tc)) txc = (int)tc->valuedouble;
  }
  double bal = (funded - spent) / 1e8;
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "address", addr);
  cJSON_AddNumberToObject(d, "balance_btc", bal);
  cJSON_AddNumberToObject(d, "funded_sat", funded);
  cJSON_AddNumberToObject(d, "spent_sat", spent);
  cJSON_AddNumberToObject(d, "tx_count", txc);
  cJSON_AddStringToObject(d, "source", "mempool.space");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "MEMPOOL_ADDR");
  cJSON_AddStringToObject(p, "chain", "btc");
  cJSON_AddStringToObject(p, "address", addr);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char rk[300], link[320], title[300];
  snprintf(rk, sizeof rk, "btcmp:%s", addr);
  snprintf(link, sizeof link, "https://mempool.space/address/%s", addr);
  snprintf(title, sizeof title, "BTC %s (%.8f BTC, %d tx)", addr, bal, txc);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.summary = "BTC address";
  it.body = bj; it.link = link; it.lang = "en";
  it.record_type = "crypto-address";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"MEMPOOL_ADDR\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "ETHPLORER_ADDR")) n = run_ethplorer(ctx, sink, enc);
  else if (!strcmp(sid, "MEMPOOL_ADDR"))   n = run_mempool(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define CX_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/crypto", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

CX_DEF(cx_ethplorer_def, "ETHPLORER_ADDR", "Ethplorer ETH Address",
  "Ethplorer keyless (freekey): ETH address balance, tx count, ERC-20 tokens held.");
CX_DEF(cx_mempool_def, "MEMPOOL_ADDR", "mempool.space BTC Address",
  "mempool.space keyless: Bitcoin address balance, funded/spent, tx count.");
