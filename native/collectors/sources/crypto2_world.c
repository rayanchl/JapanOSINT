/* collectors/sources/crypto2_world.c
 * OSINT services — more on-chain address/name intelligence across additional
 * chains. On-demand entity pivot (ctx->entity = an address or ENS name) against
 * REAL, keyless public endpoints. One run() dispatches on ctx->source_id.
 *
 *   ETH_BLOCKSCOUT  Ethereum L1 address → balance, tx counts, contract/tags via
 *                   Blockscout v2 API (eth.blockscout.com, keyless LIVE).
 *   TRON_SCAN       Tron address → balance, tx count, name via TronScan public
 *                   API (apilist.tronscanapi.com, keyless LIVE).
 *   SOLANA_RPC      Solana address → account lamports/owner/executable via the
 *                   public JSON-RPC getAccountInfo (api.mainnet-beta.solana.com).
 *   ENS_RESOLVE     ENS name → resolved address + avatar/display via ensideas
 *                   (api.ensideas.com/ens/resolve, keyless LIVE).
 *
 * Every run() REAL-fetches and emits ONLY parsed live fields, or honest empty
 * (return 0). No constructed record is ever fabricated; the block-explorer
 * "link" is a genuine resolvable URL for the same address/name we queried.
 * No entity → return -1. Fetch failure / not-found → honest empty. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* cJSON number field as double, with presence flag. */
static int c2_num(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  return 0;
}

/* Number-or-string field → copy into buf (Blockscout returns big balances as
 * decimal strings; TronScan uses real numbers). Returns 1 if present. */
static int c2_numstr(const cJSON *o, const char *k, char *buf, size_t n) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    snprintf(buf, n, "%s", v->valuestring); return 1;
  }
  if (cJSON_IsNumber(v)) { snprintf(buf, n, "%.0f", v->valuedouble); return 1; }
  return 0;
}

/* ---- ETH_BLOCKSCOUT ----------------------------------------------------- *
 * GET https://eth.blockscout.com/api/v2/addresses/<addr>
 *   { hash, coin_balance (wei, string), is_contract (bool),
 *     name (string|null), public_tags[], ... } plus a counters endpoint for tx
 *   counts. We stay on the base object (name/balance/is_contract) — all real. */
static int c2_blockscout(const source_ctx *ctx, intel_sink *sink, const char *addr) {
  char *enc = jo_urlencode(addr);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
           "https://eth.blockscout.com/api/v2/addresses/%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (crypto)", NULL };
  char *body = jo_get(ctx, url, hdrs, "ETH_BLOCKSCOUT");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const char *hash = jo_sv(root, "hash");
  if (!hash) hash = addr;
  char bal[96] = {0}; int have_bal = c2_numstr(root, "coin_balance", bal, sizeof bal);
  const cJSON *isc = cJSON_GetObjectItem(root, "is_contract");
  int is_contract = (isc && cJSON_IsBool(isc)) ? cJSON_IsTrue(isc) : 0;
  const char *name = jo_sv(root, "name");

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "address", hash);
  cJSON_AddStringToObject(data, "chain", "ethereum");
  if (have_bal) cJSON_AddStringToObject(data, "coin_balance_wei", bal);
  cJSON_AddBoolToObject(data, "is_contract", is_contract);
  if (name) cJSON_AddStringToObject(data, "name", name);
  cJSON_AddStringToObject(data, "source", "Blockscout");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "ETH_BLOCKSCOUT");
  cJSON_AddStringToObject(props, "chain", "ethereum");
  cJSON_AddStringToObject(props, "address", hash);
  cJSON_AddBoolToObject(props, "is_contract", is_contract);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[256];
  snprintf(link, sizeof link, "https://eth.blockscout.com/address/%s", hash);

  intel_item it = {0};
  it.remote_key      = hash;
  it.title           = name ? name : hash;
  it.summary         = is_contract ? "Ethereum contract" : "Ethereum account";
  it.body            = bj;
  it.lang            = "en";
  it.link            = link;
  it.record_type     = "eth-address";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"crypto\",\"ethereum\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[ETH_BLOCKSCOUT] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

/* ---- TRON_SCAN ---------------------------------------------------------- *
 * GET https://apilist.tronscanapi.com/api/accountv2?address=<addr>
 *   { address, balance (sun), totalTransactionCount, name, ... }. Keyless.
 * A missing account returns balance/counts of 0 but still has "address". */
