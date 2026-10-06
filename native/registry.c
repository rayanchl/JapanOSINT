#include "source.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
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
 * still does nothing" is a miserable thing to debug. Fail loud at startup.
 *
 * THE CHECK USED TO BE THE BOOT COST. It was a linear scan inside
 * registry_add(), i.e. O(n^2) across the constructors: at 18,643 sources that
 * is ~174 M strcmp() calls before main() runs. registry_get() was the same
 * linear scan, and db_seed_sources() calls it (through src_meta_get) once per
 * source, so every boot — `--list-sources` included — paid for it twice.
 *
 * Now registry_add() only appends, and an open-addressing hash of indices is
 * built ONCE, after the constructors: by registry_freeze() from main(), or
 * lazily by the first lookup. Building it is where duplicates are detected and
 * reported, with the same message as before (Makefile's registry-floor and
 * tests/contract/accept_new_sources.sh grep for "DUPLICATE id"), and the FIRST
 * definition keeps the slot so registry_get() answers exactly what it did.
 *
 * Immutability: every registry_add() caller is an __attribute__((constructor))
 * (REGISTER_SOURCE, and hp_register() via HP_REGISTER_TABLE), so the array is
 * complete before main() and never changes after. A late registry_add() is
 * still handled — it is inserted into the built index — but it is not
 * thread-safe and nothing in the tree does it. */
static int g_dupes;
static int     *g_slot;          /* index+1 into g_srcs, 0 = empty          */
static unsigned g_mask;          /* capacity-1 (capacity is a power of two) */
static int      g_built;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static uint64_t id_hash(const char *s) {          /* FNV-1a 64 */
  uint64_t h = 1469598103934665603ULL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h;
}

/* Insert g_srcs[i]; report and skip when its id is already present. */
static void index_insert(int i) {
  const source_def *d = g_srcs[i];
  if (!d || !d->id || !g_slot) return;
  for (unsigned p = (unsigned)id_hash(d->id) & g_mask;; p = (p + 1) & g_mask) {
    int k = g_slot[p];
    if (!k) { g_slot[p] = i + 1; return; }
    if (strcmp(g_srcs[k - 1]->id, d->id) == 0) {
      fprintf(stderr, "[registry] DUPLICATE id '%s' (collector '%s' vs '%s')"
                      " — second definition is unreachable\n",
              d->id, g_srcs[k - 1]->collector ? g_srcs[k - 1]->collector : "?",
              d->collector ? d->collector : "?");
      g_dupes++;
      return;
    }
  }
}

static void index_build(void) {
  /* Sized for MAX_SOURCES, not for g_n, so a late registry_add() can never
   * push the load factor past 1/2 and no rehash path is needed. 256 KB. */
  unsigned cap = 1;
  while (cap < 2u * (unsigned)MAX_SOURCES) cap <<= 1;
  g_slot = calloc(cap, sizeof *g_slot);
  if (!g_slot) {
    fprintf(stderr, "[registry] out of memory for the id index; lookups fall "
                    "back to a linear scan\n");
    return;
  }
  g_mask = cap - 1;
  for (int i = 0; i < g_n; i++) index_insert(i);
  g_built = 1;
}

void registry_freeze(void) { pthread_once(&g_once, index_build); }

void registry_add(const source_def *def) {
  if (!def) return;
  if (g_n < MAX_SOURCES) {
    g_srcs[g_n++] = def;
    if (g_built) index_insert(g_n - 1);
  }
  /* Loud on overflow so this never silently truncates again. */
  else fprintf(stderr, "[registry] OVERFLOW: dropped '%s' (cap %d)\n",
               def->id ? def->id : "?", MAX_SOURCES);
}

int registry_duplicate_count(void) { registry_freeze(); return g_dupes; }
int registry_count(void) { return g_n; }
const source_def **registry_all(void) { return g_srcs; }

int registry_index(const char *id) {
  if (!id) return -1;
  registry_freeze();
  if (!g_slot) {                                   /* OOM fallback */
    for (int i = 0; i < g_n; i++)
      if (g_srcs[i]->id && strcmp(g_srcs[i]->id, id) == 0) return i;
    return -1;
  }
  for (unsigned p = (unsigned)id_hash(id) & g_mask;; p = (p + 1) & g_mask) {
    int k = g_slot[p];
    if (!k) return -1;
    if (strcmp(g_srcs[k - 1]->id, id) == 0) return k - 1;
  }
}

const source_def *registry_get(const char *id) {
  int i = registry_index(id);
  return i >= 0 ? g_srcs[i] : NULL;
}
