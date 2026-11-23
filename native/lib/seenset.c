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

/* ---- hash-only set (see seenset.h) --------------------------------------- */

static unsigned long long hset_hash(const char *s) {
  unsigned long long h = 1469598103934665603ULL;      /* FNV-1a 64 offset */
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h ? h : 1ULL;        /* 0 is the empty-slot marker, never a key */
}

static void hset_put(unsigned long long *tab, size_t cap, unsigned long long h) {
  size_t i = (size_t)(h & (cap - 1));
  while (tab[i]) {
    if (tab[i] == h) return;
    i = (i + 1) & (cap - 1);
  }
  tab[i] = h;
}

int seen_hset_add(seen_hset *s, const char *key) {
  if (!s || !key) return 0;
  unsigned long long h = hset_hash(key);
  const size_t ceiling = s->max_slots ? s->max_slots : SEEN_HASH_MAX_DEFAULT;
  if (s->tab && s->n * 10 >= s->cap * 7) {
    size_t ncap = s->cap * 2;
    if (ncap > ceiling) { s->inexact = 1; return 0; }
    unsigned long long *nt = (unsigned long long *)calloc(ncap, sizeof *nt);
    if (!nt) { s->inexact = 1; return 0; }
    for (size_t i = 0; i < s->cap; i++)
      if (s->tab[i]) hset_put(nt, ncap, s->tab[i]);
    free(s->tab); s->tab = nt; s->cap = ncap;
  } else if (!s->tab) {
    size_t icap = SEEN_HASH_INIT;
    if (icap > ceiling) icap = ceiling;           /* a tiny ceiling: start there */
    s->tab = (unsigned long long *)calloc(icap, sizeof *s->tab);
    if (!s->tab) { s->inexact = 1; return 0; }
    s->cap = icap;
  }
  size_t i = (size_t)(h & (s->cap - 1));
  while (s->tab[i]) {
    if (s->tab[i] == h) return 0;                 /* already recorded */
    i = (i + 1) & (s->cap - 1);
  }
  s->tab[i] = h;
  s->n++;
  return 1;
}

void seen_hset_free(seen_hset *s) {
  if (!s) return;
  free(s->tab);
  s->tab = NULL; s->cap = s->n = 0; s->inexact = 0;
}
