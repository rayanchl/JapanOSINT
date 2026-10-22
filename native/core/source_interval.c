/* core/source_interval.c — see source_interval.h. */
#include "source_interval.h"
#include "cadence.h"
#include "scheduler.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


typedef struct { char id[160]; long sec; } si_entry;

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static si_entry       *g_cache;      /* sorted by id, ENABLED rows only */
static int             g_n;
static atomic_int      g_n_atomic;   /* fast path: nothing set → no lock   */
static atomic_uint     g_gen;

static int cmp_entry(const void *a, const void *b) {
  return strcmp(((const si_entry *)a)->id, ((const si_entry *)b)->id);
}
static int cmp_key(const void *k, const void *e) {
  return strcmp((const char *)k, ((const si_entry *)e)->id);
}

static void iso_now(char *out, size_t n) {
  time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

static int ensure_table(db_handle *db) {
  return db_exec(db,
    "CREATE TABLE IF NOT EXISTS source_interval_override("
    " source_id TEXT PRIMARY KEY, interval_sec INTEGER NOT NULL,"
    " enabled INTEGER NOT NULL DEFAULT 1,"
    " updated_at TEXT NOT NULL, updated_by TEXT)", NULL);
}

/* Rebuild the cache from the database: the whole table is small (an
 * operator sets these one source at a time), so a reload on every change is
 * simpler than keeping two copies in step. */
static void reload(db_handle *db) {
  sqlite3_stmt *s = NULL;
  si_entry *rows = NULL; int n = 0, cap = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT source_id, interval_sec FROM source_interval_override "
        "WHERE enabled=1 AND interval_sec>0", -1, &s, NULL) == SQLITE_OK) {
    while (sqlite3_step(s) == SQLITE_ROW) {
      const unsigned char *id = sqlite3_column_text(s, 0);
      if (!id) continue;
      if (n == cap) {
        int nc = cap ? cap * 2 : 16;
        si_entry *nr = realloc(rows, (size_t)nc * sizeof *nr);
        if (!nr) break;
        rows = nr; cap = nc;
      }
      snprintf(rows[n].id, sizeof rows[n].id, "%s", (const char *)id);
      rows[n].sec = (long)sqlite3_column_int64(s, 1);
      n++;
    }
    sqlite3_finalize(s);
  }
  if (n > 1) qsort(rows, (size_t)n, sizeof *rows, cmp_entry);
  pthread_mutex_lock(&g_mu);
  free(g_cache); g_cache = rows; g_n = n;
  atomic_store(&g_n_atomic, n);
  pthread_mutex_unlock(&g_mu);
}

void source_interval_init(db_handle *db) {
  if (!db || ensure_table(db) != 0) return;
  reload(db);
  int n = atomic_load(&g_n_atomic);
  if (n) fprintf(stderr, "[interval] %d per-source interval override(s) loaded\n", n);
}

long source_interval_for(const char *id) {
  if (!id || atomic_load(&g_n_atomic) == 0) return 0;
  long v = 0;
  pthread_mutex_lock(&g_mu);
  if (g_cache) {
    const si_entry *e = bsearch(id, g_cache, (size_t)g_n, sizeof *g_cache, cmp_key);
    if (e) v = e->sec;
  }
  pthread_mutex_unlock(&g_mu);
  return v;
}

int source_interval_count(void) { return atomic_load(&g_n_atomic); }
unsigned source_interval_generation(void) { return atomic_load(&g_gen); }

int source_interval_get(db_handle *db, const char *id, source_interval *out) {
  memset(out, 0, sizeof *out);
  if (!db || !id) return 0;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT interval_sec, enabled, updated_at, updated_by "
        "FROM source_interval_override WHERE source_id=?1", -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    out->present = 1;
    out->interval_sec = (long)sqlite3_column_int64(s, 0);
    out->enabled = sqlite3_column_int(s, 1) != 0;
    const unsigned char *a = sqlite3_column_text(s, 2), *b = sqlite3_column_text(s, 3);
    if (a) snprintf(out->updated_at, sizeof out->updated_at, "%s", (const char *)a);
    if (b) snprintf(out->updated_by, sizeof out->updated_by, "%s", (const char *)b);
  }
  sqlite3_finalize(s);
  return out->present;
}

