/* core/embed_pod.c — see embed_pod.h.
 *
 * SHAPE. A `_maint` pod on a 120 s interval, the same pattern as
 * collectors/pod/simhash_pod.c: `_maint` because it emits no intel_items (a
 * real collector id would trip records_drop against a permanent records=0),
 * and the scheduler's skip-if-running rule serialises ticks so two sweeps
 * never interleave.
 *
 * RESUMABILITY. Two phases, both keyed in intel_vec_meta:
 *   1. the FULL sweep walks eligible rows newest-first behind a keyset
 *      watermark (wm_p, wm_u) exactly like /api/intel/items pagination, so a
 *      restart resumes where the last tick stopped rather than at the top;
 *   2. once the full sweep runs dry, `full_done_at` is set to the time that
 *      sweep STARTED, and every later tick is a DELTA: rows with fetched_at
 *      at or after that mark. A row re-fetched with identical text is skipped
 *      by the hash in intel_vec_done; one whose text changed is re-embedded
 *      (DELETE + INSERT on the vec0 table), so the index tracks the corpus
 *      instead of freezing the first version of every row.
 * Both phases skip rows already in intel_vec_done, so the whole thing is
 * idempotent: rerunning never embeds a row twice and never loses one.
 *
 * WHY A SIDE TABLE NEXT TO THE vec0 TABLE. vec0 owns its own shadow tables
 * and a text primary key lookup through the virtual table is not a plain
 * B-tree probe. intel_vec_done(uid PRIMARY KEY, text_hash, embedded_at) is
 * an ordinary table: NOT EXISTS against it is an index seek, count(*) on it
 * is the embedded_count the coverage block reports, and the hash is what
 * makes "re-fetched but unchanged" free. The two are written in one
 * transaction so they cannot disagree.
 *
 * THE THREAD IT RUNS ON. llm_embed() routes through core/llm_worker.c, which
 * keys its worker threads on base_url; JO_EMBED_URL is a different host:port
 * from LLM_BASE_URL, so the embedding pod owns its own worker and can never
 * sit in the generation queue ahead of a user's search — nor behind one. */
#include "embed_pod.h"
#include "../source.h"
#include "llm.h"
#include "httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EMBED_DONE_TABLE "intel_vec_done"
#define EMBED_SID        "embedding-backfill"

/* ---- env knobs ----------------------------------------------------------- */

static long env_long(const char *k, long dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  return (end && end != v) ? n : dflt;
}

const char *embed_base_url(void) {
  const char *u = getenv("JO_EMBED_URL");
  return (u && *u) ? u : NULL;
}

static long since_days(void)  { long d = env_long("JO_EMBED_SINCE_DAYS", 180); return d > 0 ? d : 180; }
static int  batch_size(void)  { long b = env_long("JO_EMBED_BATCH", 32); return (b > 0 && b <= 256) ? (int)b : 32; }
static int  max_per_run(void) { long m = env_long("JO_EMBED_MAX_PER_RUN", 2000); return m > 0 ? (int)m : 2000; }
static int  max_chars(void)   { long m = env_long("JO_EMBED_MAX_CHARS", 1000); return (m >= 64) ? (int)m : 1000; }

/* JO_EMBED_RECORD_TYPES as a JSON array string for json_each(), or NULL for
 * the default (everything but the two collector notices). */
static char *record_types_json(void) {
  const char *v = getenv("JO_EMBED_RECORD_TYPES");
  if (!v || !*v) return NULL;
  cJSON *a = cJSON_CreateArray();
  char *copy = strdup(v);
  if (!copy) { cJSON_Delete(a); return NULL; }
  for (char *tok = strtok(copy, ","); tok; tok = strtok(NULL, ",")) {
    while (*tok == ' ') tok++;
    size_t n = strlen(tok);
    while (n && tok[n - 1] == ' ') tok[--n] = 0;
    if (n) cJSON_AddItemToArray(a, cJSON_CreateString(tok));
  }
  free(copy);
  char *out = cJSON_GetArraySize(a) ? cJSON_PrintUnformatted(a) : NULL;
  cJSON_Delete(a);
  return out;
}

