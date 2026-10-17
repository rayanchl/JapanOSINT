/* core/collectortab.h — display metadata for collector keys (collectors.def). */
#ifndef JO_COLLECTORTAB_H
#define JO_COLLECTORTAB_H
typedef struct {
  const char *key, *label, *label_ja, *description, *parent;
} collector_meta;
/* NULL when the key is not in collectors.def — the caller renders the key. */
const collector_meta *collector_meta_get(const char *key);
/* A GROUP is a `grp_*` entry with no parent: the natural-language heading the
 * Collectors pages nest collector keys under. */
int collector_meta_is_group(const collector_meta *m);
/* The group a collector key belongs to: its parent in collectors.def, else
 * "grp_unassigned" — a visible bucket, never a silent drop; the unit test
 * fails while any registered collector lands there. */
const char *collector_group_of(const char *collector_key);
#define COLLECTOR_GROUP_UNASSIGNED "grp_unassigned"
int                   collector_meta_count(void);
const collector_meta *collector_meta_at(int i);
#endif
