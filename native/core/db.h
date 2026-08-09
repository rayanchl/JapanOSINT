/* core/db.h — SQLite handle + boot migration (port of database.js boot path).
 *
 * The schema is applied from native/core/schema.sql, which is generated
 * verbatim from the authoritative live japanmap.db (idempotent: every object
 * is CREATE ... IF NOT EXISTS), so the C schema matches Node's exactly on
 * both fresh and existing databases. Opens WAL with the same pragmas Node
 * uses (database.js). */
#ifndef JO_DB_H
#define JO_DB_H

#include "../third_party/sqlite3.h"

typedef struct { sqlite3 *h; } db_handle;

/* Open (creating if absent), set WAL + pragmas, apply schema.sql.
 * db_path / schema_path default to repo-relative when NULL (env JO_DB /
 * JO_SCHEMA override). Returns 0 on success. */
int  db_open(db_handle *db, const char *db_path, const char *schema_path);

/* Open a SECOND connection to an already-migrated database: same file, same
 * pragmas, no schema apply / boot migration / registry seeding. A background
 * worker that runs its own transactions must not share the caller's handle —
 * a transaction belongs to a connection, not to a thread. Returns 0 on
 * success; close with db_close() as usual. */
int  db_attach(db_handle *db, const char *db_path);

void db_close(db_handle *db);

/* Secondary connection to the SAME database: identical path resolution and
 * pragmas as db_open(), but no schema apply and no boot migrations — those
 * already ran on the primary handle.
 *
 * Use this for a background pod that owns its writes. A SQLite transaction
 * belongs to a CONNECTION, not to a thread, so a worker that shares the event
 * loop's handle can interleave its BEGIN/COMMIT with request handling on the
 * same connection. Returns 0 on success. */
int  db_attach(db_handle *db, const char *db_path);

/* db_attach() with the boilerplate every off-loop caller was getting wrong.
 *
 * THE RULE. Any thread that is not the mongoose event loop and that runs a
 * transaction — every emit() is BEGIN/COMMIT, so that is every thread which
 * writes intel_items — must own its connection. A transaction belongs to a
 * CONNECTION, not a thread: a detached worker sharing the loop's handle can
 * have its rows discarded by a concurrent handler's ROLLBACK, or have its
 * half-written state published by that handler's COMMIT. SQLITE_THREADSAFE=1
 * serializes individual API CALLS; it does not scope transactions, and a
 * comment claiming otherwise is how this bug survived in three files.
 *
 * Usage — the whole pattern is three lines:
 *     db_handle own;
 *     db_handle *db = db_worker_open(&own, shared);
 *     ...                                  // use db, never `shared`
 *     db_worker_close(&own);
 *
 * Returns the private handle, or `fallback` (with a warning logged) if the
 * second connection could not be opened — degrading to the old shared-handle
 * behaviour beats dropping the work on the floor. db_worker_close() is a
 * no-op in that case, so it is always safe to call. */
db_handle *db_worker_open(db_handle *own, db_handle *fallback);
void       db_worker_close(db_handle *own);

/* PRAGMA integrity_check — returns 1 if "ok", else 0 (msg via out). */
int  db_integrity_ok(db_handle *db, char *out, int out_sz);

/* Count user objects (tables+indexes, excluding sqlite_/fts shadow). */
int  db_object_count(db_handle *db);

/* exec a SQL script (multi-statement); returns 0 on success. */
int  db_exec(db_handle *db, const char *sql, char **errmsg);

/* Idempotent ADD COLUMN — probes PRAGMA table_info first, so it is safe to
 * call on every boot. `decl` is type + constraints only ("TEXT", "REAL",
 * "INTEGER NOT NULL DEFAULT 0"), not the column name.
 *
 * THIS is how a column is added in this codebase. schema.sql is generated
 * from the live DB and every statement in it is CREATE ... IF NOT EXISTS,
 * which silently no-ops on an existing table — a column appended to a CREATE
 * there would never reach any deployed database. Call this from db_open()'s
 * boot-migration block instead. */
void ensure_column(db_handle *db, const char *table, const char *col,
                   const char *decl);

#endif
