/* collectors/sources/crypto_world2.c
 * OSINT services — net-new keyless blockchain / crypto intelligence, pivoting on
 * ctx->entity (a coin name/symbol, protocol, or on-chain address). One shared
 * run() dispatches on ctx->source_id. Every source REAL-fetches and emits ONE
 * intel_item per parsed record, or honest-empty (return 0) on fetch failure /
 * no matches. Nothing fabricated. No API keys required.
 *
 *   COINGECKO_SEARCH  api.coingecko.com/api/v3/search?query=<e>          (JSON)
 *   DEFILLAMA_TVL     api.llama.fi/protocols  (filtered by name/symbol)  (JSON)
 *   BLOCKCYPHER_ADDR  api.blockcypher.com/v1/btc/main/addrs/<addr>/balance(JSON)
 *   BLOCKCHAIN_INFO   blockchain.info/rawaddr/<addr>?limit=0             (JSON) */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CW_UA =
  "User-Agent: JapanOSINT/1.0 (+https://github.com/; osint@example.org)";

/* Shared emitter: body/props are cJSON objects owned by caller (deleted here). */
static int cw_emit(intel_sink *sink, const char *service, const char *rtype,
                   const char *remote_key, const char *title, const char *summary,
                   const char *link, cJSON *body, cJSON *props) {
  char *bj = body ? cJSON_PrintUnformatted(body) : NULL;
  if (props) cJSON_AddStringToObject(props, "service", service);
  if (props) cJSON_AddBoolToObject(props, "success", 1);
  char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
  char tags[128];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  intel_item it = {0};
  it.remote_key      = remote_key;
  it.title           = title;
  it.summary         = summary;
  it.body            = bj;
  it.link            = (link && link[0]) ? link : NULL;
  it.lang            = "en";
  it.record_type     = rtype;
  it.properties_json = pj;
  it.tags_json       = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  if (body) cJSON_Delete(body);
  if (props) cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* ---- CoinGecko search (coins/exchanges/categories by name/symbol) --------- */
static int run_coingecko(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.coingecko.com/api/v3/search?query=%s", enc);
  const char *hdrs[] = { "Accept: application/json", CW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "coingecko");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *coins = cJSON_GetObjectItem(root, "coins");
  int emitted = 0;
  if (cJSON_IsArray(coins)) {
    const cJSON *c;
    cJSON_ArrayForEach(c, coins) {
      const char *id = jo_sv(c, "id");
      const char *name = jo_sv(c, "name");
      const char *sym = jo_sv(c, "symbol");
      if (!id && !name) continue;
      const cJSON *rank = cJSON_GetObjectItem(c, "market_cap_rank");
      cJSON *d = cJSON_CreateObject();
      if (name) cJSON_AddStringToObject(d, "name", name);
      if (sym)  cJSON_AddStringToObject(d, "symbol", sym);
      if (id)   cJSON_AddStringToObject(d, "coingecko_id", id);
      if (rank && cJSON_IsNumber(rank)) cJSON_AddNumberToObject(d, "market_cap_rank", rank->valuedouble);
      cJSON_AddStringToObject(d, "source", "CoinGecko");
      cJSON *p = cJSON_CreateObject();
      if (id) cJSON_AddStringToObject(p, "coingecko_id", id);
      char link[256] = {0};
      if (id) snprintf(link, sizeof link, "https://www.coingecko.com/en/coins/%s", id);
      char title[256];
      snprintf(title, sizeof title, "%s%s%s%s", name ? name : id,
               sym ? " (" : "", sym ? sym : "", sym ? ")" : "");
      emitted += cw_emit(sink, "COINGECKO_SEARCH", "crypto-asset",
                         id ? id : name, title, sym, link, d, p);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

/* ---- DefiLlama protocols (name/symbol substring match) -------------------- */
static int ci_contains(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 0;
  size_t nl = strlen(needle);
  for (const char *p = hay; *p; p++) {
    size_t i = 0;
    while (i < nl && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
    if (i == nl) return 1;
  }
  return 0;
}
static int run_defillama(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  const char *hdrs[] = { "Accept: application/json", CW_UA, NULL };
  char *body = jo_get(ctx, "https://api.llama.fi/protocols", hdrs, "defillama");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }
  int emitted = 0;
  const cJSON *pr;
  cJSON_ArrayForEach(pr, root) {
    const char *name = jo_sv(pr, "name");
    const char *sym = jo_sv(pr, "symbol");
    if (!ci_contains(name, raw) && !ci_contains(sym, raw)) continue;
    const cJSON *tvl = cJSON_GetObjectItem(pr, "tvl");
    const char *cat = jo_sv(pr, "category");
    const char *urlf = jo_sv(pr, "url");
    const char *chain = jo_sv(pr, "chain");
    cJSON *d = cJSON_CreateObject();
    if (name) cJSON_AddStringToObject(d, "name", name);
    if (sym)  cJSON_AddStringToObject(d, "symbol", sym);
    if (cat)  cJSON_AddStringToObject(d, "category", cat);
    if (chain) cJSON_AddStringToObject(d, "chain", chain);
    if (tvl && cJSON_IsNumber(tvl)) cJSON_AddNumberToObject(d, "tvl_usd", tvl->valuedouble);
    cJSON_AddStringToObject(d, "source", "DefiLlama");
    cJSON *p = cJSON_CreateObject();
    if (name) cJSON_AddStringToObject(p, "protocol", name);
    emitted += cw_emit(sink, "DEFILLAMA_TVL", "defi-protocol",
                       name, name ? name : sym, cat, urlf, d, p);
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
  }
  cJSON_Delete(root);
  return emitted;
}

/* ---- BlockCypher BTC address balance ------------------------------------- */
static int run_blockcypher(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  char enc[256]; { char *e = jo_urlencode(raw); if (!e) return 0; snprintf(enc, sizeof enc, "%s", e); free(e); }
  char url[512];
  snprintf(url, sizeof url,
    "https://api.blockcypher.com/v1/btc/main/addrs/%s/balance", enc);
  const char *hdrs[] = { "Accept: application/json", CW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "blockcypher");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *addr = jo_sv(root, "address");
  const cJSON *bal = cJSON_GetObjectItem(root, "final_balance");
  const cJSON *ntx = cJSON_GetObjectItem(root, "n_tx");
  const cJSON *recv = cJSON_GetObjectItem(root, "total_received");
  if (!addr) { cJSON_Delete(root); return 0; }
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "address", addr);
  if (bal && cJSON_IsNumber(bal))  cJSON_AddNumberToObject(d, "final_balance_sat", bal->valuedouble);
  if (recv && cJSON_IsNumber(recv)) cJSON_AddNumberToObject(d, "total_received_sat", recv->valuedouble);
  if (ntx && cJSON_IsNumber(ntx))  cJSON_AddNumberToObject(d, "n_tx", ntx->valuedouble);
  cJSON_AddStringToObject(d, "source", "BlockCypher");
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "chain", "btc");
  cJSON_AddStringToObject(p, "address", addr);
  char link[320], rk[300], title[300];
  snprintf(link, sizeof link, "https://live.blockcypher.com/btc/address/%s/", addr);
  snprintf(rk, sizeof rk, "btc:%s", addr);
  double b = (bal && cJSON_IsNumber(bal)) ? bal->valuedouble / 1e8 : 0;
  snprintf(title, sizeof title, "BTC %s (%.8f BTC)", addr, b);
  cJSON_Delete(root);
  return cw_emit(sink, "BLOCKCYPHER_ADDR", "crypto-address", rk, title, "BTC address", link, d, p);
}

/* ---- Blockchain.info BTC rawaddr (summary only, limit=0) ------------------ */
static int run_blockchaininfo(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  char enc[256]; { char *e = jo_urlencode(raw); if (!e) return 0; snprintf(enc, sizeof enc, "%s", e); free(e); }
  char url[512];
  snprintf(url, sizeof url, "https://blockchain.info/rawaddr/%s?limit=0", enc);
  const char *hdrs[] = { "Accept: application/json", CW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "blockchain_info");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *addr = jo_sv(root, "address");
  const cJSON *fb = cJSON_GetObjectItem(root, "final_balance");
  const cJSON *ntx = cJSON_GetObjectItem(root, "n_tx");
  const cJSON *tr = cJSON_GetObjectItem(root, "total_received");
  if (!addr) { cJSON_Delete(root); return 0; }
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "address", addr);
  if (fb && cJSON_IsNumber(fb))  cJSON_AddNumberToObject(d, "final_balance_sat", fb->valuedouble);
  if (tr && cJSON_IsNumber(tr))  cJSON_AddNumberToObject(d, "total_received_sat", tr->valuedouble);
  if (ntx && cJSON_IsNumber(ntx)) cJSON_AddNumberToObject(d, "n_tx", ntx->valuedouble);
  cJSON_AddStringToObject(d, "source", "Blockchain.info");
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "chain", "btc");
  cJSON_AddStringToObject(p, "address", addr);
  char link[320], rk[300], title[300];
  snprintf(link, sizeof link, "https://www.blockchain.com/btc/address/%s", addr);
  snprintf(rk, sizeof rk, "btcinfo:%s", addr);
  double b = (fb && cJSON_IsNumber(fb)) ? fb->valuedouble / 1e8 : 0;
  snprintf(title, sizeof title, "BTC %s (%.8f BTC)", addr, b);
  cJSON_Delete(root);
  return cw_emit(sink, "BLOCKCHAIN_INFO", "crypto-address", rk, title, "BTC address", link, d, p);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "COINGECKO_SEARCH")) n = run_coingecko(ctx, sink, enc);
  else if (!strcmp(sid, "DEFILLAMA_TVL"))    n = run_defillama(ctx, sink, q);
  else if (!strcmp(sid, "BLOCKCYPHER_ADDR")) n = run_blockcypher(ctx, sink, q);
  else if (!strcmp(sid, "BLOCKCHAIN_INFO"))  n = run_blockchaininfo(ctx, sink, q);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define CW_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/crypto", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

CW_DEF(cw_coingecko_def, "COINGECKO_SEARCH", "CoinGecko Asset Search",
  "CoinGecko keyless search: coins/tokens by name or symbol (id, rank).");
CW_DEF(cw_defillama_def, "DEFILLAMA_TVL", "DefiLlama Protocol TVL",
  "DefiLlama keyless: DeFi protocols matching a name/symbol (TVL, chain, category).");
CW_DEF(cw_blockcypher_def, "BLOCKCYPHER_ADDR", "BlockCypher BTC Address",
  "BlockCypher keyless: Bitcoin address balance / tx count / total received.");
CW_DEF(cw_blockchaininfo_def, "BLOCKCHAIN_INFO", "Blockchain.info BTC Address",
  "Blockchain.info keyless: Bitcoin address summary (balance, n_tx, received).");
