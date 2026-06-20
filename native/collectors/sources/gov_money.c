/* collectors/osint/sources/gov_money.c
 * OSINT services — flows of public money & political finance.
 *   • jGrants        — LIVE public API: subsidy programs (keyword pivot).
 *   • Political funds — 総務省 政治資金収支報告書 index (anchor scrape).
 *   • Asset disclosures — 国会議員 資産公開 (anchor scrape).
 *   • GEPS / 調達ポータル — central-gov procurement (anchor scrape).
 *   • Local tenders   — 入札情報 portals (anchor scrape).
 *   • Customs trade   — 税関 貿易統計 index (anchor scrape).
 * Real fetch or honest empty. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- jGrants subsidy programs (LIVE public API) -------------------------- */
static int run_jgrants(const source_ctx *ctx, intel_sink *sink) {
  const char *kw = (ctx->entity && *ctx->entity) ? ctx->entity : "補助金";
  char *q = jo_urlencode(kw); if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://api.jgrants-portal.go.jp/exp/v1/public/subsidies?keyword=%s&sort=created_date&order=DESC&acceptance=0", q);
  free(q);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "jgrants");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *res = cJSON_GetObjectItem(root, "result");
  cJSON *row = NULL;
  cJSON_ArrayForEach(row, res) {
    const char *id = jo_sv(row, "id");
    const char *title = jo_sv(row, "title");
    if (!title) continue;
    const char *tgt = jo_sv(row, "target_area_search");
    const char *end = jo_sv(row, "acceptance_end_datetime");
    char link[256] = {0};
    if (id) snprintf(link, sizeof link, "https://www.jgrants-portal.go.jp/subsidy/%s", id);
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "jGrants");
    if (id) cJSON_AddStringToObject(props, "subsidy_id", id);
    if (tgt) cJSON_AddStringToObject(props, "target_area", tgt);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props); cJSON_Delete(props);
    intel_item it = {0};
    it.remote_key = id ? id : title; it.title = title; it.summary = tgt;
    it.link = link[0] ? link : NULL; it.published_at = end; it.lang = "ja";
    it.record_type = "subsidy-program"; it.properties_json = pj;
    it.tags_json = "[\"osint-search\",\"gov-money\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[jgrants] emitted %d\n", emitted);
  return 0;
}

static int run_polifunds(const source_ctx *ctx, intel_sink *sink) {
  jo_emit_anchors(ctx, sink,
    "https://www.soumu.go.jp/senkyo/seiji_s/seijishikin/", NULL,
    "Political Funds", "political-funds-report", "https://www.soumu.go.jp",
    (ctx->entity && *ctx->entity) ? ctx->entity : NULL, 30, "polifunds");
  return 0;
}

static int run_assets(const source_ctx *ctx, intel_sink *sink) {
  jo_emit_anchors(ctx, sink,
    "https://www.shugiin.go.jp/internet/itdb_annai.nsf/html/statics/shiryo/sisan.htm",
    NULL, "Diet Asset Disclosure", "asset-disclosure", "https://www.shugiin.go.jp",
    (ctx->entity && *ctx->entity) ? ctx->entity : NULL, 30, "assets");
  return 0;
}

static int run_geps(const source_ctx *ctx, intel_sink *sink) {
  char *q = (ctx->entity && *ctx->entity) ? jo_urlencode(ctx->entity) : NULL;
  char url[640];
  if (q) { snprintf(url, sizeof url, "https://www.p-portal.go.jp/pps-web-biz/UAA01/OAA0101?keyword=%s", q); free(q); }
  else snprintf(url, sizeof url, "https://www.p-portal.go.jp/pps-web-biz/UAA01/OAA0101");
  jo_emit_anchors(ctx, sink, url, NULL, "GEPS Procurement", "gov-tender",
                  "https://www.p-portal.go.jp",
                  (ctx->entity && *ctx->entity) ? ctx->entity : NULL, 30, "geps");
  return 0;
}

static int run_tenders(const source_ctx *ctx, intel_sink *sink) {
  /* Aggregated local-government tender notices via the open-data CKAN catalog
   * (real listing of bid/award datasets). Entity filters by keyword. */
  char *q = (ctx->entity && *ctx->entity) ? jo_urlencode(ctx->entity) : jo_urlencode("入札");
  if (!q) return 0;
  char url[640];
  snprintf(url, sizeof url, "https://www.geps.go.jp/bizportal/searchKoukoku?keyword=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, NULL, "Local Tenders", "local-tender",
                  "https://www.geps.go.jp",
                  (ctx->entity && *ctx->entity) ? ctx->entity : NULL, 30, "tenders");
  return 0;
}

static int run_customs(const source_ctx *ctx, intel_sink *sink) {
  jo_emit_anchors(ctx, sink,
    "https://www.customs.go.jp/toukei/info/index.htm", NULL,
    "Customs Trade Statistics", "trade-statistics", "https://www.customs.go.jp",
    NULL, 30, "customs");
  return 0;
}

#define DEF(SYM, ID, NAME, NAMEJA, RUN, CAT, TYPE, URL, DESC, FREE) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = RUN, .category = CAT, \
    .type = TYPE, .url = URL, .description = DESC, .layer = NULL, .free_tier = FREE }; \
  REGISTER_SOURCE(SYM)

DEF(jgrants_def, "JGRANTS", "jGrants Subsidies", "jGrants 補助金", run_jgrants,
    "government", "api", "https://www.jgrants-portal.go.jp/",
    "Government subsidy programs & recipients (keyword pivot, LIVE API)", 1);
DEF(polifunds_def, "POLITICAL_FUNDS", "Political Funds Reports", "政治資金収支報告書",
    run_polifunds, "government", "scraped", "https://www.soumu.go.jp/",
    "Political donations, expenditures & donor names (report index)", 1);
DEF(assets_def, "ASSET_DISCLOSURE", "Diet Asset Disclosures", "国会議員 資産公開",
    run_assets, "government", "scraped", "https://www.shugiin.go.jp/",
    "Legislators' assets, real estate & securities disclosures", 1);
DEF(geps_def, "GEPS_PROCUREMENT", "GEPS Procurement", "政府電子調達 調達ポータル",
    run_geps, "government", "scraped", "https://www.p-portal.go.jp/",
    "Central-government tenders, bidders, awards & contract values", 1);
DEF(tenders_def, "LOCAL_TENDERS", "Local Government Tenders", "自治体 入札・契約結果",
    run_tenders, "government", "scraped", "https://www.geps.go.jp/",
    "Local-government tenders & awarded vendors (keyword pivot)", 1);
DEF(customs_def, "CUSTOMS_TRADE", "Customs Trade Statistics", "税関 貿易統計",
    run_customs, "economy", "scraped", "https://www.customs.go.jp/",
    "Import/export statistics by HS code, partner country & port", 1);
