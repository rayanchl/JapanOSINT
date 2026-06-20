/* collectors/osint/sources/corp_financials.c
 * OSINT services — listed-company financials. Company/ticker pivot.
 *   • UFOCatch   — LIVE: parses the public Atom feed of recent EDINET filings
 *                  (有価証券報告書 etc.) and emits entries whose title mentions
 *                  the queried company. Keyless.
 *   • Buffett Code — real API, gated on BUFFETT_CODE_TOKEN (honest empty without).
 *   • Shikiho     — paid; gated on SHIKIHO_API_KEY (honest empty without).
 * No fabricated financials — only fetched filing metadata / API rows. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- UFOCatch: Atom feed of EDINET filings (LIVE, keyless) ---------------- */
static int run_ufocatch(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *body = jo_get(ctx, "https://resource.ufocatch.com/atom/edinetx", NULL, "ufocatch");
  if (!body) return 0;
  int emitted = 0;
  char *p = body;
  char *e;
  while (emitted < 25 && (e = strstr(p, "<entry")) != NULL) {
    char *end = strstr(e, "</entry>");
    if (!end) break;
    char saved = *end; *end = 0;            /* bound parsing to this entry */
    char *cur = e;
    char *title = jo_tag_inner((const char **)&cur, "title");
    char *updated = (cur = e, jo_tag_inner((const char **)&cur, "updated"));
    /* atom link is an attribute: <link ... href="URL"/> */
    char link[800] = {0};
    char *lh = strstr(e, "href=\"");
    if (lh) { lh += 6; char *le = strchr(lh, '"'); if (le && le - lh < 790) snprintf(link, sizeof link, "%.*s", (int)(le - lh), lh); }
    *end = saved;
    p = end + 8;

    if (title && strstr(title, ctx->entity)) {
      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "UFOCatch");
      cJSON_AddStringToObject(props, "source", "EDINET via UFOCatch");
      cJSON_AddStringToObject(props, "query", ctx->entity);
      if (link[0]) cJSON_AddStringToObject(props, "document_url", link);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = link[0] ? link : title;
      it.title           = title;
      it.link            = link[0] ? link : NULL;
      it.published_at    = updated;
      it.lang            = "ja";
      it.record_type     = "edinet-filing";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"financials\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(pj);
    }
    free(title); free(updated);
  }
  free(body);
  fprintf(stderr, "[ufocatch] emitted %d\n", emitted);
  return 0;
}

/* ---- Buffett Code: normalized financial metrics (NEEDS_KEY) --------------- */
static int run_buffett(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  const char *key = jo_env("BUFFETT_CODE_TOKEN");
  if (!key) { fprintf(stderr, "[buffett-code] gated (no BUFFETT_CODE_TOKEN)\n"); return 0; }
  /* API keys on a 4-digit TSE ticker; skip non-ticker queries. */
  int dig = 1, n = 0; for (const char *c = ctx->entity; *c; c++, n++) if (!isdigit((unsigned char)*c)) dig = 0;
  if (!(dig && n == 4)) { fprintf(stderr, "[buffett-code] entity not a ticker\n"); return 0; }
  char url[256];
  snprintf(url, sizeof url, "https://api.buffett-code.com/api/v3/company?tickers=%s", ctx->entity);
  char hdr[160]; snprintf(hdr, sizeof hdr, "x-api-key: %s", key);
  const char *hdrs[] = { hdr, "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "buffett-code");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *data = cJSON_GetObjectItem(root, "data");
  cJSON *row = NULL;
  cJSON_ArrayForEach(row, (cJSON_IsArray(data) ? data : root)) {
    const char *name = jo_sv(row, "company_name");
    if (!name) continue;
    char *pj = cJSON_PrintUnformatted(row);
    intel_item it = {0};
    it.remote_key = ctx->entity; it.title = name; it.lang = "ja";
    it.record_type = "company-financials"; it.body = pj;
    it.properties_json = "{\"service\":\"Buffett Code\",\"success\":true}";
    it.tags_json = "[\"osint-search\",\"financials\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[buffett-code] emitted %d\n", emitted);
  return 0;
}

/* ---- Shikiho: paid; honest empty without key ------------------------------ */
static int run_shikiho(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  if (!ctx->entity || !*ctx->entity) return -1;
  if (!jo_env("SHIKIHO_API_KEY")) { fprintf(stderr, "[shikiho] gated (no SHIKIHO_API_KEY, paid)\n"); return 0; }
  /* Paid product without a public REST contract; wire the real endpoint once a
   * subscription/key is provisioned. Honest empty until then. */
  fprintf(stderr, "[shikiho] key present but endpoint not provisioned\n");
  return 0;
}

static const source_def ufocatch_def = {
  .id = "UFOCATCH", .collector = "osint",
  .name = "UFOCatch (EDINET filings)", .name_ja = "有報キャッチャー",
  .update_interval_sec = 0, .run = run_ufocatch,
  .category = "government", .type = "api", .url = "https://ufocatch.com/",
  .description = "Full-text-adjacent search over recent EDINET/有価証券報告書 disclosures (entity pivot)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ufocatch_def)

static const source_def buffett_def = {
  .id = "BUFFETT_CODE", .collector = "osint",
  .name = "Buffett Code Financials", .name_ja = "Buffett Code 財務指標",
  .update_interval_sec = 0, .run = run_buffett,
  .category = "economy", .type = "api", .url = "https://buffett-code.com/",
  .description = "Normalized financial metrics for listed companies by ticker (needs BUFFETT_CODE_TOKEN)",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(buffett_def)

static const source_def shikiho_def = {
  .id = "SHIKIHO", .collector = "osint",
  .name = "Kaisha Shikiho", .name_ja = "会社四季報オンライン",
  .update_interval_sec = 0, .run = run_shikiho,
  .category = "economy", .type = "api", .url = "https://shikiho.toyokeizai.net/",
  .description = "Listed-company financials, forecasts & top shareholders (paid; needs SHIKIHO_API_KEY)",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(shikiho_def)
