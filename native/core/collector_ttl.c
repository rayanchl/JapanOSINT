/* core/collector_ttl.c — see collector_ttl.h. */
#include "collector_ttl.h"
#include "audit.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* A source id longer than this is not one (registry ids are short); refusing
 * it before the lookup keeps an attacker-sized path out of the audit row. */
#define CTTL_KEY_MAX 255

static long long now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int has_column(db_handle *db, const char *table, const char *col) {
  char sql[128];
  snprintf(sql, sizeof sql, "PRAGMA table_info(%s)", table);
  sqlite3_stmt *s; int found = 0;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const unsigned char *n = sqlite3_column_text(s, 1);
    if (n && !strcmp((const char *)n, col)) { found = 1; break; }
  }
  sqlite3_finalize(s);
  return found;
}

void collector_ttl_migrate(db_handle *db) {
  if (!db || !db->h) return;
  /* The same declaration as core/schema.sql, for a database that predates it
   * or was created without it; a no-op everywhere else. */
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS collector_ttls ("
    " key TEXT PRIMARY KEY, ttl_ms INTEGER NOT NULL,"
    " source TEXT NOT NULL, updated_at INTEGER NOT NULL)", NULL, NULL, NULL);
  /* Who set it, beside when — the audit chain has it too, but the admin page
   * should not need a chain walk to say who chose a number. NULL on rows the
   * Node side wrote: nobody recorded it, so nothing is claimed. */
  if (!has_column(db, "collector_ttls", "updated_by"))
    sqlite3_exec(db->h, "ALTER TABLE collector_ttls ADD COLUMN updated_by TEXT",
                 NULL, NULL, NULL);
}

static char *err_body(const char *error, const char *detail) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", error);
  if (detail) cJSON_AddStringToObject(o, "detail", detail);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

static char *print_root(cJSON *root) {
  char *s = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return s;
}

static int key_ok(const char *key) {
  return key && *key && strlen(key) <= CTTL_KEY_MAX;
}

/* One row as JSON, or NULL when the key has no row. */
static cJSON *row_json(db_handle *db, const char *key) {
  sqlite3_stmt *s; cJSON *o = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT key, ttl_ms, source, updated_at, updated_by FROM collector_ttls "
        "WHERE key=?1", -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    o = cJSON_CreateObject();
    const char *k = (const char *)sqlite3_column_text(s, 0);
    cJSON_AddStringToObject(o, "key", k ? k : key);
    cJSON_AddNumberToObject(o, "ttl_ms", (double)sqlite3_column_int64(s, 1));
    const char *src = (const char *)sqlite3_column_text(s, 2);
    cJSON_AddStringToObject(o, "source", src ? src : "");
    if (sqlite3_column_type(s, 3) == SQLITE_NULL) cJSON_AddNullToObject(o, "updated_at");
    else cJSON_AddNumberToObject(o, "updated_at", (double)sqlite3_column_int64(s, 3));
    const char *by = (const char *)sqlite3_column_text(s, 4);
    if (by) cJSON_AddStringToObject(o, "updated_by", by);
    else    cJSON_AddNullToObject(o, "updated_by");
    cJSON_AddBoolToObject(o, "registered", registry_get(k ? k : key) != NULL);
    cJSON_AddBoolToObject(o, "in_range",
      sqlite3_column_int64(s, 1) >= COLLECTOR_TTL_MIN_MS &&
      sqlite3_column_int64(s, 1) <= COLLECTOR_TTL_MAX_MS);
  }
  sqlite3_finalize(s);
  return o;
}

/* What the readers resolve for `key` with no row: the default. */
static cJSON *default_json(const char *key) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "key", key);
  cJSON_AddNumberToObject(o, "ttl_ms", (double)COLLECTOR_TTL_DEFAULT_MS);
  cJSON_AddStringToObject(o, "source", "default");
  cJSON_AddNullToObject(o, "updated_at");
  cJSON_AddNullToObject(o, "updated_by");
  cJSON_AddBoolToObject(o, "registered", registry_get(key) != NULL);
  cJSON_AddBoolToObject(o, "in_range", 1);
  return o;
}

static void add_meta(cJSON *root) {
  cJSON *m = cJSON_AddObjectToObject(root, "meta");
  cJSON_AddNumberToObject(m, "default_ttl_ms", (double)COLLECTOR_TTL_DEFAULT_MS);
  cJSON_AddNumberToObject(m, "min_ttl_ms", (double)COLLECTOR_TTL_MIN_MS);
  cJSON_AddNumberToObject(m, "max_ttl_ms", (double)COLLECTOR_TTL_MAX_MS);
  cJSON_AddStringToObject(m, "key", "source id (registry)");
  cJSON_AddStringToObject(m, "applies",
    "the source list's ttl_ms at once; the /api/data cache freshness from the "
    "source's next cache write, which stamps the TTL it was written with");
}

char *collector_ttl_api_list(db_handle *db, int *status) {
  collector_ttl_migrate(db);
  cJSON *root = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(root, "data");
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT key FROM collector_ttls ORDER BY key",
                         -1, &s, NULL) != SQLITE_OK) {
    cJSON_Delete(root);
    *status = 500;
    return err_body("server_error", NULL);
  }
  /* Every stored row, including one whose source is no longer registered:
   * hiding it would hide a row the readers still answer for that id. */
  int n = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *k = (const char *)sqlite3_column_text(s, 0);
    if (!k) continue;
    cJSON *o = row_json(db, k);
    if (o) { cJSON_AddItemToArray(arr, o); n++; }
  }
  sqlite3_finalize(s);
  add_meta(root);
  cJSON_AddNumberToObject(cJSON_GetObjectItem(root, "meta"), "count", n);
  *status = 200;
  return print_root(root);
}

