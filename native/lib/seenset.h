/* lib/seenset.h — the growable "already seen this key" set.
 *
 * Fixed-size dedupe rings were a recurring exhaustive-use violation
 * (docs/SOURCE_EXHAUSTIVENESS.md): `char *seen[500]` with `for (… && sc < 500)`
 * silently stops emitting once the table fills, so a domain with 600 certs
 * loses 100 of them with no error and no notice. This set grows, so dedupe
 * never costs records, and it is hashed, so dedupe never costs a quadratic
 * scan either (see lib/seenset.c). */
#ifndef JO_SEENSET_H
#define JO_SEENSET_H

/* `v` holds the keys in insertion order; `h` is an open-addressing index into
 * it (slot -> index in v, -1 empty), `hcap` slots. Zero-initialise ({0}). */
typedef struct { char **v; int n, cap; int *h; int hcap; } seen_set;

/* 1 = key is new (and now recorded), 0 = duplicate. NULL key = 0. */
int  seen_add(seen_set *s, const char *key);
/* 1 if present, without recording. */
int  seen_has(const seen_set *s, const char *key);
void seen_free(seen_set *s);

/* ---- hash-only set with a ceiling and an exactness flag -------------------
 * The sink (core/intel.c) counts DISTINCT stored uids per run (rule 4b). It
 * needs the count, not the keys, and a 278,000-record source would hold tens of
 * MB of strdup'd uids in seen_set where a table of 64-bit hashes is 4 MB. So
 * this variant keeps only FNV-1a/64 hashes (0 is the empty-slot marker, so a
 * hash of 0 is stored as 1), in an open-addressed table that doubles at 70%
 * load from `SEEN_HASH_INIT` slots up to `max_slots` (0 = SEEN_HASH_MAX_DEFAULT,
 * a power of two). Two distinct keys whose 64-bit hashes collide count once:
 * at 278K keys a ~2e-9 chance, erring toward reporting loss that is not there.
 *
 * `inexact` goes to 1, and stays there, the first time a key could not be
 * recorded (ceiling reached, or out of memory). `n` is then a FLOOR; a caller
 * that publishes it must say so (`stored>=N`). Zero-initialise ({0}). */
#include <stddef.h>
#define SEEN_HASH_INIT         1024u
#define SEEN_HASH_MAX_DEFAULT  (1u << 22)
typedef struct {
  unsigned long long *tab;
  size_t cap;          /* slots, a power of two                               */
  size_t n;            /* distinct keys recorded                              */
  size_t max_slots;    /* ceiling on cap; 0 = SEEN_HASH_MAX_DEFAULT           */
  int    inexact;      /* 1 once a key could not be recorded: n is a floor    */
} seen_hset;

/* 1 = new and recorded; 0 = already there, or could not be recorded (then
 * s->inexact is set). NULL key = 0. */
int  seen_hset_add(seen_hset *s, const char *key);
void seen_hset_free(seen_hset *s);

#endif
