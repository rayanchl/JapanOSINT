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

/* Design capacity (the `expected_items` it was sized for) and the number of
 * bloom_add() calls it has seen. Both are persisted, so a caller can tell that
 * a filter restored from disk is already past the size it was built for.
 *
 * This matters because a bloom hit is treated as "already seen" and the row is
 * SKIPPED: once the filter saturates, false positives stop being a performance
 * detail and start silently discarding real data. Check bloom_saturated()
 * before letting a hit suppress a write. */
unsigned long long bloom_capacity(const bloom *b);
unsigned long long bloom_count(const bloom *b);
/* Estimated false-positive rate at the current fill: (1-e^(-k*count/m))^k. */
double bloom_fp_estimate(const bloom *b);
/* 1 once insertions exceed the design capacity (i.e. the FP rate has drifted
 * well past the target it was sized for). */
int    bloom_saturated(const bloom *b);

/* Persist to / restore from disk. bloom_load returns NULL if the file is
 * missing or malformed (caller should then bloom_new). Reads both the current
 * format and the older one that carried no counters. */
int    bloom_save(const bloom *b, const char *path);
bloom *bloom_load(const char *path);

void bloom_free(bloom *b);

#endif