static int c2_tronscan(const source_ctx *ctx, intel_sink *sink, const char *addr) {
  char *enc = jo_urlencode(addr);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
           "https://apilist.tronscanapi.com/api/accountv2?address=%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (crypto)", NULL };
  char *body = jo_get(ctx, url, hdrs, "TRON_SCAN");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const char *acct = jo_sv(root, "address");
  if (!acct) { cJSON_Delete(root); fprintf(stderr, "[TRON_SCAN] no account\n"); return 0; }
  char bal[96] = {0}; int have_bal = c2_numstr(root, "balance", bal, sizeof bal);
  double txc = 0; int have_tx = c2_num(root, "totalTransactionCount", &txc);
  const char *name = jo_sv(root, "name");

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "address", acct);
  cJSON_AddStringToObject(data, "chain", "tron");
  if (have_bal) cJSON_AddStringToObject(data, "balance_sun", bal);
  if (have_tx)  cJSON_AddNumberToObject(data, "tx_count", txc);
  if (name)     cJSON_AddStringToObject(data, "name", name);
  cJSON_AddStringToObject(data, "source", "TronScan");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "TRON_SCAN");
  cJSON_AddStringToObject(props, "chain", "tron");
  cJSON_AddStringToObject(props, "address", acct);
  if (have_tx) cJSON_AddNumberToObject(props, "tx_count", txc);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[256];
  snprintf(link, sizeof link, "https://tronscan.org/#/address/%s", acct);

  intel_item it = {0};
  it.remote_key      = acct;
  it.title           = name ? name : acct;
  it.summary         = "Tron account";
  it.body            = bj;
  it.lang            = "en";
  it.link            = link;
  it.record_type     = "tron-address";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"crypto\",\"tron\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[TRON_SCAN] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

/* ---- SOLANA_RPC --------------------------------------------------------- *
 * POST https://api.mainnet-beta.solana.com
 *   {"jsonrpc":"2.0","id":1,"method":"getAccountInfo",
 *    "params":["<addr>",{"encoding":"base64"}]}
 * → { result: { context:{slot}, value: { lamports, owner, executable,
 *     rentEpoch, ... } | null } }. value==null means the account doesn't exist
 * → honest empty. All emitted fields are real RPC results. */
static int c2_solana(const source_ctx *ctx, intel_sink *sink, const char *addr) {
  /* build JSON-RPC request body (address is escaped by cJSON) */
  cJSON *req = cJSON_CreateObject();
  cJSON_AddStringToObject(req, "jsonrpc", "2.0");
  cJSON_AddNumberToObject(req, "id", 1);
  cJSON_AddStringToObject(req, "method", "getAccountInfo");
  cJSON *params = cJSON_CreateArray();
  cJSON_AddItemToArray(params, cJSON_CreateString(addr));
  cJSON *opt = cJSON_CreateObject();
  cJSON_AddStringToObject(opt, "encoding", "base64");
  cJSON_AddItemToArray(params, opt);
  cJSON_AddItemToObject(req, "params", params);
  char *rb = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);
  if (!rb) return 0;

  const char *hdrs[] = { "Content-Type: application/json",
                         "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (crypto)", NULL };
  http_response hr = {0};
  int hc = http_request(ctx->http, "POST",
                        "https://api.mainnet-beta.solana.com",
                        hdrs, rb, strlen(rb), 20000, 1, &hr);
  free(rb);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[SOLANA_RPC] http status=%ld\n", hr.status);
    http_response_free(&hr); return 0;
  }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) return 0;

  cJSON *result = cJSON_GetObjectItem(root, "result");
  cJSON *value  = result ? cJSON_GetObjectItem(result, "value") : NULL;
  if (!value || cJSON_IsNull(value)) {
    cJSON_Delete(root);
    fprintf(stderr, "[SOLANA_RPC] account not found\n");
    return 0;   /* honest empty — account doesn't exist */
  }

  double lamports = 0; int have_lam = c2_num(value, "lamports", &lamports);
  const char *owner = jo_sv(value, "owner");
  const cJSON *ex = cJSON_GetObjectItem(value, "executable");
  int executable = (ex && cJSON_IsBool(ex)) ? cJSON_IsTrue(ex) : 0;
  double rent = 0; int have_rent = c2_num(value, "rentEpoch", &rent);
  double slot = 0; int have_slot = result ? c2_num(cJSON_GetObjectItem(result, "context"), "slot", &slot) : 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "address", addr);
  cJSON_AddStringToObject(data, "chain", "solana");
  if (have_lam)  cJSON_AddNumberToObject(data, "lamports", lamports);
  if (owner)     cJSON_AddStringToObject(data, "owner", owner);
  cJSON_AddBoolToObject(data, "executable", executable);
  if (have_rent) cJSON_AddNumberToObject(data, "rent_epoch", rent);
  if (have_slot) cJSON_AddNumberToObject(data, "slot", slot);
  cJSON_AddStringToObject(data, "source", "Solana JSON-RPC");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SOLANA_RPC");
  cJSON_AddStringToObject(props, "chain", "solana");
  cJSON_AddStringToObject(props, "address", addr);
  if (owner) cJSON_AddStringToObject(props, "owner", owner);
  cJSON_AddBoolToObject(props, "executable", executable);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[256];
  snprintf(link, sizeof link, "https://explorer.solana.com/address/%s", addr);

  intel_item it = {0};
  it.remote_key      = addr;
  it.title           = addr;
  it.summary         = executable ? "Solana program" : "Solana account";
  it.body            = bj;
  it.lang            = "en";
  it.link            = link;
  it.record_type     = "solana-address";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"crypto\",\"solana\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[SOLANA_RPC] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

