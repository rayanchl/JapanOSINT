/* collectors/sources/crypto_world4.c
 * OSINT services — Etherscan-family EVM explorers (key-gated). Pivot on
 * ctx->entity (an address). Each needs its per-chain API key; honest-empty when
 * the key is unset. Shared etherscan_family() parser (all return
 * {status,message,result} with result = wei balance as a decimal string).
 *
 *   ETHERSCAN_BALANCE  api.etherscan.io      ETHERSCAN_API_KEY     (ETH)
 *   BSCSCAN_BALANCE    api.bscscan.com       BSCSCAN_API_KEY       (BNB)
 *   POLYGONSCAN_BALANCE api.polygonscan.com  POLYGONSCAN_API_KEY   (MATIC)
 *   ARBISCAN_BALANCE   api.arbiscan.io       ARBISCAN_API_KEY      (ETH/Arb) */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *C4_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int etherscan_family(const source_ctx *ctx, intel_sink *sink,
                            const char *service, const char *host, const char *keyenv,
                            const char *chain, const char *symbol, const char *explorer,
                            const char *enc) {
  const char *key = jo_env(keyenv);
  if (!key) { fprintf(stderr, "[%s] no %s\n", service, keyenv); return 0; }
  char url[640];
  snprintf(url, sizeof url,
    "https://%s/api?module=account&action=balance&address=%s&tag=latest&apikey=%s",
    host, enc, key);
  const char *hdrs[] = { "Accept: application/json", C4_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, service);
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *status = jo_sv(root, "status");
  const char *result = jo_sv(root, "result");
  /* status "1" = OK with a numeric wei string in result. */
  if (!status || strcmp(status, "1") != 0 || !result) { cJSON_Delete(root); return 0; }
  /* wei → coin via long double to avoid overflow on big balances */
  long double wei = strtold(result, NULL);
  double coin = (double)(wei / 1e18L);
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "address", ctx->entity);
  cJSON_AddStringToObject(d, "chain", chain);
  cJSON_AddStringToObject(d, "balance_wei", result);
  cJSON_AddNumberToObject(d, "balance", coin);
  cJSON_AddStringToObject(d, "symbol", symbol);
  cJSON_AddStringToObject(d, "source", service);
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", service);
  cJSON_AddStringToObject(p, "chain", chain);
  cJSON_AddStringToObject(p, "address", ctx->entity);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  char rk[300], link[360], title[320];
  snprintf(rk, sizeof rk, "%s:%s", chain, ctx->entity);
  snprintf(link, sizeof link, "https://%s/address/%s", explorer, ctx->entity);
  snprintf(title, sizeof title, "%s %s (%.6f %s)", chain, ctx->entity, coin, symbol);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.summary = symbol;
  it.body = bj; it.link = link; it.lang = "en";
  it.record_type = "crypto-address";
  it.properties_json = pj; it.tags_json = tags;
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
  if      (!strcmp(sid, "ETHERSCAN_BALANCE"))   n = etherscan_family(ctx, sink, "ETHERSCAN_BALANCE", "api.etherscan.io", "ETHERSCAN_API_KEY", "eth", "ETH", "etherscan.io", enc);
  else if (!strcmp(sid, "BSCSCAN_BALANCE"))     n = etherscan_family(ctx, sink, "BSCSCAN_BALANCE", "api.bscscan.com", "BSCSCAN_API_KEY", "bsc", "BNB", "bscscan.com", enc);
  else if (!strcmp(sid, "POLYGONSCAN_BALANCE")) n = etherscan_family(ctx, sink, "POLYGONSCAN_BALANCE", "api.polygonscan.com", "POLYGONSCAN_API_KEY", "polygon", "MATIC", "polygonscan.com", enc);
  else if (!strcmp(sid, "ARBISCAN_BALANCE"))    n = etherscan_family(ctx, sink, "ARBISCAN_BALANCE", "api.arbiscan.io", "ARBISCAN_API_KEY", "arbitrum", "ETH", "arbiscan.io", enc);
  else if (!strcmp(sid, "OPTIMISM_BALANCE"))    n = etherscan_family(ctx, sink, "OPTIMISM_BALANCE", "api-optimistic.etherscan.io", "OPTIMISM_API_KEY", "optimism", "ETH", "optimistic.etherscan.io", enc);
  else if (!strcmp(sid, "BASE_BALANCE"))        n = etherscan_family(ctx, sink, "BASE_BALANCE", "api.basescan.org", "BASESCAN_API_KEY", "base", "ETH", "basescan.org", enc);
  else if (!strcmp(sid, "GNOSIS_BALANCE"))      n = etherscan_family(ctx, sink, "GNOSIS_BALANCE", "api.gnosisscan.io", "GNOSISSCAN_API_KEY", "gnosis", "xDAI", "gnosisscan.io", enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define C4_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/crypto", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

C4_DEF(c4_eth_def, "ETHERSCAN_BALANCE", "Etherscan ETH Balance",
  "Etherscan (ETHERSCAN_API_KEY): Ethereum address balance.");
C4_DEF(c4_bsc_def, "BSCSCAN_BALANCE", "BscScan BNB Balance",
  "BscScan (BSCSCAN_API_KEY): BNB Chain address balance.");
C4_DEF(c4_poly_def, "POLYGONSCAN_BALANCE", "PolygonScan MATIC Balance",
  "PolygonScan (POLYGONSCAN_API_KEY): Polygon address balance.");
C4_DEF(c4_arb_def, "ARBISCAN_BALANCE", "Arbiscan Balance",
  "Arbiscan (ARBISCAN_API_KEY): Arbitrum address balance.");
C4_DEF(c4_op_def, "OPTIMISM_BALANCE", "Optimism Balance",
  "Optimistic Etherscan (OPTIMISM_API_KEY): Optimism address balance.");
C4_DEF(c4_base_def, "BASE_BALANCE", "BaseScan Balance",
  "BaseScan (BASESCAN_API_KEY): Base chain address balance.");
C4_DEF(c4_gnosis_def, "GNOSIS_BALANCE", "GnosisScan Balance",
  "GnosisScan (GNOSISSCAN_API_KEY): Gnosis Chain address balance (xDAI).");
