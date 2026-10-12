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