char *embed_bound_text(const char *text) {
  if (!text) return strdup("");
  size_t cap = (size_t)max_chars();
  size_t n = strlen(text);
  if (n <= cap) return strdup(text);
  /* Back up to a UTF-8 lead byte so a multibyte character is never split —
   * a half character is a different token to the model, not a shorter one. */
  while (cap > 0 && ((unsigned char)text[cap] & 0xC0) == 0x80) cap--;
  char *o = malloc(cap + 1);
  if (!o) return NULL;
  memcpy(o, text, cap);
  o[cap] = 0;
  return o;
}

/* ---- meta table ---------------------------------------------------------- */

static int meta_get(db_handle *db, const char *k, char *out, size_t cap) {
  out[0] = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT v FROM " EMBED_META_TABLE " WHERE k=?1",
                         -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
  int found = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *v = (const char *)sqlite3_column_text(s, 0);
    snprintf(out, cap, "%s", v ? v : "");
    found = 1;
  }
  sqlite3_finalize(s);
  return found;
}

static void meta_set(db_handle *db, const char *k, const char *v) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO " EMBED_META_TABLE "(k,v) VALUES(?1,?2) "
        "ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
  if (v) sqlite3_bind_text(s, 2, v, -1, SQLITE_TRANSIENT);
  else   sqlite3_bind_null(s, 2);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

static void meta_set_long(db_handle *db, const char *k, long v) {
  char b[32];
  snprintf(b, sizeof b, "%ld", v);
  meta_set(db, k, b);
}

static long meta_long(db_handle *db, const char *k) {
  char b[32];
  return meta_get(db, k, b, sizeof b) ? strtol(b, NULL, 10) : 0;
}

static int table_exists(db_handle *db, const char *name) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, name, -1, SQLITE_STATIC);
  int ok = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return ok;
}

int embed_table_exists(db_handle *db) {
  return db && db->h && table_exists(db, EMBED_VEC_TABLE);
}

/* The meta table is created on every path that might read it, so the
 * coverage block can be built on a database the pod has never touched. */
static void ensure_meta(db_handle *db) {
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS " EMBED_META_TABLE "(k TEXT PRIMARY KEY, v TEXT)",
    NULL, NULL, NULL);
}

int embed_index_dim(db_handle *db, char *model, size_t cap) {
  if (model && cap) model[0] = 0;
  if (!db || !db->h || !table_exists(db, EMBED_META_TABLE)) return 0;
  if (model && cap) meta_get(db, "model", model, cap);
  return (int)meta_long(db, "dim");
}

