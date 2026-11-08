/* collectors/sources/_emodnet_wfs.inc — page an EMODnet Human Activities WFS
 * layer to its end through lib/pagewalk.c (2026-11-07).
 *
 * WHY. Every mar_emodnet_*.c collector made ONE GetFeature request — some with
 * a `count=` well below the layer, none with `startIndex` — and filed no
 * notice. Measured 2026-11-07 (numberMatched, one request per layer):
 * wwshipwrecks 67,391 against count=5000, heritageshipwrecks 7,073 against
 * count=2000, portvessels 1,702,622 against count=5000. So those rows stored a
 * fixed first slice for ever and said nothing.
 *
 * ORDER. The GeoServer behind ows.emodnet-humanactivities.eu answers a paged
 * request in its PRIMARY-KEY order when no sortBy is given: measured on
 * wwshipwrecks, startIndex=0 → fids .1 .2 .3, startIndex=3 → .4 .5 .6,
 * startIndex=60000 → .60001 …, so offsets are honoured and disjoint. No
 * sortBy is declared ON PURPOSE: the obvious attribute (wreck_id) is a STRING
 * with ties — 67,387 distinct over 67,391 features — and a sort with ties can
 * reorder inside a tie between requests, which is exactly the skip this tree
 * calls its worst shape. The natural key order has no ties. (A bogus
 * `sortBy=frobnicate` answers an OWS ExceptionReport, so the server validates
 * the parameter — a sort is available should a layer ever need one.)
 *
 * The walk discloses its own stops: a page ceiling or a failed later page
 * files a collector-truncation-notice carrying numberMatched (pagewalk reads
 * it as the declared total).
 *
 * Included, never compiled standalone; the includer must have source.h,
 * cJSON and lib/feedlib.h. */
#ifndef JO_EMODNET_WFS_INC
#define JO_EMODNET_WFS_INC

#include "lib/pagewalk.h"
#include "lib/feedlib.h"

/* Emit every feature of one page; returns records emitted. */
typedef int (*emw_feats_fn)(const source_ctx *ctx, intel_sink *sink,
                            cJSON *feats, void *ud);

typedef struct { emw_feats_fn fn; void *ud; } emw_ctx;

static inline cJSON *emw_fetch(const source_ctx *ctx, const char *url, void *ud) {
  (void)ud;
  return feed_get_json(ctx->http, url, 120000);
}

static inline int emw_page(const source_ctx *ctx, intel_sink *sink,
                           const char *id, cJSON *doc, void *ud, int *seen) {
  (void)id;
  const emw_ctx *c = (const emw_ctx *)ud;
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  *seen = cJSON_IsArray(feats) ? cJSON_GetArraySize(feats) : 0;
  if (!cJSON_IsArray(feats)) return 0;
  int n = c->fn(ctx, sink, feats, c->ud);
  return n < 0 ? 0 : n;
}

/* Walk `url` (which must carry `count=` and `startIndex=0`) to its end, or to
 * `page_max` pages (0 = the tree default; $JO_PAGE_MAX still brakes it).
 * Returns records emitted, or -1 when the FIRST page failed. */
static inline int emw_walk(const source_ctx *ctx, intel_sink *sink,
                           const char *sid, const char *url, int page_max,
                           emw_feats_fn fn, void *ud) {
  emw_ctx c = { fn, ud };
  return pw_walk_max(ctx, sink, sid, url, emw_fetch, emw_page, &c, page_max);
}

/* The same, read over a CYCLE (pw_walk_max_resume): for a layer too large to
 * read in one run. Natural key order is ascending, so new rows land at the
 * TAIL where the walk has not been yet. On an entity pivot resume_in/out are
 * NULL and this is an ordinary walk. */
static inline int emw_walk_cycle(const source_ctx *ctx, intel_sink *sink,
                                 const char *sid, const char *url, int page_max,
                                 emw_feats_fn fn, void *ud) {
  emw_ctx c = { fn, ud };
  char next[2048];
  next[0] = 0;
  int n = pw_walk_max_resume(ctx, sink, sid, url, emw_fetch, emw_page, &c,
                             page_max, ctx->resume_in, next, sizeof next);
  if (ctx->resume_out) ctx->resume_out(ctx, next[0] ? next : NULL);
  return n;
}

#endif
