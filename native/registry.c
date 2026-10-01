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

const source_def *registry_get(const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < g_n; i++)
    if (strcmp(g_srcs[i]->id, id) == 0) return g_srcs[i];
  return NULL;
}
