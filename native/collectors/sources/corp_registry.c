/* collectors/osint/sources/corp_registry.c
 * OSINT services — public business registries. Entity pivot (company name or
 * 13-digit corporate number / 登録番号).
 *   • Invoice issuer (NTA)   — real API, gated on INVOICE_APP_ID (free app id).
 *   • Construction licenses  — MLIT 建設業者検索 (server-rendered anchor scrape).
 *   • Minpaku registry       — 民泊 届出住宅 search (anchor scrape).
 *   • NPO portal             — 内閣府 NPO法人ポータル search (anchor scrape).
 * Real fetch or honest empty; nothing synthesized. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- NTA Qualified-Invoice issuer registry (NEEDS_KEY) -------------------- */
static int run_invoice(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  const char *app = jo_env("INVOICE_APP_ID");
  if (!app) { fprintf(stderr, "[invoice] gated (no INVOICE_APP_ID)\n"); return 0; }
  /* Lookup is by registration number (T + 13-digit corporate number). Accept a
   * bare 13-digit corporate number and prefix T, or a T-number as given. */
  char num[24] = {0};
  const char *e = ctx->entity;
  int dig = 1, n = 0; for (const char *c = e; *c; c++, n++) if (!isdigit((unsigned char)*c)) dig = 0;
  if (dig && n == 13) snprintf(num, sizeof num, "T%s", e);
  else if ((e[0] == 'T' || e[0] == 't') && n == 14) snprintf(num, sizeof num, "T%s", e + 1);
  else { fprintf(stderr, "[invoice] entity not a 法人番号/登録番号\n"); return 0; }

  char url[512];
  snprintf(url, sizeof url,
    "https://web-api.invoice-kohyo.nta.go.jp/1/num?id=%s&number=%s&type=21&history=0",
    app, num + 1);   /* API wants the 13 digits without the leading T */
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "invoice");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *arr = cJSON_GetObjectItem(root, "announcement");
  cJSON *a = NULL;
  cJSON_ArrayForEach(a, (cJSON_IsArray(arr) ? arr : root)) {
    const char *name = jo_sv(a, "name");
    const char *rn   = jo_sv(a, "registratedNumber");
    const char *addr = jo_sv(a, "address");
    if (!name && !rn) continue;
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "Invoice Registry");
    if (rn)   cJSON_AddStringToObject(props, "registration_number", rn);
    if (addr) cJSON_AddStringToObject(props, "address", addr);
    cJSON_AddBoolToObject(props, "registered", 1);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);
    intel_item it = {0};
    it.remote_key = rn ? rn : name; it.title = name ? name : rn;
    it.summary = addr; it.lang = "ja"; it.record_type = "invoice-issuer";
    it.properties_json = pj; it.tags_json = "[\"osint-search\",\"registry\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[invoice] emitted %d\n", emitted);
  return 0;
}

/* ---- MLIT construction-business license search (anchor scrape) ----------- */
static int run_kensetsu(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://etsuran2.mlit.go.jp/TAKKEN/takkenKensaku.do?gyousyaMei=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, NULL, "MLIT Construction License",
                  "construction-license", "https://etsuran2.mlit.go.jp",
                  ctx->entity, 20, "kensetsu");
  return 0;
}

/* ---- Minpaku (住宅宿泊事業) registered-listing search (anchor scrape) ------ */
static int run_minpaku(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://www.minpaku.mlit.go.jp/minpakuSearch/?keyword=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, NULL, "Minpaku Registry",
                  "minpaku-listing", "https://www.minpaku.mlit.go.jp",
                  ctx->entity, 20, "minpaku");
  return 0;
}

/* ---- Cabinet Office NPO portal search (anchor scrape) --------------------- */
static int run_npo(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://www.npo-homepage.go.jp/npoportal/search?keyword=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, NULL, "NPO Portal",
                  "npo-corporation", "https://www.npo-homepage.go.jp",
                  ctx->entity, 20, "npo");
  return 0;
}

static const source_def invoice_def = {
  .id = "INVOICE_REGISTRY", .collector = "osint",
  .name = "NTA Invoice Issuer Registry", .name_ja = "適格請求書発行事業者公表",
  .update_interval_sec = 0, .run = run_invoice,
  .category = "government", .type = "api", .url = "https://www.invoice-kohyo.nta.go.jp/",
  .description = "Confirm a business is a registered invoice issuer by 法人番号/登録番号 (needs INVOICE_APP_ID)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(invoice_def)

static const source_def kensetsu_def = {
  .id = "CONSTRUCTION_LICENSE", .collector = "osint",
  .name = "Construction Business License", .name_ja = "建設業許可業者検索",
  .update_interval_sec = 0, .run = run_kensetsu,
  .category = "industry", .type = "scraped", .url = "https://www.mlit.go.jp/",
  .description = "MLIT licensed contractors — execs, capital, addresses, sanctions (entity pivot)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(kensetsu_def)

static const source_def minpaku_def = {
  .id = "MINPAKU_REGISTRY", .collector = "osint",
  .name = "Minpaku Registry", .name_ja = "民泊 届出住宅検索",
  .update_interval_sec = 0, .run = run_minpaku,
  .category = "economy", .type = "scraped", .url = "https://www.minpaku.mlit.go.jp/",
  .description = "Registered short-term-rental hosts: property address + license number (entity pivot)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(minpaku_def)

static const source_def npo_def = {
  .id = "NPO_PORTAL", .collector = "osint",
  .name = "NPO Corporation Portal", .name_ja = "NPO法人ポータル",
  .update_interval_sec = 0, .run = run_npo,
  .category = "government", .type = "scraped", .url = "https://www.npo-homepage.go.jp/",
  .description = "Cabinet Office NPO registry — officers, finances, activities (entity pivot)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(npo_def)
