/* core/layertab.c — see layertab.h. The table itself lives in
 * core/layers.def (X-macro); this file is only the loader and the resolver.
 *
 * The resolution map is built ONCE, lazily, under pthread_once. That is safe
 * for the same reason source_registry_dyn.c's extra-index is: every
 * REGISTER_SOURCE / HP_REGISTER_TABLE runs from __attribute__((constructor))
 * before main(), so by the time any HTTP request or scheduler tick can reach
 * us the registry is complete and immutable. All strings stored here alias
 * static storage (source_def literals / the generated overlay / this file's
 * own table), so no copying and no lifetime question. */
#include "layertab.h"
#include "source_registry.h"
#include "statusapi.h"          /* statusapi_strip_has — see step 2 in the .h */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── the curated table ──────────────────────────────────────────────────── */

#define LAYER(ID, NAME, CAT, DT, MOD, MATCH) { ID, NAME, CAT, DT, MOD, MATCH },
static const layer_row TAB[] = {
#include "layers.def"
};
#undef LAYER
enum { NTAB = (int)(sizeof TAB / sizeof *TAB) };

int layertab_count(void) { return NTAB; }
const layer_row *layertab_at(int i) {
  return (i >= 0 && i < NTAB) ? &TAB[i] : NULL;
}
const layer_row *layertab_get(const char *layer_id) {
  if (!layer_id) return NULL;
  for (int i = 0; i < NTAB; i++)
    if (strcmp(TAB[i].id, layer_id) == 0) return &TAB[i];
  return NULL;
}

/* ── match terms ────────────────────────────────────────────────────────── */

/* One `key:value` term against one source. `m` is the resolved src_meta —
 * NOTE it is source_registry_dyn's per-thread scratch row, so only its
 * STRING FIELDS (static storage) may be retained, never the struct pointer. */
static int term_matches(const char *t, size_t tlen, const src_meta *m) {
  const char *colon = memchr(t, ':', tlen);
  if (!colon) return 0;
  size_t klen = (size_t)(colon - t);
  const char *v = colon + 1;
  size_t vlen = tlen - klen - 1;
  if (vlen == 0) return 0;

  const char *field = NULL;
  int prefix = 0;
  if (klen == 2 && !memcmp(t, "id", 2))         field = m->id;
  else if (klen == 3 && !memcmp(t, "idp", 3)) { field = m->id; prefix = 1; }
  else if (klen == 5 && !memcmp(t, "layer", 5)) field = m->layer;
  else if (klen == 3 && !memcmp(t, "cat", 3))   field = m->category;
  else return 0;                     /* unknown key — never a wildcard match */

  if (!field || !field[0]) return 0;
  if (prefix) return strncmp(field, v, vlen) == 0;
  return strlen(field) == vlen && memcmp(field, v, vlen) == 0;
}

static int row_matches(const layer_row *r, const src_meta *m) {
  const char *p = r->match;
  if (!p) return 0;
  while (*p) {
    const char *bar = strchr(p, '|');
    size_t tlen = bar ? (size_t)(bar - p) : strlen(p);
    if (tlen && term_matches(p, tlen, m)) return 1;
    if (!bar) break;
    p = bar + 1;
  }
  return 0;
}

/* ── the resolution map ─────────────────────────────────────────────────── */

typedef struct { const char *src; const char *layer; } assign_t;

static assign_t *g_map;          /* only assigned sources are stored        */
static int g_map_n;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static void build_map(void) {
  int n = src_meta_count();
  if (n <= 0) return;
  g_map = calloc((size_t)n, sizeof *g_map);
  if (!g_map) return;
  int k = 0;
  for (int i = 0; i < n; i++) {
    const src_meta *m = src_meta_at(i);
    if (!m || !m->id) continue;
    const char *layer = NULL;
    for (int r = 0; r < NTAB; r++)
      if (row_matches(&TAB[r], m)) { layer = TAB[r].id; break; }
    if (!layer && m->layer && m->layer[0]) {
      /* Declared-layer fallback — but a declared id that statusapi still
       * strips would name a layer no listing ever shows, and rows behind an
       * invisible layer are exactly the silent-nothing rules 2/3 forbid.
       * Skip it: the source stays unassigned and its geocoded rows surface
       * through the generated catch-all instead. */
      if (!statusapi_strip_has(m->layer)) layer = m->layer;
    }
    if (layer) { g_map[k].src = m->id; g_map[k].layer = layer; k++; }
  }
  g_map_n = k;
}

const char *layertab_layer_for_source(const char *source_id) {
  if (!source_id) return NULL;
  pthread_once(&g_once, build_map);
  for (int i = 0; i < g_map_n; i++)
    if (strcmp(g_map[i].src, source_id) == 0) return g_map[i].layer;
  return NULL;
}

/* ── SQL IN-list fragments ──────────────────────────────────────────────── */

/* Append `id` to a growing "'a','b'" buffer, doubling any single quote.
 * Registry ids are [A-Za-z0-9_-] today, but escaping costs nothing and an id
 * with a quote must never be able to break the statement it is spliced into. */
static int frag_append(char **buf, size_t *len, size_t *cap, const char *id,
                       int first) {
  size_t need = strlen(id) * 2 + 4;
  if (*len + need + 1 > *cap) {
    size_t nc = *cap ? *cap * 2 : 4096;
    while (nc < *len + need + 1) nc *= 2;
    char *nb = realloc(*buf, nc);
    if (!nb) return -1;
    *buf = nb; *cap = nc;
  }
  char *w = *buf + *len;
  if (!first) *w++ = ',';
  *w++ = '\'';
  for (const char *p = id; *p; p++) {
    if (*p == '\'') *w++ = '\'';
    *w++ = *p;
  }
  *w++ = '\'';
  *w = 0;
  *len = (size_t)(w - *buf);
  return 0;
}

char *layertab_members_in(const char *layer_id, int *count) {
  if (count) *count = 0;
  if (!layer_id) return NULL;
  pthread_once(&g_once, build_map);
  char *buf = NULL; size_t len = 0, cap = 0; int n = 0;
  for (int i = 0; i < g_map_n; i++) {
    if (strcmp(g_map[i].layer, layer_id) != 0) continue;
    if (frag_append(&buf, &len, &cap, g_map[i].src, n == 0) != 0) {
      free(buf); return NULL;
    }
    n++;
  }
  if (count) *count = n;
  return buf;                                   /* NULL when no member */
}

char *layertab_assigned_in(int *count) {
  if (count) *count = 0;
  pthread_once(&g_once, build_map);
  char *buf = NULL; size_t len = 0, cap = 0; int n = 0;
  for (int i = 0; i < g_map_n; i++) {
    if (frag_append(&buf, &len, &cap, g_map[i].src, n == 0) != 0) {
      free(buf); return NULL;
    }
    n++;
  }
  if (count) *count = n;
  return buf;
}

void layertab_rt_slug(const char *record_type, char *out, size_t cap) {
  size_t j = 0; int dash = 1;                    /* no leading '-' */
  if (!cap) return;
  for (const char *p = record_type ? record_type : ""; *p && j + 1 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out[j++] = (char)c; dash = 0;
    } else if (!dash && j + 1 < cap) {
      out[j++] = '-'; dash = 1;
    }
  }
  while (j > 0 && out[j - 1] == '-') j--;        /* no trailing '-' */
  out[j] = 0;
}
