/* collectors/osint/sources/gov_money.c
 * OSINT services — flows of public money & political finance.
 *   • jGrants        — LIVE public API: subsidy programs (keyword pivot).
 *   • Political funds — 総務省 政治資金収支報告書 index (anchor scrape).
 *   • Asset disclosures — 国会議員 資産公開 (anchor scrape).
 *   • GEPS / 調達ポータル — central-gov procurement (anchor scrape).
 *   • Local tenders   — 入札情報 portals (anchor scrape).
 *   • Customs trade   — 税関 貿易統計 index (anchor scrape).
 * Real fetch or honest empty. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/csv.h"
#include "lib/seenset.h"
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

/* ---- charset-safe anchor scrape ------------------------------------------
 * Several of the ministry portals below serve legacy Shift_JIS and send no
 * charset in the Content-Type header. The shared jo_get()/jo_emit_anchors()
 * pair has no transcoding step, so those bytes went into TEXT columns as-is
 * and every row came back "Could not decode to UTF-8" — observed live on
 * CUSTOMS_TRADE (customs.go.jp) and POLITICAL_FUNDS (soumu.go.jp). That
 * corrupts FTS and breaks any client reading the table.
 *
 * jo_emit_anchors() is included by 134 collectors, so it is not patched here;
 * this file gets a local, charset-sniffing replacement instead. See
 * report_a7.md — the shared helper needs the same treatment centrally. */

/* Strict UTF-8 validation: 1 if every sequence is well-formed. */
static int gm_is_utf8(const unsigned char *s, size_t n) {
  for (size_t i = 0; i < n; ) {
    unsigned char c = s[i];
    int extra;
    if (c < 0x80) { i++; continue; }
    else if ((c & 0xE0) == 0xC0) { extra = 1; if (c < 0xC2) return 0; }
    else if ((c & 0xF0) == 0xE0) extra = 2;
    else if ((c & 0xF8) == 0xF0) { extra = 3; if (c > 0xF4) return 0; }
    else return 0;
    if (i + (size_t)extra >= n) return 0;
    for (int k = 1; k <= extra; k++)
      if ((s[i + k] & 0xC0) != 0x80) return 0;
    i += (size_t)extra + 1;
  }
  return 1;
}

/* GET url and return a guaranteed-UTF-8 body (caller frees), or NULL. */
static char *gm_fetch_utf8(const source_ctx *ctx, const char *url, const char *tag) {
  char *raw = jo_get(ctx, url, NULL, tag);
  if (!raw) return NULL;
  size_t len = strlen(raw);
  if (gm_is_utf8((const unsigned char *)raw, len)) return raw;
  char *conv = csv_decode_sjis(raw, len);
  free(raw);
  if (conv) fprintf(stderr, "[%s] transcoded Shift_JIS -> UTF-8\n", tag);
  return conv;
}

/* Anchor scrape over an already-decoded page.
 *  href_must : if set, keep only hrefs containing this substring.
 *  query     : if set, keep only anchors whose text or href contains it.
 * Skips in-page nav anchors (href starting '#') — those produced rows titled
 * "skip to main content" in GEPS / LOCAL_TENDERS / CUSTOMS_TRADE. */
static int gm_emit_anchors(const source_ctx *ctx, intel_sink *sink,
                           const char *url, const char *href_must,
                           const char *service, const char *record_type,
                           const char *base, const char *query,
                           int max, const char *tag) {
  char *html = gm_fetch_utf8(ctx, url, tag);
  if (!html) return 0;
  int emitted = 0;
  const char *p = html;
  /* Was char *seen[64]: past the 64th anchor the dedupe stopped RECORDING while
   * the loop kept emitting, so anchor 65 onwards could be emitted twice. The
   * growable set costs nothing and cannot silently stop working. */
  seen_set seen = {0};
  while (emitted < max && (p = strstr(p, "<a ")) != NULL) {
    const char *h = strstr(p, "href=\"");
    const char *tagend = strchr(p, '>');
    p += 3;
    if (!h || !tagend || h > tagend) continue;
    h += 6;
    const char *he = strchr(h, '"');
    if (!he) continue;
    size_t hlen = (size_t)(he - h);
    if (hlen == 0 || hlen > 800) continue;
    const char *atext = strchr(he, '>');
    const char *aclose = atext ? strstr(atext, "</a>") : NULL;
    if (!atext || !aclose) continue;
    atext += 1;
    char text[512]; size_t tj = 0; int intag = 0;
    for (const char *q2 = atext; q2 < aclose && tj < sizeof text - 1; q2++) {
      if (*q2 == '<') intag = 1;
      else if (*q2 == '>') intag = 0;
      else if (!intag && *q2 != '\n' && *q2 != '\t') text[tj++] = *q2;
    }
    while (tj && text[tj - 1] == ' ') tj--;
    text[tj] = 0;
    char href[820];
    snprintf(href, sizeof href, "%.*s", (int)hlen, h);
    p = aclose + 4;
    if (tj < 3) continue;
    if (href[0] == '#') continue;                  /* in-page nav */
    if (href_must && !strstr(href, href_must)) continue;
    if (query && *query && !strstr(text, query) && !strstr(href, query)) continue;
    if (!seen_add(&seen, href)) continue;

    char link[900];
    if (!strncmp(href, "http", 4)) snprintf(link, sizeof link, "%s", href);
    else snprintf(link, sizeof link, "%s%s", base ? base : "", href);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", service);
    if (query) cJSON_AddStringToObject(props, "query", query);
    cJSON_AddStringToObject(props, "href", link);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = link;
    it.title           = text;
    it.link            = link;
    it.lang            = "ja";
    it.record_type     = record_type;
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"gov-money\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  seen_free(&seen);
  free(html);
  (void)ctx;
  fprintf(stderr, "[%s] emitted %d\n", tag, emitted);
  return emitted;
}

static int run_polifunds(const source_ctx *ctx, intel_sink *sink) {
  gm_emit_anchors(ctx, sink,
    "https://www.soumu.go.jp/senkyo/seiji_s/seijishikin/", NULL,
    "Political Funds", "political-funds-report", "https://www.soumu.go.jp",
    (ctx->entity && *ctx->entity) ? ctx->entity : NULL, 30, "polifunds");
  return 0;
}

static int run_assets(const source_ctx *ctx, intel_sink *sink) {
  gm_emit_anchors(ctx, sink,
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
  gm_emit_anchors(ctx, sink, url, NULL, "GEPS Procurement", "gov-tender",
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
  gm_emit_anchors(ctx, sink, url, NULL, "Local Tenders", "local-tender",
                  "https://www.geps.go.jp",
                  (ctx->entity && *ctx->entity) ? ctx->entity : NULL, 30, "tenders");
  return 0;
}

/* Index-page anchor scrape of the trade-statistics section. customs.go.jp
 * serves Shift_JIS with no charset header - gm_emit_anchors() sniffs and
 * transcodes. NOTE: these rows are the section's link labels, NOT import/
 * export statistics; the real numbers sit behind the e-Stat query forms. */
static int run_customs(const source_ctx *ctx, intel_sink *sink) {
  gm_emit_anchors(ctx, sink,
    "https://www.customs.go.jp/toukei/info/index.htm", "/toukei/",
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