int source_interval_set(db_handle *db, const char *id, long interval_sec,
                        int enabled, const char *who, const char **err) {
  const char *e = NULL;
  const source_def *d = id ? registry_get(id) : NULL;
  if (!db)                              e = "no database";
  else if (!d)                          e = "no registered source has this id";
  else if (d->update_interval_sec <= 0) e = "this source has no schedule to override: it runs when asked (an entity pivot)";
  else if (interval_sec < SI_MIN_SEC)   e = "an interval under 10 s would run this source continuously";
  else if (interval_sec > SI_MAX_SEC)   e = "an interval longer than a year is a typo";
  if (e) { if (err) *err = e; return -1; }
  if (ensure_table(db) != 0) { if (err) *err = "could not create source_interval_override"; return -1; }
  char at[32]; iso_now(at, sizeof at);
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO source_interval_override(source_id,interval_sec,enabled,updated_at,updated_by) "
        "VALUES(?1,?2,?3,?4,?5) ON CONFLICT(source_id) DO UPDATE SET interval_sec=excluded.interval_sec, "
        "enabled=excluded.enabled, updated_at=excluded.updated_at, updated_by=excluded.updated_by",
        -1, &s, NULL) != SQLITE_OK) { if (err) *err = "could not prepare the write"; return -1; }
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(s, 2, (sqlite3_int64)interval_sec);
  sqlite3_bind_int(s, 3, enabled ? 1 : 0);
  sqlite3_bind_text(s, 4, at, -1, SQLITE_TRANSIENT);
  if (who && *who) sqlite3_bind_text(s, 5, who, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 5);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) { if (err) *err = "the write failed"; return -1; }
  reload(db);
  atomic_fetch_add(&g_gen, 1);
  fprintf(stderr, "[interval] %s: %s every %ld s%s%s\n", id, enabled ? "set" : "stored (disabled)",
          interval_sec, who && *who ? " by " : "", who && *who ? who : "");
  return 0;
}

int source_interval_clear(db_handle *db, const char *id, const char **err) {
  if (!db || !id) { if (err) *err = "no database"; return -1; }
  if (ensure_table(db) != 0) { if (err) *err = "could not create source_interval_override"; return -1; }
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, "DELETE FROM source_interval_override WHERE source_id=?1", -1, &s, NULL) != SQLITE_OK) {
    if (err) *err = "could not prepare the delete";
    return -1;
  }
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) { if (err) *err = "the delete failed"; return -1; }
  reload(db);
  atomic_fetch_add(&g_gen, 1);
  fprintf(stderr, "[interval] %s: override cleared\n", id);
  return 0;
}

char *source_interval_json(db_handle *db, const char *id, int can_write) {
  const source_def *d = id ? registry_get(id) : NULL;
  if (!d) return NULL;
  source_interval ov; source_interval_get(db, id, &ov);
  sched_state st = {0};
  if (db) sched_state_load_one(db, id, &st);
  long declared = d->update_interval_sec;
  long measured = (d->adaptive_interval && st.freshness_interval > 0) ? st.freshness_interval : 0;
  long base     = sched_state_base(&st, declared, d->adaptive_interval);
  long policy   = cadence_apply(base);
  long eff      = sched_row_interval(&st, d);
  const char *basis = declared <= 0 ? "on demand"
                    : (ov.present && ov.enabled && ov.interval_sec > 0) ? "override"
                    : strcmp(cadence_mode_for(base), "declared") ? "policy"
                    : measured > 0 ? "measured" : "declared";
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "source", id);
  cJSON_AddBoolToObject(o, "scheduled", declared > 0);
  cJSON_AddNumberToObject(o, "declared_interval_sec", (double)declared);
  if (measured > 0) cJSON_AddNumberToObject(o, "measured_interval_sec", (double)measured);
  else cJSON_AddNullToObject(o, "measured_interval_sec");
  cJSON_AddNumberToObject(o, "policy_interval_sec", (double)policy);
  cJSON_AddStringToObject(o, "policy", cadence_mode_for(base));
  if (ov.present) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "interval_sec", (double)ov.interval_sec);
    cJSON_AddBoolToObject(j, "enabled", ov.enabled);
    cJSON_AddStringToObject(j, "updated_at", ov.updated_at);
    if (ov.updated_by[0]) cJSON_AddStringToObject(j, "updated_by", ov.updated_by);
    else cJSON_AddNullToObject(j, "updated_by");
    cJSON_AddItemToObject(o, "override", j);
  } else cJSON_AddNullToObject(o, "override");
  cJSON_AddNumberToObject(o, "effective_interval_sec", (double)eff);
  cJSON_AddStringToObject(o, "effective_basis", basis);
  cJSON_AddNumberToObject(o, "min_interval_sec", (double)SI_MIN_SEC);
  cJSON_AddNumberToObject(o, "max_interval_sec", (double)SI_MAX_SEC);
  cJSON_AddBoolToObject(o, "can_write", can_write != 0);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}
