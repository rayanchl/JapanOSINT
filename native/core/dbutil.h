/* core/dbutil.h — the sqlite3 boilerplate that was rewritten per file.
 *
 * `ctext()` alone exists 24 times, byte-identical, across core/ and the three
 * collectors that read the DB back. The rest of the duplication is the
 * prepare → step → finalize dance around 437 sites, whose recurring defect is
 * SILENCE: the return of the final sqlite3_step() is routinely discarded, so a
 * failed write reads as a successful one (§9 of the 2026-08-07 audit).
 * db_run_stmt() exists so that return has to be looked at.
 *
 * Header-only `static inline`: 45 translation units, most needing one helper,
 * and no unused-function warning for static inline in a header.
 */
#ifndef JO_DBUTIL_H
#define JO_DBUTIL_H

#include "db.h"
#include <stddef.h>   /* NULL — db.h pulls in sqlite3.h, which does not */

/* Column as TEXT, or NULL when the column is SQL NULL.
 *
 * sqlite3_column_text() on a NULL column returns NULL already, but it ALSO
 * returns NULL on an OOM, and — the reason every copy of this exists — it
 * coerces a non-text column to text as a side effect, which invalidates any
 * pointer previously returned for that row. Checking the type first keeps
 * "absent" distinguishable from "empty" without triggering the conversion. */
static inline const char *db_ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}

/* Step a write statement to completion and finalize it. Returns 0 only when
 * sqlite3_step() actually reported DONE — the check that the 437 hand-written
 * copies of this sequence keep dropping. The statement is finalized either
 * way, so there is no leak on the error path. */
static inline int db_run_stmt(sqlite3_stmt *s) {
  if (!s) return -1;
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  return rc == SQLITE_DONE ? 0 : -1;
}

/* Prepare or fail loudly-but-cheaply: *out is left NULL on failure so the
 * caller cannot step a garbage handle. Returns 0 on success. */
static inline int db_prepare(sqlite3 *h, const char *sql, sqlite3_stmt **out) {
  *out = NULL;
  return sqlite3_prepare_v2(h, sql, -1, out, NULL) == SQLITE_OK ? 0 : -1;
}

#endif /* JO_DBUTIL_H */
