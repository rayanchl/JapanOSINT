/* core/thresholds.c — see thresholds.h. */
#include "thresholds.h"
#include "audit.h"
#include "../third_party/sqlite3.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The defaults are the modules' own constants, so the header that explains a
 * number stays the place that explains it. */
#include "entity_vec.h"     /* ENTITY_VEC_COS_FLOOR        */
#include "story.h"          /* STORY_TAU_DEFAULT           */
#include "alert_sem.h"      /* ALERT_SEM_THRESHOLD_DEFAULT */
#include "relevance.h"      /* RELEV_SIM_MIN               */
#include "suggest.h"        /* SUGGEST_MIN_COS             */
#include "camera_dupes.h"   /* CAM_DUP_COSINE_MIN          */

/* Defined beside their callers (static there); the values are repeated here
 * and the callers read through threshold_get(), so these are the only copy
 * in force. */
#define RESOLVE_EMB_MIN_COS_DEFAULT    0.60
#define RESOLVE_EMB_MARGIN_DEFAULT     0.10
#define REPAIR_SEM_COS_FLOOR_DEFAULT   0.70
#define CORPUS_VEC_MAX_DISTANCE_DEFAULT 1.41421356
#define ROUTE_MIN_COS_DEFAULT          0.55

static const threshold_def DEFS[] = {
  { "entity_vec_cos_floor", "JO_ENTITY_VEC_COS_FLOOR", ENTITY_VEC_COS_FLOOR,
    0.01, 1.0, "cosine",
    "Entity merge candidates: a neighbour at or above this cosine is asked "
    "about by the LLM judge (core/entity_vec.h). Nothing auto-merges." },
  { "story_tau", "JO_STORY_TAU", STORY_TAU_DEFAULT, 0.01, 1.0, "cosine",
    "Story joining: an item joins its nearest story at or above this cosine "
    "(core/story.h)." },
  { "alert_sem_threshold", "JO_ALERT_SEM_THRESHOLD",
    ALERT_SEM_THRESHOLD_DEFAULT, 0.01, 1.0, "cosine",
    "Semantic alert rules: default query-to-item cosine for a rule that sets "
    "no threshold of its own; a rule's own threshold still wins "
    "(core/alert_sem.h)." },
  { "relevance_sim_min", "JO_RELEVANCE_SIM_MIN", RELEV_SIM_MIN, 0.01, 0.99,
    "cosine",
    "Record relevance: cosine at which a pivot's record counts as related to "
    "the entity asked about (core/relevance.h)." },
  { "suggest_min_cos", "JO_SUGGEST_MIN_COS", SUGGEST_MIN_COS, 0.01, 0.99,
    "cosine",
    "Search suggestions: candidates below this cosine to the query are not "
    "offered (core/suggest.h)." },
  { "resolve_emb_min_cos", "JO_RESOLVE_EMB_MIN_COS",
    RESOLVE_EMB_MIN_COS_DEFAULT, 0.01, 1.0, "cosine",
    "Source-id resolver: an embedding match must be at least this near to be "
    "accepted (core/osint_dispatch.c)." },
  { "resolve_emb_margin", "JO_RESOLVE_EMB_MARGIN", RESOLVE_EMB_MARGIN_DEFAULT,
    0.0, 1.0, "cosine_margin",
    "Source-id resolver: the winner must beat the runner-up by this much "
    "cosine, or the answer is \"we do not know\" (core/osint_dispatch.c)." },
  { "camera_dup_cosine_min", "JO_CAM_DUP_COSINE_MIN", CAM_DUP_COSINE_MIN,
    0.01, 1.0, "cosine",
    "Camera duplicates: whole-frame embedding cosine at or above which two "
    "nearby cameras are proposed as the same feed (core/camera_dupes.h)." },
  { "repair_sem_cos_floor", "JO_REPAIR_SEM_FLOOR",
    REPAIR_SEM_COS_FLOOR_DEFAULT, 0.01, 1.0, "cosine",
    "Collector repair gate: a candidate payload must reach this cosine to the "
    "source's own item centroid (collectors/pod/collector_repair.c)." },
  { "corpus_vec_max_distance", "JO_CORPUS_VEC_MAX_DISTANCE",
    CORPUS_VEC_MAX_DISTANCE_DEFAULT, 0.01, 2.0, "l2_distance",
    "Corpus lookup: a KNN neighbour at or past this L2 distance is not a "
    "candidate (sqrt(2) = cosine 0 on unit vectors; lower is stricter) "
    "(collectors/sources/jp_corpus_lookup.c)." },
  { "route_min_cos", "JO_ROUTE_MIN_COS", ROUTE_MIN_COS_DEFAULT, 0.01, 1.0,
    "cosine",
    "Question routing: a question with no entity reads the stored records of "
    "every scheduled source whose card is at least this similar; lower reads "
    "more sources (collectors/sources/jp_corpus_lookup.c)." },
};
#define NDEFS ((int)(sizeof DEFS / sizeof DEFS[0]))