static void iso_utc(char *buf, size_t n, time_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

cJSON *embed_coverage_json(db_handle *db) {
  cJSON *o = cJSON_CreateObject();
  const char *url = embed_base_url();
  cJSON_AddBoolToObject(o, "enabled", url != NULL);
  cJSON_AddBoolToObject(o, "index_present", embed_table_exists(db));
  char buf[512];
  int have_meta = db && db->h && table_exists(db, EMBED_META_TABLE);
  if (have_meta && meta_get(db, "model", buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, "model", buf);
  else cJSON_AddNullToObject(o, "model");
  cJSON_AddNumberToObject(o, "dim", have_meta ? (double)meta_long(db, "dim") : 0);
  cJSON_AddNumberToObject(o, "embedded_count",
                          have_meta ? (double)meta_long(db, "embedded_count") : 0);
  /* eligible_count is a measured number with a timestamp, or null: an
   * estimate would let a caller compute a coverage ratio that was never
   * observed. */
  if (have_meta && meta_get(db, "eligible_count", buf, sizeof buf) && buf[0]) {
    cJSON_AddNumberToObject(o, "eligible_count", (double)strtol(buf, NULL, 10));
    meta_get(db, "eligible_counted_at", buf, sizeof buf);
    cJSON_AddStringToObject(o, "eligible_counted_at", buf);
  } else {
    cJSON_AddNullToObject(o, "eligible_count");
    cJSON_AddNullToObject(o, "eligible_counted_at");
  }
  cJSON_AddNumberToObject(o, "since_days", (double)since_days());
  char *rt = record_types_json();
  if (rt) { cJSON *a = cJSON_Parse(rt); cJSON_AddItemToObject(o, "record_types", a ? a : cJSON_CreateNull()); free(rt); }
  else cJSON_AddStringToObject(o, "record_types",
         "all except collector-truncation-notice, collector-shape-notice");
  cJSON_AddNumberToObject(o, "max_chars", (double)max_chars());
  if (have_meta && meta_get(db, "full_done_at", buf, sizeof buf) && buf[0]) {
    cJSON_AddStringToObject(o, "sweep", "delta");
    cJSON_AddStringToObject(o, "full_sweep_done_at", buf);
  } else if (have_meta && meta_get(db, "wm_p", buf, sizeof buf) && buf[0]) {
    cJSON_AddStringToObject(o, "sweep", "full_in_progress");
    cJSON_AddStringToObject(o, "watermark", buf);
  } else {
    cJSON_AddStringToObject(o, "sweep", url ? "not_started" : "disabled");
  }
  if (have_meta && meta_get(db, "last_error", buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, "last_error", buf);
  else cJSON_AddNullToObject(o, "last_error");
  if (have_meta && meta_get(db, "refused", buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, "refused", buf);
  else cJSON_AddNullToObject(o, "refused");
  if (have_meta && meta_get(db, "last_run_at", buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, "last_run_at", buf);
  else cJSON_AddNullToObject(o, "last_run_at");
  return o;
}

/* ---- schema -------------------------------------------------------------- */

static int ensure_index(db_handle *db, int dim) {
  char sql[256];
  snprintf(sql, sizeof sql,
    "CREATE VIRTUAL TABLE IF NOT EXISTS " EMBED_VEC_TABLE
    " USING vec0(uid TEXT PRIMARY KEY, embedding float[%d])", dim);
  char *err = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "[embed] cannot create %s: %s\n", EMBED_VEC_TABLE, err ? err : "?");
    sqlite3_free(err);
    return -1;
  }
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS " EMBED_DONE_TABLE
    "(uid TEXT PRIMARY KEY, text_hash INTEGER NOT NULL, embedded_at TEXT NOT NULL)",
    NULL, NULL, NULL);
  return 0;
}

/* ---- model identity ------------------------------------------------------ */

/* JO_EMBED_MODEL if set, else what the server calls itself in GET
 * /v1/models (llama-server reports the GGUF path there), else "unknown".
 * Recorded once and then compared on every run. */
static void detect_model_name(http_client *http, const char *base, char *out,
                              size_t cap) {
  const char *m = getenv("JO_EMBED_MODEL");
  if (m && *m) { snprintf(out, cap, "%s", m); return; }
  snprintf(out, cap, "unknown");
  if (!http) return;
  char url[1024];
  size_t bl = strlen(base);
  snprintf(url, sizeof url, "%.*s/v1/models",
           (int)(bl && base[bl - 1] == '/' ? bl - 1 : bl), base);
  http_response r = {0};
  if (http_request(http, "GET", url, NULL, NULL, 0, 5000, 0, &r) == 0 &&
      r.status >= 200 && r.status < 300 && r.body) {
    cJSON *j = cJSON_Parse(r.body);
    cJSON *data = j ? cJSON_GetObjectItem(j, "data") : NULL;
    cJSON *d0 = data ? cJSON_GetArrayItem(data, 0) : NULL;
    cJSON *id = d0 ? cJSON_GetObjectItem(d0, "id") : NULL;
    if (cJSON_IsString(id) && id->valuestring[0]) {
      /* Basename only: the path is the deployment's, the model is the name. */
      const char *b = strrchr(id->valuestring, '/');
      snprintf(out, cap, "%s", b ? b + 1 : id->valuestring);
    }
    cJSON_Delete(j);
  }
  http_response_free(&r);
}

/* ---- the sweep ----------------------------------------------------------- */

static unsigned long long fnv1a(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h;
}

/* One row picked up by the selection query. */
typedef struct {
  char *uid, *text, *p;
  unsigned long long hash;
  int changed;            /* 1 = already embedded, text differs → re-embed */
} pick;

static void pick_free(pick *p) { free(p->uid); free(p->text); free(p->p); }

/* title + ' ' + summary, falling back to the first 512 bytes of body. */
static char *compose_text(const char *title, const char *summary,
                          const char *body512) {
  const char *t = title ? title : "";
  const char *s = (summary && *summary) ? summary : (body512 ? body512 : "");
  size_t n = strlen(t) + 1 + strlen(s) + 1;
  char *o = malloc(n);
  if (!o) return NULL;
  snprintf(o, n, "%s%s%s", t, (*t && *s) ? " " : "", s);
  char *b = embed_bound_text(o);
  free(o);
  return b;
}

/* Selection SQL. The record-type predicate is bound as a JSON array through
 * json_each so the allowlist never touches SQL text. Two shapes:
 *   full  — keyset behind (wm_p, wm_u), newest first;
 *   delta — fetched_at >= full_done_at, newest first, INCLUDING rows already
 *           in intel_vec_done (their hash decides whether they changed).
 * `since` bounds both. */
#define SEL_COLS \
  "intel_items.uid, intel_items.title, intel_items.summary, " \
  "substr(intel_items.body,1,512), " \
  "COALESCE(intel_items.published_at,intel_items.fetched_at), " \
  "COALESCE(" EMBED_DONE_TABLE ".text_hash, 0), " \
  "(" EMBED_DONE_TABLE ".uid IS NOT NULL)"
#define SEL_TYPE \
  "((?2 IS NULL AND (intel_items.record_type IS NULL OR intel_items.record_type " \
  "NOT IN ('collector-truncation-notice','collector-shape-notice'))) OR " \
  "(?2 IS NOT NULL AND intel_items.record_type IN " \
  "(SELECT value FROM json_each(?2))))"

static const char *SQL_FULL =
  "SELECT " SEL_COLS " FROM intel_items "
  "LEFT JOIN " EMBED_DONE_TABLE " ON " EMBED_DONE_TABLE ".uid = intel_items.uid "
  "WHERE COALESCE(intel_items.published_at,intel_items.fetched_at) >= ?1 "
  "AND " SEL_TYPE " "
  "AND " EMBED_DONE_TABLE ".uid IS NULL "
  "AND (?3 IS NULL OR COALESCE(intel_items.published_at,intel_items.fetched_at) < ?3 "
  "     OR (COALESCE(intel_items.published_at,intel_items.fetched_at) = ?3 "
  "         AND intel_items.uid > ?4)) "
  "ORDER BY COALESCE(intel_items.published_at,intel_items.fetched_at) DESC, "
  "intel_items.uid ASC LIMIT ?5";

static const char *SQL_DELTA =
  "SELECT " SEL_COLS " FROM intel_items "
  "LEFT JOIN " EMBED_DONE_TABLE " ON " EMBED_DONE_TABLE ".uid = intel_items.uid "
  "WHERE COALESCE(intel_items.published_at,intel_items.fetched_at) >= ?1 "
  "AND " SEL_TYPE " "
  "AND intel_items.fetched_at >= ?3 "
  "AND (" EMBED_DONE_TABLE ".uid IS NULL OR " EMBED_DONE_TABLE ".embedded_at <= intel_items.fetched_at) "
  "AND (?4 IS NULL OR intel_items.uid > ?4) "
  "ORDER BY intel_items.uid ASC LIMIT ?5";

static const char *SQL_ELIGIBLE =
  "SELECT count(*) FROM intel_items "
  "WHERE COALESCE(intel_items.published_at,intel_items.fetched_at) >= ?1 AND " SEL_TYPE;

/* Fill `out[0..cap)` from one selection query; returns the number picked, or
 * -1 on a SQL failure. `full` selects the shape. */
static int select_batch(db_handle *db, int full, const char *since,
                        const char *types, const char *wm_p, const char *wm_u,
                        int cap, pick *out) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, full ? SQL_FULL : SQL_DELTA, -1, &s, NULL)
      != SQLITE_OK) {
    fprintf(stderr, "[embed] select failed: %s\n", sqlite3_errmsg(db->h));
    return -1;
  }
  sqlite3_bind_text(s, 1, since, -1, SQLITE_STATIC);
  if (types) sqlite3_bind_text(s, 2, types, -1, SQLITE_STATIC); else sqlite3_bind_null(s, 2);
  if (wm_p && *wm_p) sqlite3_bind_text(s, 3, wm_p, -1, SQLITE_STATIC); else sqlite3_bind_null(s, 3);
  if (wm_u && *wm_u) sqlite3_bind_text(s, 4, wm_u, -1, SQLITE_STATIC); else sqlite3_bind_null(s, 4);
  sqlite3_bind_int(s, 5, cap);
  int n = 0;
  while (n < cap && sqlite3_step(s) == SQLITE_ROW) {
    const char *uid = (const char *)sqlite3_column_text(s, 0);
    if (!uid) continue;
    pick *p = &out[n];
    memset(p, 0, sizeof *p);
    p->uid  = strdup(uid);
    p->text = compose_text((const char *)sqlite3_column_text(s, 1),
                           (const char *)sqlite3_column_text(s, 2),
                           (const char *)sqlite3_column_text(s, 3));
    const char *pp = (const char *)sqlite3_column_text(s, 4);
    p->p = strdup(pp ? pp : "");
    if (!p->uid || !p->text || !p->p) { pick_free(p); break; }
    p->hash = fnv1a(p->text);
    int done = sqlite3_column_int(s, 6);
    unsigned long long old = (unsigned long long)sqlite3_column_int64(s, 5);
    p->changed = done ? (old != p->hash) : 0;
    /* Already embedded AND unchanged → nothing to send. Still counts as
     * walked so the delta keyset advances past it. */
    if (done && !p->changed) p->text[0] = 0;
    n++;
  }
  sqlite3_finalize(s);
  return n;
}

