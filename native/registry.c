#include "source.h"
#include <string.h>
#include <stdio.h>

/* Sources self-register via REGISTER_SOURCE constructors at load time.
 * Headroom well above the current source count (~551) so late-loading
 * collectors (alphabetically last, e.g. world_reg_*) can't be silently
 * dropped past the cap — that failure mode cost us the 8 regional registries. */
#define MAX_SOURCES 1024
static const source_def *g_srcs[MAX_SOURCES];
static int g_n = 0;

void registry_add(const source_def *def) {
  if (def && g_n < MAX_SOURCES) g_srcs[g_n++] = def;
  /* Loud on overflow so this never silently truncates again. */
  else if (def) fprintf(stderr, "[registry] OVERFLOW: dropped '%s' (cap %d)\n",
                        def->id ? def->id : "?", MAX_SOURCES);
}
int registry_count(void) { return g_n; }
const source_def **registry_all(void) { return g_srcs; }

const source_def *registry_get(const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < g_n; i++)
    if (strcmp(g_srcs[i]->id, id) == 0) return g_srcs[i];
  return NULL;
}