/* The DB layer, cached. */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int    g_has[NDEFS];
static double g_val[NDEFS];
static char   g_by[NDEFS][128];
static char   g_at[NDEFS][32];

const threshold_def *threshold_defs(int *n) { if (n) *n = NDEFS; return DEFS; }

static int idx_of(const char *key) {
  if (!key) return -1;
  for (int i = 0; i < NDEFS; i++) if (!strcmp(DEFS[i].key, key)) return i;
  return -1;
}
const threshold_def *threshold_find(const char *key) {
  int i = idx_of(key);
  return i < 0 ? NULL : &DEFS[i];
}

static double clampd(double v, double lo, double hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

/* The env layer, read every call (as the modules did): a value that does not
 * parse, or is outside [min,max], is ignored — the old per-module rule. */
static int env_value(const threshold_def *d, double *out) {
  const char *e = getenv(d->env);
  if (!e || !*e) return 0;
  char *end = NULL;
  double v = strtod(e, &end);
  if (!end || end == e || !isfinite(v) || v < d->min || v > d->max) return 0;
  *out = v;
  return 1;
}

const char *threshold_src_name(threshold_src s) {
  return s == THR_SRC_DB ? "db" : s == THR_SRC_ENV ? "env" : "default";
}

int threshold_resolve(const char *key, double *value, threshold_src *src) {
  int i = idx_of(key);
  if (i < 0) return -1;
  const threshold_def *d = &DEFS[i];
  double v; threshold_src s;
  pthread_mutex_lock(&g_lock);
  int has = g_has[i]; double dv = g_val[i];
  pthread_mutex_unlock(&g_lock);
  if (has)                     { v = dv; s = THR_SRC_DB; }
  else if (env_value(d, &v))   { s = THR_SRC_ENV; }
  else                         { v = d->dflt; s = THR_SRC_DEFAULT; }
  if (value) *value = v;
  if (src) *src = s;
  return 0;
}

double threshold_get(const char *key) {
  double v;
  if (threshold_resolve(key, &v, NULL) != 0) {
    static int warned;
    if (!warned) { warned = 1;
      fprintf(stderr, "[thresholds] unknown key '%s'\n", key ? key : "(null)"); }
    return NAN;
  }
  return v;
}

void threshold_reload(db_handle *db) {
  if (!db || !db->h) return;
  int    has[NDEFS] = {0};
  double val[NDEFS] = {0};
  char   by[NDEFS][128], at[NDEFS][32];
  memset(by, 0, sizeof by); memset(at, 0, sizeof at);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT key, value, updated_by, updated_at FROM settings_thresholds",
        -1, &s, NULL) != SQLITE_OK) return;   /* cache left as it was */
  while (sqlite3_step(s) == SQLITE_ROW) {
    int i = idx_of((const char *)sqlite3_column_text(s, 0));
    if (i < 0 || sqlite3_column_type(s, 1) == SQLITE_NULL) continue;
    double v = sqlite3_column_double(s, 1);
    if (!isfinite(v)) continue;
    has[i] = 1;
    val[i] = clampd(v, DEFS[i].min, DEFS[i].max);
    const unsigned char *b = sqlite3_column_text(s, 2);
    const unsigned char *a = sqlite3_column_text(s, 3);
    if (b) snprintf(by[i], sizeof by[i], "%s", (const char *)b);
    if (a) snprintf(at[i], sizeof at[i], "%s", (const char *)a);
  }
  sqlite3_finalize(s);
  pthread_mutex_lock(&g_lock);
  memcpy(g_has, has, sizeof has);
  memcpy(g_val, val, sizeof val);
  memcpy(g_by, by, sizeof by);
  memcpy(g_at, at, sizeof at);
  pthread_mutex_unlock(&g_lock);
}

void threshold_migrate(db_handle *db) {
  if (!db || !db->h) return;
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS settings_thresholds ("
    " key        TEXT PRIMARY KEY,"
    " value      REAL NOT NULL,"
    " updated_by TEXT,"
    " updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')))",
    NULL, NULL, NULL);
  threshold_reload(db);
}

int threshold_set(db_handle *db, const char *key, double value,
                  const char *updated_by) {
  int i = idx_of(key);
  if (i < 0) return -1;
  if (!isfinite(value) || value < DEFS[i].min || value > DEFS[i].max) return -2;
  if (!db || !db->h) return -3;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO settings_thresholds(key, value, updated_by, updated_at) "
        "VALUES(?1, ?2, ?3, strftime('%Y-%m-%dT%H:%M:%SZ','now')) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value, "
        "updated_by=excluded.updated_by, updated_at=excluded.updated_at",
        -1, &s, NULL) != SQLITE_OK) return -3;
  sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(s, 2, value);
  if (updated_by && *updated_by) sqlite3_bind_text(s, 3, updated_by, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, 3);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) return -3;
  threshold_reload(db);
  return 0;
}

