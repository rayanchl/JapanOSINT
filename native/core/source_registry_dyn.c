/* core/source_registry_dyn.c — public src_meta accessors that merge the
 * curated generated table (source_registry.gen.c) with metadata synthesized
 * from the runtime source_def registry.
 *
 * The 420 hand-curated map collectors live in the generated table and win
 * unchanged. Any registered source NOT in that table (the OSINT-SaaS services
 * and a few map collectors) gets a src_meta synthesized from the metadata it
 * carries inline on its `source_def` (see native/source.h). This gives every
 * one of the ~476 registered sources full metadata + identical status/level,
 * and lets new sources self-describe without touching the generated table. */
#include "source_registry.h"
#include "../source.h"

/* generated-table accessors (source_registry.gen.c) */
const src_meta *gen_meta_get(const char *id);
const src_meta *gen_meta_at(int i);
int            gen_meta_count(void);

/* Synthesize a src_meta from a registered source_def. Returned by pointer to
 * one scratch row per thread; no caller retains two synthesized pointers at
 * once. All string fields alias the source_def's static storage (const literals
 * that live for the program's lifetime) — no copying, so a per-thread row is
 * sufficient and costs nothing.
 *
 * This used to be a single process-wide static, justified by "mongoose is
 * single-threaded". That premise is false: anomaly-triage (60 s) and
 * collector-repair (120 s) both call src_meta_get from the SCHEDULER thread, so
 * a collector tick could overwrite the row mid-request and /api/status would
 * serve one source's name/type/url under another's id. */
static __thread src_meta g_syn;
static const src_meta *synth_from_def(const source_def *d) {
  if (!d) return NULL;
  g_syn.id          = d->id;
  g_syn.name        = d->name;
  g_syn.name_ja     = d->name_ja;
  g_syn.category    = d->category ? d->category : "investigation";
  g_syn.type        = d->type ? d->type : "api";
  g_syn.url         = d->url;
  g_syn.description = d->description;
  g_syn.license     = d->license;
  g_syn.layer       = d->layer;     /* NULL for services → excluded from /api/layers */
  g_syn.free        = d->free_tier;
  g_syn.update_interval = d->update_interval_sec > 0 ? d->update_interval_sec : -1;
  return &g_syn;
}

const src_meta *src_meta_get(const char *id) {
  const src_meta *m = gen_meta_get(id);   /* curated table wins */
  if (m) return m;
  return synth_from_def(registry_get(id));
}

/* Enumeration stays over the curated table only. Its sole consumers iterate
 * to build /api/layers (miscapi_list_layers / miscapi_layer_geojson), and
 * synthesized service rows are layerless (skipped there anyway), so excluding
 * them from enumeration is both correct and the lowest-risk choice. */
const src_meta *src_meta_at(int i) { return gen_meta_at(i); }
int src_meta_count(void) { return gen_meta_count(); }
