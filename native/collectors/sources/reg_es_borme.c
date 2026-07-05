/* collectors/sources/reg_es_borme.c
 * OSINT service — ES_BORME. On-demand entity pivot (entity = company / person /
 * administrator name) against Spain's BORME (Boletín Oficial del Registro
 * Mercantil), published as part of the BOE (Boletín Oficial del Estado) open
 * platform. BORME is the official gazette of the Spanish Commercial Registry:
 * incorporations, dissolutions, appointments, capital changes, etc.
 *
 * BORME is served as daily PDF sections, so there is no clean machine-readable
 * per-record API for free-text search. We use the BOE's real public BORME
 * search endpoint (https://www.boe.es/buscar/borme.php) which returns a
 * server-rendered results listing, and extract one intel_item per real result
 * anchor pointing at a /borme/ document. Nothing is synthesized.
 *
 * HONESTY: the BOE search UI is partly JS-driven and shaped for humans; when
 * the server-rendered HTML yields no /borme/ anchors for the query, run()
 * emits nothing (honest empty, return 0) rather than fabricating records. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define ES_BORME_CAP 40   /* cap the fan-out for a single query */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;

  char *q = jo_urlencode(e);   /* UTF-8 safe %-encode of the query */
  if (!q) return 0;

  /* Real BOE/BORME free-text search endpoint. The BORME-C ("otros actos" /
   * commercial acts) filter is where company/administrator name text appears;
   * `dato[0]` is the search term. Result page is server-rendered HTML with
   * anchors into /borme/dias/... documents. */
  char url[1024];
  snprintf(url, sizeof url,
    "https://www.boe.es/buscar/borme.php?campo%%5B0%%5D=BORME-C-A&dato%%5B0%%5D=%s",
    q);

  /* Real fetch + real anchor extraction: keep only anchors whose href points
   * at an actual BORME document and whose text/href mentions the entity, so
   * we don't emit site chrome. JS-only / empty result pages return 0. */
  int emitted = jo_emit_anchors(ctx, sink, url, "/borme/",
                                "BORME (ES)", "es-borme",
                                "https://www.boe.es", e,
                                ES_BORME_CAP, "es_borme");
  free(q);
  fprintf(stderr, "[es_borme] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def es_borme_def = {
  .id = "ES_BORME", .collector = "osint",
  .name = "Spain BORME (Commercial Registry Gazette)",
  .name_ja = "スペイン BORME 商業登記官報",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "scraped",
  .url = "internal://osint/reg_es_borme",
  .description = "Entity pivot across Spain's BORME (Boletin Oficial del Registro Mercantil) via the BOE open search; anchor scrape of real /borme/ documents, honest-empty when no result anchors",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(es_borme_def)
