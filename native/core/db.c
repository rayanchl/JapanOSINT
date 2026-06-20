#include "db.h"
#include "source_registry.h"   /* src_meta_get (merged metadata)        */
#include "../source.h"         /* registry_all / registry_count (sources) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JO_REPO_ROOT is -D'd by the Makefile to the JapanOSINT repo root so the
 * binary finds the DB + schema without args; JO_DB / JO_SCHEMA env override. */
#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

static char *slurp(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc(n + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t rd = fread(buf, 1, (size_t)n, f);
  fclose(f);
  buf[rd] = '\0';
  return buf;
}

int db_exec(db_handle *db, const char *sql, char **errmsg) {
  return sqlite3_exec(db->h, sql, NULL, NULL, errmsg) == SQLITE_OK ? 0 : 1;
}

/* Seed the sources table from the runtime registry so EVERY registered source
 * (Japan map collector and OSINT-SaaS service alike) has a row and therefore
 * appears in /api/status, /api/sources, /api/keys with full metadata. Pulls
 * type/category/url/name from the merged src_meta_get() (curated table, else
 * synthesized from the source_def). Idempotent: ON CONFLICT(id) DO NOTHING
 * preserves existing rows' status/probe/schedule columns untouched. Runs after
 * the REGISTER_SOURCE constructors (which fire before main), so the registry
 * is fully populated. */
static void db_seed_sources(db_handle *db) {
  const source_def **a = registry_all();
  int n = registry_count();
  static const char *SQL =
    "INSERT INTO sources (id,name,type,category,url,status) "
    "VALUES (?1,?2,?3,?4,?5,'pending') ON CONFLICT(id) DO NOTHING";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, SQL, -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_exec(db->h, "BEGIN", NULL, NULL, NULL);
  int added = 0;
  for (int i = 0; i < n; i++) {
    const source_def *d = a[i];
    const src_meta *m = src_meta_get(d->id);
    const char *type = (m && m->type)     ? m->type     : "api";
    const char *cat  = (m && m->category) ? m->category : "investigation";
    const char *url  = (m && m->url)      ? m->url      : NULL;
    const char *name = (m && m->name)     ? m->name
                       : (d->name ? d->name : d->id);
    sqlite3_bind_text(s, 1, d->id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, name,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, type,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, cat,   -1, SQLITE_TRANSIENT);
    if (url) sqlite3_bind_text(s, 5, url, -1, SQLITE_TRANSIENT);
    else     sqlite3_bind_null(s, 5);
    if (sqlite3_step(s) == SQLITE_DONE) added += sqlite3_changes(db->h);
    sqlite3_reset(s);
    sqlite3_clear_bindings(s);
  }
  sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL);
  sqlite3_finalize(s);
  fprintf(stderr, "[db] seeded sources from registry (%d new rows)\n", added);
}

int db_open(db_handle *db, const char *db_path, const char *schema_path) {
  const char *dbp = db_path ? db_path
    : (getenv("JO_DB") ? getenv("JO_DB") : JO_REPO_ROOT "/data/japanmap.db");
  const char *scp = schema_path ? schema_path
    : (getenv("JO_SCHEMA") ? getenv("JO_SCHEMA") : JO_REPO_ROOT "/native/core/schema.sql");

  int rc = sqlite3_open_v2(dbp, &db->h,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[db] open failed: %s\n", sqlite3_errmsg(db->h));
    return 1;
  }
  /* Mirror database.js: WAL + sane pragmas. */
  sqlite3_exec(db->h, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
  sqlite3_exec(db->h, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);
  sqlite3_exec(db->h, "PRAGMA busy_timeout=5000;", NULL, NULL, NULL);
  sqlite3_exec(db->h, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);

  char *schema = slurp(scp);
  if (!schema) {
    fprintf(stderr, "[db] cannot read schema: %s\n", scp);
    return 1;
  }
  char *err = NULL;
  rc = sqlite3_exec(db->h, schema, NULL, NULL, &err);
  free(schema);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[db] schema apply failed: %s\n", err ? err : "?");
    sqlite3_free(err);
    return 1;
  }
  /* Boot migrations (idempotent, same discipline as the tenancy ALTERs in
   * Node). schema.sql is generated from the live DB so new runtime-only
   * columns are added here, not in the generated schema. */
  {
    sqlite3_stmt *st;
    int has_sched = 0;
    if (sqlite3_prepare_v2(db->h, "PRAGMA table_info(sources)", -1, &st, NULL)
        == SQLITE_OK) {
      while (sqlite3_step(st) == SQLITE_ROW)
        if (strcmp((const char *)sqlite3_column_text(st, 1), "schedule_mode") == 0)
          has_sched = 1;
      sqlite3_finalize(st);
    }
    if (!has_sched)
      sqlite3_exec(db->h,
        "ALTER TABLE sources ADD COLUMN schedule_mode TEXT NOT NULL "
        "DEFAULT 'map_cron' "
        "CHECK(schedule_mode IN ('map_cron','search_only'))",
        NULL, NULL, NULL);
  }

  /* Every registered source gets a sources-table row (idempotent). */
  db_seed_sources(db);

  fprintf(stderr, "[db] opened %s, schema applied (%d objects)\n",
          dbp, db_object_count(db));
  return 0;
}

void db_close(db_handle *db) {
  if (db && db->h) { sqlite3_close(db->h); db->h = NULL; }
}

int db_integrity_ok(db_handle *db, char *out, int out_sz) {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h, "PRAGMA integrity_check;", -1, &st, NULL) != SQLITE_OK)
    return 0;
  int ok = 0;
  if (sqlite3_step(st) == SQLITE_ROW) {
    const unsigned char *v = sqlite3_column_text(st, 0);
    if (v) {
      if (out && out_sz > 0) { strncpy(out, (const char *)v, out_sz - 1); out[out_sz - 1] = 0; }
      ok = strcmp((const char *)v, "ok") == 0;
    }
  }
  sqlite3_finalize(st);
  return ok;
}

int db_object_count(db_handle *db) {
  sqlite3_stmt *st = NULL;
  const char *q =
    "SELECT COUNT(*) FROM sqlite_master "
    "WHERE name NOT LIKE 'sqlite_%' "
    "AND name NOT GLOB '*_fts_data' AND name NOT GLOB '*_fts_idx' "
    "AND name NOT GLOB '*_fts_content' AND name NOT GLOB '*_fts_docsize' "
    "AND name NOT GLOB '*_fts_config' AND sql IS NOT NULL;";
  if (sqlite3_prepare_v2(db->h, q, -1, &st, NULL) != SQLITE_OK) return -1;
  int n = (sqlite3_step(st) == SQLITE_ROW) ? sqlite3_column_int(st, 0) : -1;
  sqlite3_finalize(st);
  return n;
}
