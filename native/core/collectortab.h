/* core/collectortab.h — display metadata for collector keys (collectors.def). */
#ifndef JO_COLLECTORTAB_H
#define JO_COLLECTORTAB_H
typedef struct {
  const char *key, *label, *label_ja, *description, *parent;
} collector_meta;
/* NULL when the key is not in collectors.def — the caller renders the key. */
const collector_meta *collector_meta_get(const char *key);
int                   collector_meta_count(void);
const collector_meta *collector_meta_at(int i);
#endif
