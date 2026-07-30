/* collectors/sources/reg_be_kbo.c
 * OSINT service — BE_KBO. On-demand entity pivot (entity = company / trader
 * name) against Belgium's KBO/BCE (Crossroads Bank for Enterprises,
 * Kruispuntbank van Ondernemingen), the authoritative public enterprise
 * register run by the FPS Economy.
 *
 * run() actually FETCHES the KBO public phonetic-name search results page
 * (kbopub.economie.fgov.be) for ctx->entity and emits ONE intel_item per real
 * enterprise-detail <a> hit extracted by jo_emit_anchors (real anchor
 * extraction — nothing synthesized). Anchors are filtered to the detail-page
 * path (/kbopub/toonondernemingps.html?ondernemingsnummer=…) so navigation
 * chrome is dropped.
 *
 * HONESTY: the KBO public search is largely server-rendered HTML, but if a
 * given query yields a JS-only / anti-bot / empty page it simply returns 0
 * (honest empty) — expected and fine; we NEVER fabricate an enterprise record.
 * The search path is KBO's real public endpoint. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"


static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;

  char *q = jo_urlencode(e);   /* UTF-8 safe %-encode of the query */
  if (!q) return 0;

  /* KBO/BCE public phonetic name search results page (server-rendered HTML). */
  char url[1200];
  snprintf(url, sizeof url,
    "https://kbopub.economie.fgov.be/kbopub/zoeknaamfonetischform.html?searchWord=%s",
    q);
  free(q);

  /* Enterprise detail pages carry ?ondernemingsnummer=… — filter on that so we
   * only keep real result rows, not page chrome. */
  jo_emit_anchors(
      ctx, sink, url,
      "ondernemingsnummer",                 /* href_must */
      "KBO/BCE (BE)", "be-enterprise",
      "https://kbopub.economie.fgov.be",    /* base for root-relative hrefs */
      NULL,                                  /* accept any matching detail row */
      0, "be_kbo");   /* 0 = every matching row on the page */

  return 0;   /* honest empty is not an error */
}

static const source_def be_kbo_def = {
  .id = "BE_KBO", .collector = "osint",
  .name = "Belgium KBO/BCE Enterprise Register",
  .name_ja = "ベルギー KBO/BCE 企業登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_be_kbo",
  .description = "Belgium Crossroads Bank for Enterprises (KBO/BCE) public name search; anchor scrape of enterprise-detail hits, honest-empty on JS-only/anti-bot",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(be_kbo_def)
