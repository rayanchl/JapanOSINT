/* collectors/sources/reg_nz_companies.c
 * OSINT service — New Zealand Companies Office (Companies Register).
 * Entity pivot: company name or NZBN/company number.
 *
 * The public search UI at app.companiesoffice.govt.nz is a JS single-page app,
 * so the results HTML is not server-rendered. We first try the internal
 * disclosure JSON endpoint (companies/app/service/companies/search); if it is
 * reachable and parseable we emit one intel_item per matched company. If it is
 * not (JS-only / shape changed), we fall back to jo_emit_anchors over the
 * server-side search results page and emit only real anchors that link to a
 * company detail page. If neither yields anything → honest empty (return 0).
 * Nothing is synthesized. type="scraped". */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define NZ_BASE "https://app.companiesoffice.govt.nz"

/* Try the disclosure JSON search endpoint. Returns number emitted, or -1 if the
 * endpoint was not usable (so the caller can fall back to anchor scraping). */
static int nz_try_json_page(const source_ctx *ctx, intel_sink *sink,
                            const char *enc, int start);

/* Walk the result offset until a page yields nothing. */
static int nz_try_json(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  int total = 0;
  for (int start = 0; start < 20 * 20; start += 20) {
    int n = nz_try_json_page(ctx, sink, enc, start);
    if (n < 0) return total > 0 ? total : -1;   /* endpoint unusable */
    total += n;
    if (n < 20) break;                          /* short page = exhausted */
  }
  return total;
}

/* One page of the NZ search. The caller walks `start` until a page comes back
 * empty — reading start=0 only discarded every match past the 20th
 * (docs/SOURCE_EXHAUSTIVENESS.md). */
static int nz_try_json_page(const source_ctx *ctx, intel_sink *sink,
                            const char *enc, int start) {
  char url[768];
  snprintf(url, sizeof url,
    NZ_BASE "/companies/app/service/services/companies/search?q=%s&entityStatuses=&entityTypes=&start=%d&limit=20",
    enc, start);
  const char *hdrs[] = { "Accept: application/json",
                         "X-Requested-With: XMLHttpRequest", NULL };
  char *body = jo_get(ctx, url, hdrs, "nz_companies");
  if (!body) return -1;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return -1;

  /* Tolerate a few plausible container shapes. */
  cJSON *arr = cJSON_GetObjectItem(root, "results");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "items");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "companies");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return -1; }

  int emitted = 0;
  cJSON *c = NULL;
  cJSON_ArrayForEach(c, arr) {
    const char *name = jo_sv(c, "entityName");
    if (!name) name = jo_sv(c, "name");
    if (!name) name = jo_sv(c, "companyName");
    const char *num  = jo_sv(c, "companyNumber");
    if (!num) num = jo_sv(c, "entityNumber");
    if (!num) num = jo_sv(c, "number");
    const char *nzbn   = jo_sv(c, "nzbn");
    const char *status = jo_sv(c, "entityStatus");
    if (!status) status = jo_sv(c, "status");
    const char *type   = jo_sv(c, "entityType");
    const char *reg    = jo_sv(c, "registrationDate");
    if (!name && !num && !nzbn) continue;

    char link[900] = {0};
    if (num) snprintf(link, sizeof link,
      NZ_BASE "/companies/app/ui/pages/companies/%s", num);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "NZ Companies Office");
    if (num)    cJSON_AddStringToObject(props, "company_number", num);
    if (nzbn)   cJSON_AddStringToObject(props, "nzbn", nzbn);
    if (status) cJSON_AddStringToObject(props, "status", status);
    if (type)   cJSON_AddStringToObject(props, "entity_type", type);
    if (link[0]) cJSON_AddStringToObject(props, "href", link);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = num ? num : (nzbn ? nzbn : name);
    it.title           = name ? name : (num ? num : nzbn);
    it.summary         = status;
    it.published_at    = reg;
    it.link            = link[0] ? link : NULL;
    it.lang            = "en";
    it.record_type     = "nz-company";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"registry\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[nz_companies] json emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  char *enc = jo_urlencode(ctx->entity);
  if (!enc) return 0;

  int n = nz_try_json(ctx, sink, enc);
  if (n > 0) { free(enc); return 0; }

  /* Fallback: scrape the server-rendered search results page for anchors that
   * point at company detail pages (href contains /companies/). JS-only pages
   * simply yield 0. */
  char url[768];
  snprintf(url, sizeof url,
    NZ_BASE "/companies/app/ui/pages/companies/search?q=%s&mode=standard", enc);
  free(enc);
  jo_emit_anchors(ctx, sink, url, "/companies/", "NZ Companies Office",
                  "nz-company", NZ_BASE, ctx->entity, 20, "nz_companies");
  return 0;
}

static const source_def nz_companies_def = {
  .id = "NZ_COMPANIES", .collector = "osint",
  .name = "NZ Companies Office", .name_ja = "ニュージーランド会社登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "https://app.companiesoffice.govt.nz/companies/app/",
  .description = "New Zealand Companies Register — company name/NZBN pivot (JS UI; disclosure JSON with anchor-scrape fallback, honest-empty if JS-only)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(nz_companies_def)
