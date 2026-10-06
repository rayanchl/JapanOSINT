/* lib/keyqual.h — order-independent uid qualification for records that share
 * a key.
 *
 * THE DEFECT THIS REPLACES (docs/audit-2026-10-02.md, open item 7)
 * Ten hand-written collectors resolved a shared key with a seen-set: the FIRST
 * record to arrive kept the plain key as its uid, every later one got
 * `key|<discriminator>`. That makes a record's identity a function of the order
 * the upstream happened to serve the set in. Socrata's newest-first window,
 * an Opendatasoft walk with no order_by, a device list ordered by last report:
 * the order moves, so the plain uid moves from one record to another between
 * runs, and the stored row under it is overwritten by a DIFFERENT record while
 * the record that used to own it re-appears under a qualified uid. Nothing in
 * `records=N stored=M` can see it — each run, taken alone, stores everything.
 *
 * THE RULE THIS IMPLEMENTS
 * Two passes over the WHOLE set:
 *   pass 1  keyqual_add() every record's base key (and its discriminator);
 *   pass 2  keyqual_uid() every record:
 *             base unique in the set                -> base, unchanged
 *             base shared, discriminator unique     -> "base|disc"
 *               among the records sharing that base
 *             base shared, discriminator absent or  -> "base|<sha1-80 of content>"
 *               shared as well
 * EVERY member of a colliding group is qualified, so which one owns the plain
 * key no longer depends on encounter order: none of them does. A key that is
 * unique keeps the uid it was always stored under.
 *
 * Byte-identical content yields one uid (a real duplicate collapses, nothing
 * is invented); content that differs gets its own. keyqual_claim() lets the
 * caller skip emitting the second copy of a byte-identical record, so
 * `records=` and `stored=` agree instead of the sink folding it silently.
 *
 * The set must be complete before pass 2 starts — a paged or multi-feed
 * collector buffers its pages first. Qualifying per page would bring the
 * defect back at every page boundary.
 *
 * USE
 *   keyqual q = {0};
 *   for (each record) keyqual_add(&q, base, disc);          // pass 1
 *   keyqual_seal(&q);
 *   for (each record) {                                      // pass 2
 *     char buf[512];
 *     const char *uid = keyqual_uid(&q, base, disc, content, buf, sizeof buf);
 *     if (!keyqual_claim(&q, base, uid)) continue;           // byte-identical repeat
 *     ... emit with remote_key = uid ...
 *   }
 *   keyqual_free(&q);
 */
#ifndef JO_KEYQUAL_H
#define JO_KEYQUAL_H

#include <stddef.h>

typedef struct {
  char **base; int nb, cb;      /* every base key added, sorted by seal    */
  char **pair; int np, cp;      /* base \x1f disc, for discriminated adds  */
  char **claimed; int nc, cc;   /* qualified uids handed out (claim)       */
  int sealed;
  int oom;                      /* an add could not be recorded            */
} keyqual;

/* Pass 1. `disc` may be NULL or "". Returns 0, or -1 when out of memory (the
 * set then under-counts and q->oom is set; callers may report it). NULL base
 * is ignored. */
int keyqual_add(keyqual *q, const char *base, const char *disc);

/* End of pass 1: sorts the set. keyqual_count/uid seal implicitly if needed. */
void keyqual_seal(keyqual *q);

/* How many records in the set carry `base` (0 if none). */
int keyqual_count(keyqual *q, const char *base);

/* Pass 2: the uid for one record — see the table above. Returns `base` itself
 * when it is unique, otherwise `buf` (always NUL-terminated). `content` is the
 * record's own bytes (e.g. cJSON_PrintUnformatted of the upstream record); it
 * is only read when the hash fallback is needed. A composite that does not fit
 * in `cap` is shortened to a prefix of `base` plus a hash of the whole of
 * base, disc and content, so truncation can never manufacture a collision. */
const char *keyqual_uid(keyqual *q, const char *base, const char *disc,
                        const char *content, char *buf, size_t cap);

/* 1 the first time a qualified uid is handed out this run, 0 for a repeat —
 * which only a byte-identical record (same base, same hash) can produce.
 * Always 1 for a base that is unique. */
int keyqual_claim(keyqual *q, const char *base, const char *uid);

void keyqual_free(keyqual *q);

#endif /* JO_KEYQUAL_H */
