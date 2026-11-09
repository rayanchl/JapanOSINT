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
 *   JO_PAGE_MAX   pages per run, default 20, for EVERY walk in the tree — this
 *                 module, lib/jsonlist.c, lib/hpengine.c and (since 2026-11-07,
 *                 when it was found reading only $JO_GEOJSON_PAGE_MAX)
 *                 lib/geojson.c alike. It used to reach only this one; see
 *                 jo_page_max() in pagewalk.c for what the others did instead. 1 restores the old single-fetch
 *                 behaviour while KEEPING the disclosure.
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
 * the FIRST fetch failed — a dead endpoint is an error, an empty one is not.
 * A first page answered with the upstream's own "nothing matched" (a 204, or
 * a 404 whose body says so — feed_status_declared_empty) is that empty: 0. */
/* The page ceiling every walk in the tree uses: $JO_PAGE_MAX when set, else
 * `legacy_env` when that name is set (NULL for none), else 20. One function
 * because there were three bounds with two numbers and two knobs, and one of
 * them had no knob at all — see jo_page_max() in pagewalk.c. */
int jo_page_max(const char *legacy_env);

/* The same ceiling for a caller that carries a source's own DECLARED page_max
 * (0 for none): $JO_PAGE_MAX when set — in both directions, because it is the
 * operator's brake — else the declared value, else the default above. Call
 * this rather than `declared > 0 ? declared : jo_page_max(NULL)`, which is
 * what hpengine did, and which left the knob unable to bound the 669 rows that
 * declare a ceiling of their own. */
int jo_page_max_for(int declared);

/* OData v2 "verbose" JSON states its total as `d.__count`, and as a STRING:
 * `{"d":{"results":[…],"__count":"46144"}}` when the request carries
 * `$inlinecount=allpages`. Returns that number, or -1 — for an absent key, an
 * empty string, or any string that is not entirely digits. One reader, called
 * by both declared-total lists (pagewalk's and lib/jsonlist.c's), because a
 * total read one way in one engine and not at all in the other is the shape
 * this tree has had to repair three times. */
long pw_odata_v2_count(const cJSON *doc);

/* THE UPSTREAM'S DECLARED TOTAL — the one reader for every walk in the tree
 * (lib/pagewalk.c, lib/jsonlist.c, lib/geojson.c; lib/hpengine.c should call
 * it too). Returns the size of the whole match set the body states, or -1
 * when it states none. `basis` (may be NULL) receives the key that carried
 * it ("total", "result.count", "d.__count", "count", …), "" when -1.
 *
 * `page_records` is how many records THIS page holds, and decides only one
 * thing: whether a bare root `count` is a total. It is, only when it EXCEEDS
 * page_records (a count of the page's own records cannot be larger than the
 * page). Pass -1 when the page size is unknown, and bare `count` is not read.
 * JO_TOTAL_TRUST_BARE_COUNT reads it unconditionally — the old jsonlist rule,
 * kept for hptest 58c's fixture, which states no page size; production
 * callers pass the page's record count. Matching is case-insensitive; a value must be a non-negative
 * integer. Why each of those is so: the comment on the definition. */
#define JO_TOTAL_TRUST_BARE_COUNT (-2L)
long jo_declared_total(const cJSON *doc, long page_records,
                       char *basis, size_t cap);

/* THE NEXT-PAGE LINK the server published, resolved against `base` (the URL
 * the page was read from; may be NULL, and then only an absolute link is
 * returned). Every key spelling any of the three walkers knew, plus the
 * `links[rel=next]` array form. A relative link is resolved only in its two
 * unambiguous spellings, "?query" and "/path"; anything else is refused.
 * Returns a malloc'd absolute URL, or NULL. */
char *jo_next_link(const cJSON *doc, const char *base);
/* The resolution step alone: `href` against `base`, as jo_next_link does it.
 * malloc'd, or NULL when href is not a URL this rule accepts. */
char *jo_resolve_link(const char *base, const char *href);