int threshold_reset(db_handle *db, const char *key) {
  if (idx_of(key) < 0) return -1;
  if (!db || !db->h) return -3;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "DELETE FROM settings_thresholds WHERE key=?1",
                         -1, &s, NULL) != SQLITE_OK) return -3;
  sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) return -3;
  threshold_reload(db);
  return 0;
}

cJSON *threshold_json(const char *key) {
  int i = idx_of(key);
  if (i < 0) return NULL;
  const threshold_def *d = &DEFS[i];
  double v; threshold_src src;
  threshold_resolve(key, &v, &src);
  char by[128], at[32];
  pthread_mutex_lock(&g_lock);
  int has = g_has[i];
  memcpy(by, g_by[i], sizeof by); memcpy(at, g_at[i], sizeof at);
  pthread_mutex_unlock(&g_lock);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "key", d->key);
  cJSON_AddNumberToObject(o, "value", v);
  cJSON_AddStringToObject(o, "source", threshold_src_name(src));
  cJSON_AddNumberToObject(o, "default", d->dflt);
  cJSON_AddNumberToObject(o, "min", d->min);
  cJSON_AddNumberToObject(o, "max", d->max);
  cJSON_AddStringToObject(o, "env", d->env);
  double ev;
  if (env_value(d, &ev)) cJSON_AddNumberToObject(o, "env_value", ev);
  else                   cJSON_AddNullToObject(o, "env_value");
  cJSON_AddStringToObject(o, "metric", d->metric);
  cJSON_AddStringToObject(o, "description", d->description);
  if (has && by[0]) cJSON_AddStringToObject(o, "updated_by", by);
  else              cJSON_AddNullToObject(o, "updated_by");
  if (has && at[0]) cJSON_AddStringToObject(o, "updated_at", at);
  else              cJSON_AddNullToObject(o, "updated_at");
  return o;
}

char *threshold_list_json(void) {
  cJSON *root = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(root, "data");
  for (int i = 0; i < NDEFS; i++)
    cJSON_AddItemToArray(arr, threshold_json(DEFS[i].key));
  cJSON *meta = cJSON_AddObjectToObject(root, "meta");
  cJSON_AddNumberToObject(meta, "count", NDEFS);
  cJSON_AddStringToObject(meta, "order", "db > env > default");
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}

static char *err_body(const char *error, const char *detail) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", error);
  if (detail) cJSON_AddStringToObject(o, "detail", detail);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

char *thresholds_api_put(db_handle *db, const char *key, const char *body,
                         size_t body_len, const char *user_id, int *status) {
  int st = 200;
  const threshold_def *d = threshold_find(key);
  if (!d) { *status = 404; return err_body("unknown_threshold", key); }
  cJSON *jb = (body && body_len) ? cJSON_ParseWithLength(body, body_len) : NULL;
  if (!jb || !cJSON_IsObject(jb)) {
    cJSON_Delete(jb);
    *status = 400;
    return err_body("bad_request", "body must be {\"value\":number} or {\"reset\":true}");
  }
  const cJSON *jv = cJSON_GetObjectItem(jb, "value");
  const cJSON *jr = cJSON_GetObjectItem(jb, "reset");
  int reset = jr && cJSON_IsTrue(jr);
  if (!reset && !(jv && cJSON_IsNumber(jv))) {
    cJSON_Delete(jb);
    *status = 400;
    return err_body("bad_request", "body must be {\"value\":number} or {\"reset\":true}");
  }
  double old_v; threshold_src old_s;
  threshold_resolve(key, &old_v, &old_s);
  int rc = reset ? threshold_reset(db, key)
                 : threshold_set(db, key, jv->valuedouble, user_id);
  if (rc == -2) {
    char det[160];
    snprintf(det, sizeof det, "value must be a number in [%g, %g]", d->min, d->max);
    cJSON_Delete(jb);
    *status = 400;
    return err_body("out_of_range", det);
  }
  if (rc != 0) { cJSON_Delete(jb); *status = 500; return err_body("server_error", NULL); }

  double new_v; threshold_src new_s;
  threshold_resolve(key, &new_v, &new_s);
  cJSON *ap = cJSON_CreateObject();
  cJSON_AddStringToObject(ap, "key", key);
  cJSON_AddNumberToObject(ap, "old_value", old_v);
  cJSON_AddStringToObject(ap, "old_source", threshold_src_name(old_s));
  cJSON_AddNumberToObject(ap, "new_value", new_v);
  cJSON_AddStringToObject(ap, "new_source", threshold_src_name(new_s));
  char *pj = cJSON_PrintUnformatted(ap);
  cJSON_Delete(ap);
  audit_write(db, "platform", user_id, reset ? "threshold.reset" : "threshold.set",
              key, pj ? pj : "{}");
  free(pj);
  cJSON_Delete(jb);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", threshold_json(key));
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  *status = st;
  return out;
}
