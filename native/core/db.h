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
void db_close(db_handle *db);

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
