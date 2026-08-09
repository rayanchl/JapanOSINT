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

#endif
