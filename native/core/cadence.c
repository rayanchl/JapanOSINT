/* core/cadence.c — see cadence.h. */
#include "cadence.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define CAD_MAX_SEC (366L * 86400L)   /* a year: anything slower is a typo */

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static cadence_policy  g_pol;                 /* zeros = declared */
static const char     *g_src = "default";
static char            g_updated_at[32];
static char            g_updated_by[128];
static atomic_uint     g_gen;

static const char *K_GLOBAL = "sched.global_interval_sec";
static const char *K_MIN    = "sched.min_interval_sec";
static const char *K_MAX    = "sched.max_interval_sec";

static long env_long(const char *name) {
  const char *v = getenv(name);
  if (!v || !*v) return 0;
  char *end = NULL; long n = strtol(v, &end, 10);
  return (end && *end == 0 && n > 0) ? n : 0;
}

static void iso_now(char *out, size_t n) {
  time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

static int ensure_table(db_handle *db) {
  return db_exec(db,
    "CREATE TABLE IF NOT EXISTS app_settings("
    " key TEXT PRIMARY KEY, value TEXT NOT NULL,"
    " updated_at TEXT NOT NULL, updated_by TEXT)", NULL);
}

/* read one key; returns 1 and fills *out when present */
static int read_key(db_handle *db, const char *key, long *out, char *at, size_t atn, char *by, size_t byn) {
  sqlite3_stmt *s = NULL; int hit = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT value, updated_at, updated_by FROM app_settings WHERE key=?1", -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, key, -1, SQLITE_STATIC);
  if (sqlite3_step(s) == SQLITE_ROW) {
    const unsigned char *v = sqlite3_column_text(s, 0);
    *out = v ? strtol((const char *)v, NULL, 10) : 0;
    const unsigned char *a = sqlite3_column_text(s, 1), *b = sqlite3_column_text(s, 2);
    if (at && a) snprintf(at, atn, "%s", (const char *)a);
    if (by && b) snprintf(by, byn, "%s", (const char *)b);
    hit = 1;
  }
  sqlite3_finalize(s);
  return hit;
}

void cadence_init(db_handle *db) {
  cadence_policy p = {0, 0, 0}; const char *src = "default";
  char at[32] = {0}, by[128] = {0};
  if (db && ensure_table(db) == 0) {
    int n = 0;
    n += read_key(db, K_GLOBAL, &p.global_sec, at, sizeof at, by, sizeof by);
    n += read_key(db, K_MIN,    &p.min_sec,    at, sizeof at, by, sizeof by);
    n += read_key(db, K_MAX,    &p.max_sec,    at, sizeof at, by, sizeof by);
    if (n) src = "db";
  }
  if (!strcmp(src, "default")) {
    p.global_sec = env_long("JO_SCHED_INTERVAL_SEC");
    p.min_sec    = env_long("JO_SCHED_MIN_INTERVAL_SEC");
    p.max_sec    = env_long("JO_SCHED_MAX_INTERVAL_SEC");
    if (p.global_sec || p.min_sec || p.max_sec) src = "env";
  }
  pthread_mutex_lock(&g_mu);
  g_pol = p; g_src = src;
  snprintf(g_updated_at, sizeof g_updated_at, "%s", at);
  snprintf(g_updated_by, sizeof g_updated_by, "%s", by);
  pthread_mutex_unlock(&g_mu);
  if (p.global_sec || p.min_sec || p.max_sec)
    fprintf(stderr, "[cadence] policy from %s: global=%ld min=%ld max=%ld\n", src, p.global_sec, p.min_sec, p.max_sec);
}

cadence_policy cadence_get(void) {
  pthread_mutex_lock(&g_mu); cadence_policy p = g_pol; pthread_mutex_unlock(&g_mu);
  return p;
}
const char *cadence_source(void) {
  pthread_mutex_lock(&g_mu); const char *s = g_src; pthread_mutex_unlock(&g_mu);
  return s;
}
unsigned cadence_generation(void) { return atomic_load(&g_gen); }

static long apply(const cadence_policy *p, long declared) {
  if (declared <= 0) return declared;
  if (p->global_sec > 0) return p->global_sec;
  long v = declared;
  if (p->min_sec > 0 && v < p->min_sec) v = p->min_sec;
  if (p->max_sec > 0 && v > p->max_sec) v = p->max_sec;
  return v;
}

long cadence_apply(long declared) {
  cadence_policy p = cadence_get();
  return apply(&p, declared);
}

const char *cadence_mode_for(long declared) {
  if (declared <= 0) return "declared";
  cadence_policy p = cadence_get();
  if (p.global_sec > 0) return "global";
  return apply(&p, declared) != declared ? "clamped" : "declared";
}

static const char *validate(const cadence_policy *p) {
  if (p->global_sec < 0 || p->min_sec < 0 || p->max_sec < 0) return "an interval cannot be negative";
  if (p->global_sec > CAD_MAX_SEC || p->min_sec > CAD_MAX_SEC || p->max_sec > CAD_MAX_SEC) return "an interval longer than a year is a typo";
  if (p->global_sec > 0 && p->global_sec < 10) return "global_interval_sec under 10 s would run every scheduled row continuously";
  if (p->min_sec > 0 && p->max_sec > 0 && p->min_sec > p->max_sec) return "min_interval_sec exceeds max_interval_sec";
  return NULL;
}

static int write_key(db_handle *db, const char *key, long v, const char *at, const char *who) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO app_settings(key,value,updated_at,updated_by) VALUES(?1,?2,?3,?4) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value, updated_at=excluded.updated_at, updated_by=excluded.updated_by",
        -1, &s, NULL) != SQLITE_OK) return -1;
  char buf[32]; snprintf(buf, sizeof buf, "%ld", v);
  sqlite3_bind_text(s, 1, key, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, buf, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, at, -1, SQLITE_STATIC);
  if (who && *who) sqlite3_bind_text(s, 4, who, -1, SQLITE_STATIC); else sqlite3_bind_null(s, 4);
  int rc = sqlite3_step(s) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(s);
  return rc;
}

