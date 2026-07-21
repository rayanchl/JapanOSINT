/* collectors/osint/sources/corp_reviews.c
 * OSINT services — Japanese employee-review / salary sites. On-demand company
 * pivot (entity = company name). Each does a real GET of the site's public
 * search page and emits one intel_item per result anchor that mentions the
 * company (real extracted links/titles — see jo_emit_anchors; nothing is
 * synthesized). JS-rendered/login-walled results simply yield honest empty.
 * Run three sites so the same company can be cross-checked across corpora.
 *
 * NOTE: these sites are partly client-rendered; the selectors are intentionally
 * generic (match the company name in result anchors). Verify yield against live
 * HTML — like other scrape sources, no-match degrades to honest empty. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int run_openwork(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity);
  if (!q) return 0;
  char url[768];
  snprintf(url, sizeof url, "https://www.openwork.jp/company_list.php?keyword=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, NULL, "OpenWork", "company-review",
                  "https://www.openwork.jp", ctx->entity, 15, "openwork");
  return 0;
}

static int run_lighthouse(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity);
  if (!q) return 0;
  char url[768];
  snprintf(url, sizeof url, "https://en-hyouban.com/search/?word=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, NULL, "en Lighthouse", "company-review",
                  "https://en-hyouban.com", ctx->entity, 15, "lighthouse");
  return 0;
}

static int run_jobtalk(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *q = jo_urlencode(ctx->entity);
  if (!q) return 0;
  char url[768];
  snprintf(url, sizeof url, "https://jobtalk.jp/companies?keyword=%s", q);
  free(q);
  jo_emit_anchors(ctx, sink, url, NULL, "Tenshoku-kaigi", "company-review",
                  "https://jobtalk.jp", ctx->entity, 15, "jobtalk");
  return 0;
}

static const source_def openwork_def = {
  .id = "OPENWORK", .collector = "osint",
  .name = "OpenWork Reviews", .name_ja = "OpenWork 企業口コミ",
  .update_interval_sec = 0, .run = run_openwork,
  .category = "commercial", .type = "scraped", .url = "https://www.openwork.jp/",
  .description = "Employee reviews, salary bands & turnover signals by company (entity pivot)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(openwork_def)

static const source_def lighthouse_def = {
  .id = "EN_LIGHTHOUSE", .collector = "osint",
  .name = "en Lighthouse Reviews", .name_ja = "エン ライトハウス 企業口コミ",
  .update_interval_sec = 0, .run = run_lighthouse,
  .category = "commercial", .type = "scraped", .url = "https://en-hyouban.com/",
  .description = "Second employee-review/salary corpus for cross-checking (entity pivot)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(lighthouse_def)

static const source_def jobtalk_def = {
  .id = "TENSHOKU_KAIGI", .collector = "osint",
  .name = "Tenshoku-kaigi Reviews", .name_ja = "転職会議 企業口コミ",
  .update_interval_sec = 0, .run = run_jobtalk,
  .category = "commercial", .type = "scraped", .url = "https://jobtalk.jp/",
  .description = "Third employee-review/salary corpus for cross-checking (entity pivot)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(jobtalk_def)