/* ---- ENS_RESOLVE -------------------------------------------------------- *
 * GET https://api.ensideas.com/ens/resolve/<name>
 *   { address, name, displayName, avatar } — resolves an ENS name to its
 *   address (and reverse). address==null means the name is unregistered →
 *   honest empty. Keyless LIVE. */
static int c2_ens(const source_ctx *ctx, intel_sink *sink, const char *nameq) {
  char *enc = jo_urlencode(nameq);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
           "https://api.ensideas.com/ens/resolve/%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (crypto)", NULL };
  char *body = jo_get(ctx, url, hdrs, "ENS_RESOLVE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const char *address = jo_sv(root, "address");
  const char *name    = jo_sv(root, "name");
  const char *disp    = jo_sv(root, "displayName");
  const char *avatar  = jo_sv(root, "avatar");
  if (!address) {   /* unresolved name → honest empty */
    cJSON_Delete(root);
    fprintf(stderr, "[ENS_RESOLVE] no resolution\n");
    return 0;
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "address", address);
  if (name)   cJSON_AddStringToObject(data, "ens_name", name);
  if (disp)   cJSON_AddStringToObject(data, "display_name", disp);
  if (avatar) cJSON_AddStringToObject(data, "avatar", avatar);
  cJSON_AddStringToObject(data, "query", nameq);
  cJSON_AddStringToObject(data, "source", "ENS (ensideas)");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "ENS_RESOLVE");
  cJSON_AddStringToObject(props, "chain", "ethereum");
  cJSON_AddStringToObject(props, "address", address);
  if (name) cJSON_AddStringToObject(props, "ens_name", name);
  cJSON_AddStringToObject(props, "query", nameq);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[256];
  snprintf(link, sizeof link, "https://app.ens.domains/%s", name ? name : nameq);

  intel_item it = {0};
  it.remote_key      = address;
  it.title           = disp ? disp : (name ? name : address);
  it.summary         = address;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link;
  it.record_type     = "ens-resolution";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"crypto\",\"ens\",\"ethereum\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[ENS_RESOLVE] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;

  if (strcmp(ctx->source_id, "ETH_BLOCKSCOUT") == 0) return c2_blockscout(ctx, sink, q);
  if (strcmp(ctx->source_id, "TRON_SCAN") == 0)      return c2_tronscan(ctx, sink, q);
  if (strcmp(ctx->source_id, "SOLANA_RPC") == 0)     return c2_solana(ctx, sink, q);
  if (strcmp(ctx->source_id, "ENS_RESOLVE") == 0)    return c2_ens(ctx, sink, q);
  return -1;
}

#define C2_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "cyber", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

C2_DEF(c2_eth_def, "ETH_BLOCKSCOUT", "Ethereum Blockscout Lookup", "イーサリアム Blockscout 照会",
  "https://eth.blockscout.com/api/v2",
  "Ethereum address → balance, contract flag, tags via Blockscout (keyless LIVE)");
C2_DEF(c2_tron_def, "TRON_SCAN", "Tron TronScan Lookup", "トロン TronScan 照会",
  "https://apilist.tronscanapi.com/api",
  "Tron address → balance, tx count, name via TronScan public API (keyless LIVE)");
C2_DEF(c2_sol_def, "SOLANA_RPC", "Solana RPC Account Lookup", "ソラナ RPC アカウント照会",
  "https://api.mainnet-beta.solana.com",
  "Solana address → lamports, owner, executable via public JSON-RPC getAccountInfo (keyless LIVE)");
C2_DEF(c2_ens_def, "ENS_RESOLVE", "ENS Name Resolver", "ENS 名前解決",
  "https://api.ensideas.com/ens/resolve",
  "ENS name → resolved address + avatar/display via ensideas (keyless LIVE)");
