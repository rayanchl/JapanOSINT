/* collectors/osint/sources/corp_markets_media.c
 * OSINT services — market signals, regulator registries & press wires.
 *   • PR TIMES    — LIVE: parses the public RSS of press releases; with an
 *                   entity, filters to releases mentioning the company.
 *   • JPX short   — anchor scrape of the short-selling balance report index
 *                   (surfaces the real published report files).
 *   • FSA registry— anchor scrape of the licensed financial-business list.
 *   • iTownpage   — anchor scrape of the business directory search.
 * Real fetch or honest empty. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- PR TIMES press-release RSS (LIVE, keyless) -------------------------- */
static int run_prtimes(const source_ctx *ctx, intel_sink *sink) {
  char *body = jo_get(ctx, "https://prtimes.jp/index.rdf", NULL, "prtimes");
  if (!body) return 0;
  int emitted = 0;
  char *p = body, *e;
  while (emitted < 30 && (e = strstr(p, "<item")) != NULL) {
    char *end = strstr(e, "</item>");
    if (!end) break;
    char saved = *end; *end = 0;
    char *cur = e; char *title = jo_tag_inner((const char **)&cur, "title");
    cur = e; char *link = jo_tag_inner((const char **)&cur, "link");
    cur = e; char *date = jo_tag_inner((const char **)&cur, "dc:date");
    cur = e; char *desc = jo_tag_inner((const char **)&cur, "description");
    *end = saved; p = end + 7;

    int keep = 1;
    if (ctx->entity && *ctx->entity)
      keep = (title && strstr(title, ctx->entity)) ||
             (desc && strstr(desc, ctx->entity));
    if (keep && title) {
      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "PR TIMES");
      if (ctx->entity && *ctx->entity) cJSON_AddStringToObject(props, "query", ctx->entity);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);
      intel_item it = {0};
      it.remote_key = link ? link : title; it.title = title;
      it.summary = desc; it.link = link; it.published_at = date; it.lang = "ja";
      it.record_type = "press-release"; it.properties_json = pj;
      it.tags_json = "[\"osint-search\",\"press\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(pj);
    }
    free(title); free(link); free(date); free(desc);
  }
  free(body);
  fprintf(stderr, "[prtimes] emitted %d\n", emitted);
  return 0;
}

/* ---- JPX short-selling balance reports (anchor scrape) -------------------- */
static int run_jpx_short(const source_ctx *ctx, intel_sink *sink) {
  /* Feed-style: surface the latest published report files (real links). */
  jo_emit_anchors(ctx, sink,
    "https://www.jpx.co.jp/markets/statistics-equities/short-selling/index.html",
    NULL, "JPX Short Selling", "short-selling-report",
    "https://www.jpx.co.jp", NULL, 25, "jpx-short");
  return 0;
}

/* ---- FSA licensed financial-business list (anchor scrape) ----------------- */
static int run_fsa_registry(const source_ctx *ctx, intel_sink *sink) {
  jo_emit_anchors(ctx, sink,
    "https://www.fsa.go.jp/menkyo/menkyoj/", NULL,
    "FSA Financial Business Registry", "fsa-registered-firm",
    "https://www.fsa.go.jp", (ctx->entity && *ctx->entity) ? ctx->entity : NULL,
    30, "fsa-registry");
  return 0;
}

/* ---- iTownpage business directory (anchor scrape) ------------------------- */
static int run_itownpage(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url, "https://itp.ne.jp/keyword/?keyword=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, "/info/", "iTownpage", "business-listing",
                  "https://itp.ne.jp", NULL, 25, "itownpage");
  return 0;
}

static const source_def prtimes_def = {
  .id = "PRTIMES", .collector = "osint",
  .name = "PR TIMES Press Releases", .name_ja = "PR TIMES プレスリリース",
  .update_interval_sec = 1800, .run = run_prtimes,
  .category = "commercial", .type = "api", .url = "https://prtimes.jp/",
  .description = "Press-release wire: funding, exec changes, partnerships (latest feed + entity filter)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(prtimes_def)

static const source_def jpx_short_def = {
  .id = "JPX_SHORT_SELLING", .collector = "osint",
  .name = "JPX Short-Selling Balances", .name_ja = "JPX 空売り残高報告",
  .update_interval_sec = 0, .run = run_jpx_short,
  .category = "economy", .type = "scraped", .url = "https://www.jpx.co.jp/",
  .description = "Large short-position balance reports by fund & ticker (report index)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(jpx_short_def)

static const source_def fsa_registry_def = {
  .id = "FSA_FINBIZ_REGISTRY", .collector = "osint",
  .name = "FSA Financial Business Registry", .name_ja = "金融商品取引業者 登録一覧",
  .update_interval_sec = 0, .run = run_fsa_registry,
  .category = "government", .type = "scraped", .url = "https://www.fsa.go.jp/",
  .description = "Registered securities/advisory/fund/money-lender firms (FSA license lists)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(fsa_registry_def)

static const source_def itownpage_def = {
  .id = "ITOWNPAGE", .collector = "osint",
  .name = "iTownpage Directory", .name_ja = "iタウンページ",
  .update_interval_sec = 0, .run = run_itownpage,
  .category = "commercial", .type = "scraped", .url = "https://itp.ne.jp/",
  .description = "Business (and legacy residential) directory by name/address/phone (entity pivot)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(itownpage_def)
