/* collectors/sources/crypto_world.c
 * OSINT services — on-demand blockchain-address pivot (ctx->entity = a crypto
 * address). Three keyless/free sources, dispatched by ctx->source_id:
 *
 *   BTC_MEMPOOL  — mempool.space public REST. For a BTC address we GET
 *                  https://mempool.space/api/address/<addr> and emit the parsed
 *                  chain/mempool funded/spent stats + tx count. Keyless LIVE.
 *   BLOCKCHAIR   — api.blockchair.com/<chain>/dashboards/address/<addr> (free
 *                  tier, keyless). Chain inferred from the address shape (btc /
 *                  bitcoin-cash / litecoin / dogecoin / ethereum). Emits the
 *                  address dashboard summary (balance, received, spent, tx count).
 *   OFAC_CRYPTO  — U.S. Treasury OFAC SDN "advanced" XML. We fetch the published
 *                  sdn_advanced.xml and scan its <Feature> "Digital Currency
 *                  Address" values for an exact match of the queried address; on
 *                  a hit we emit the sanctioned listing. No match → honest empty.
 *
 * HONESTY: every run() REAL-fetches and emits only parsed real data, else
 * honest-empty (return 0). Nothing is fabricated; no constructed-URL cards. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* number field → malloc'd decimal string (caller frees), or NULL. */
static char *cw_num(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || !cJSON_IsNumber(v)) return NULL;
  char buf[64];
  double d = v->valuedouble;
  if (d == (double)(long long)d) snprintf(buf, sizeof buf, "%lld", (long long)d);
  else snprintf(buf, sizeof buf, "%.8g", d);
  return strdup(buf);
}

