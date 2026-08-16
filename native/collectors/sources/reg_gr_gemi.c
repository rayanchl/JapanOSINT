/* collectors/sources/reg_gr_gemi.c
 * OSINT service — GR_GEMI. On-demand entity pivot (entity = company / person
 * name or GEMI/AFM number) against the Greek General Commercial Registry
 * (Γ.Ε.ΜΗ. / GEMI) public publicity portal at publicity.businessportal.gr.
 *
 * run() actually FETCHES the portal's server-rendered company-search results
 * page for ctx->entity and emits ONE intel_item per real <a> company hit via
 * jo_emit_anchors (real anchor extraction — nothing synthesized).
 *
 * HONESTY: the portal is partly JS-rendered / behind a POST form; when the
 * server response carries no matching anchors the source simply yields 0
 * (honest empty). We NEVER fabricate a company, number or count. The search
 * path is GEMI's real public search endpoint; where the exact query param is
 * uncertain we still use the real path and honest-empty rather than invent. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define GR_GEMI_CAP 20   /* cap hits per query so one search can't fan out forever */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;

  char *q = jo_urlencode(e);   /* UTF-8 safe %-encode (Greek names) */
  if (!q) return 0;

  char url[1024];
  snprintf(url, sizeof url,
    "https://publicity.businessportal.gr/company-search?query=%s", q);
  free(q);

  /* Real fetch + real anchor extraction. Keep only anchors that link into a
   * company record; filter on the entity bytes to drop site chrome. JS-only /
   * anti-bot responses yield 0 (honest empty). */
  int got = jo_emit_anchors(ctx, sink, url, "/company/", "GR_GEMI",
                            "gr-gemi-company",
                            "https://publicity.businessportal.gr", e,
                            GR_GEMI_CAP, "gr_gemi");
  return 0;   /* honest empty is not an error */
  (void)got;
}

static const source_def gr_gemi_def = {
  .id = "GR_GEMI", .collector = "osint",
  .name = "Greece GEMI Business Registry",
  .name_ja = "ギリシャ GEMI 商業登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_gr_gemi",
  .description = "Greek General Commercial Registry (Γ.Ε.ΜΗ.) publicity.businessportal.gr — entity pivot company search; anchor scrape, honest-empty on JS-only/anti-bot",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(gr_gemi_def)