/* Embed and store one batch. Returns rows written (>=0) or -1. */
static int embed_batch(db_handle *db, llm_client *llm, pick *rows, int n,
                       int *dim_io, const char *now) {
  const char *texts[256];
  int idx[256], m = 0;
  for (int i = 0; i < n && m < 256; i++)
    if (rows[i].text[0]) { texts[m] = rows[i].text; idx[m] = i; m++; }
  if (!m) return 0;
  float *vecs = NULL; int dim = 0; llm_status st;
  if (llm_embed(llm, texts, m, &vecs, &dim, 120000, &st) != 0) {
    char msg[160];
    snprintf(msg, sizeof msg, "%s: /v1/embeddings failed for a batch of %d (%s)",
             now, m, llm_status_code(st));
    meta_set(db, "last_error", msg);
    fprintf(stderr, "[embed] %s\n", msg);
    return -1;
  }
  if (*dim_io == 0) {
    *dim_io = dim;
    if (ensure_index(db, dim) != 0) { free(vecs); return -1; }
    meta_set_long(db, "dim", dim);
  } else if (dim != *dim_io) {
    char msg[200];
    snprintf(msg, sizeof msg, "server returned dim %d but the index is %d-d; "
             "refusing to mix (rebuild: DROP TABLE intel_vec, intel_vec_done)", dim, *dim_io);
    meta_set(db, "refused", msg);
    fprintf(stderr, "[embed] %s\n", msg);
    free(vecs);
    return -1;
  }
  sqlite3_stmt *del = NULL, *ins = NULL, *done = NULL;
  int rc = -1, written = 0;
  sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL);
  if (sqlite3_prepare_v2(db->h, "DELETE FROM " EMBED_VEC_TABLE " WHERE uid=?1", -1, &del, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(db->h, "INSERT INTO " EMBED_VEC_TABLE "(uid,embedding) VALUES(?1,?2)", -1, &ins, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(db->h, "INSERT INTO " EMBED_DONE_TABLE "(uid,text_hash,embedded_at) VALUES(?1,?2,?3) "
                                "ON CONFLICT(uid) DO UPDATE SET text_hash=excluded.text_hash, embedded_at=excluded.embedded_at",
                         -1, &done, NULL) != SQLITE_OK) {
    fprintf(stderr, "[embed] prepare failed: %s\n", sqlite3_errmsg(db->h));
    goto out;
  }
  for (int j = 0; j < m; j++) {
    pick *p = &rows[idx[j]];
    if (p->changed) {
      sqlite3_reset(del); sqlite3_bind_text(del, 1, p->uid, -1, SQLITE_STATIC);
      if (sqlite3_step(del) != SQLITE_DONE) { fprintf(stderr, "[embed] delete %s: %s\n", p->uid, sqlite3_errmsg(db->h)); goto out; }
    }
    sqlite3_reset(ins);
    sqlite3_bind_text(ins, 1, p->uid, -1, SQLITE_STATIC);
    sqlite3_bind_blob(ins, 2, vecs + (size_t)j * dim, (int)(dim * sizeof(float)), SQLITE_STATIC);
    if (sqlite3_step(ins) != SQLITE_DONE) { fprintf(stderr, "[embed] insert %s: %s\n", p->uid, sqlite3_errmsg(db->h)); goto out; }
    sqlite3_reset(done);
    sqlite3_bind_text(done, 1, p->uid, -1, SQLITE_STATIC);
    sqlite3_bind_int64(done, 2, (sqlite3_int64)p->hash);
    sqlite3_bind_text(done, 3, now, -1, SQLITE_STATIC);
    if (sqlite3_step(done) != SQLITE_DONE) { fprintf(stderr, "[embed] done %s: %s\n", p->uid, sqlite3_errmsg(db->h)); goto out; }
    written++;
  }
  rc = written;
out:
  sqlite3_finalize(del); sqlite3_finalize(ins); sqlite3_finalize(done);
  sqlite3_exec(db->h, rc < 0 ? "ROLLBACK" : "COMMIT", NULL, NULL, NULL);
  free(vecs);
  return rc;
}

static void refresh_counts(db_handle *db, const char *since, const char *types,
                           const char *now, int force) {
  sqlite3_stmt *s;
  long embedded = -1;
  if (sqlite3_prepare_v2(db->h, "SELECT count(*) FROM " EMBED_DONE_TABLE, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) {
      embedded = (long)sqlite3_column_int64(s, 0);
      meta_set_long(db, "embedded_count", embedded);
    }
    sqlite3_finalize(s);
  }
  /* embedded > eligible is impossible in reality and possible in the meta
   * only because eligible is refreshed lazily — a burst of new rows since the
   * last count. Do not let the coverage block publish a ratio above 1. */
  if (embedded > meta_long(db, "eligible_count")) force = 1;
  /* eligible_count is a full scan of intel_items (COALESCE is not
   * indexable), so it is refreshed at most hourly, and the timestamp of the
   * count travels with it. */
  char at[64];
  long refresh = env_long("JO_EMBED_ELIGIBLE_REFRESH_SEC", 3600);
  if (!force && meta_get(db, "eligible_counted_at", at, sizeof at) && at[0]) {
    struct tm tm = {0};
    if (sscanf(at, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
      tm.tm_year -= 1900; tm.tm_mon -= 1;
      time_t then = timegm(&tm);
      if (time(NULL) - then < refresh) return;
    }
  }
  if (sqlite3_prepare_v2(db->h, SQL_ELIGIBLE, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, since, -1, SQLITE_STATIC);
    if (types) sqlite3_bind_text(s, 2, types, -1, SQLITE_STATIC); else sqlite3_bind_null(s, 2);
    if (sqlite3_step(s) == SQLITE_ROW) {
      meta_set_long(db, "eligible_count", sqlite3_column_int64(s, 0));
      meta_set(db, "eligible_counted_at", now);
    }
    sqlite3_finalize(s);
  }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                               /* writes intel_vec, not intel */
  const char *base = embed_base_url();
  if (!base) {
    fprintf(stderr, "[embed] disabled: JO_EMBED_URL is unset; /api/intel/semantic "
                    "will answer 503 with coverage.enabled=false\n");
    return 0;
  }
  /* Own connection: every batch is a transaction and a transaction belongs
   * to a connection, not a thread (db.h). */
  db_handle own;
  db_handle *db = db_worker_open(&own, ctx->db);
  ensure_meta(db);
  char now[64]; iso_utc(now, sizeof now, time(NULL));
  meta_set(db, "last_run_at", now);

  char model[256], stored_model[256];
  detect_model_name(ctx->http, base, model, sizeof model);
  int dim = (int)meta_long(db, "dim");
  meta_get(db, "model", stored_model, sizeof stored_model);
  if (stored_model[0] && strcmp(stored_model, model) != 0) {
    char msg[600];
    snprintf(msg, sizeof msg, "index was built with model '%.200s' but the server "
             "is '%.200s'; refusing to mix (rebuild: DROP TABLE intel_vec, "
             "intel_vec_done; DELETE FROM intel_vec_meta)", stored_model, model);
    meta_set(db, "refused", msg);
    fprintf(stderr, "[embed] %s\n", msg);
    db_worker_close(&own);
    return -1;
  }
  if (!stored_model[0]) meta_set(db, "model", model);
  meta_set(db, "refused", NULL);
  if (dim) { if (ensure_index(db, dim) != 0) { db_worker_close(&own); return -1; } }
  else sqlite3_exec(db->h, "CREATE TABLE IF NOT EXISTS " EMBED_DONE_TABLE
                    "(uid TEXT PRIMARY KEY, text_hash INTEGER NOT NULL, embedded_at TEXT NOT NULL)",
                    NULL, NULL, NULL);

  llm_client llm = { .http = ctx->http, .base_url = base, .interactive = 0 };
  /* Probe the dimension UP FRONT when an index exists. Without this a server
   * swapped for one of a different width is only noticed when a row needs
   * embedding, so a quiet corpus could sit behind a mismatched server for
   * days with every query embedded into the wrong space. One tiny request
   * per tick buys the refusal at the moment the mismatch appears. */
  if (dim) {
    const char *probe[1] = { "dimension probe" };
    float *pv = NULL; int pdim = 0; llm_status pst;
    if (llm_embed(&llm, probe, 1, &pv, &pdim, 30000, &pst) != 0) {
      char msg[160];
      snprintf(msg, sizeof msg, "%s: /v1/embeddings unreachable for the dimension probe (%s)",
               now, llm_status_code(pst));
      meta_set(db, "last_error", msg);
      fprintf(stderr, "[embed] %s\n", msg);
      db_worker_close(&own);
      return -1;
    }
    free(pv);
    if (pdim != dim) {
      char msg[200];
      snprintf(msg, sizeof msg, "server returns %d-d vectors but the index is %d-d; "
               "refusing to mix (rebuild: DROP TABLE intel_vec, intel_vec_done; "
               "DELETE FROM intel_vec_meta)", pdim, dim);
      meta_set(db, "refused", msg);
      fprintf(stderr, "[embed] %s\n", msg);
      db_worker_close(&own);
      return -1;
    }
  }
  char since[64]; iso_utc(since, sizeof since, time(NULL) - since_days() * 86400L);
  char *types = record_types_json();
  char full_done[64], wm_p[256], wm_u[600], sweep_start[64];
  meta_get(db, "full_done_at", full_done, sizeof full_done);
  int full = !full_done[0];
  if (full) {
    meta_get(db, "wm_p", wm_p, sizeof wm_p);
    meta_get(db, "wm_u", wm_u, sizeof wm_u);
    if (!meta_get(db, "sweep_start", sweep_start, sizeof sweep_start) || !sweep_start[0]) {
      snprintf(sweep_start, sizeof sweep_start, "%s", now);
      meta_set(db, "sweep_start", now);
    }
  } else {
    snprintf(wm_p, sizeof wm_p, "%s", full_done);   /* delta lower bound */
    wm_u[0] = 0;
  }
  refresh_counts(db, since, types, now, 0);

  int bs = batch_size(), budget = max_per_run(), rc = 0, total = 0;
  pick rows[256];
  while (budget > 0 && !(ctx->cancel && *ctx->cancel)) {
    int want = bs < budget ? bs : budget;
    int n = select_batch(db, full, since, types, wm_p, wm_u, want, rows);
    if (n < 0) { rc = -1; break; }
    if (n == 0) {
      if (full) {
        /* The full sweep is complete: from now on only what arrived since it
         * began needs looking at. The watermark is retired with it. */
        meta_set(db, "full_done_at", sweep_start);
        meta_set(db, "wm_p", NULL); meta_set(db, "wm_u", NULL);
        fprintf(stderr, "[embed] full sweep complete (started %s); switching to delta\n", sweep_start);
      } else {
        /* Delta drained: rows fetched after this run started are the next
         * delta. Advancing to `now` (this run's start), not the wall clock,
         * so a row written while this run was scanning is not skipped. */
        meta_set(db, "full_done_at", now);
      }
      break;
    }
    int w = embed_batch(db, &llm, rows, n, &dim, now);
    if (w >= 0) {
      /* Advance the keyset only after the batch is durably stored, so a
       * failed batch is retried at the same position next tick. */
      snprintf(wm_u, sizeof wm_u, "%s", rows[n - 1].uid);
      if (full) {                       /* delta keeps wm_p = full_done_at */
        snprintf(wm_p, sizeof wm_p, "%s", rows[n - 1].p);
        meta_set(db, "wm_p", wm_p); meta_set(db, "wm_u", wm_u);
      }
      total += w; budget -= n;
    }
    for (int i = 0; i < n; i++) pick_free(&rows[i]);
    if (w < 0) { rc = -1; break; }
    if (w > 0) meta_set(db, "last_error", NULL);
  }
  refresh_counts(db, since, types, now, 0);
  /* A tick that ended cleanly — including one with nothing to embed — has
   * proven the server answers, so a stale last_error from an earlier outage
   * must not keep showing in coverage. */
  if (rc == 0) meta_set(db, "last_error", NULL);
  fprintf(stderr, "[embed] %s sweep: embedded %d row(s) this tick, index now %ld row(s) at %d-d (%s)%s\n",
          full ? "full" : "delta", total, meta_long(db, "embedded_count"), dim, model,
          (ctx->cancel && *ctx->cancel) ? " [cancelled]" : "");
  free(types);
  db_worker_close(&own);
  return rc;
}

static const source_def embed_def = {
  .id = EMBED_SID, .collector = "_maint",
  .name = "Semantic Embedding Backfill",
  .name_ja = "セマンティック埋め込み補完",
  .description = "Embeds intel_items title+summary into the sqlite-vec index "
                 "behind /api/intel/semantic. Inert unless JO_EMBED_URL is set.",
  .url = "internal://embedding-backfill",
  .update_interval_sec = 120, .run = run };
REGISTER_SOURCE(embed_def)
