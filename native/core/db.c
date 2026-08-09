#include "db.h"
#include "translate.h"         /* translate_migrate (owns its own index) */
#include "simhash.h"           /* simhash_ensure_schema (owns its own index) */
#include "content_change.h"    /* content_change_ensure_schema (same reason) */
#include "media.h"             /* media_migrate (same reason) */
#include "camera_stills.h"     /* camera_stills_migrate (same reason) */
#include "fts_schema.h"        /* fts_schema_migrate (widens intel_items_fts) */
#include "source_registry.h"   /* src_meta_get (merged metadata)        */
#include "../source.h"         /* registry_all / registry_count (sources) */
#include "../third_party/cJSON.h" /* live-id array for the stale-source prune */
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
  /* ftell returns -1 on a non-seekable stream (a FIFO, /dev/stdin, a process
   * substitution — all of which fopen happily). Unchecked, that became
   * malloc(0) followed by fread(buf, 1, SIZE_MAX, f): an unbounded heap
   * overflow at boot, and a one-byte overwrite even when the read is empty. */
  if (n < 0) { fclose(f); return NULL; }
  char *buf = malloc((size_t)n + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t rd = fread(buf, 1, (size_t)n, f);
  fclose(f);
  buf[rd] = '\0';
  return buf;
}

int db_exec(db_handle *db, const char *sql, char **errmsg) {
  return sqlite3_exec(db->h, sql, NULL, NULL, errmsg) == SQLITE_OK ? 0 : 1;
}

/* ADD COLUMN, but only when it is genuinely absent.
 *
 * SQLite has no "ALTER TABLE ... ADD COLUMN IF NOT EXISTS", and db_open()
 * re-applies schema.sql on every boot, so an unguarded ALTER fails on the
 * second start and would abort boot. Probing PRAGMA table_info first makes
 * the migration idempotent and keeps every added column in one auditable
 * place. `decl` is the type + constraints only (e.g. "TEXT",
 * "INTEGER NOT NULL DEFAULT 0") — not the column name.
 *
 * NOTE the SQLite restriction this quietly respects: a column added with a
 * NOT NULL constraint must carry a non-NULL DEFAULT, otherwise existing rows
 * would violate it and the ALTER is rejected. */
void ensure_column(db_handle *db, const char *table, const char *col,
                   const char *decl) {
  if (!db || !db->h || !table || !col || !decl) return;
  char q[256];
  snprintf(q, sizeof q, "PRAGMA table_info(%s)", table);
  sqlite3_stmt *st;
  int found = 0, any = 0;
  if (sqlite3_prepare_v2(db->h, q, -1, &st, NULL) != SQLITE_OK) return;
  while (sqlite3_step(st) == SQLITE_ROW) {
    any = 1;
    const unsigned char *n = sqlite3_column_text(st, 1);
    if (n && strcmp((const char *)n, col) == 0) { found = 1; break; }
  }
  sqlite3_finalize(st);
  /* No rows at all => the table does not exist yet. Adding a column to a
   * missing table is not an error we should paper over, so skip silently and
   * let whoever owns the CREATE handle it. */
  if (!any || found) return;

  char sql[512];
  snprintf(sql, sizeof sql, "ALTER TABLE %s ADD COLUMN %s %s", table, col, decl);
  char *err = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "[db] ensure_column %s.%s failed: %s\n",
            table, col, err ? err : "?");
    sqlite3_free(err);
  } else {
    fprintf(stderr, "[db] migrated: added %s.%s\n", table, col);
  }
}

/* Seed the sources table from the runtime registry so EVERY registered source
 * (Japan map collector and OSINT-SaaS service alike) has a row and therefore
 * appears in /api/status, /api/sources, /api/keys with full metadata. Pulls
 * type/category/url/name from the merged src_meta_get() (curated table, else
 * synthesized from the source_def). Runs after the REGISTER_SOURCE
 * constructors (which fire before main), so the registry is fully populated.
 *
 * Idempotent, and on conflict it REFRESHES the four registry-derived columns
 * (name/type/category/url) while leaving every operational column —
 * status, last_check/last_success, probe_consent, schedule_mode — untouched.
 * It used to be ON CONFLICT DO NOTHING, which meant a row seeded once was
 * frozen forever: correcting a source's metadata in the curated table changed
 * nothing on any existing install. That bit us with the camera collectors,
 * whose rows had been seeded from synthesized defaults (category
 * "investigation", url NULL) and stayed that way after the curated rows were
 * re-pointed onto their real ids. Those four columns have exactly one author —
 * this function — so refreshing them can't clobber user state. `url` is
 * COALESCEd so a metadata row without a url never blanks a seeded one. */
