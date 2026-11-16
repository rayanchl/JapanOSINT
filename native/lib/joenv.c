/* lib/joenv.c — jo_getenv() and the per-thread credential resolver (see
 * source.h). Thread-local because the dispatcher runs each pivot on its own
 * thread and a resolver belongs to exactly one run: a workspace's key must
 * never be visible to a run on another thread. No database code here — the
 * resolver itself lives in core/osint_dispatch.c. */
#include "../source.h"

#include <stdlib.h>

static __thread jo_env_resolver_fn g_fn;
static __thread void *g_ctx;

void jo_env_resolver_set(jo_env_resolver_fn fn, void *ctx) {
  g_fn = fn;
  g_ctx = fn ? ctx : NULL;
}

const char *jo_getenv(const char *name) {
  if (!name || !*name) return NULL;
  if (g_fn) {
    const char *v = g_fn(g_ctx, name);
    if (v) return v;
  }
  return getenv(name);
}
