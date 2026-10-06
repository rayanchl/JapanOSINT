/* lib/keyqual.c — see keyqual.h for the rule and why it exists. */
#include "keyqual.h"
#include "feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KQ_SEP '\x1f'   /* cannot occur in a key a collector would build */

static int push(char ***v, int *n, int *cap, char *s) {
  if (!s) return -1;
  if (*n == *cap) {
    int nc = *cap ? *cap * 2 : 256;
    char **nv = realloc(*v, (size_t)nc * sizeof *nv);
    if (!nv) { free(s); return -1; }
    *v = nv; *cap = nc;
  }
  (*v)[(*n)++] = s;
  return 0;
}

static char *pair_of(const char *base, const char *disc) {
  size_t lb = strlen(base), ld = strlen(disc);
  char *s = malloc(lb + ld + 2);
  if (!s) return NULL;
  memcpy(s, base, lb);
  s[lb] = KQ_SEP;
  memcpy(s + lb + 1, disc, ld + 1);
  return s;
}

int keyqual_add(keyqual *q, const char *base, const char *disc) {
  if (!q || !base) return 0;
  q->sealed = 0;
  int rc = push(&q->base, &q->nb, &q->cb, strdup(base));
  if (rc == 0 && disc && *disc)
    rc = push(&q->pair, &q->np, &q->cp, pair_of(base, disc));
  if (rc) q->oom = 1;
  return rc;
}

static int cmp_str(const void *a, const void *b) {
  return strcmp(*(char *const *)a, *(char *const *)b);
}

void keyqual_seal(keyqual *q) {
  if (!q || q->sealed) return;
  if (q->nb > 1) qsort(q->base, (size_t)q->nb, sizeof *q->base, cmp_str);
  if (q->np > 1) qsort(q->pair, (size_t)q->np, sizeof *q->pair, cmp_str);
  q->sealed = 1;
}

/* occurrences of `s` in the sorted v[0..n) */
static int occurrences(char **v, int n, const char *s) {
  int lo = 0, hi = n;                       /* first index with v[i] >= s */
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (strcmp(v[mid], s) < 0) lo = mid + 1; else hi = mid;
  }
  int c = 0;
  while (lo + c < n && strcmp(v[lo + c], s) == 0) c++;
  return c;
}

int keyqual_count(keyqual *q, const char *base) {
  if (!q || !base) return 0;
  keyqual_seal(q);
  return occurrences(q->base, q->nb, base);
}

static int pair_count(keyqual *q, const char *base, const char *disc) {
  char *p = pair_of(base, disc);
  if (!p) return 0;                         /* treat as not unique: hash */
  int c = occurrences(q->pair, q->np, p);
  free(p);
  return c;
}

/* "<base>|<suffix>" when it fits; otherwise a prefix of base plus a hash of
 * everything that identifies the record, so a cut never merges two records. */
static const char *compose(char *buf, size_t cap, const char *base,
                           const char *suffix, const char *disc,
                           const char *content) {
  int w = snprintf(buf, cap, "%s|%s", base, suffix);
  if (w >= 0 && (size_t)w < cap) return buf;
  char h[21];
  const char *parts[3] = { base, disc ? disc : "", content ? content : "" };
  feed_hash_key(h, parts, 3);
  int keep = (int)cap - 22;                 /* '|' + 20 hex + NUL */
  if (keep < 0) keep = 0;
  snprintf(buf, cap, "%.*s|%s", keep, base, h);
  return buf;
}

const char *keyqual_uid(keyqual *q, const char *base, const char *disc,
                        const char *content, char *buf, size_t cap) {
  if (!base) return NULL;
  if (!q || !buf || cap == 0) return base;
  if (keyqual_count(q, base) <= 1) return base;
  if (disc && *disc && pair_count(q, base, disc) == 1)
    return compose(buf, cap, base, disc, disc, content);
  char h[21];
  const char *parts[1] = { content ? content : "" };
  feed_hash_key(h, parts, 1);
  return compose(buf, cap, base, h, disc, content);
}

/* ── claimed set: open addressing, FNV-1a, grows at 1/2 load ─────────────── */
static unsigned long fnv(const char *s) {
  unsigned long h = 1469598103934665603UL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211UL; }
  return h;
}

static int claim_insert(char **t, int cap, char *s) {
  unsigned long i = fnv(s) & (unsigned long)(cap - 1);
  while (t[i]) {
    if (!strcmp(t[i], s)) return 0;
    i = (i + 1) & (unsigned long)(cap - 1);
  }
  t[i] = s;
  return 1;
}

int keyqual_claim(keyqual *q, const char *base, const char *uid) {
  if (!q || !base || !uid) return 1;
  if (uid == base || keyqual_count(q, base) <= 1) return 1;
  if ((q->nc + 1) * 2 > q->cc) {
    int nc = q->cc ? q->cc * 2 : 256;
    char **nt = calloc((size_t)nc, sizeof *nt);
    if (!nt) return 1;                      /* cannot track: emit, never drop */
    for (int i = 0; i < q->cc; i++) if (q->claimed[i]) claim_insert(nt, nc, q->claimed[i]);
    free(q->claimed);
    q->claimed = nt; q->cc = nc;
  }
  char *s = strdup(uid);
  if (!s) return 1;
  if (!claim_insert(q->claimed, q->cc, s)) { free(s); return 0; }
  q->nc++;
  return 1;
}

void keyqual_free(keyqual *q) {
  if (!q) return;
  for (int i = 0; i < q->nb; i++) free(q->base[i]);
  for (int i = 0; i < q->np; i++) free(q->pair[i]);
  for (int i = 0; i < q->cc; i++) free(q->claimed[i]);
  free(q->base); free(q->pair); free(q->claimed);
  memset(q, 0, sizeof *q);
}
