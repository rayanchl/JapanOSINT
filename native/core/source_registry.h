/* Source registry metadata (generated table). */
#ifndef JO_SRC_REGISTRY_H
#define JO_SRC_REGISTRY_H
typedef struct {
  const char *id,*name,*name_ja,*category,*type,*url,*description,*license;
  const char *layer;       /* registry layer id; NULL if absent */
  int free;                /* 0 | 1 */
  long update_interval;    /* seconds; <0 == null/absent (0 also → JSON null) */
} src_meta;
const src_meta *src_meta_get(const char *id);
const src_meta *src_meta_at(int i);   /* registry-order access; NULL if oob */
int src_meta_count(void);
#endif
