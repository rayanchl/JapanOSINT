#include "source.h"
#include <string.h>
#include <stdio.h>

/* Sources self-register via REGISTER_SOURCE constructors at load time.
 * Headroom well above the current source count so late-loading collectors
 * (alphabetically last, e.g. world_reg_*) can't be silently dropped past the
 * cap — that failure mode cost us the 8 regional registries once already.
 *
 * Do NOT read a source count out of this comment; it was stale by 5x within
 * two months of being written ("~551" when the real figure was 2,563), and
 * that stale number is one of six contradictory counts that ended up
 * enshrined across the tree. The authority is the binary:
 *
 *     ./bin/japanosint --list-sources | wc -l
 *
 * The cap has already been hit once in earnest: adding 1,747 verified sources
 * to 2,563 existing ones needs 4,310, and at 4,096 the constructor for every
 * source past the cap logged OVERFLOW and was dropped. Sized here for room to
 * roughly triple again. The array is `const source_def *`, so the cost is one
 * pointer per slot — 128 KB at 16384 on LP64, paid once in .bss.
 *
 * 2026-08-09: doubled 16384 -> 32768 (256 KB in .bss) ahead of the second bulk
 * verified-source batch. Raising the cap is deliberately cheap and is done
 * BEFORE the sources land, because the failure mode when it is not is silent
 * in the only place that matters: the build succeeds, the fleet looks healthy,
 * and the sources past the cap simply do not exist at runtime. */
#define MAX_SOURCES 32768
static const source_def *g_srcs[MAX_SOURCES];
static int g_n = 0;

/* Duplicate ids are a SILENT failure without this check: registry_get()
 * returns the first match, so the second definition is shadowed — it is
 * scheduled (scheduler_loop walks the array, not the lookup) but can never be
 * dispatched, --run'd, or resolved by the OSINT router, and both write to the
 * same source_id rows. With sources authored in parallel across many files
 * that collision is a matter of time, and "my collector is registered and
 * still does nothing" is a miserable thing to debug. Fail loud at startup. */
static int g_dupes;

void registry_add(const source_def *def) {
  if (!def) return;
  if (def->id) {
    for (int i = 0; i < g_n; i++) {
      if (strcmp(g_srcs[i]->id, def->id) == 0) {
        fprintf(stderr, "[registry] DUPLICATE id '%s' (collector '%s' vs '%s')"
                        " — second definition is unreachable\n",
                def->id, g_srcs[i]->collector ? g_srcs[i]->collector : "?",
                def->collector ? def->collector : "?");
        g_dupes++;
        break;
      }
    }
  }
  if (g_n < MAX_SOURCES) g_srcs[g_n++] = def;
  /* Loud on overflow so this never silently truncates again. */
  else fprintf(stderr, "[registry] OVERFLOW: dropped '%s' (cap %d)\n",
               def->id ? def->id : "?", MAX_SOURCES);
}

int registry_duplicate_count(void) { return g_dupes; }
int registry_count(void) { return g_n; }
const source_def **registry_all(void) { return g_srcs; }

/* Lookup is by bsearch over a sorted shadow of g_srcs, rebuilt lazily when
 * the registry has grown since it was last built. registry_all() keeps
 * registration order (the scheduler and the floors walk it), so the shadow is
 * a second array, not a reordering. A linear scan here cost the provenance
 * index 20 s on first use — one registry_get per docs/ row, 18,883 strcmp
 * each — and the OSINT dispatcher resolves ids on every search. Constructors
 * register before main() on one thread; later callers come from the worker
 * threads, hence the mutex around the rebuild. */
#include <pthread.h>
#include <stdlib.h>
static const source_def **g_sorted = NULL;
static int g_sorted_n = 0;
static pthread_mutex_t g_sorted_mu = PTHREAD_MUTEX_INITIALIZER;
static int def_cmp(const void *a, const void *b) {
  const source_def *const *x = a, *const *y = b;
  return strcmp((*x)->id, (*y)->id);
}
static int def_key_cmp(const void *k, const void *e) {
  return strcmp((const char *)k, (*(const source_def *const *)e)->id);
}
const source_def *registry_get(const char *id) {
  if (!id || g_n == 0) return NULL;
  pthread_mutex_lock(&g_sorted_mu);
  if (g_sorted_n != g_n) {
    const source_def **ns = realloc((void *)g_sorted, (size_t)g_n * sizeof *ns);
    if (ns) {
      memcpy(ns, g_srcs, (size_t)g_n * sizeof *ns);
      qsort(ns, (size_t)g_n, sizeof *ns, def_cmp);
      g_sorted = ns; g_sorted_n = g_n;
    }
  }
  const source_def **sorted = g_sorted; int n = g_sorted_n;
  pthread_mutex_unlock(&g_sorted_mu);
  if (!sorted) {                      /* allocation failed: the slow path still answers */
    for (int i = 0; i < g_n; i++)
      if (strcmp(g_srcs[i]->id, id) == 0) return g_srcs[i];
    return NULL;
  }
  /* duplicate ids: the FIRST registered wins, as before — bsearch may land on
   * either copy, so step back to the lowest registration index among equals */
  const source_def **hit = bsearch(id, sorted, (size_t)n, sizeof *sorted, def_key_cmp);
  if (!hit) return NULL;
  const source_def *best = *hit;
  if (g_dupes) {
    for (int i = 0; i < g_n; i++)
      if (strcmp(g_srcs[i]->id, id) == 0) return g_srcs[i];
  }
  return best;
}

#include "lib/hpengine.h"
int registry_is_entity_pivot(const source_def *d) {
  if (!d || d->update_interval_sec != 0) return 0;
  if (!d->collector || d->collector[0] == '_') return 0;
  if (d->engine && !strcmp(d->engine, "hpengine"))
    return hp_source_uses_entity(d->id);
  return 1;
}