static void db_seed_sources(db_handle *db) {
  const source_def **a = registry_all();
  int n = registry_count();
  static const char *SQL =
    "INSERT INTO sources (id,name,type,category,url,status) "
    "VALUES (?1,?2,?3,?4,?5,'pending') "
    "ON CONFLICT(id) DO UPDATE SET "
    "  name=excluded.name, type=excluded.type, category=excluded.category, "
    "  url=COALESCE(excluded.url, sources.url) "
    " WHERE sources.name     IS NOT excluded.name "
    "    OR sources.type     IS NOT excluded.type "
    "    OR sources.category IS NOT excluded.category "
    "    OR sources.url      IS NOT COALESCE(excluded.url, sources.url)";
  /* Counting inserts separately from refreshes: sqlite3_changes() reports 1
   * for both, so ask the table whether the row existed before the step. */
  static const char *EXISTS_SQL = "SELECT 1 FROM sources WHERE id=?1";
  sqlite3_stmt *s, *ex;
  if (sqlite3_prepare_v2(db->h, SQL, -1, &s, NULL) != SQLITE_OK) return;
  if (sqlite3_prepare_v2(db->h, EXISTS_SQL, -1, &ex, NULL) != SQLITE_OK) {
    sqlite3_finalize(s); return;
  }
  sqlite3_exec(db->h, "BEGIN", NULL, NULL, NULL);
  int added = 0, refreshed = 0;
  for (int i = 0; i < n; i++) {
    const source_def *d = a[i];
    const src_meta *m = src_meta_get(d->id);
    const char *type = (m && m->type)     ? m->type     : "api";
    const char *cat  = (m && m->category) ? m->category : "investigation";
    const char *url  = (m && m->url)      ? m->url      : NULL;
    const char *name = (m && m->name)     ? m->name
                       : (d->name ? d->name : d->id);
    sqlite3_reset(ex);
    sqlite3_bind_text(ex, 1, d->id, -1, SQLITE_TRANSIENT);
    int existed = sqlite3_step(ex) == SQLITE_ROW;
    sqlite3_reset(ex);

    sqlite3_bind_text(s, 1, d->id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, name,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, type,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, cat,   -1, SQLITE_TRANSIENT);
    if (url) sqlite3_bind_text(s, 5, url, -1, SQLITE_TRANSIENT);
    else     sqlite3_bind_null(s, 5);
    if (sqlite3_step(s) == SQLITE_DONE) {
      int ch = sqlite3_changes(db->h);
      if (ch) { if (existed) refreshed++; else added++; }
    }
    sqlite3_reset(s);
    sqlite3_clear_bindings(s);
  }
  sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL);
  sqlite3_finalize(s);
  sqlite3_finalize(ex);

  /* Prune rows for sources that no longer exist.
   *
   * This only ever inserted and refreshed, so a database seeded before a
   * collector was retired keeps that collector's row forever. Once the
   * curated metadata row goes too, the API serves it with a null nameJa /
   * description / free / layer — a source that cannot be run, cannot be
   * explained, and is indistinguishable from a live one in the dashboard.
   *
   * Deletion is guarded rather than unconditional. `foreign_keys=ON` and four
   * tables reference sources(id) with no ON DELETE clause, so a row with
   * history would abort the statement; more importantly that history is worth
   * keeping — a retired source's fetch_log is still the record of what it did.
   * So: drop only the rows nothing refers to, and report the rest rather than
   * failing silently. */
  static const char *PRUNE_SQL =
    "DELETE FROM sources WHERE id NOT IN (SELECT value FROM json_each(?1)) "
    "  AND NOT EXISTS (SELECT 1 FROM fetch_log            f WHERE f.source_id = sources.id) "
    "  AND NOT EXISTS (SELECT 1 FROM collector_anomaly    c WHERE c.source_id = sources.id) "
    "  AND NOT EXISTS (SELECT 1 FROM collector_repair     r WHERE r.source_id = sources.id) "
    "  AND NOT EXISTS (SELECT 1 FROM collector_url_overrides o WHERE o.source_id = sources.id)";
  static const char *KEPT_SQL =
    "SELECT COUNT(*) FROM sources WHERE id NOT IN (SELECT value FROM json_each(?1))";

  /* An empty registry is never evidence that every source was retired — it is
   * evidence that this BINARY does not link the collectors (a unit-test or
   * sanitiser build, which resolves JO_DB to the same default path as the
   * server). With n == 0 the live-id array is `[]` and the DELETE below matches
   * every history-free row, i.e. it would silently empty `sources` on a freshly
   * seeded install. Prune only when we actually know what is live. */
  if (n <= 0) return;

  cJSON *ids = cJSON_CreateArray();
  for (int i = 0; i < n; i++)
    cJSON_AddItemToArray(ids, cJSON_CreateString(a[i]->id));
  char *ids_json = cJSON_PrintUnformatted(ids);
  cJSON_Delete(ids);
  if (!ids_json) return;

  int pruned = 0, kept = 0;
  sqlite3_stmt *p = NULL;
  if (sqlite3_prepare_v2(db->h, PRUNE_SQL, -1, &p, NULL) == SQLITE_OK) {
    sqlite3_bind_text(p, 1, ids_json, -1, SQLITE_STATIC);
    if (sqlite3_step(p) == SQLITE_DONE) pruned = sqlite3_changes(db->h);
    sqlite3_finalize(p);
  }
  if (sqlite3_prepare_v2(db->h, KEPT_SQL, -1, &p, NULL) == SQLITE_OK) {
    sqlite3_bind_text(p, 1, ids_json, -1, SQLITE_STATIC);
    if (sqlite3_step(p) == SQLITE_ROW) kept = sqlite3_column_int(p, 0);
    sqlite3_finalize(p);
  }
  free(ids_json);

  fprintf(stderr,
          "[db] seeded sources from registry (%d new, %d metadata-refreshed",
          added, refreshed);
  if (pruned || kept)
    fprintf(stderr, ", %d stale pruned, %d stale kept for their history",
            pruned, kept);
  fprintf(stderr, ")\n");
}

