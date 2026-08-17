/* lib/pager.h — "has the upstream told us there is more?", in one place.
 *
 * WHY THIS EXISTS. Two engines walk paged endpoints: jsonlist_emit_paged()
 * (lib/jsonlist.c), which serves the ~6,500 generated VJSON collectors, and
 * hp_run() (lib/hpengine.c), which serves the deep-record rows. jsonlist grew
 * a careful auto-pager — server next-links first, then cursor arithmetic that
 * only moves when the URL itself declared a page size and the page came back
 * exactly full. hpengine had none: it paged only when a row spelled out
 * next_path or page_param, and did exactly one request otherwise.
 *
 * That asymmetry was a trap, not a preference. Moving a row from VJSON to an
 * hp_source — which is what wiring a proven detail hop requires — silently
 * traded the page walk for the second hop, so a source got deeper and lost
 * every page after the first. Discarding data to buy penetrancy is still
 * discarding data (docs/SOURCE_EXHAUSTIVENESS.md). So the logic lives here and
 * both engines call it, and a row that moves between them keeps its walk.
 *
 * THE ONE RULE this file enforces: we advance only on evidence the upstream
 * itself supplied. Either it published a next link, or the URL declares a
 * page-size parameter AND the page came back exactly that full — a short page
 * is the upstream saying it is finished. We never guess a page that was never
 * offered, and we never fabricate a total. */
#ifndef JO_PAGER_H
#define JO_PAGER_H
#include "../third_party/cJSON.h"

/* Read `name=<int>` out of a query string. -1 when absent or unparseable. */
long pager_query_int(const char *url, const char *name);

/* Replace `name=<old>` with `name=<value>`, or append it when absent. This is
 * a REPLACE, not an append-always: appending a second `page=` and letting the
 * server pick a winner is how a walk silently re-reads page 1 forever.
 * Caller frees. */
char *pager_query_set(const char *url, const char *name, long value);

/* The server's own next-page link, or NULL when it published none (or
 * published it as null/false/empty, which means "this is the last page" in
 * every dialect we accept). Only absolute http(s) links are returned, so a
 * relative link is treated as absent rather than joined by guesswork.
 * Caller frees. */
char *pager_next_link(const cJSON *doc);

/* The upstream's own count of what exists, or -1 when it did not say. -1 is
 * reported as "unknown" in the truncation notice: unknown is honest, a guessed
 * total is not. */
long pager_declared_total(const cJSON *doc);

/* Next page URL by cursor arithmetic, or NULL when the URL declares no
 * page-size parameter or `got` says the page was short. `got` is the number of
 * records the page actually produced. Caller frees. */
char *pager_advance(const char *url, int got);

/* Last resort, for the URLs pager_advance() cannot move: `?page=1` with no page
 * size anywhere in it. Those are not unpaged — they are paged endpoints whose
 * page size only the server knows (ROR serves 20, bio.tools 10) — and reading
 * page 1 of 100,000 ROR organisations and stopping is the discard this whole
 * file exists to prevent.
 *
 * The evidence here is the upstream's own arithmetic: it declared a total, it
 * has handed over fewer records than that, and the URL names a page cursor. Then
 * there IS more and it says so itself. Returns NULL when it declared no total
 * (`total` < 0), when `seen` has reached it, or when no cursor is present —
 * never on an assumption about page size.
 *
 * `total` is pager_declared_total()'s answer, `seen` the records emitted so far
 * across the walk. Caller frees. */
char *pager_advance_by_total(const char *url, long total, long seen);

#endif
