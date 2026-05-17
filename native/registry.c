#include "source.h"
#include <string.h>

/* Sources self-register via REGISTER_SOURCE constructors at load time. */
#define MAX_SOURCES 512
static const source_def *g_srcs[MAX_SOURCES];
static int g_n = 0;

void registry_add(const source_def *def) {
  if (def && g_n < MAX_SOURCES) g_srcs[g_n++] = def;
}
int registry_count(void) { return g_n; }
const source_def **registry_all(void) { return g_srcs; }

const source_def *registry_get(const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < g_n; i++)
    if (strcmp(g_srcs[i]->id, id) == 0) return g_srcs[i];
  return NULL;
}
