/* lib/seenset.h — the growable "already seen this key" set.
 *
 * Fixed-size dedupe rings were a recurring exhaustive-use violation
 * (docs/SOURCE_EXHAUSTIVENESS.md): `char *seen[500]` with `for (… && sc < 500)`
 * silently stops emitting once the table fills, so a domain with 600 certs
 * loses 100 of them with no error and no notice. This set grows, so dedupe
 * never costs records. */
#ifndef JO_SEENSET_H
#define JO_SEENSET_H

typedef struct { char **v; int n, cap; } seen_set;

/* 1 = key is new (and now recorded), 0 = duplicate. NULL key = 0. */
int  seen_add(seen_set *s, const char *key);
/* 1 if present, without recording. */
int  seen_has(const seen_set *s, const char *key);
void seen_free(seen_set *s);

#endif
