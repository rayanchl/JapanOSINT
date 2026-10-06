/* lib/seenset.c — see header.
 *
 * Hashed. It was a linear scan on the reasoning that "collector run sizes are
 * hundreds to low thousands of keys, where a hash table's setup costs more than
 * it saves" — true for most callers, and false for the ones that matter: the
 * HTML engine dedupes every link of a listing through this set, and
 * PYPI_SIMPLE_INDEX is ONE page of 824,355 links. A linear scan per add made
 * that page quadratic — over twelve CPU-minutes per run, measured 2026-10-06,
 * doing nothing but strcmp. The index is open addressing over the insertion
 * array, so `v` keeps its order and every caller keeps working unchanged. */
#include "seenset.h"
#include <stdlib.h>
#include <string.h>

static unsigned long long seen_hash(const char *k) {
  unsigned long long h = 1469598103934665603ULL;
  for (const unsigned char *p = (const unsigned char *)k; *p; p++)
    h = (h ^ *p) * 1099511628211ULL;
  return h;
}

/* (Re)build the index at `cap` slots (a power of two). 0 on failure, and the
 * set then falls back to the linear scan — slower, never wrong. */
static int seen_reindex(seen_set *s, int cap) {
  int *h = (int *)malloc((size_t)cap * sizeof *h);
  if (!h) return 0;
  for (int i = 0; i < cap; i++) h[i] = -1;
  for (int i = 0; i < s->n; i++) {
    unsigned long long x = seen_hash(s->v[i]) & (unsigned long long)(cap - 1);
    while (h[x] >= 0) x = (x + 1) & (unsigned long long)(cap - 1);
    h[x] = i;
  }
  free(s->h);
  s->h = h;
  s->hcap = cap;
  return 1;
}

int seen_has(const seen_set *s, const char *key) {
  if (!s || !key) return 0;
  if (s->h && s->hcap) {
    unsigned long long m = (unsigned long long)(s->hcap - 1);
    for (unsigned long long x = seen_hash(key) & m; s->h[x] >= 0; x = (x + 1) & m)
      if (strcmp(s->v[s->h[x]], key) == 0) return 1;
    return 0;
  }
  for (int i = 0; i < s->n; i++) if (strcmp(s->v[i], key) == 0) return 1;
  return 0;
}

int seen_add(seen_set *s, const char *key) {
  if (!s || !key) return 0;
  if (seen_has(s, key)) return 0;
  if (s->n == s->cap) {
    int nc = s->cap ? s->cap * 2 : 64;
    char **nv = (char **)realloc(s->v, (size_t)nc * sizeof *nv);
    if (!nv) return 1;               /* cannot record it; still a new key */
    s->v = nv; s->cap = nc;
  }
  s->v[s->n] = strdup(key);
  if (!s->v[s->n]) return 1;
  s->n++;
  /* Keep the index at most half full; it is rebuilt from `v` when it grows. */
  if (s->n * 2 > s->hcap) {
    int nc = s->hcap ? s->hcap * 2 : 128;
    while (s->n * 2 > nc) nc *= 2;
    if (!seen_reindex(s, nc)) { free(s->h); s->h = NULL; s->hcap = 0; }
  } else if (s->h) {
    unsigned long long m = (unsigned long long)(s->hcap - 1);
    unsigned long long x = seen_hash(key) & m;
    while (s->h[x] >= 0) x = (x + 1) & m;
    s->h[x] = s->n - 1;
  }
  return 1;
}

void seen_free(seen_set *s) {
  if (!s) return;
  for (int i = 0; i < s->n; i++) free(s->v[i]);
  free(s->v);
  free(s->h);
  s->v = NULL; s->n = s->cap = 0;
  s->h = NULL; s->hcap = 0;
}
