#include "collectortab.h"
#include <string.h>

static const collector_meta TAB[] = {
#define COLLECTOR(k, l, lj, d, p) { k, l, lj, d, p },
#include "collectors.def"
#undef COLLECTOR
};
#define NTAB ((int)(sizeof TAB / sizeof TAB[0]))

const collector_meta *collector_meta_get(const char *key) {
  if (!key) return NULL;
  for (int i = 0; i < NTAB; i++)
    if (!strcmp(TAB[i].key, key)) return &TAB[i];
  return NULL;
}
int collector_meta_count(void) { return NTAB; }
const collector_meta *collector_meta_at(int i) {
  return (i >= 0 && i < NTAB) ? &TAB[i] : NULL;
}

int collector_meta_is_group(const collector_meta *m) {
  return m && m->parent == NULL && strncmp(m->key, "grp_", 4) == 0;
}
const char *collector_group_of(const char *collector_key) {
  const collector_meta *m = collector_meta_get(collector_key);
  if (m && m->parent && m->parent[0]) return m->parent;
  if (collector_meta_is_group(m)) return m->key;
  return COLLECTOR_GROUP_UNASSIGNED;
}
