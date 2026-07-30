/* lib/seenset.c — see header. Linear scan: collector run sizes are hundreds to
 * low thousands of keys, where a hash table's setup costs more than it saves. */
#include "seenset.h"
#include <stdlib.h>
#include <string.h>

int seen_has(const seen_set *s, const char *key) {
  if (!s || !key) return 0;
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
  if (s->v[s->n]) s->n++;
  return 1;
}

void seen_free(seen_set *s) {
  if (!s) return;
  for (int i = 0; i < s->n; i++) free(s->v[i]);
  free(s->v);
  s->v = NULL; s->n = s->cap = 0;
}
