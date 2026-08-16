/* lib/jsonstream.h — emit a huge JSON array over HTTP in bounded memory.
 *
 * WHY THIS EXISTS. Bulk government registers are some of the highest-value
 * OSINT data there is — Ukraine's Interior Ministry publishes 73k wanted
 * persons, 122k missing persons, 79k stolen vehicles, ~1M stolen handsets by
 * IMEI and ~1.1M firearms as plain JSON arrays on data.gov.ua. The firearms
 * file alone is ~714 MB.
 *
 * Every existing path in this tree refuses them, for two different reasons:
 *
 *   collectors/sources/_verified_macros.inc  VJSON calls feed_get_json(), which
 *     buffers the whole body and then hands it to cJSON. A 714 MB array needs
 *     the body in RAM AND the parsed tree at the same time — several GB for one
 *     collector run.
 *   collectors/verify_feeds.py               caps at 8 MB and returns TOO_BIG,
 *     so the source never even reaches the manifest.
 *
 * Between them, a size limit was silently deciding which public registers this
 * platform can see. That is a tooling constraint masquerading as a coverage
 * decision, and it drops precisely the records an investigation wants.
 *
 * HOW IT WORKS. The response is consumed as it arrives. The reader tracks brace
 * depth and string state to find the boundaries of each top-level element of
 * the array, parses THAT ELEMENT ALONE with cJSON, emits it, and drops it.
 * Peak memory is one record plus a small window — flat in the size of the file.
 * A 714 MB array costs the same RAM as a 714 KB one.
 *
 * It is not a general JSON parser: it finds element boundaries and delegates
 * the actual parsing of each element to cJSON, so the records are parsed by
 * exactly the same code as everywhere else in the tree.
 *
 * EXHAUSTIVE USE. There is no record cap. The walk ends when the array ends.
 * If it is cut short — transport failure mid-stream, or the operator-set
 * JO_JSONSTREAM_MAX_RECORDS ceiling — the shortfall is emitted as a
 * collector-truncation-notice carrying records_used and the reason, the same
 * disclosure lib/hpengine.c and lib/jsonlist.c make. A partial read is never
 * presented as a complete one. See docs/SOURCE_EXHAUSTIVENESS.md. */
#ifndef JO_JSONSTREAM_H
#define JO_JSONSTREAM_H

#include "../source.h"

/* Stream the JSON array at `url` and emit one intel row per element.
 *
 * `path` selects the array inside an enclosing object, dotted, exactly as
 * jsonlist_emit's path argument ("" or NULL = the document IS the array).
 * Only a top-level or one-level-nested array is supported; anything deeper
 * would require buffering the enclosing object, which defeats the purpose.
 *
 * `record_type`, `lang` and `tags_json` are stamped on every row, as in
 * jsonlist_emit. Records are labelled by the same rules (jsonlist's field-name
 * precedence), so a row that streams looks identical to a row that did not.
 *
 * Returns the number of rows emitted (>= 0), or -1 if the transfer never
 * started — so a dead endpoint stays an error and an honest empty stays 0 (R3).
 *
 * The URL clears hostgate's floor before a byte is fetched; a blocked
 * destination returns -1 rather than being silently skipped. */
int jsonstream_emit(intel_sink *sink, const char *source_id, const char *url,
                    int timeout_ms, const char *path, const char *record_type,
                    const char *lang, const char *tags_json);

#endif