/* -------------------------------------------------------------- BTC_MEMPOOL */
static int run_mempool(const source_ctx *ctx, intel_sink *sink) {
  const char *addr = ctx->entity;
  if (!addr || !*addr) return -1;
  char *enc = jo_urlencode(addr);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url, "https://mempool.space/api/address/%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "btc_mempool");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const cJSON *chain = cJSON_GetObjectItem(root, "chain_stats");
  const cJSON *mem   = cJSON_GetObjectItem(root, "mempool_stats");
  if (!cJSON_IsObject(chain)) { cJSON_Delete(root); return 0; }

  char *funded = cw_num(chain, "funded_txo_sum");   /* satoshis */
  char *spent  = cw_num(chain, "spent_txo_sum");
  char *txc    = cw_num(chain, "tx_count");
  long long bal = 0; int have_bal = 0;
  {
    const cJSON *f = cJSON_GetObjectItem(chain, "funded_txo_sum");
    const cJSON *s = cJSON_GetObjectItem(chain, "spent_txo_sum");
    if (f && cJSON_IsNumber(f) && s && cJSON_IsNumber(s)) {
      bal = (long long)f->valuedouble - (long long)s->valuedouble; have_bal = 1;
    }
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "address", addr);
  cJSON_AddStringToObject(data, "chain", "bitcoin");
  if (funded) cJSON_AddStringToObject(data, "funded_sat", funded);
  if (spent)  cJSON_AddStringToObject(data, "spent_sat", spent);
  if (have_bal) cJSON_AddNumberToObject(data, "balance_sat", (double)bal);
  if (txc)    cJSON_AddStringToObject(data, "chain_tx_count", txc);
  if (cJSON_IsObject(mem)) {
    char *mtx = cw_num(mem, "tx_count");
    if (mtx) { cJSON_AddStringToObject(data, "mempool_tx_count", mtx); free(mtx); }
  }
  cJSON_AddStringToObject(data, "source", "mempool.space");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "BTC_MEMPOOL");
  cJSON_AddStringToObject(props, "chain", "bitcoin");
  cJSON_AddStringToObject(props, "address", addr);
  if (have_bal) cJSON_AddNumberToObject(props, "balance_sat", (double)bal);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[600];
  snprintf(link, sizeof link, "https://mempool.space/address/%s", addr);
  char title[256];
  snprintf(title, sizeof title, "BTC %s", addr);

  intel_item it = {0};
  it.remote_key      = addr;
  it.title           = title;
  it.summary         = have_bal ? "on-chain address stats" : "address stats";
  it.body            = bj;
  it.lang            = "en";
  it.link            = link;
  it.record_type     = "crypto-address";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"BTC_MEMPOOL\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(funded); free(spent); free(txc);
  cJSON_Delete(root);
  fprintf(stderr, "[btc_mempool] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

/* --------------------------------------------------------------- BLOCKCHAIR */
/* Infer a Blockchair chain slug from the address shape. NULL = unknown. */
static const char *bc_chain(const char *a) {
  if (!a || !*a) return NULL;
  size_t n = strlen(a);
  if ((a[0] == '0' && a[1] == 'x') && n == 42) return "ethereum";
  if (a[0] == 'L' || a[0] == 'M' || strncmp(a, "ltc1", 4) == 0) return "litecoin";
  if (a[0] == 'D') return "dogecoin";
  if (strncmp(a, "bitcoincash:", 12) == 0 || a[0] == 'q' || a[0] == 'p')
    return "bitcoin-cash";
  if (a[0] == '1' || a[0] == '3' || strncmp(a, "bc1", 3) == 0) return "bitcoin";
  return NULL;
}

static int run_blockchair(const source_ctx *ctx, intel_sink *sink) {
  const char *addr = ctx->entity;
  if (!addr || !*addr) return -1;
  const char *chain = bc_chain(addr);
  if (!chain) { fprintf(stderr, "[blockchair] unknown chain\n"); return 0; }
  char *enc = jo_urlencode(addr);
  if (!enc) return 0;
  char url[600];
  snprintf(url, sizeof url,
    "https://api.blockchair.com/%s/dashboards/address/%s", chain, enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "blockchair");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  /* data -> <address> -> address{...} */
  const cJSON *data = cJSON_GetObjectItem(root, "data");
  const cJSON *entry = (data && cJSON_IsObject(data)) ? cJSON_GetObjectItem(data, addr) : NULL;
  const cJSON *info  = entry ? cJSON_GetObjectItem(entry, "address") : NULL;
  if (!cJSON_IsObject(info)) { cJSON_Delete(root); fprintf(stderr, "[blockchair] emitted 0\n"); return 0; }

  char *bal  = cw_num(info, "balance");
  char *recv = cw_num(info, "received");
  char *spent= cw_num(info, "spent");
  char *txc  = cw_num(info, "transaction_count");
  const char *ftx = jo_sv(info, "first_seen_receiving");
  const char *ltx = jo_sv(info, "last_seen_spending");

  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "address", addr);
  cJSON_AddStringToObject(d, "chain", chain);
  if (bal)  cJSON_AddStringToObject(d, "balance", bal);
  if (recv) cJSON_AddStringToObject(d, "received", recv);
  if (spent)cJSON_AddStringToObject(d, "spent", spent);
  if (txc)  cJSON_AddStringToObject(d, "transaction_count", txc);
  if (ftx)  cJSON_AddStringToObject(d, "first_seen_receiving", ftx);
  cJSON_AddStringToObject(d, "source", "Blockchair");
  char *bj = cJSON_PrintUnformatted(d);
  cJSON_Delete(d);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "BLOCKCHAIR");
  cJSON_AddStringToObject(props, "chain", chain);
  cJSON_AddStringToObject(props, "address", addr);
  if (bal) cJSON_AddStringToObject(props, "balance", bal);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[600];
  snprintf(link, sizeof link, "https://blockchair.com/%s/address/%s", chain, addr);
  char title[256];
  snprintf(title, sizeof title, "%s %s", chain, addr);

  intel_item it = {0};
  it.remote_key      = addr;
  it.title           = title;
  it.summary         = txc ? "address dashboard" : "address";
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = ltx;
  it.link            = link;
  it.record_type     = "crypto-address";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"BLOCKCHAIR\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(bal); free(recv); free(spent); free(txc);
  cJSON_Delete(root);
  fprintf(stderr, "[blockchair] emitted %d\n", rc >= 0 ? 1 : 0);
  return 0;
}

/* -------------------------------------------------------------- OFAC_CRYPTO */
/* Scan the OFAC SDN advanced XML for a Digital Currency Address feature whose
 * value exactly equals the queried address. On a hit, emit the sanctioned entry
 * (the enclosing <sanctionsEntry>/<distinctParty> is too deep to reconstruct
 * robustly with a light scanner, so we anchor on the matched address value and
 * the nearest preceding party identity name). No match → honest empty. */
