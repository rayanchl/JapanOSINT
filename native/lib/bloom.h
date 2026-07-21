/* lib/bloom.h — persistent Bloom filter used as the ingest dedup prefilter and
 * a fast "definitely-not-present" gate at lookup time. See docs/breach-check-pipeline.md S5.
 * Overlapping combo/stealer dumps re-share billions of identical rows; this keeps
 * the sharded index from storing the same hash over and over. */
#ifndef JO_BLOOM_H
#define JO_BLOOM_H
#include <stddef.h>

typedef struct bloom bloom;

/* Size for ~expected_items at target false-positive rate (e.g. 0.001). */
bloom *bloom_new(unsigned long long expected_items, double fp_rate);

void bloom_add(bloom *b, const void *data, size_t len);
/* 1 = maybe present (or false positive); 0 = definitely NOT present. */
int  bloom_maybe(const bloom *b, const void *data, size_t len);

/* Persist to / restore from disk. bloom_load returns NULL if the file is
 * missing or malformed (caller should then bloom_new). */
int    bloom_save(const bloom *b, const char *path);
bloom *bloom_load(const char *path);

void bloom_free(bloom *b);

#endif
