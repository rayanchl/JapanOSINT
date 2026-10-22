/* core/source_settings.{c,h} — per-source operator knobs, whitelisted.
 *
 * WHY THIS EXISTS. `ods-public-geonames` needs a response-body ceiling above
 * the 64 MB default in core/httpclient.c: GeoNames cities with population
 * >= 1,000 is 171,176 records, the ODS records API refuses offset+limit past
 * 10,000, and the only complete answer is a ~111 MB exports/jsonl file.
 * Measured 2026-10-20: with the allowance the row stores 171,176; without it,
 * 10,000 and a truncation notice.
 *
 * That was first written as a #define in the collector, which is the wrong
 * place for it twice over. A ceiling is a DEPLOYMENT decision — how much
 * memory one run of one collector may hold — and it belongs with the operator
 * who knows their machine, not frozen into C by whoever shipped the row. And a
 * second row needing the same thing would have meant a second #define and
 * another build.
 *
 * THE KEY SET IS CLOSED, and that is the point. A free key/value store hung
 * off every source is a configuration surface nobody can audit: a typo reads
 * as a setting that is silently ignored, and a value out of range reads as
 * accepted. Every key here declares a type, a minimum and a maximum, and
 * source_settings_put() refuses anything else by name. Adding a knob is a
 * deliberate edit to the table in the .c file.
 *
 * ENABLED IS NOT THE SAME AS SET. A row carries a value and an `enabled` flag
 * separately so that turning a knob off keeps the number the operator tuned;
 * re-enabling it does not start from a default guess. A disabled row resolves
 * exactly as an absent one.
 *
 * WHAT A SETTING CANNOT DO. It cannot lower a bound below what the code would
 * have used anyway (the resolver returns the default on anything it refuses),
 * and the body ceiling it raises is still enforced on the DECOMPRESSED stream
 * in httpclient's on_data(), so a generous allowance does not make a gzip bomb
 * reachable — it only decides how large an HONEST body may be. An operator who
 * sets $JO_HTTP_MAX_BODY explicitly still caps everything: see body_ceiling()
 * in core/httpclient.c. */
#ifndef JO_SOURCE_SETTINGS_H
#define JO_SOURCE_SETTINGS_H

#include <stddef.h>

/* db.h, not a forward declaration: `db_handle` is a typedef of an ANONYMOUS
 * struct (core/db.h), so `typedef struct db_handle db_handle;` here would
 * declare a different, incomplete type and conflict at every include site. */
#include "db.h"

/* The keys. Strings rather than an enum so the API, the client and the audit
 * log all name the same thing. */
#define SRCSET_BODY_CEILING "http_body_ceiling_bytes"
/* The per-source collection interval (core/source_interval.h), offered here
 * so one Settings section holds every per-source knob. It is NOT stored in
 * source_settings: reads, writes and the JSON for this key delegate to the
 * source_interval module, whose cache the scheduler reads every tick and whose
 * generation makes it re-plan. Absent from the JSON for an entity pivot,
 * which has no schedule to set. */
#define SRCSET_INTERVAL     "interval_sec"

/* The ENABLED value of `key` for `source_id`, or `def` when the source has no
 * row, the row is disabled, the key is unknown, or the stored value is out of
 * the key's declared range. Never returns something the key does not allow —
 * a caller can use the result without re-checking it. */
long source_settings_int(db_handle *db, const char *source_id,
                         const char *key, long def);

/* Store (or update) one setting. Returns 0 on success, or:
 *   -1 unknown key          -2 value not an integer / out of range
 *   -3 no such source       -4 storage failure
 * `who` is recorded in updated_by for the audit trail; may be NULL. */
int source_settings_put(db_handle *db, const char *source_id, const char *key,
                        const char *value, int enabled, const char *who);

/* Every known key for one source as a JSON object the API serves verbatim:
 * the declared type, bounds and default beside the stored value and state, so
 * the settings UI needs no second source of truth about what a knob accepts.
 *
 * `can_write` is carried in the payload because the gate lives at the ROUTE
 * (operator-only PUT) and the page has to know before it offers a control:
 * a form that is rendered editable and then refused with 403 is a worse answer
 * than one that says up front who may change this. Caller frees. */
char *source_settings_json(db_handle *db, const char *source_id,
                           int can_write);

#endif /* JO_SOURCE_SETTINGS_H */
