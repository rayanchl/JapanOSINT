/* core/source_registry_dyn.c — the public src_meta accessors.
 *
 * THE LIVE REGISTRY IS THE BASE; source_registry.gen.c IS AN OVERLAY.
 *
 * Every field src_meta carries also exists inline on `source_def`
 * (native/source.h), so a source describes itself: declare
 * .category/.type/.url/.description/.license/.layer/.free_tier next to .run
 * and it appears in /api/status, /api/intel/sources and — if it declares a
 * .layer — /api/layers, with no generated file to hand-edit. That is the whole
 * point of item 6 in the audit's unification list.
 *
 * What the generated table still contributes is the 321 Node-era rows whose
 * rich Japanese names, descriptions and canonical URLs were never transcribed
 * back onto their defs. It is layered ON TOP, PER FIELD: a curated string wins
 * where it has one, the def fills every gap. The table used to win WHOLESALE,
 * so a curated row with a NULL field erased whatever the def declared there —
 * silently, and in exactly the direction that makes inline metadata useless.
 * No field differs today (checked: for all 320 ids present in both, every
 * curated string is either equal to the def's or the def has none), so this is
 * a no-op on the current tree by construction; it is what keeps the next
 * `.layer = "…"` someone writes from disappearing into a sparse curated row.
 *
 * Two fields do NOT come from the overlay:
 *   - update_interval: the source_def's update_interval_sec is what the
 *     scheduler actually runs on. 12 curated rows disagreed with it (cam-* said
 *     21600 s against a real 3600 s, mlit-dam said 600 s against 604800 s), so
 *     /api/status was publishing a poll schedule the engine does not keep.
 *     The def wins whenever there is a def.
 *   - free: 0 and "unset" are indistinguishable in an int, so the curated row
 *     stays authoritative when it exists; otherwise .free_tier. */
#include "source_registry.h"
#include "../source.h"

/* generated-table accessors (source_registry.gen.c) */
const src_meta *gen_meta_get(const char *id);
const src_meta *gen_meta_at(int i);
int            gen_meta_count(void);

/* Build a src_meta from a registered source_def, optionally overlaid with a
 * curated row. Returned by pointer to one scratch row per thread; no caller
 * retains two resolved pointers at once. All string fields alias static
 * storage (const literals in the defs and in the generated table, both alive
 * for the program's lifetime) — no copying, so a per-thread row is sufficient
 * and costs nothing.
 *
 * This used to be a single process-wide static, justified by "mongoose is
 * single-threaded". That premise is false: anomaly-triage (60 s) and
 * collector-repair (120 s) both call src_meta_get from the SCHEDULER thread, so
 * a collector tick could overwrite the row mid-request and /api/status would
 * serve one source's name/type/url under another's id. */
static __thread src_meta g_syn;
#define PICK(cur, def) (((cur) && (cur)[0]) ? (cur) : (def))
static const src_meta *resolve(const source_def *d, const src_meta *cur) {
  if (!d) return cur;                 /* curated-only row → the table verbatim */
  g_syn.id          = d->id;
  g_syn.name        = PICK(cur ? cur->name        : NULL, d->name);
  g_syn.name_ja     = PICK(cur ? cur->name_ja     : NULL, d->name_ja);
  g_syn.category    = PICK(cur ? cur->category    : NULL,
                           d->category ? d->category : "investigation");
  g_syn.type        = PICK(cur ? cur->type        : NULL,
                           d->type ? d->type : "api");
  g_syn.url         = PICK(cur ? cur->url         : NULL, d->url);
  g_syn.description = PICK(cur ? cur->description : NULL, d->description);
  g_syn.license     = PICK(cur ? cur->license     : NULL, d->license);
  /* NULL layer → excluded from /api/layers, which is what an entity-pivot
   * service wants. A curated row may name the layer for a def that doesn't. */
  g_syn.layer       = PICK(cur ? cur->layer       : NULL, d->layer);
  g_syn.free        = cur ? cur->free : d->free_tier;
  g_syn.update_interval =
    d->update_interval_sec > 0 ? d->update_interval_sec : -1;
  return &g_syn;
}
#undef PICK

const src_meta *src_meta_get(const char *id) {
  const source_def *d = registry_get(id);
  const src_meta *cur = gen_meta_get(id);
  if (!d && !cur) return NULL;
  return resolve(d, cur);
}

/* Enumeration covers the curated table AND every registered source that is not
 * in it.
 *
 * It used to be curated-only, justified by "synthesized rows are layerless, so
 * they'd be skipped anyway". That premise stopped being true: 16 camera
 * collectors (and counting) declare `.layer` inline on their source_def and are
 * synthesized, so the layer they declare was invisible to /api/layers. The
 * visible symptom was the Layers tab reporting 8 camera sources where the
 * Sources tab reported 18 — and, structurally, that a source added since the
 * Node port could never join a map layer without hand-editing the generated
 * table.
 *
 * Enumeration ORDER is unchanged — curated rows in table order, then the
 * registered sources absent from the table — because it is the order
 * /api/layers and /api/intel/sources emit in, and a client sees it. Only the
 * RESOLUTION changed: each curated index is now resolved through resolve(), so
 * an enumerated row and a src_meta_get() of the same id can no longer differ.
 *
 * The extra index is built once and never mutated. registry_add() runs from
 * __attribute__((constructor)) before main(), so the registry is complete and
 * immutable by the time any request or scheduler tick can call this. */
#include <pthread.h>
#include <stdlib.h>

static const source_def **g_extra;
static int g_extra_n;
static pthread_once_t g_extra_once = PTHREAD_ONCE_INIT;

static void build_extra(void) {
  const source_def **all = registry_all();
  int n = registry_count();
  if (n <= 0) return;
  g_extra = (const source_def **)calloc((size_t)n, sizeof *g_extra);
  if (!g_extra) return;
  int k = 0;
  for (int i = 0; i < n; i++)
    if (all[i] && all[i]->id && !gen_meta_get(all[i]->id)) g_extra[k++] = all[i];
  g_extra_n = k;
}

const src_meta *src_meta_at(int i) {
  int g = gen_meta_count();
  if (i < 0) return NULL;
  if (i < g) {
    const src_meta *cur = gen_meta_at(i);
    return cur ? resolve(registry_get(cur->id), cur) : NULL;
  }
  pthread_once(&g_extra_once, build_extra);
  int j = i - g;
  if (!g_extra || j >= g_extra_n) return NULL;
  return resolve(g_extra[j], NULL);
}

int src_meta_count(void) {
  pthread_once(&g_extra_once, build_extra);
  return gen_meta_count() + g_extra_n;
}