static int run_ofac(const source_ctx *ctx, intel_sink *sink) {
  const char *addr = ctx->entity;
  if (!addr || !*addr) return -1;
  /* The advanced XML tags crypto addresses as Feature values; the canonical,
   * human-readable SDN list also carries "Digital Currency Address - XBT ..."
   * text. We fetch the advanced XML (authoritative) and look for the literal
   * address bytes preceded by a VersionDetail/value marker. */
  char *xml = jo_get(ctx,
    "https://www.treasury.gov/ofac/downloads/sdn_advanced.xml", NULL, "ofac_crypto");
  if (!xml) return 0;

  int emitted = 0;
  const char *hit = strstr(xml, addr);
  /* Require the address to appear as a Digital Currency feature value, not an
   * incidental substring: OFAC wraps crypto addresses in <VersionDetail ...>ADDR
   * </VersionDetail> under a "Digital Currency Address" feature type. Accept a
   * match only when the surrounding bytes look like a value delimiter. */
  while (hit) {
    char before = (hit > xml) ? hit[-1] : '>';
    char after  = hit[strlen(addr)];
    int delim_ok = (before == '>' || before == '"' || before == ' ') &&
                   (after == '<' || after == '"' || after == ' ' || after == 0);
    if (delim_ok) {
      /* nearest preceding readable identity: OFAC identity names appear in
       * <Alias><DocumentedName>...<NamePartValue>NAME</NamePartValue> blocks.
       * Grab the closest preceding NamePartValue as a best-effort label. */
      const char *name = NULL; size_t namelen = 0;
      const char *scan = xml, *lastnpv = NULL;
      while ((scan = strstr(scan, "<NamePartValue")) && scan < hit) {
        lastnpv = scan; scan += 14;
      }
      if (lastnpv) {
        const char *gt = strchr(lastnpv, '>');
        const char *end = gt ? strstr(gt, "</NamePartValue>") : NULL;
        if (gt && end) { name = gt + 1; namelen = (size_t)(end - name); }
      }
      char label[256];
      if (name && namelen && namelen < sizeof label) {
        memcpy(label, name, namelen); label[namelen] = 0;
      } else {
        snprintf(label, sizeof label, "OFAC SDN entry");
      }

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "address", addr);
      cJSON_AddStringToObject(data, "list", "OFAC SDN");
      cJSON_AddStringToObject(data, "sanctioned_party", label);
      cJSON_AddBoolToObject(data, "sanctioned", 1);
      cJSON_AddStringToObject(data, "source", "US Treasury OFAC");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "OFAC_CRYPTO");
      cJSON_AddStringToObject(props, "address", addr);
      cJSON_AddStringToObject(props, "list", "OFAC SDN");
      cJSON_AddBoolToObject(props, "sanctioned", 1);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      char title[320];
      snprintf(title, sizeof title, "OFAC-sanctioned: %s", label);

      intel_item it = {0};
      it.remote_key      = addr;
      it.title           = title;
      it.summary         = "Address on OFAC SDN sanctions list";
      it.body            = bj;
      it.lang            = "en";
      it.link            = "https://sanctionssearch.ofac.treas.gov/";
      it.record_type     = "crypto-sanction";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"OFAC_CRYPTO\",\"sanctions\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
      break;   /* one authoritative hit is enough */
    }
    hit = strstr(hit + 1, addr);
  }
  free(xml);
  fprintf(stderr, "[ofac_crypto] emitted %d\n", emitted);
  return 0;
}

/* --------------------------------------------------------------- dispatch */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *id = ctx->source_id ? ctx->source_id : "";
  if (strcmp(id, "BTC_MEMPOOL") == 0) return run_mempool(ctx, sink);
  if (strcmp(id, "BLOCKCHAIR")  == 0) return run_blockchair(ctx, sink);
  if (strcmp(id, "OFAC_CRYPTO") == 0) return run_ofac(ctx, sink);
  return run_mempool(ctx, sink);   /* sane default */
}

static const source_def btc_mempool_def = {
  .id = "BTC_MEMPOOL", .collector = "osint",
  .name = "mempool.space BTC Address", .name_ja = "mempool.space BTCアドレス",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://mempool.space/api",
  .description = "mempool.space — keyless Bitcoin address on-chain/mempool stats (balance, tx count)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(btc_mempool_def)

static const source_def blockchair_def = {
  .id = "BLOCKCHAIR", .collector = "osint",
  .name = "Blockchair Address Dashboard", .name_ja = "Blockchair アドレス",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.blockchair.com",
  .description = "Blockchair free tier — multi-chain address dashboard (BTC/BCH/LTC/DOGE/ETH): balance, received, spent",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(blockchair_def)

static const source_def ofac_crypto_def = {
  .id = "OFAC_CRYPTO", .collector = "osint",
  .name = "OFAC Crypto SDN Check", .name_ja = "OFAC 暗号資産制裁リスト照合",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "dataset",
  .url = "https://www.treasury.gov/ofac/downloads/sdn_advanced.xml",
  .description = "US Treasury OFAC SDN advanced XML — checks a crypto address against the sanctions list",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ofac_crypto_def)