char *collector_ttl_api_get(db_handle *db, const char *key, int *status) {
  if (!key_ok(key)) { *status = 400; return err_body("bad_request", "invalid key"); }
  collector_ttl_migrate(db);
  cJSON *o = row_json(db, key);
  if (!o) {
    if (!registry_get(key)) { *status = 404; return err_body("unknown_source", key); }
    o = default_json(key);
  }
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", o);
  add_meta(root);
  *status = 200;
  return print_root(root);
}

/* The current ttl for the audit row: the stored value, else the default. */
static long long current_ttl(db_handle *db, const char *key, int *had_row) {
  sqlite3_stmt *s; long long v = COLLECTOR_TTL_DEFAULT_MS; *had_row = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT ttl_ms FROM collector_ttls WHERE key=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) { v = sqlite3_column_int64(s, 0); *had_row = 1; }
    sqlite3_finalize(s);
  }
  return v;
}

static void audit_change(db_handle *db, const char *user_id, const char *action,
                         const char *key, long long old_v, int old_row,
                         long long new_v, int new_row) {
  cJSON *ap = cJSON_CreateObject();
  cJSON_AddStringToObject(ap, "key", key);
  cJSON_AddNumberToObject(ap, "old_ttl_ms", (double)old_v);
  cJSON_AddStringToObject(ap, "old_source", old_row ? "user" : "default");
  cJSON_AddNumberToObject(ap, "new_ttl_ms", (double)new_v);
  cJSON_AddStringToObject(ap, "new_source", new_row ? "user" : "default");
  char *pj = cJSON_PrintUnformatted(ap);
  cJSON_Delete(ap);
  audit_write(db, "platform", user_id, action, key, pj ? pj : "{}");
  free(pj);
}

char *collector_ttl_api_put(db_handle *db, const char *key, const char *body,
                            size_t body_len, const char *user_id, int *status) {
  if (!key_ok(key)) { *status = 400; return err_body("bad_request", "invalid key"); }
  if (!registry_get(key)) { *status = 404; return err_body("unknown_source", key); }
  cJSON *jb = (body && body_len) ? cJSON_ParseWithLength(body, body_len) : NULL;
  const cJSON *jv = cJSON_IsObject(jb) ? cJSON_GetObjectItem(jb, "ttl_ms") : NULL;
  if (!jv || !cJSON_IsNumber(jv)) {
    cJSON_Delete(jb);
    *status = 400;
    return err_body("bad_request", "body must be {\"ttl_ms\": integer}");
  }
  double d = jv->valuedouble;
  cJSON_Delete(jb);
  /* An integer in [min,max]; 1.5 ms or 1e300 is refused, never rounded. */
  if (!isfinite(d) || d != floor(d) ||
      d < (double)COLLECTOR_TTL_MIN_MS || d > (double)COLLECTOR_TTL_MAX_MS) {
    char det[160];
    snprintf(det, sizeof det, "ttl_ms must be an integer in [%lld, %lld]",
             COLLECTOR_TTL_MIN_MS, COLLECTOR_TTL_MAX_MS);
    *status = 400;
    return err_body("out_of_range", det);
  }
  long long v = (long long)d;
  collector_ttl_migrate(db);
  int old_row; long long old_v = current_ttl(db, key, &old_row);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO collector_ttls(key, ttl_ms, source, updated_at, updated_by) "
        "VALUES(?1, ?2, 'user', ?3, ?4) "
        "ON CONFLICT(key) DO UPDATE SET ttl_ms=excluded.ttl_ms, "
        "source=excluded.source, updated_at=excluded.updated_at, "
        "updated_by=excluded.updated_by", -1, &s, NULL) != SQLITE_OK) {
    *status = 500;
    return err_body("server_error", NULL);
  }
  sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(s, 2, v);
  sqlite3_bind_int64(s, 3, now_ms());
  if (user_id && *user_id) sqlite3_bind_text(s, 4, user_id, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, 4);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) { *status = 500; return err_body("server_error", NULL); }
  audit_change(db, user_id, "collector_ttl.set", key, old_v, old_row, v, 1);
  return collector_ttl_api_get(db, key, status);
}

char *collector_ttl_api_delete(db_handle *db, const char *key,
                               const char *user_id, int *status) {
  if (!key_ok(key)) { *status = 400; return err_body("bad_request", "invalid key"); }
  collector_ttl_migrate(db);
  int old_row; long long old_v = current_ttl(db, key, &old_row);
  /* A key with no row has nothing to reset; saying 200 would audit a change
   * that did not happen. An unregistered key WITH a row may be deleted: that
   * is how a stale row left by a removed source is cleared. */
  if (!old_row) { *status = 404; return err_body("no_override", key); }
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "DELETE FROM collector_ttls WHERE key=?1",
                         -1, &s, NULL) != SQLITE_OK) {
    *status = 500;
    return err_body("server_error", NULL);
  }
  sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) { *status = 500; return err_body("server_error", NULL); }
  audit_change(db, user_id, "collector_ttl.reset", key, old_v, 1,
               COLLECTOR_TTL_DEFAULT_MS, 0);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", default_json(key));
  add_meta(root);
  *status = 200;
  return print_root(root);
}
