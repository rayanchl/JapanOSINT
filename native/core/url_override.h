/* core/url_override.h — runtime collector URL override map (the apply mechanism
 * for verified repairs). The Node maintenance pod text-patched sourceRegistry.js
 * and opened a PR; the C engine has no per-collector registry file, so a verified
 * URL swap is applied at runtime through this map: http_request() routes every
 * outbound URL through url_override_apply(), so an override takes effect for all
 * collectors with zero per-collector edits and no recompile.
 *
 * Loaded from the collector_url_overrides table at boot and after each applied
 * repair. The internal list only grows (one entry per source, plus the rare
 * re-application) — domain-bounded and sub-GB — so a pointer returned by
 * url_override_apply() stays valid for the process lifetime even across reloads. */
#ifndef JO_URL_OVERRIDE_H
#define JO_URL_OVERRIDE_H

#include "db.h"

/* (Re)load the in-memory map from collector_url_overrides. Idempotent: a row
 * already represented (same old_url -> new_url) is not duplicated. Thread-safe
 * against concurrent url_override_apply() callers. */
void url_override_reload(db_handle *db);

/* Returns the replacement URL when `url` exactly matches a stored old_url, else
 * `url` unchanged. The returned pointer is stable for the process lifetime. */
const char *url_override_apply(const char *url);

/* Removes the override for `old_url` (all matching entries). Returns the number
 * removed. A misapplied repair used to be irreversible without a restart —
 * the list was append-only and nothing ever unset an entry.
 *
 * Entries are tombstoned, not freed: url_override_apply() hands out interior
 * pointers that callers may still hold, so freeing a node would dangle. A
 * tombstone is skipped by apply() and reused by a later reload. */
int url_override_remove(db_handle *db, const char *old_url);

/* Drops every override from memory (the table is left alone unless `db` is
 * non-NULL, in which case rows are deleted too). Returns the count. */
int url_override_reset(db_handle *db);

#endif
