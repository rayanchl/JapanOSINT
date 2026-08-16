/* lib/jsonlist.h — generic "JSON array of records" → intel.
 *
 * The tree already has one toolkit per well-known shape: rss_atom.h for
 * RSS/Atom, geojson.h for the FeatureCollection family, csv.h for CSV. The
 * gap was plain JSON APIs, which have no shared schema at all — and that gap
 * is why JSON-only upstreams either got a bespoke 80-line collector each or
 * did not get collected. This is the missing fourth toolkit.
 *
 * It does NOT guess at meaning. It maps field NAMES by a fixed precedence
 * list (title/name/headline…, url/link/href…, published/date/timestamp…) and
 * emits nothing for a record where the mapping finds no title — a record with
 * no human-readable label is not an intel row, per SOURCE_AUTHORING_CONTRACT
 * R1. Every unmapped field survives verbatim in properties_json, so nothing
 * the upstream returned is silently dropped.
 *
 * Geometry follows R2 strictly: has_geo is set ONLY when the record itself
 * carried a finite numeric coordinate pair (or a GeoJSON Point geometry).
 * There is no fallback centroid, ever. */
#ifndef JO_JSONLIST_H
#define JO_JSONLIST_H
#include "../source.h"
#include "../third_party/cJSON.h"

/* Locate the record array inside `doc`.
 *   path == NULL or ""  → `doc` itself must be an array
 *   path == "*"         → auto-detect the longest array of objects, searching
 *                         the top level then one level down
 *   path == "."         → `doc` is ONE record, not a list (see jsonlist_emit)
 *   otherwise           → dot-separated path, e.g. "data.items"
 * Returns the array, or NULL. Does not allocate. */
cJSON *jsonlist_find_array(cJSON *doc, const char *path);

/* Emit one intel row per object in the array named by `path`.
 * `record_type`, `lang` and `tags_json` are stamped on every row; any may be
 * NULL. Returns the number emitted (>= 0). Never negative — a fetch failure
 * is the caller's to report, so that an honest empty stays 0 (R3). */
int jsonlist_emit(intel_sink *sink, const char *source_id, cJSON *doc,
                  const char *path, const char *record_type,
                  const char *lang, const char *tags_json);

/* Fetch `url` and emit every record — ACROSS PAGES.
 *
 * jsonlist_emit() above takes a document that is already in hand, so it can
 * only ever see page 1. That is the shape the whole generated fleet was built
 * on (_verified_macros.inc: one GET, one emit), and it meant ~6,500 sources
 * silently stopped at the first page — 2,081 of them against URLs that
 * hard-code a page size, so the discard was both guaranteed and invisible.
 * `api.dane.gov.pl/1.4/datasets?page=1&per_page=100` answers with
 * `meta.count: 26536` and a `links.next`, and the collector kept 100 of them.
 *
 * The walk continues while the upstream keeps saying there is more, and stops
 * on the first of: no next-page signal, a page that produced no records, or
 * the page ceiling ($JO_JSONLIST_PAGE_MAX, default 20). Two signals are
 * honoured, in this order:
 *
 *   1. a server-supplied next link (links.next, next, next_url, meta.next,
 *      paging.next, @odata.nextLink) — authoritative, no guessing;
 *   2. otherwise, offset/page arithmetic, but ONLY when the URL declares a
 *      page size AND the page came back exactly full. A short page is the
 *      upstream saying it is done, so a partial page never triggers another
 *      request.
 *
 * When the ceiling stops a walk that had more to give, that shortfall is
 * emitted as a `collector-truncation-notice` record — the same disclosure
 * hpengine makes — because a log line nobody reads is not a disclosure.
 *
 * Returns total records emitted (>= 0), or -1 if the FIRST fetch failed (so
 * the caller can still distinguish a dead endpoint from an honest empty, R3). */
int jsonlist_emit_paged(intel_sink *sink, const char *source_id,
                        http_client *http, const char *url, int timeout_ms,
                        const char *path, const char *record_type,
                        const char *lang, const char *tags_json);

#endif
