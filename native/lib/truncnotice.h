/* lib/truncnotice.h — "we did not use everything" as a record, not a log line.
 *
 * docs/SOURCE_EXHAUSTIVENESS.md: when a collector leaves fetched-or-fetchable
 * data unused, that shortfall is reported as data. Both engines already do it
 * (lib/hpengine.c, lib/jsonlist.c, lib/jsonstream.c) and so do a handful of
 * bespoke collectors — each with its own hand-rolled copy of the same twenty
 * lines. Copies drift: some name a remedy, some do not, one forgets the
 * upstream's own count, and the shape a downstream consumer has to recognise
 * ends up depending on which collector it came from.
 *
 * One emitter, one shape. `available` may be -1 when the upstream never said how
 * much exists — that is reported as unknown rather than filled in with a guess,
 * because "unknown remainder" and "no remainder" are different facts. */
#ifndef JO_TRUNCNOTICE_H
#define JO_TRUNCNOTICE_H
#include "../source.h"

/* Emit one collector-truncation-notice. `query` may be NULL (a scheduled feed
 * has none); `remedy` may be NULL. `available` < 0 means the upstream declared
 * no total. Keyed on source_id + query so repeated runs upsert one row rather
 * than piling up. */
void trunc_notice(intel_sink *sink, const char *source_id, const char *endpoint,
                  const char *query, long used, long available,
                  const char *reason, const char *remedy);

#endif