/* WHY A LATER PAGE STOPPED A WALK, and what moves it — the one describer.
 * `status` is in feedlib's vocabulary (feed_last_json_status): an HTTP status,
 * FEED_ST_TRANSPORT (0) for an exchange that never completed,
 * FEED_ST_UNPARSED (-2) for a 2xx body that could not be read,
 * FEED_ST_DECLARED_EMPTY / 204 for the upstream's own "nothing matched",
 * FEED_ST_UNKNOWN (-1) for unrecorded. A caller with another vocabulary maps
 * into it (hpengine: its -1 transport is FEED_ST_TRANSPORT, its `unreadable`
 * flag is FEED_ST_UNPARSED). `failed_page` > 0 is named in the reason ("page
 * 3 answered HTTP 400 …"), else "a later page". Either buffer may be NULL.
 * The remedy distinguishes a deep-paging cap (a 4xx after evidence of more:
 * narrow each request — no gap reaches it) from a rate limit (429: a measured
 * per-host gap), a refusal, a server error, a transport failure (one reading;
 * can be our own egress) and an unreadable 2xx (read the body). */
void pw_cut_describe(int failed_page, long status, char *reason, size_t rcap,
                     char *remedy, size_t mcap);

/* WHICH CEILING STOPPED A WALK, named, with the remedy that moves it:
 * $JO_PAGE_MAX when an operator set it (it wins over everything), else the
 * source's own declared ceiling (`declared` > 0), else `legacy_env` when that
 * older variable is what set it (may be NULL), else the tree's default. */
void jo_page_ceiling_describe(int page_max, int declared, const char *legacy_env,
                              char *reason, size_t rcap,
                              char *remedy, size_t mcap);

int pw_walk(const source_ctx *c, intel_sink *s, const char *id,
            const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page, void *ud);

/* As pw_walk, with an explicit page ceiling for a source whose collection size
 * has been MEASURED and is larger than the 20-page default.
 *
 * The default exists as a brake on a runaway walk against an unknown upstream,
 * and raising it globally would lift that brake for all 2,727 of them. But a
 * source whose total the upstream itself publishes — EMSC's 48,729 moment
 * tensors, CBS's 159,120 CPI rows, OpenAerialMap's 21,437 images — is not
 * unknown, and capping it at 20 pages turns a complete read into a permanently
 * disclosed shortfall. Pass the number of pages that covers the measured total
 * with headroom, and say in the caller where the measurement came from.
 *
 * 0 means "use the default". JO_PAGE_MAX still overrides, in both directions:
 * it is the operator's brake, so a source cannot talk past it, and an operator
 * who lowers it to make a run cheap gets that from every source. */
int pw_walk_max(const source_ctx *c, intel_sink *s, const char *id,
                const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page,
                void *ud, int page_max_declared);

/* As pw_walk_max, reading the collection over a CYCLE: hpengine's page_resume,
 * for a collector that walks through this module.
 *
 *   resume_in   the position this source stored on its previous run (the
 *               scheduler hands it over as source_ctx.resume_in), or NULL
 *   resume_out  receives the position to store for the next run: the URL the
 *               walk would have requested next when the page ceiling stopped
 *               it, or "" — the cycle completed, the first page answered an
 *               honest empty, or a page FAILED (a refused page is not
 *               bookmarked; it would be retried for ever) — which the caller
 *               stores as a cleared position (source_ctx.resume_out(ctx, NULL))
 *   cap         resume_out's size; a position that does not fit is not stored
 *
 * The walk starts at resume_in only when its HOST is the row's own: a reading
 * position may move a walk through a collection, never to another server.
 * Every stop is still disclosed, and the notice carries next_run_resumes_at.
 * As with hpengine, do not use this on an entity pivot (the scheduler passes
 * no position there) or on a recency feed whose order is newest-first: new
 * records land at the head and a resumed walk re-reads and skips at the seam. */
int pw_walk_max_resume(const source_ctx *c, intel_sink *s, const char *id,
                       const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page,
                       void *ud, int page_max_declared,
                       const char *resume_in, char *resume_out, size_t cap);

/* The default fetcher: feed_get_json with the 25 s timeout the macros used. */
cJSON *pw_fetch_json(const source_ctx *c, const char *url, void *ud);

#endif /* JO_PAGEWALK_H */
