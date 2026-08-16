/* lib/pagewalk.h — paging and truncation disclosure for list endpoints.
 *
 * WHY THIS EXISTS
 * The generated collectors (collectors/sources/_verified_macros.inc: VJSON,
 * VGEO, VCSV) did ONE fetch of ONE literal URL. 2,727 of those URLs carry a
 * hardcoded page-size parameter — `limit=`, `rows=`, `per_page=`, `$top=`,
 * `resultRecordCount=` — so every one of those sources returned page 1 and
 * nothing else, forever, with no record of what it left behind. ROR answers 20
 * of ~110,000 organisations and puts `number_of_results` in the same envelope.
 * `make audit-sources` cannot see any of it: it greps C control flow, and these
 * discards live entirely inside string literals.
 *
 * WHAT THIS DOES, AND DELIBERATELY DOES NOT DO
 * It continues a walk only when the upstream itself said how:
 *   1. a next-page LINK in the response envelope (JSON:API `links.next`, DRF
 *      `next`, OData `@odata.nextLink`, and friends) — the server handed us the
 *      exact URL, so following it invents nothing;
 *   2. an offset/page parameter ALREADY PRESENT in the collector's own URL —
 *      advancing a number the author wrote is not guessing either. An OFFSET
 *      additionally needs a declared size, because that size is the stride;
 *      a PAGE NUMBER does not, because it always advances by one, and the
 *      server's own first page supplies the "was that page full" yardstick.
 *      (That second case is why ROR below is walked at all: `?page=1` states
 *      no size, so requiring one meant this module did a single fetch of the
 *      very source its own docstring cites.)
 * It never adds a parameter that was not there. Only ~135 of those 2,727 URLs
 * carry both a size and an offset, and the rest use conventions that differ per
 * API; inventing `&offset=` for them would fabricate requests, and a wrong guess
 * against 2,592 endpoints is how you get banned rather than complete.
 *
 * So for everything it cannot legitimately continue, it does the other half of
 * the house rule: it DISCLOSES. A page that came back exactly full is evidence
 * that more exists, and that fact is emitted as a `collector-truncation-notice`
 * record — the same shape lib/hpengine.c uses — rather than a log line nobody
 * reads. Silence is the violation; a stated bound is not.
 *
 * ENV
 *   JO_PAGE_MAX   pages per run, default 20. 1 restores the old
 *                 single-fetch behaviour while KEEPING the disclosure.
 *   JO_PAGE_WALK  0 disables continuation entirely (disclosure still emitted).
 */
#ifndef JO_PAGEWALK_H
#define JO_PAGEWALK_H

#include "../source.h"
#include "../third_party/cJSON.h"

/* Fetch one page. Returns a parsed document the caller owns, or NULL. */
typedef cJSON *(*pw_fetch_fn)(const source_ctx *c, const char *url, void *ud);

/* Emit every record in one already-fetched page.
 *
 * Returns the number EMITTED, and must report through `seen` (never NULL) the
 * number of records the page CONTAINED. The two differ whenever the emitter
 * refuses a record — jsonlist_emit drops any record it cannot derive a title
 * for, geojson_emit_features skips non-objects.
 *
 * The distinction is load-bearing: `seen` is what decides "did this page come
 * back full", and therefore both whether to continue and whether to disclose.
 * Driving that off the emitted count meant a full page holding two unlabelled
 * records looked short, so the walk stopped AND suppressed its own truncation
 * notice — a silent stop plus a silent claim of completeness, which is the
 * exact failure this module exists to prevent. */
typedef int (*pw_emit_fn)(const source_ctx *c, intel_sink *s, const char *id,
                          cJSON *doc, void *ud, int *seen);

/* Walk `url` to exhaustion where the upstream permits, emitting through
 * `emit_page`, and disclose anything left behind as a truncation-notice record.
 *
 * Returns the number of records emitted (the notice is not counted), or -1 if
 * the FIRST fetch failed — a dead endpoint is an error, an empty one is not. */
int pw_walk(const source_ctx *c, intel_sink *s, const char *id,
            const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page, void *ud);

/* The default fetcher: feed_get_json with the 25 s timeout the macros used. */
cJSON *pw_fetch_json(const source_ctx *c, const char *url, void *ud);

#endif /* JO_PAGEWALK_H */
