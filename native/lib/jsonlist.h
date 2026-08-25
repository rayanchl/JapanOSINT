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

/* As jsonlist_emit, but also reports how many records the page CONTAINED.
 *
 * Why the distinction matters, twice over. A record this emitter cannot label
 * is not emitted (emit_record returns 0 when no title can be derived), so the
 * emitted count is silently smaller than the page. That gap used to be
 * invisible, and it broke two things:
 *
 *   1. house rule 2 — records were dropped with no counter and no notice, so a
 *      source that fetched 10,000 rows and labelled none of them was
 *      indistinguishable from an upstream that is honestly empty;
 *   2. lib/pagewalk.c — which used the EMITTED count as its "did this page come
 *      back full" test. A full page of 20 holding 2 unlabelled records reported
 *      18, so the walk stopped AND suppressed its own truncation notice: a
 *      silent stop plus a silent claim of completeness.
 *
 * `seen` may be NULL. When it is, this function discloses any shortfall itself
 * as a collector-truncation-notice. When it is non-NULL the caller is taking
 * responsibility for the disclosure (pagewalk discloses once per walk rather
 * than once per page), and nothing is emitted here. */
int jsonlist_emit_ex(intel_sink *sink, const char *source_id, cJSON *doc,
                     const char *path, const char *record_type,
                     const char *lang, const char *tags_json, int *seen);

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
 * This is the JSON-list-shaped ENTRY POINT to lib/pagewalk.c, not a second
 * implementation of it. There is one walk loop in this tree and this is a
 * ~20-line adapter onto it: the continuation rules, the repeat-page guards,
 * the seen-vs-emitted accounting and the truncation disclosure all live in
 * pw_walk(), so a caller that says `jsonlist_emit_paged` and a caller that
 * says `pw_walk` cannot drift apart. Two engines answering the same question
 * differently is how a disclosure becomes a lie.
 *
 * Pass the whole document's timeout in `timeout_ms`; every page uses it.
 *
 * Returns total records emitted (>= 0), or -1 if the FIRST fetch failed (so
 * the caller can still distinguish a dead endpoint from an honest empty, R3). */
int jsonlist_emit_paged(intel_sink *sink, const char *source_id,
                        http_client *http, const char *url, int timeout_ms,
                        const char *path, const char *record_type,
                        const char *lang, const char *tags_json);

/* Query-string cursor arithmetic, shared so that the GeoJSON walk in
 * lib/geojson.c advances a page the same way this one does rather than growing
 * a second, subtly different copy.
 *
 * jsonlist_query_int  — value of `name=` as a long, or -1 if absent/not a number.
 * jsonlist_query_set  — url with `name=value` replaced, or appended. Caller frees. */
long  jsonlist_query_int(const char *url, const char *name);
char *jsonlist_query_set(const char *url, const char *name, long value);

#endif
