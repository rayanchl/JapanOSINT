/* collectors/osint/sources/global_icij.c
 * OSINT service — ICIJ Offshore Leaks (offshoreleaks.icij.org). On-demand entity
 * pivot (entity = person/company name) against the combined Panama/Paradise/
 * Pandora/Bahamas/Offshore leaks index. The public search UI is a JS SPA, but the
 * server also renders result-list HTML at /search?q=<entity>&e=Entities whose
 * result rows link to /nodes/<id> detail pages. We fetch that URL and
 * jo_emit_anchors on hrefs containing "/nodes/": each emitted item is a REAL
 * linked entity extracted from the live page (title = anchor text = entity name).
 * If the response is JS-only / has no /nodes/ links, we emit nothing (honest
 * empty). Nothing is fabricated. type="scraped". */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) { fprintf(stderr, "[ICIJ_OFFSHORE] no entity\n"); return 0; }

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://offshoreleaks.icij.org/search?q=%s&e=Entities", enc);
  free(enc);

  /* Server-rendered result rows link to /nodes/<id> detail pages. Real anchor
   * scrape; JS-only or empty pages yield 0 (honest empty). */
  jo_emit_anchors(ctx, sink, url, "/nodes/", "ICIJ Offshore Leaks",
                  "offshore-leak-entity", "https://offshoreleaks.icij.org",
                  NULL, 30, "ICIJ_OFFSHORE");
  return 0;
}

static const source_def icij_offshore_def = {
  .id = "ICIJ_OFFSHORE", .collector = "osint",
  .name = "ICIJ Offshore Leaks", .name_ja = "ICIJ オフショア・リークス",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "scraped",
  .url = "https://offshoreleaks.icij.org/",
  .description = "ICIJ Offshore Leaks — Panama/Paradise/Pandora/Bahamas entity search (scraped /nodes/ links)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(icij_offshore_def)