/* The pragmas every connection to this database must carry.
 *
 * cache_size/mmap_size are here and not only on the primary handle because
 * they are per-CONNECTION, and the connections that do the heavy reading are
 * the attached ones (scheduler workers, dispatch pool, the background pods).
 * The measured effect on /api/intel/items was 35.1 s -> 0.002 s together with
 * idx_intel_items_pub (schema.sql): the index removes the sort, these remove
 * the page churn underneath it.
 *
 *   cache_size=-65536  → 64 MiB of page cache (negative = KiB, not pages, so
 *                        it does not change meaning with the page size).
 *   mmap_size=256MiB   → read the DB through the page cache instead of
 *                        copying every page via pread. Advisory: SQLite
 *                        silently ignores it where mmap is unavailable.
 * Both are ceilings, not reservations. */
static void db_apply_pragmas(sqlite3 *h) {
  sqlite3_exec(h, "PRAGMA journal_mode=WAL;",  NULL, NULL, NULL);
  sqlite3_exec(h, "PRAGMA foreign_keys=ON;",   NULL, NULL, NULL);
  sqlite3_exec(h, "PRAGMA busy_timeout=5000;", NULL, NULL, NULL);
  sqlite3_exec(h, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
  sqlite3_exec(h, "PRAGMA cache_size=-65536;", NULL, NULL, NULL);
  sqlite3_exec(h, "PRAGMA mmap_size=268435456;", NULL, NULL, NULL);
}

/* Secondary connection — see db_attach() in db.h. Deliberately does NOT apply
 * schema.sql or the boot migrations: those ran on the primary handle at boot,
 * and re-running them from a worker thread while the event loop is serving
 * requests means concurrent DDL on a live WAL database for no gain. */
int db_attach(db_handle *db, const char *db_path) {
  if (!db) return 1;
  const char *dbp = db_path ? db_path
    : (getenv("JO_DB") ? getenv("JO_DB") : JO_REPO_ROOT "/data/japanmap.db");
  int rc = sqlite3_open_v2(dbp, &db->h, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[db] attach failed: %s\n", sqlite3_errmsg(db->h));
    sqlite3_close(db->h);
    db->h = NULL;
    return 1;
  }
  /* The same set as the primary handle — these are per-connection, and this
   * is the connection kind that does the bulk of the reading. */
  db_apply_pragmas(db->h);
  return 0;
}

/* See db.h. Deliberately falls back rather than failing: an off-loop pod that
 * cannot get its own connection should run slightly unsafely and log it, not
 * silently do nothing (which is what a hard failure would look like from the
 * outside — an investigation that "found nothing"). */
db_handle *db_worker_open(db_handle *own, db_handle *fallback) {
  if (!own) return fallback;
  own->h = NULL;
  if (db_attach(own, NULL) == 0) return own;
  fprintf(stderr, "[db] worker connection failed; falling back to the shared "
                  "handle (its transactions can now interleave)\n");
  return fallback;
}

void db_worker_close(db_handle *own) {
  if (own && own->h) db_close(own);
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
  db_apply_pragmas(db->h);

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
   * columns are added here, not in the generated schema.
   *
   * SQLite has no "ADD COLUMN IF NOT EXISTS", and schema.sql re-runs on every
   * boot, so a bare ALTER would error on the second start. ensure_column()
   * probes table_info first — this is THE way to add a column in this
   * codebase; do not append it to the generated schema.sql instead, because
   * CREATE TABLE IF NOT EXISTS silently skips an existing table and the
   * column would never appear on any deployed database. */
  ensure_column(db, "sources", "schedule_mode",
    "TEXT NOT NULL DEFAULT 'map_cron' "
    "CHECK(schedule_mode IN ('map_cron','search_only'))");

  /* Notification inbox (roadmap item 11): read/unread state per event. */
  ensure_column(db, "alert_events", "read_at", "TEXT");

  /* Correlation scoring (roadmap item 22): significance over co-mention
   * edges. NULL until the stats pod has run, and NULL for pairs below the
   * support floor — a missing score is "not enough evidence", not zero. */
  ensure_column(db, "entity_relationships", "pmi",      "REAL");
  ensure_column(db, "entity_relationships", "lift",     "REAL");
  ensure_column(db, "entity_relationships", "co_count", "INTEGER");
  ensure_column(db, "entity_relationships", "stats_at", "TEXT");

  /* Breach exposure monitoring (roadmap item 24): the email host, so a domain
   * monitor is an index seek instead of an unindexable LIKE '%@host' scan over
   * the whole corpus. The INDEX on this column is created by
   * breach_monitor_migrate(), not schema.sql — schema.sql runs above this
   * block, so it would reference a column that does not exist yet. */
  ensure_column(db, "breach_items", "value_domain", "TEXT");

  /* Evidence capture (roadmap 17) is per-source OPT-IN and defaults OFF. 415
   * sources on schedules down to 60s would fill a disk in days otherwise, and
   * the hot-path check fails closed if this column is missing. */
  ensure_column(db, "sources", "capture_evidence", "INTEGER NOT NULL DEFAULT 0");

  /* Translation (roadmap 29) owns its own migration: its pending-scan partial
   * index references title_en/translated_at/translate_failed, which do not
   * exist until its own ensure_column calls have run. Putting that index in
   * schema.sql (which executes above this block) would fail with "no such
   * column", abandon the rest of the script, and brick first boot. */
  translate_migrate(db);

  /* Near-duplicate clustering (roadmap 25) likewise owns its own migration:
   * idx_intel_items_cluster and its partial backfill index both reference
   * columns added below, so they cannot live in schema.sql either. */
  simhash_ensure_schema(db);

  /* Content change detection (roadmap 26). watch_content is tri-state:
   * NULL = use the source type's default, 0 = never, 1 = always. Its index is
   * created inside content_change_ensure_schema() for the same reason as
   * above — the column does not exist when schema.sql runs. */
  ensure_column(db, "sources", "watch_content", "INTEGER");
  content_change_ensure_schema(db);

  /* Media analysis (roadmap 27). media_migrate() owns the rest, including the
   * one index that references the column added just above. */
  ensure_column(db, "intel_items", "media_scanned_at", "TEXT");
  media_migrate(db);

  /* Camera stills (roadmap 28). Cameras are intel_items rows
   * ("camera-discovery|<uid>") — there is no separate cameras table — so the
   * opt-in flag lives there. camera_stills_migrate() owns the partial index
   * over it, for the usual schema.sql-ordering reason. */
  ensure_column(db, "intel_items", "capture_stills", "INTEGER NOT NULL DEFAULT 0");
  camera_stills_migrate(db);

  /* Widen intel_items_fts to also index link/author/tags/properties. No-op
   * unless the live index is still the old column set. Deliberately LAST of
   * the migrations: it rewinds translate.c's FTS watermark (so translate_state
   * must already exist) and recreates the uid_map rowid index that
   * translate_migrate() created. It is also the only migration here that can
   * take minutes on a large corpus — everything cheap has already run, so a
   * JO_FTS_REBUILD=0 boot skips only this. */
  fts_schema_migrate(db);

  /* Every registered source gets a sources-table row (idempotent). */
  db_seed_sources(db);

  /* Give the planner statistics to choose between the intel_items indexes.
   * PRAGMA optimize, not a bare ANALYZE: it re-analyses only tables whose
   * stats are actually stale, and analysis_limit caps the scan per index so
   * boot cost stays bounded on a large corpus instead of growing with it. */
  sqlite3_exec(db->h, "PRAGMA analysis_limit=1000;", NULL, NULL, NULL);
  sqlite3_exec(db->h, "PRAGMA optimize;", NULL, NULL, NULL);

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