int cadence_set(db_handle *db, const cadence_policy *p, const char *who, const char **err) {
  const char *e = validate(p);
  if (e) { if (err) *err = e; return -1; }
  if (!db || ensure_table(db) != 0) { if (err) *err = "settings table unavailable"; return -1; }
  char at[32]; iso_now(at, sizeof at);
  if (db_exec(db, "BEGIN", NULL) != 0) { if (err) *err = "database busy"; return -1; }
  if (write_key(db, K_GLOBAL, p->global_sec, at, who) || write_key(db, K_MIN, p->min_sec, at, who) ||
      write_key(db, K_MAX, p->max_sec, at, who) || db_exec(db, "COMMIT", NULL) != 0) {
    db_exec(db, "ROLLBACK", NULL);
    if (err) *err = "could not persist the policy";
    return -1;
  }
  pthread_mutex_lock(&g_mu);
  g_pol = *p; g_src = "db";
  snprintf(g_updated_at, sizeof g_updated_at, "%s", at);
  snprintf(g_updated_by, sizeof g_updated_by, "%s", who ? who : "");
  pthread_mutex_unlock(&g_mu);
  atomic_fetch_add(&g_gen, 1);
  fprintf(stderr, "[cadence] policy set by %s: global=%ld min=%ld max=%ld\n", who && *who ? who : "?", p->global_sec, p->min_sec, p->max_sec);
  return 0;
}

static int take(cJSON *j, const char *k, long *out, const char **err) {
  cJSON *v = cJSON_GetObjectItemCaseSensitive(j, k);
  if (!v || cJSON_IsNull(v)) return 0;
  if (!cJSON_IsNumber(v)) { if (err) *err = "intervals are numbers of seconds"; return -1; }
  *out = (long)v->valuedouble;
  return 0;
}

int cadence_parse(const char *json, const cadence_policy *base, cadence_policy *out, const char **err) {
  cJSON *j = json ? cJSON_Parse(json) : NULL;
  if (!j || !cJSON_IsObject(j)) { cJSON_Delete(j); if (err) *err = "body must be a JSON object"; return -1; }
  cadence_policy p = base ? *base : (cadence_policy){0, 0, 0};
  int rc = take(j, "global_interval_sec", &p.global_sec, err) | take(j, "min_interval_sec", &p.min_sec, err) |
           take(j, "max_interval_sec", &p.max_sec, err);
  cJSON_Delete(j);
  if (rc) return -1;
  *out = p;
  return 0;
}

char *cadence_json(db_handle *db) {
  (void)db;
  cadence_policy p = cadence_get();
  cJSON *o = cJSON_CreateObject();
  cJSON *pj = cJSON_CreateObject();
  cJSON_AddNumberToObject(pj, "global_interval_sec", (double)p.global_sec);
  cJSON_AddNumberToObject(pj, "min_interval_sec", (double)p.min_sec);
  cJSON_AddNumberToObject(pj, "max_interval_sec", (double)p.max_sec);
  cJSON_AddStringToObject(pj, "mode", p.global_sec > 0 ? "global" : (p.min_sec > 0 || p.max_sec > 0) ? "clamped" : "declared");
  cJSON_AddItemToObject(o, "policy", pj);
  pthread_mutex_lock(&g_mu);
  cJSON_AddStringToObject(o, "source", g_src);
  if (g_updated_at[0]) cJSON_AddStringToObject(o, "updated_at", g_updated_at); else cJSON_AddNullToObject(o, "updated_at");
  if (g_updated_by[0]) cJSON_AddStringToObject(o, "updated_by", g_updated_by); else cJSON_AddNullToObject(o, "updated_by");
  pthread_mutex_unlock(&g_mu);
  /* what it costs, measured over the registry */
  const source_def **a = registry_all(); int n = registry_count();
  long rows = 0; double decl = 0, eff = 0; long fastest = 0, slowest = 0;
  for (int i = 0; i < n; i++) {
    long d = a[i]->update_interval_sec;
    if (d <= 0) continue;
    long e = apply(&p, d);
    rows++; decl += 86400.0 / (double)d; eff += 86400.0 / (double)e;
    if (!fastest || e < fastest) fastest = e;
    if (e > slowest) slowest = e;
  }
  cJSON *est = cJSON_CreateObject();
  cJSON_AddNumberToObject(est, "scheduled_rows", (double)rows);
  cJSON_AddNumberToObject(est, "runs_per_day_declared", (double)(long)(decl + 0.5));
  cJSON_AddNumberToObject(est, "runs_per_day_effective", (double)(long)(eff + 0.5));
  cJSON_AddNumberToObject(est, "fastest_effective_sec", (double)fastest);
  cJSON_AddNumberToObject(est, "slowest_effective_sec", (double)slowest);
  cJSON_AddStringToObject(est, "basis", "every scheduled row's declared interval, with the policy applied; pivots excluded");
  cJSON_AddItemToObject(o, "estimate", est);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js ? js : strdup("{\"error\":\"out_of_memory\"}");
}
