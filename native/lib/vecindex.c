/* lib/vecindex.c — see vecindex.h.
 *
 * Moved here from core/embed_pod.c (the incremental writer, the split-batch
 * retry, model identity, coverage) and core/service_vec.c (the RRF fusion and
 * the FTS arm). The comments that explain WHY each piece is shaped the way it
 * is moved with it; the incidents they cite happened in those two files. */
#include "vecindex.h"
#include "../core/httpclient.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

/* Failed rows listed in a coverage block. The count is always complete. */
#define FAIL_SAMPLE 5

static long env_long(const char *k, long dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  return (end && end != v) ? n : dflt;
}

static long long wall_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

/* Append to a fixed SQL buffer. On the first append that would not fit, *ok
 * goes to 0 and nothing more is written: a cut-off statement is never run —
 * a truncated WHERE clause is a query with fewer filters, i.e. a different
 * question answered confidently. */
static void sql_appendf(char *buf, size_t cap, size_t *len, int *ok,
                        const char *fmt, ...) {
  if (!*ok || *len >= cap) { *ok = 0; return; }
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf + *len, cap - *len, fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= cap - *len) { *ok = 0; return; }
  *len += (size_t)n;
}

/* ---- the embedding server ------------------------------------------------ */

const char *vecindex_base_url(void) {
  const char *u = getenv("JO_EMBED_URL");
  return (u && *u) ? u : NULL;
}

const char *vecindex_query_url(void) {
  const char *b = vecindex_base_url();
  if (!b) return NULL;                 /* no index server, no query server */
  const char *u = getenv("JO_EMBED_QUERY_URL");
  return (u && *u) ? u : b;
}

int vecindex_detect_model(http_client *http, const char *base, char *out,
                          size_t cap) {
  const char *m = getenv("JO_EMBED_MODEL");
  if (m && *m) { snprintf(out, cap, "%s", m); return 0; }
  return vecindex_served_model(http, base, out, cap);
}

int vecindex_served_model(http_client *http, const char *base, char *out,
                          size_t cap) {
  if (!base || !*base) { snprintf(out, cap, "no embedding server configured"); return -1; }
  http_client *own = NULL;
  if (!http) http = own = http_client_new();
  char url[1024];
  size_t bl = strlen(base);
  snprintf(url, sizeof url, "%.*s/v1/models",
           (int)(bl && base[bl - 1] == '/' ? bl - 1 : bl), base);
  http_response r = {0};
  int rc = -1;
  if (!http) snprintf(out, cap, "no http client");
  else if (http_request(http, "GET", url, NULL, NULL, 0, 5000, 0, &r) != 0 ||
           r.status == 0)
    snprintf(out, cap, "GET /v1/models: no answer");
  else if (r.status < 200 || r.status >= 300 || !r.body)
    snprintf(out, cap, "GET /v1/models: HTTP %ld", (long)r.status);
  else {
    cJSON *j = cJSON_Parse(r.body);
    cJSON *data = j ? cJSON_GetObjectItem(j, "data") : NULL;
    /* llama-server serves ONE model per instance and lists it as data[0]; the
     * array is the OpenAI envelope, not a record set. */
    cJSON *d0 = data ? cJSON_GetArrayItem(data, 0) : NULL; /* exhaustive-ok: one model per llama-server instance; /v1/models is identity, not records */
    cJSON *id = d0 ? cJSON_GetObjectItem(d0, "id") : NULL;
    if (cJSON_IsString(id) && id->valuestring[0]) {
      /* Basename only: the path is the deployment's, the model is the name. */
      const char *b = strrchr(id->valuestring, '/');
      snprintf(out, cap, "%s", b ? b + 1 : id->valuestring);
      rc = 0;
    } else snprintf(out, cap, "GET /v1/models: no data[0].id in the answer");
    cJSON_Delete(j);
  }
  http_response_free(&r);
  if (own) http_client_free(own);
  return rc;
}

/* A request path asks on every query; a model swap is an operator action, so
 * a few seconds of staleness is the right trade for not adding a round trip
 * to each search. Only SUCCESSFUL answers are cached.
 * JO_EMBED_MODEL_CHECK_TTL_MS overrides the 10 s (0 = ask every time). */
int vecindex_live_model(char *out, size_t cap) {
  static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
  static char cached[256], cached_base[512];
  static long long cached_at = 0;
  const char *base = vecindex_base_url();
  long long now = wall_ms();
  long ttl = env_long("JO_EMBED_MODEL_CHECK_TTL_MS", 10000);
  pthread_mutex_lock(&mu);
  if (base && cached_at && now - cached_at < ttl &&
      !strcmp(cached_base, base) && !getenv("JO_EMBED_MODEL")) {
    snprintf(out, cap, "%s", cached);
    pthread_mutex_unlock(&mu);
    return 0;
  }
  pthread_mutex_unlock(&mu);
  char got[256];
  int rc = vecindex_detect_model(NULL, base, got, sizeof got);
  snprintf(out, cap, "%s", got);
  if (rc == 0 && base) {
    pthread_mutex_lock(&mu);
    snprintf(cached, sizeof cached, "%s", got);
    snprintf(cached_base, sizeof cached_base, "%s", base);
    cached_at = now;
    pthread_mutex_unlock(&mu);
  }
  return rc;
}

/* THE QUERY SLOT MUST SERVE THE INDEX'S MODEL. JO_EMBED_QUERY_URL exists so a
 * user's query does not wait behind a 120 s backfill batch on the one worker
 * JO_EMBED_URL owns; it is a second instance, or a second --parallel slot of
 * the same one. A second INSTANCE can be started with a different GGUF, and a
 * query embedded by model B ranked against vectors from model A answers 200
 * with rankings that mean nothing — the same-width-swap failure a refused
 * index exists to stop. So when the two URLs differ, both servers are asked
 * what they serve and the query side is refused unless they agree. Cached on
 * success only, as vecindex_live_model() is. */
int vecindex_live_query_model(char *out, size_t cap) {
  int rc = vecindex_live_model(out, cap);
  if (rc != 0) return rc;
  const char *b = vecindex_base_url(), *q = vecindex_query_url();
  if (!b || !q || !strcmp(b, q)) return 0;
  static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
  static char ok_b[512], ok_q[512];
  static long long ok_at = 0;
  long long now = wall_ms();
  long ttl = env_long("JO_EMBED_MODEL_CHECK_TTL_MS", 10000);
  pthread_mutex_lock(&mu);
  int hit = ok_at && now - ok_at < ttl && !strcmp(ok_b, b) && !strcmp(ok_q, q);
  pthread_mutex_unlock(&mu);
  if (hit) return 0;
  char mb[256], mq[256];
  if (vecindex_served_model(NULL, b, mb, sizeof mb) != 0) {
    snprintf(out, cap, "cannot verify the query slot: JO_EMBED_URL %.200s", mb);
    return -1;
  }
  if (vecindex_served_model(NULL, q, mq, sizeof mq) != 0) {
    snprintf(out, cap, "cannot verify the query slot: JO_EMBED_QUERY_URL %.200s",
             mq);
    return -1;
  }
  if (strcmp(mb, mq) != 0) {
    snprintf(out, cap, "JO_EMBED_QUERY_URL serves '%.100s' but JO_EMBED_URL "
             "serves '%.100s'; a query embedded by one model cannot be ranked "
             "against vectors from the other, so queries are refused until "
             "both serve the same model", mq, mb);
    return -1;
  }
  pthread_mutex_lock(&mu);
  snprintf(ok_b, sizeof ok_b, "%s", b);
  snprintf(ok_q, sizeof ok_q, "%s", q);
  ok_at = now;
  pthread_mutex_unlock(&mu);
  return 0;
}

int vecindex_probe_dim(llm_client *llm, int timeout_ms, llm_status *st) {
  const char *probe[1] = { VECINDEX_PROBE_TEXT };
  float *pv = NULL; int pdim = 0; llm_status s = LLM_OK;
  int rc = llm_embed(llm, probe, 1, &pv, &pdim, timeout_ms, &s);
  free(pv);
  if (st) *st = s;
  return (rc == 0 && pdim > 0) ? pdim : -1;
}

/* ---- text ---------------------------------------------------------------- */

/* Length of the valid UTF-8 sequence at `s` (RFC 3629: no overlongs, no
 * surrogates, nothing past U+10FFFF), or 0 when the bytes there are not one.
 * That is exactly what llama-server's JSON parser accepts. */
static size_t utf8_seq(const unsigned char *s) {
  unsigned char c = s[0];
  if (c < 0x80) return 1;
#define CONT(x) (((x) & 0xC0) == 0x80)
  if (c >= 0xC2 && c <= 0xDF) return CONT(s[1]) ? 2 : 0;
  if (c == 0xE0) return (s[1] >= 0xA0 && s[1] <= 0xBF && CONT(s[2])) ? 3 : 0;
  if ((c >= 0xE1 && c <= 0xEC) || c == 0xEE || c == 0xEF)
    return (CONT(s[1]) && CONT(s[2])) ? 3 : 0;
  if (c == 0xED) return (s[1] >= 0x80 && s[1] <= 0x9F && CONT(s[2])) ? 3 : 0;
  if (c == 0xF0) return (s[1] >= 0x90 && s[1] <= 0xBF && CONT(s[2]) && CONT(s[3])) ? 4 : 0;
  if (c >= 0xF1 && c <= 0xF3) return (CONT(s[1]) && CONT(s[2]) && CONT(s[3])) ? 4 : 0;
  if (c == 0xF4) return (s[1] >= 0x80 && s[1] <= 0x8F && CONT(s[2]) && CONT(s[3])) ? 4 : 0;
#undef CONT
  return 0;
}

char *vecindex_utf8_sanitize(const char *text, size_t *replaced) {
  if (replaced) *replaced = 0;
  if (!text) return strdup("");
  size_t n = strlen(text);
  char *o = malloc(n * 3 + 1);            /* worst case: every byte → U+FFFD */
  if (!o) return NULL;
  const unsigned char *p = (const unsigned char *)text;
  size_t j = 0;
  while (*p) {
    size_t k = utf8_seq(p);
    if (k) { memcpy(o + j, p, k); j += k; p += k; continue; }
    /* One invalid byte → one U+FFFD, then resync on the next byte. The
     * character is replaced, not dropped, so the text keeps its shape. */
    o[j++] = (char)0xEF; o[j++] = (char)0xBF; o[j++] = (char)0xBD;
    p++;
    if (replaced) (*replaced)++;
  }
  o[j] = 0;
  return o;
}

char *vecindex_bound_text(const char *text, size_t cap) {
  char *clean = vecindex_utf8_sanitize(text, NULL);
  if (!clean) return NULL;
  size_t n = strlen(clean);
  if (n <= cap) return clean;
  /* Back up to a UTF-8 lead byte so a multibyte character is never split —
   * a half character is a different token to the model, not a shorter one. */
  while (cap > 0 && ((unsigned char)clean[cap] & 0xC0) == 0x80) cap--;
  clean[cap] = 0;
  return clean;
}

/* ---- meta + tables ------------------------------------------------------- */

int vecindex_table_exists(db_handle *db, const char *name) {
  if (!db || !db->h || !name) return 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, name, -1, SQLITE_STATIC);
  int ok = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return ok;
}

int vecindex_exists(db_handle *db, const vecindex_spec *s) {
  return vecindex_table_exists(db, s->table);
}

long vecindex_count(db_handle *db, const char *table) {
  if (!vecindex_table_exists(db, table)) return 0;
  char sql[160];
  snprintf(sql, sizeof sql, "SELECT count(*) FROM \"%s\"", table);
  sqlite3_stmt *s;
  long n = 0;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  return n;
}

/* The meta table is created on every path that might read it, so a coverage
 * block can be built on a database the pod has never touched. */
void vecindex_ensure_meta(db_handle *db, const vecindex_spec *s) {
  char sql[200];
  snprintf(sql, sizeof sql,
           "CREATE TABLE IF NOT EXISTS \"%s\"(k TEXT PRIMARY KEY, v TEXT)",
           s->meta_table);
  sqlite3_exec(db->h, sql, NULL, NULL, NULL);
}

static int column_exists(db_handle *db, const char *table, const char *col) {
  char sql[200];
  snprintf(sql, sizeof sql, "SELECT 1 FROM pragma_table_info('%s') WHERE name=?1",
           table);
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(st, 1, col, -1, SQLITE_STATIC);
  int ok = sqlite3_step(st) == SQLITE_ROW;
  sqlite3_finalize(st);
  return ok;
}

/* WHY A SIDE TABLE NEXT TO THE vec0 TABLE. vec0 owns its own shadow tables
 * and a text primary key lookup through the virtual table is not a plain
 * B-tree probe. The done table is an ordinary table: NOT EXISTS against it is
 * an index seek, count(*) on it is the embedded count, and the hash is what
 * makes "re-fetched but unchanged" free. The two are written in one
 * transaction so they cannot disagree. */
void vecindex_ensure_side_tables(db_handle *db, const vecindex_spec *s) {
  char sql[400];
  if (s->done_table) {
    snprintf(sql, sizeof sql,
      "CREATE TABLE IF NOT EXISTS \"%s\"(\"%s\" TEXT PRIMARY KEY, "
      "text_hash INTEGER NOT NULL, embedded_at TEXT NOT NULL, meta_hash INTEGER)",
      s->done_table, s->key_col);
    sqlite3_exec(db->h, sql, NULL, NULL, NULL);
    /* A done table from before metadata columns existed. ADD COLUMN on an
     * ordinary table — the vec0 table itself is never altered. NULL reads as
     * "metadata unknown", so the next look at that row rewrites it once. */
    if (!column_exists(db, s->done_table, "meta_hash")) {
      snprintf(sql, sizeof sql, "ALTER TABLE \"%s\" ADD COLUMN meta_hash INTEGER",
               s->done_table);
      sqlite3_exec(db->h, sql, NULL, NULL, NULL);
    }
  }
  if (s->fail_table) {
    snprintf(sql, sizeof sql,
      "CREATE TABLE IF NOT EXISTS \"%s\"(\"%s\" TEXT PRIMARY KEY, "
      "text_hash INTEGER NOT NULL, failed_at TEXT NOT NULL, reason TEXT NOT NULL)",
      s->fail_table, s->key_col);
    sqlite3_exec(db->h, sql, NULL, NULL, NULL);
  }
}

static const char *coltype_sql(vecindex_coltype t) {
  return t == VI_INT ? "integer" : t == VI_FLOAT ? "float" : "text";
}

int vecindex_create(db_handle *db, const vecindex_spec *s, int dim) {
  if (dim <= 0) return -1;
  char sql[1024];
  size_t len = 0;
  int ok = 1;
  sql_appendf(sql, sizeof sql, &len, &ok,
    "CREATE VIRTUAL TABLE IF NOT EXISTS \"%s\" USING vec0(%s TEXT PRIMARY KEY",
    s->table, s->key_col);
  for (int i = 0; i < s->ncols; i++)
    sql_appendf(sql, sizeof sql, &len, &ok, ", %s %s%s", s->cols[i].name,
                coltype_sql(s->cols[i].type),
                s->cols[i].partition ? " partition key" : "");
  sql_appendf(sql, sizeof sql, &len, &ok, ", embedding float[%d])", dim);
  if (!ok) return -1;
  char *err = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "[vecindex] cannot create %s: %s\n", s->table, err ? err : "?");
    sqlite3_free(err);
    return -1;
  }
  return 0;
}

int vecindex_drop(db_handle *db, const vecindex_spec *s) {
  char sql[160];
  snprintf(sql, sizeof sql, "DROP TABLE IF EXISTS \"%s\"", s->table);
  return sqlite3_exec(db->h, sql, NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

int vecindex_meta_get(db_handle *db, const vecindex_spec *s, const char *k,
                      char *out, size_t cap) {
  if (out && cap) out[0] = 0;
  if (!db || !db->h) return 0;
  char sql[160];
  snprintf(sql, sizeof sql, "SELECT v FROM \"%s\" WHERE k=?1", s->meta_table);
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(st, 1, k, -1, SQLITE_STATIC);
  int found = 0;
  if (sqlite3_step(st) == SQLITE_ROW) {
    const char *v = (const char *)sqlite3_column_text(st, 0);
    if (out && cap) snprintf(out, cap, "%s", v ? v : "");
    found = 1;
  }
  sqlite3_finalize(st);
  return found;
}

char *vecindex_meta_dup(db_handle *db, const vecindex_spec *s, const char *k) {
  if (!db || !db->h) return NULL;
  char sql[160];
  snprintf(sql, sizeof sql, "SELECT v FROM \"%s\" WHERE k=?1", s->meta_table);
  sqlite3_stmt *st;
  char *out = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(st, 1, k, -1, SQLITE_STATIC);
  if (sqlite3_step(st) == SQLITE_ROW) {
    const unsigned char *v = sqlite3_column_text(st, 0);
    if (v) out = strdup((const char *)v);
  }
  sqlite3_finalize(st);
  return out;
}

int vecindex_meta_set(db_handle *db, const vecindex_spec *s, const char *k,
                      const char *v) {
  char sql[200];
  snprintf(sql, sizeof sql, "INSERT INTO \"%s\"(k,v) VALUES(?1,?2) "
           "ON CONFLICT(k) DO UPDATE SET v=excluded.v", s->meta_table);
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(st, 1, k, -1, SQLITE_STATIC);
  if (v) sqlite3_bind_text(st, 2, v, -1, SQLITE_TRANSIENT);
  else   sqlite3_bind_null(st, 2);
  int rc = sqlite3_step(st) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(st);
  return rc;
}

int vecindex_meta_set_long(db_handle *db, const vecindex_spec *s, const char *k,
                           long v) {
  char b[32];
  snprintf(b, sizeof b, "%ld", v);
  return vecindex_meta_set(db, s, k, b);
}

long vecindex_meta_long(db_handle *db, const vecindex_spec *s, const char *k) {
  char b[32];
  return vecindex_meta_get(db, s, k, b, sizeof b) ? strtol(b, NULL, 10) : 0;
}

/* ---- identity ------------------------------------------------------------ */

vecindex_identity vecindex_check_identity(db_handle *db, const vecindex_spec *s,
                                          const char *live_model, int live_dim,
                                          char *why, size_t cap) {
  if (why && cap) why[0] = 0;
  char model[256];
  vecindex_meta_get(db, s, "model", model, sizeof model);
  int dim = (int)vecindex_meta_long(db, s, "dim");
  if (!model[0] && dim == 0) return VI_ID_EMPTY;
  const char *hint = s->rebuild_hint ? s->rebuild_hint : "";
  if (model[0] && live_model && live_model[0] && strcmp(model, live_model) != 0) {
    if (why && cap)
      snprintf(why, cap, "index was built with model '%.200s' but the server "
               "is '%.200s'; refusing to mix%s", model, live_model, hint);
    return VI_ID_MODEL_MISMATCH;
  }
  if (dim && live_dim > 0 && live_dim != dim) {
    if (why && cap)
      snprintf(why, cap, "server returns %d-d vectors but the index is %d-d; "
               "refusing to mix%s", live_dim, dim, hint);
    return VI_ID_DIM_MISMATCH;
  }
  return VI_ID_OK;
}

int vecindex_record_identity(db_handle *db, const vecindex_spec *s,
                             const char *model, int dim) {
  return (vecindex_meta_set_long(db, s, "dim", dim) == 0 &&
          vecindex_meta_set(db, s, "model", model) == 0) ? 0 : -1;
}

int vecindex_refuse(db_handle *db, const vecindex_spec *s, const char *why) {
  fprintf(stderr, "[vecindex] %s: %s\n", s->name, why);
  return vecindex_meta_set(db, s, "refused", why);
}

int vecindex_refused(db_handle *db, const vecindex_spec *s, char *why, size_t cap) {
  if (why && cap) why[0] = 0;
  if (!db || !db->h || !vecindex_table_exists(db, s->meta_table)) return 0;
  char buf[600];
  if (!vecindex_meta_get(db, s, "refused", buf, sizeof buf) || !buf[0]) return 0;
  if (why && cap) snprintf(why, cap, "%s", buf);
  return 1;
}

/* ---- transactions -------------------------------------------------------- */

int vecindex_begin(db_handle *db) {
  return sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

int vecindex_end(db_handle *db, int ok) {
  if (ok && sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL) == SQLITE_OK) return 0;
  sqlite3_exec(db->h, "ROLLBACK", NULL, NULL, NULL);
  return -1;
}

void vecindex_sql_error(db_handle *db, const vecindex_spec *s, const char *now,
                        const char *what) {
  char msg[400];
  snprintf(msg, sizeof msg, "%s: %s failed: %.300s", now ? now : "", what,
           sqlite3_errmsg(db->h));
  fprintf(stderr, "[vecindex] %s: %s\n", s->name, msg);
  vecindex_meta_set(db, s, "last_error", msg);
}

/* ---- writing ------------------------------------------------------------- */

int vecindex_writer_open(vecindex_writer *w, db_handle *db, const vecindex_spec *s) {
  memset(w, 0, sizeof *w);
  w->db = db; w->s = s;
  char sql[1024];
  snprintf(sql, sizeof sql, "DELETE FROM \"%s\" WHERE %s=?1", s->table, s->key_col);
  if (sqlite3_prepare_v2(db->h, sql, -1, &w->del, NULL) != SQLITE_OK) goto fail;
  size_t len = 0;
  int ok = 1;
  sql_appendf(sql, sizeof sql, &len, &ok, "INSERT INTO \"%s\"(%s", s->table, s->key_col);
  for (int i = 0; i < s->ncols; i++)
    sql_appendf(sql, sizeof sql, &len, &ok, ",%s", s->cols[i].name);
  sql_appendf(sql, sizeof sql, &len, &ok, ",embedding) VALUES(?1");
  for (int i = 0; i < s->ncols + 1; i++)
    sql_appendf(sql, sizeof sql, &len, &ok, ",?%d", i + 2);
  sql_appendf(sql, sizeof sql, &len, &ok, ")");
  if (!ok) goto fail;
  if (sqlite3_prepare_v2(db->h, sql, -1, &w->ins, NULL) != SQLITE_OK) goto fail;
  if (s->done_table) {
    snprintf(sql, sizeof sql,
      "INSERT INTO \"%s\"(\"%s\",text_hash,embedded_at,meta_hash) VALUES(?1,?2,?3,?4) "
      "ON CONFLICT(\"%s\") DO UPDATE SET text_hash=excluded.text_hash, "
      "embedded_at=excluded.embedded_at, meta_hash=excluded.meta_hash",
      s->done_table, s->key_col, s->key_col);
    if (sqlite3_prepare_v2(db->h, sql, -1, &w->done, NULL) != SQLITE_OK) goto fail;
  }
  if (s->fail_table) {
    snprintf(sql, sizeof sql, "DELETE FROM \"%s\" WHERE \"%s\"=?1",
             s->fail_table, s->key_col);
    if (sqlite3_prepare_v2(db->h, sql, -1, &w->unfail, NULL) != SQLITE_OK) goto fail;
  }
  return 0;
fail:
  vecindex_writer_close(w);
  return -1;
}

static void bind_val(sqlite3_stmt *st, int idx, const vecindex_col *c,
                     const vecindex_val *v) {
  switch (c->type) {
    case VI_INT:
      sqlite3_bind_int64(st, idx, (v && !v->is_null) ? v->i : 0); break;
    case VI_FLOAT:
      sqlite3_bind_double(st, idx, (v && !v->is_null) ? v->f : 0.0); break;
    default:
      sqlite3_bind_text(st, idx, (v && !v->is_null && v->s) ? v->s : "", -1,
                        SQLITE_TRANSIENT);
  }
}

int vecindex_writer_put(vecindex_writer *w, const char *key, const float *vec,
                        int dim, const vecindex_val *vals,
                        unsigned long long text_hash,
                        unsigned long long meta_hash, const char *mark) {
  const vecindex_spec *s = w->s;
  sqlite3_reset(w->del);
  sqlite3_bind_text(w->del, 1, key, -1, SQLITE_STATIC);
  if (sqlite3_step(w->del) != SQLITE_DONE) return -1;
  sqlite3_reset(w->ins);
  sqlite3_clear_bindings(w->ins);
  sqlite3_bind_text(w->ins, 1, key, -1, SQLITE_STATIC);
  for (int i = 0; i < s->ncols; i++)
    bind_val(w->ins, i + 2, &s->cols[i], vals ? &vals[i] : NULL);
  sqlite3_bind_blob(w->ins, s->ncols + 2, vec, (int)(dim * sizeof(float)),
                    SQLITE_STATIC);
  if (sqlite3_step(w->ins) != SQLITE_DONE) return -1;
  if (w->done) {
    sqlite3_reset(w->done);
    sqlite3_bind_text(w->done, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_int64(w->done, 2, (sqlite3_int64)text_hash);
    sqlite3_bind_text(w->done, 3, mark ? mark : "", -1, SQLITE_STATIC);
    sqlite3_bind_int64(w->done, 4, (sqlite3_int64)meta_hash);
    if (sqlite3_step(w->done) != SQLITE_DONE) return -1;
  }
  if (w->unfail) {
    sqlite3_reset(w->unfail);
    sqlite3_bind_text(w->unfail, 1, key, -1, SQLITE_STATIC);
    if (sqlite3_step(w->unfail) != SQLITE_DONE) return -1;
  }
  return 0;
}

void vecindex_writer_close(vecindex_writer *w) {
  sqlite3_finalize(w->del); sqlite3_finalize(w->ins);
  sqlite3_finalize(w->done); sqlite3_finalize(w->unfail);
  w->del = w->ins = w->done = w->unfail = NULL;
}

int vecindex_load(db_handle *db, const vecindex_spec *s, const char *key,
                  float **out, int *dim) {
  *out = NULL; if (dim) *dim = 0;
  if (!vecindex_exists(db, s)) return 1;
  char sql[200];
  snprintf(sql, sizeof sql, "SELECT embedding FROM \"%s\" WHERE %s=?1",
           s->table, s->key_col);
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
  int rc = 1;
  if (sqlite3_step(st) == SQLITE_ROW) {
    int bytes = sqlite3_column_bytes(st, 0);
    const void *b = sqlite3_column_blob(st, 0);
    if (b && bytes > 0 && bytes % (int)sizeof(float) == 0) {
      float *v = malloc((size_t)bytes);
      if (v) {
        memcpy(v, b, (size_t)bytes);
        *out = v;
        if (dim) *dim = bytes / (int)sizeof(float);
        rc = 0;
      } else rc = -1;
    } else rc = -1;
  }
  sqlite3_finalize(st);
  return rc;
}

double vec_cosine(const float *a, const float *b, int dim) {
  double ab = 0, aa = 0, bb = 0;
  for (int i = 0; i < dim; i++) {
    ab += (double)a[i] * b[i];
    aa += (double)a[i] * a[i];
    bb += (double)b[i] * b[i];
  }
  if (aa <= 0 || bb <= 0) return 0;
  return ab / (sqrt(aa) * sqrt(bb));
}

unsigned long long vecindex_hash(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  for (; s && *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h;
}

unsigned long long vecindex_vals_hash(const vecindex_spec *s, const vecindex_val *vals) {
  unsigned long long h = 1469598103934665603ULL;
  for (int i = 0; vals && i < s->ncols; i++) {
    char buf[64];
    const char *p;
    const vecindex_val *v = &vals[i];
    if (s->cols[i].type == VI_INT) { snprintf(buf, sizeof buf, "%lld", v->is_null ? 0 : v->i); p = buf; }
    else if (s->cols[i].type == VI_FLOAT) { snprintf(buf, sizeof buf, "%.17g", v->is_null ? 0.0 : v->f); p = buf; }
    else p = (!v->is_null && v->s) ? v->s : "";
    for (; *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211ULL; }
    h ^= 0x1f; h *= 1099511628211ULL;      /* field separator */
  }
  return h;
}

/* ---- hooks --------------------------------------------------------------- */

#define MAX_HOOKS 16
static struct { const vecindex_spec *s; vecindex_hook_fn fn; void *user; } g_hooks[MAX_HOOKS];
static int g_nhooks = 0;
static pthread_mutex_t g_hook_mu = PTHREAD_MUTEX_INITIALIZER;

int vecindex_hook_register(const vecindex_spec *s, vecindex_hook_fn fn, void *user) {
  if (!s || !fn) return -1;
  pthread_mutex_lock(&g_hook_mu);
  int rc = -1;
  if (g_nhooks < MAX_HOOKS) {
    g_hooks[g_nhooks].s = s; g_hooks[g_nhooks].fn = fn; g_hooks[g_nhooks].user = user;
    g_nhooks++;
    rc = 0;
  }
  pthread_mutex_unlock(&g_hook_mu);
  return rc;
}

void vecindex_hook_unregister(const vecindex_spec *s, vecindex_hook_fn fn, void *user) {
  pthread_mutex_lock(&g_hook_mu);
  for (int i = 0; i < g_nhooks; i++) {
    if (g_hooks[i].s != s || g_hooks[i].fn != fn || g_hooks[i].user != user) continue;
    memmove(&g_hooks[i], &g_hooks[i + 1], (size_t)(g_nhooks - i - 1) * sizeof g_hooks[0]);
    g_nhooks--;
    break;
  }
  pthread_mutex_unlock(&g_hook_mu);
}

void vecindex_fire_hooks(db_handle *db, const vecindex_batch *b) {
  if (!b || b->n <= 0) return;
  /* Copy under the lock, call outside it: a hook that registers another (or
   * takes long) must not hold every other writer's registration hostage. */
  pthread_mutex_lock(&g_hook_mu);
  int n = 0;
  vecindex_hook_fn fns[MAX_HOOKS]; void *users[MAX_HOOKS];
  for (int i = 0; i < g_nhooks; i++)
    if (g_hooks[i].s == b->spec) { fns[n] = g_hooks[i].fn; users[n] = g_hooks[i].user; n++; }
  pthread_mutex_unlock(&g_hook_mu);
  for (int i = 0; i < n; i++) fns[i](db, b, users[i]);
}

/* ---- incremental embedding ---------------------------------------------- */

void vecindex_item_state(db_handle *db, const vecindex_spec *s, vecindex_item *it) {
  it->hash = vecindex_hash(it->text);
  it->meta_hash = vecindex_vals_hash(s, it->vals);
  it->in_done = it->in_failed = it->has_done_meta = 0;
  it->done_hash = it->done_meta_hash = it->failed_hash = 0;
  char sql[256];
  sqlite3_stmt *st;
  if (s->done_table && vecindex_table_exists(db, s->done_table)) {
    snprintf(sql, sizeof sql, "SELECT text_hash, meta_hash FROM \"%s\" WHERE \"%s\"=?1",
             s->done_table, s->key_col);
    if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) == SQLITE_OK) {
      sqlite3_bind_text(st, 1, it->key, -1, SQLITE_STATIC);
      if (sqlite3_step(st) == SQLITE_ROW) {
        it->in_done = 1;
        it->done_hash = (unsigned long long)sqlite3_column_int64(st, 0);
        it->has_done_meta = sqlite3_column_type(st, 1) != SQLITE_NULL;
        it->done_meta_hash = (unsigned long long)sqlite3_column_int64(st, 1);
      }
      sqlite3_finalize(st);
    }
  }
  if (s->fail_table && vecindex_table_exists(db, s->fail_table)) {
    snprintf(sql, sizeof sql, "SELECT text_hash FROM \"%s\" WHERE \"%s\"=?1",
             s->fail_table, s->key_col);
    if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) == SQLITE_OK) {
      sqlite3_bind_text(st, 1, it->key, -1, SQLITE_STATIC);
      if (sqlite3_step(st) == SQLITE_ROW) {
        it->in_failed = 1;
        it->failed_hash = (unsigned long long)sqlite3_column_int64(st, 0);
      }
      sqlite3_finalize(st);
    }
  }
}

int vecindex_item_unchanged(const vecindex_item *it) {
  return (it->in_done && it->done_hash == it->hash) ||
         (!it->in_done && it->in_failed && it->failed_hash == it->hash);
}

/* Does a settled-and-unchanged done row need its metadata rewritten? */
static int meta_stale(const vecindex_spec *s, const vecindex_item *it) {
  return s->ncols > 0 && it->in_done && it->done_hash == it->hash &&
         (!it->has_done_meta || it->done_meta_hash != it->meta_hash);
}

int vecindex_mark_seen(db_handle *db, const vecindex_spec *s,
                       vecindex_item **rows, int n, const char *mark,
                       const char *now, vecindex_stats *st) {
  if (n <= 0) return 0;
  if (vecindex_begin(db) != 0) { vecindex_sql_error(db, s, now, "BEGIN IMMEDIATE"); return -1; }
  char sql[256];
  sqlite3_stmt *ud = NULL, *uf = NULL;
  int ok = 1;
  if (s->done_table) {
    snprintf(sql, sizeof sql, "UPDATE \"%s\" SET embedded_at=?2 WHERE \"%s\"=?1",
             s->done_table, s->key_col);
    ok = sqlite3_prepare_v2(db->h, sql, -1, &ud, NULL) == SQLITE_OK;
  }
  if (ok && s->fail_table) {
    snprintf(sql, sizeof sql, "UPDATE \"%s\" SET failed_at=?2 WHERE \"%s\"=?1",
             s->fail_table, s->key_col);
    ok = sqlite3_prepare_v2(db->h, sql, -1, &uf, NULL) == SQLITE_OK;
  }
  vecindex_writer w = {0};
  int have_w = 0;
  for (int i = 0; ok && i < n; i++) {
    vecindex_item *it = rows[i];
    if (meta_stale(s, it)) {
      /* Same text, different metadata (a re-fetch moved published_at, say):
       * the vector is still right, its filter columns are not. Rewritten from
       * the stored vector — re-embedding unchanged text would spend the
       * server's time on an answer we already have. A vector that cannot be
       * read back is left alone: the done row keeps its old meta_hash, so the
       * next look tries again. */
      float *v = NULL; int d = 0;
      if (vecindex_load(db, s, it->key, &v, &d) == 0) {
        if (!have_w) { ok = vecindex_writer_open(&w, db, s) == 0; have_w = ok; }
        if (ok) ok = vecindex_writer_put(&w, it->key, v, d, it->vals, it->hash,
                                         it->meta_hash, mark) == 0;
        if (ok && st) st->rewritten++;
        free(v);
        continue;
      }
      free(v);
    }
    sqlite3_stmt *u = (it->in_done && it->done_hash == it->hash) ? ud : uf;
    if (!u) continue;
    sqlite3_reset(u);
    sqlite3_bind_text(u, 1, it->key, -1, SQLITE_STATIC);
    sqlite3_bind_text(u, 2, mark, -1, SQLITE_STATIC);
    if (sqlite3_step(u) != SQLITE_DONE) ok = 0;
  }
  if (!ok) vecindex_sql_error(db, s, now, "marking unchanged rows");
  if (have_w) vecindex_writer_close(&w);
  sqlite3_finalize(ud); sqlite3_finalize(uf);
  if (vecindex_end(db, ok) != 0) {
    if (ok) vecindex_sql_error(db, s, now, "COMMIT");
    return -1;
  }
  return 0;
}

/* Store vectors for rows[0..m). On the very first write (*dim_io == 0) the
 * vec0 table is created and model + dim are recorded INSIDE the same
 * transaction, so the meta never names a model nothing was embedded with.
 * Hooks fire after the commit. Returns 0, -1 (SQL failure, recorded), or -2
 * (dimension refused). */
static int write_rows(db_handle *db, const vecindex_spec *s, vecindex_item **rows,
                      int m, const float *vecs, int dim, int *dim_io,
                      const char *model, const char *mark, const char *now) {
  if (*dim_io && dim != *dim_io) {
    char msg[400];
    snprintf(msg, sizeof msg, "server returned dim %d but the index is %d-d; "
             "refusing to mix%s", dim, *dim_io, s->rebuild_hint ? s->rebuild_hint : "");
    vecindex_refuse(db, s, msg);
    return -2;
  }
  if (vecindex_begin(db) != 0) { vecindex_sql_error(db, s, now, "BEGIN IMMEDIATE"); return -1; }
  int ok = 1, first = (*dim_io == 0);
  if (first) {
    ok = vecindex_create(db, s, dim) == 0 &&
         vecindex_record_identity(db, s, model, dim) == 0;
    if (!ok) vecindex_sql_error(db, s, now, "creating the vector index");
  }
  vecindex_writer w;
  if (ok && vecindex_writer_open(&w, db, s) != 0) { ok = 0; vecindex_sql_error(db, s, now, "prepare"); }
  else if (ok) {
    for (int j = 0; ok && j < m; j++) {
      vecindex_item *p = rows[j];
      if (vecindex_writer_put(&w, p->key, vecs + (size_t)j * dim, dim, p->vals,
                              p->hash, p->meta_hash, mark) != 0) {
        ok = 0;
        vecindex_sql_error(db, s, now, "writing a vector");
      }
    }
    vecindex_writer_close(&w);
  }
  if (vecindex_end(db, ok) != 0) {
    if (ok) vecindex_sql_error(db, s, now, "COMMIT");
    return -1;
  }
  if (first) *dim_io = dim;
  /* Post-batch hooks see exactly what committed. */
  const char **keys = malloc((size_t)m * sizeof *keys);
  vecindex_val *vals = s->ncols ? malloc((size_t)m * (size_t)s->ncols * sizeof *vals) : NULL;
  if (keys && (!s->ncols || vals)) {
    for (int j = 0; j < m; j++) {
      keys[j] = rows[j]->key;
      for (int c = 0; c < s->ncols; c++)
        vals[(size_t)j * s->ncols + c] = rows[j]->vals ? rows[j]->vals[c]
                                                       : (vecindex_val){ .is_null = 1 };
    }
    vecindex_batch b = { .spec = s, .n = m, .dim = dim, .keys = keys,
                         .vecs = vecs, .vals = vals };
    vecindex_fire_hooks(db, &b);
  }
  free(keys); free(vals);
  return 0;
}

/* Record one row the server will not embed. If an OLDER text of the row was
 * embedded, that vector is removed with it: the index must not keep claiming
 * a row by text the row no longer has. */
static int record_failure(db_handle *db, const vecindex_spec *s, vecindex_item *p,
                          const char *reason, const char *mark, const char *now) {
  if (!s->fail_table) return 0;
  if (vecindex_begin(db) != 0) { vecindex_sql_error(db, s, now, "BEGIN IMMEDIATE"); return -1; }
  int ok = 1;
  char sql[300];
  sqlite3_stmt *st = NULL;
  if (p->in_done) {
    if (vecindex_exists(db, s)) {
      snprintf(sql, sizeof sql, "DELETE FROM \"%s\" WHERE %s=?1", s->table, s->key_col);
      ok = sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) == SQLITE_OK;
      if (ok) { sqlite3_bind_text(st, 1, p->key, -1, SQLITE_STATIC); ok = sqlite3_step(st) == SQLITE_DONE; }
      sqlite3_finalize(st); st = NULL;
    }
    if (ok && s->done_table) {
      snprintf(sql, sizeof sql, "DELETE FROM \"%s\" WHERE \"%s\"=?1", s->done_table, s->key_col);
      ok = sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) == SQLITE_OK;
      if (ok) { sqlite3_bind_text(st, 1, p->key, -1, SQLITE_STATIC); ok = sqlite3_step(st) == SQLITE_DONE; }
      sqlite3_finalize(st); st = NULL;
    }
  }
  if (ok) {
    snprintf(sql, sizeof sql,
      "INSERT INTO \"%s\"(\"%s\",text_hash,failed_at,reason) VALUES(?1,?2,?3,?4) "
      "ON CONFLICT(\"%s\") DO UPDATE SET text_hash=excluded.text_hash, "
      "failed_at=excluded.failed_at, reason=excluded.reason",
      s->fail_table, s->key_col, s->key_col);
    if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) == SQLITE_OK) {
      sqlite3_bind_text(st, 1, p->key, -1, SQLITE_STATIC);
      sqlite3_bind_int64(st, 2, (sqlite3_int64)p->hash);
      sqlite3_bind_text(st, 3, mark, -1, SQLITE_STATIC);
      sqlite3_bind_text(st, 4, reason, -1, SQLITE_STATIC);
      ok = sqlite3_step(st) == SQLITE_DONE;
    } else ok = 0;
    sqlite3_finalize(st);
  }
  if (!ok) vecindex_sql_error(db, s, now, "recording a failed row");
  if (vecindex_end(db, ok) != 0) {
    if (ok) vecindex_sql_error(db, s, now, "COMMIT");
    return -1;
  }
  return 0;
}

/* A failure that says nothing about the INPUT: nothing answered, it ran out
 * of time, or the server said "not now". Such a row is retried, never
 * recorded as failed. */
static int transient(llm_status st, long http) {
  if (st == LLM_ERR_UNREACHABLE || st == LLM_ERR_TIMEOUT) return 1;
  /* never sent at all: the worker's queue was busy, not the input bad */
  if (st == LLM_ERR_QUEUE_TIMEOUT || st == LLM_ERR_QUEUE_FULL) return 1;
  if (st == LLM_ERR_HTTP && (http == 0 || http == 503 || http == 429)) return 1;
  return st == LLM_ERR_BAD_REQUEST;           /* our side: OOM building JSON */
}

static void fail_reason(char *buf, size_t cap, llm_status st, long http) {
  if (st == LLM_ERR_HTTP) snprintf(buf, cap, "%s %ld", llm_status_code(st), http);
  else snprintf(buf, cap, "%s", llm_status_code(st));
}

/* The server works, in general, right now? One known-good input. Used only
 * when EVERY row of a split batch failed on its own — then either all of
 * those rows are bad, or the server is, and recording them as failed must
 * wait until it is clear which. */
static int server_answers(llm_client *llm, int dim) {
  int pdim = vecindex_probe_dim(llm, 30000, NULL);
  return pdim > 0 && (dim == 0 || pdim == dim);
}

int vecindex_embed_items(db_handle *db, const vecindex_spec *s, llm_client *llm,
                         vecindex_item **rows, int m, int *dim_io,
                         const char *model, const char *mark, const char *now,
                         vecindex_stats *ts, int *n_done) {
  *n_done = 0;
  if (m <= 0) return 0;
  if (m > 256) m = 256;
  const char *texts[256];
  for (int i = 0; i < m; i++) texts[i] = rows[i]->text;
  float *vecs = NULL; int dim = 0; llm_status st; long http = 0;
  ts->sent += m;
  if (llm_embed_ex(llm, texts, m, &vecs, &dim, 120000, &st, &http) == 0) {
    int w = write_rows(db, s, rows, m, vecs, dim, dim_io, model, mark, now);
    free(vecs);
    if (w != 0) return w;
    ts->embedded += m;
    *n_done = m;
    return 0;
  }
  char why[96];
  fail_reason(why, sizeof why, st, http);
  if (transient(st, http)) {
    /* 240, not 200: `now` is char[40] and `why` is char[96], and with the
     * 56-byte literal that is 203 worst case — so the trailing "); retried
     * next tick" was what got cut, turning a transient-retry message into one
     * that reads like a permanent failure. The message is STORED (meta
     * last_error), so a truncated tail is wrong data, not an ugly log line. */
    char msg[240];
    snprintf(msg, sizeof msg, "%s: /v1/embeddings failed for a batch of %d (%s); "
             "retried next tick", now, m, why);
    vecindex_meta_set(db, s, "last_error", msg);
    fprintf(stderr, "[vecindex] %s: %s\n", s->name, msg);
    return -1;
  }

  /* The server ANSWERED and refused this batch. One bad input fails the whole
   * request, so find out which: one row at a time, in order. Failures are
   * held until the server is shown to work — by any row succeeding, or by
   * the probe — so a server that refuses everything never gets the corpus
   * recorded as "failed". */
  fprintf(stderr, "[vecindex] %s: batch of %d refused (%s); retrying row by row\n",
          s->name, m, why);
  int pend[256], np = 0, any_ok = 0, rc = 0, stop = m;
  char preason[256][48];
  for (int i = 0; i < m; i++) {
    float *v1 = NULL; int d1 = 0; llm_status s1; long h1 = 0;
    const char *t1[1] = { rows[i]->text };
    if (llm_embed_ex(llm, t1, 1, &v1, &d1, 120000, &s1, &h1) == 0) {
      int w = write_rows(db, s, &rows[i], 1, v1, d1, dim_io, model, mark, now);
      free(v1);
      if (w != 0) { rc = w; stop = i; break; }
      ts->embedded++;
      any_ok = 1;
      continue;
    }
    if (transient(s1, h1)) {
      char msg[200], w1[96];
      fail_reason(w1, sizeof w1, s1, h1);
      snprintf(msg, sizeof msg, "%s: /v1/embeddings failed mid-split (%s); retried next tick",
               now, w1);
      vecindex_meta_set(db, s, "last_error", msg);
      rc = -1; stop = i;
      break;
    }
    fail_reason(preason[np], sizeof preason[np], s1, h1);
    pend[np++] = i;
  }
  if (np > 0) {
    if (!any_ok && !server_answers(llm, *dim_io)) {
      char msg[200];
      snprintf(msg, sizeof msg, "%s: every row of a batch of %d was refused and the "
               "dimension probe failed too — treating it as a server fault; nothing "
               "recorded, retried next tick", now, m);
      vecindex_meta_set(db, s, "last_error", msg);
      fprintf(stderr, "[vecindex] %s: %s\n", s->name, msg);
      /* Nothing at or after the first refused row is settled. */
      if (pend[0] < stop) stop = pend[0];
      rc = -1;
    } else {
      for (int k = 0; k < np; k++) {
        if (pend[k] >= stop) break;
        char reason[160];
        snprintf(reason, sizeof reason, "embedding server refused this input (%.47s)",
                 preason[k]);
        if (record_failure(db, s, rows[pend[k]], reason, mark, now) != 0) {
          rc = -1; stop = pend[k];
          break;
        }
        ts->failed++;
        fprintf(stderr, "[vecindex] %s: row %s recorded as failed: %s\n", s->name,
                rows[pend[k]]->key, reason);
      }
    }
  }
  *n_done = stop;
  return rc;
}

int vecindex_sync(db_handle *db, const vecindex_spec *s, llm_client *llm,
                  const char *model, vecindex_item *items, int n, int batch,
                  const char *mark, const char *now, vecindex_stats *st) {
  vecindex_stats local = {0};
  if (!st) st = &local;
  if (batch <= 0 || batch > 256) batch = 32;
  vecindex_ensure_meta(db, s);
  vecindex_ensure_side_tables(db, s);
  /* Identity before anything is written: the live dimension, probed, never
   * read from another index. */
  llm_status pst;
  int live_dim = vecindex_probe_dim(llm, 30000, &pst);
  if (live_dim <= 0) {
    char msg[200];
    snprintf(msg, sizeof msg, "%s: /v1/embeddings unreachable for the dimension "
             "probe (%s)", now, llm_status_code(pst));
    vecindex_meta_set(db, s, "last_error", msg);
    return -1;
  }
  char why[600];
  vecindex_identity id = vecindex_check_identity(db, s, model, live_dim, why, sizeof why);
  if (id == VI_ID_MODEL_MISMATCH || id == VI_ID_DIM_MISMATCH) {
    vecindex_refuse(db, s, why);
    return -2;
  }
  vecindex_meta_set(db, s, "refused", NULL);
  int dim = (int)vecindex_meta_long(db, s, "dim");
  if (dim && vecindex_create(db, s, dim) != 0) {
    vecindex_sql_error(db, s, now, "opening the vector index");
    return -1;
  }
  vecindex_item **seen = malloc((size_t)(n > 0 ? n : 1) * sizeof *seen);
  vecindex_item **need = malloc((size_t)(n > 0 ? n : 1) * sizeof *need);
  if (!seen || !need) { free(seen); free(need); return -1; }
  int ns = 0, nn = 0;
  for (int i = 0; i < n; i++) {
    vecindex_item_state(db, s, &items[i]);
    if (vecindex_item_unchanged(&items[i])) {
      seen[ns++] = &items[i];
      if (items[i].in_done) st->unchanged++; else st->failed_seen++;
    } else need[nn++] = &items[i];
  }
  int rc = vecindex_mark_seen(db, s, seen, ns, mark, now, st);
  for (int off = 0; rc == 0 && off < nn; ) {
    int cnt = nn - off < batch ? nn - off : batch;
    int done_n = 0;
    rc = vecindex_embed_items(db, s, llm, &need[off], cnt, &dim, model, mark,
                              now, st, &done_n);
    off += done_n;
    if (rc == 0 && done_n < cnt) break;
  }
  st->walked += n;
  free(seen); free(need);
  vecindex_meta_set_long(db, s, "embedded_count",
                         s->done_table ? vecindex_count(db, s->done_table) : 0);
  vecindex_meta_set(db, s, "last_run_at", now);
  if (rc == 0) vecindex_meta_set(db, s, "last_error", NULL);
  return rc;
}

/* ---- coverage ------------------------------------------------------------ */

static void add_meta_str(cJSON *o, const char *key, db_handle *db,
                         const vecindex_spec *s, const char *mk, int have_meta) {
  char buf[600];
  if (have_meta && vecindex_meta_get(db, s, mk, buf, sizeof buf) && buf[0])
    cJSON_AddStringToObject(o, key, buf);
  else cJSON_AddNullToObject(o, key);
}

/* embedded and eligible as read from meta; -1 = not measured. */
static void counts(db_handle *db, const vecindex_spec *s, int have_meta,
                   long *embedded, long *eligible, long *failed) {
  char buf[64];
  *embedded = have_meta ? vecindex_meta_long(db, s, "embedded_count") : 0;
  *eligible = (have_meta && vecindex_meta_get(db, s, "eligible_count", buf, sizeof buf) && buf[0])
              ? strtol(buf, NULL, 10) : -1;
  *failed = (s->fail_table && vecindex_table_exists(db, s->fail_table))
            ? vecindex_count(db, s->fail_table) : 0;
}

cJSON *vecindex_coverage_json(db_handle *db, const vecindex_spec *s) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "index", s->name);
  cJSON_AddStringToObject(o, "table", s->table);
  cJSON_AddBoolToObject(o, "index_present", vecindex_exists(db, s));
  int have_meta = db && db->h && vecindex_table_exists(db, s->meta_table);
  add_meta_str(o, "model", db, s, "model", have_meta);
  cJSON_AddNumberToObject(o, "dim", have_meta ? (double)vecindex_meta_long(db, s, "dim") : 0);
  long emb, elig, failed;
  counts(db, s, have_meta, &emb, &elig, &failed);
  cJSON_AddNumberToObject(o, "embedded_count", (double)emb);
  /* eligible_count is a measured number with a timestamp, or null. */
  if (elig >= 0) {
    cJSON_AddNumberToObject(o, "eligible_count", (double)elig);
    add_meta_str(o, "eligible_counted_at", db, s, "eligible_counted_at", have_meta);
  } else {
    cJSON_AddNullToObject(o, "eligible_count");
    cJSON_AddNullToObject(o, "eligible_counted_at");
  }
  /* Rows the server would not embed, as data: the count is exact, the list
   * is a bounded sample (newest first) with each row's reason. */
  cJSON_AddNumberToObject(o, "failed_count", (double)failed);
  cJSON *fs = cJSON_CreateArray();
  if (failed > 0) {
    char sql[256];
    snprintf(sql, sizeof sql, "SELECT \"%s\", reason, failed_at FROM \"%s\" "
             "ORDER BY failed_at DESC, \"%s\" LIMIT ?1", s->key_col, s->fail_table,
             s->key_col);
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) == SQLITE_OK) {
      sqlite3_bind_int(st, 1, FAIL_SAMPLE);
      while (sqlite3_step(st) == SQLITE_ROW) {
        cJSON *f = cJSON_CreateObject();
        const char *u = (const char *)sqlite3_column_text(st, 0);
        const char *r = (const char *)sqlite3_column_text(st, 1);
        const char *a = (const char *)sqlite3_column_text(st, 2);
        cJSON_AddStringToObject(f, "uid", u ? u : "");
        cJSON_AddStringToObject(f, "reason", r ? r : "");
        cJSON_AddStringToObject(f, "checked_at", a ? a : "");
        cJSON_AddItemToArray(fs, f);
      }
      sqlite3_finalize(st);
    }
  }
  cJSON_AddItemToObject(o, "failed_sample", fs);
  /* Backlog = eligible rows neither embedded nor recorded as failed: only
   * when eligible was measured, never below zero (eligible is refreshed
   * lazily, so a burst of new rows can briefly put embedded above it). */
  if (elig >= 0) {
    long b = elig - emb - failed;
    cJSON_AddNumberToObject(o, "backlog", (double)(b > 0 ? b : 0));
  } else cJSON_AddNullToObject(o, "backlog");
  add_meta_str(o, "state", db, s, "state", have_meta);
  add_meta_str(o, "refused", db, s, "refused", have_meta);
  add_meta_str(o, "last_error", db, s, "last_error", have_meta);
  add_meta_str(o, "last_run_at", db, s, "last_run_at", have_meta);
  char buf[600];
  if (have_meta && vecindex_meta_get(db, s, "last_tick", buf, sizeof buf) && buf[0]) {
    cJSON *lt = cJSON_Parse(buf);
    cJSON_AddItemToObject(o, "last_tick", lt ? lt : cJSON_CreateNull());
  } else cJSON_AddNullToObject(o, "last_tick");
  return o;
}

cJSON *vecindex_coverage_compact(db_handle *db, const vecindex_spec *s) {
  cJSON *o = cJSON_CreateObject();
  int have_meta = db && db->h && vecindex_table_exists(db, s->meta_table);
  long emb, elig, failed;
  counts(db, s, have_meta, &emb, &elig, &failed);
  cJSON_AddNumberToObject(o, "embedded", (double)emb);
  if (elig >= 0) cJSON_AddNumberToObject(o, "eligible", (double)elig);
  else cJSON_AddNullToObject(o, "eligible");
  cJSON_AddNumberToObject(o, "failed", (double)failed);
  add_meta_str(o, "refused", db, s, "refused", have_meta);
  if (elig >= 0) {
    long b = elig - emb - failed;
    cJSON_AddNumberToObject(o, "backlog", (double)(b > 0 ? b : 0));
  } else cJSON_AddNullToObject(o, "backlog");
  add_meta_str(o, "model", db, s, "model", have_meta);
  cJSON_AddNumberToObject(o, "dim", have_meta ? (double)vecindex_meta_long(db, s, "dim") : 0);
  add_meta_str(o, "state", db, s, "state", have_meta);
  add_meta_str(o, "last_run", db, s, "last_run_at", have_meta);
  return o;
}

#define MAX_INDEXES 16
static const vecindex_spec *g_specs[MAX_INDEXES];
static int g_nspecs = 0;

/* Called from constructors (VECINDEX_REGISTER), before main and before any
 * thread exists, so no lock. A duplicate registration is ignored. */
void vecindex_register(const vecindex_spec *s) {
  for (int i = 0; i < g_nspecs; i++) if (g_specs[i] == s) return;
  if (g_nspecs < MAX_INDEXES) g_specs[g_nspecs++] = s;
  else fprintf(stderr, "[vecindex] more than %d indexes; %s not listed in status\n",
               MAX_INDEXES, s->name);
}

int vecindex_all(const vecindex_spec *const **out) {
  if (out) *out = g_specs;
  return g_nspecs;
}

cJSON *vecindex_status_json(db_handle *db) {
  cJSON *o = cJSON_CreateObject();
  for (int i = 0; i < g_nspecs; i++)
    cJSON_AddItemToObject(o, g_specs[i]->name, vecindex_coverage_compact(db, g_specs[i]));
  return o;
}

/* ---- querying ------------------------------------------------------------ */

static const vecindex_col *find_col(const vecindex_spec *s, const char *name) {
  for (int i = 0; name && i < s->ncols; i++)
    if (!strcmp(s->cols[i].name, name)) return &s->cols[i];
  return NULL;
}

static const char *sql_op(const char *op) {
  static const char *ok[] = { "=", "!=", "<", "<=", ">", ">=", NULL };
  for (int i = 0; op && ok[i]; i++) if (!strcmp(op, ok[i])) return ok[i];
  return NULL;
}

void vecindex_hits_free(vecindex_hit *h, int n) {
  if (!h) return;
  for (int i = 0; i < n; i++) free(h[i].key);
  free(h);
}

static int hit_cmp(const void *a, const void *b) {
  const vecindex_hit *x = a, *y = b;
  if (x->distance != y->distance) return x->distance < y->distance ? -1 : 1;
  return strcmp(x->key, y->key);
}

/* One KNN with every non-partition filter, and the partition filter pinned to
 * `pval` (NULL = no partition constraint). Appends into *out. */
static int knn_once(db_handle *db, const vecindex_spec *s, const float *q, int dim,
                    int k, const vecindex_filter *f, int nf,
                    const vecindex_col *pcol, const vecindex_val *pval,
                    vecindex_hit **out, int *n, int *cap, char *err, size_t errcap) {
  char sql[2048];
  size_t len = 0;
  int ok = 1;
  sql_appendf(sql, sizeof sql, &len, &ok, "SELECT %s, distance FROM \"%s\" WHERE "
              "embedding MATCH ?1 AND k = ?2", s->key_col, s->table);
  int arg = 3;
  if (pcol && pval)
    sql_appendf(sql, sizeof sql, &len, &ok, " AND %s = ?%d", pcol->name, arg++);
  for (int i = 0; i < nf; i++) {
    const vecindex_col *c = find_col(s, f[i].col);
    if (c == pcol) continue;                     /* handled per partition */
    if (!strcmp(f[i].op, "IN")) {
      sql_appendf(sql, sizeof sql, &len, &ok, " AND %s IN (", c->name);
      for (int j = 0; j < f[i].nvals; j++)
        sql_appendf(sql, sizeof sql, &len, &ok, "%s?%d", j ? "," : "", arg++);
      sql_appendf(sql, sizeof sql, &len, &ok, ")");
    } else {
      sql_appendf(sql, sizeof sql, &len, &ok, " AND %s %s ?%d", c->name,
                  sql_op(f[i].op), arg++);
    }
  }
  sql_appendf(sql, sizeof sql, &len, &ok, " ORDER BY distance");
  if (!ok) { snprintf(err, errcap, "filter too long"); return -1; }
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) {
    snprintf(err, errcap, "%s", sqlite3_errmsg(db->h));
    return -1;
  }
  sqlite3_bind_blob(st, 1, q, (int)(dim * sizeof(float)), SQLITE_STATIC);
  sqlite3_bind_int(st, 2, k);
  arg = 3;
  if (pcol && pval) bind_val(st, arg++, pcol, pval);
  for (int i = 0; i < nf; i++) {
    const vecindex_col *c = find_col(s, f[i].col);
    if (c == pcol) continue;
    int nv = strcmp(f[i].op, "IN") ? 1 : f[i].nvals;
    for (int j = 0; j < nv; j++) bind_val(st, arg++, c, &f[i].vals[j]);
  }
  int rc;
  while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
    const char *key = (const char *)sqlite3_column_text(st, 0);
    if (!key) continue;
    if (*n == *cap) {
      int ncap = *cap ? *cap * 2 : 64;
      vecindex_hit *nh = realloc(*out, (size_t)ncap * sizeof *nh);
      if (!nh) { rc = SQLITE_NOMEM; break; }
      *out = nh; *cap = ncap;
    }
    (*out)[*n].key = strdup(key);
    (*out)[*n].distance = sqlite3_column_double(st, 1);
    if (!(*out)[*n].key) { rc = SQLITE_NOMEM; break; }
    (*n)++;
  }
  if (rc != SQLITE_DONE) {
    snprintf(err, errcap, "%s", rc == SQLITE_NOMEM ? "out of memory" : sqlite3_errmsg(db->h));
    sqlite3_finalize(st);
    return -1;
  }
  sqlite3_finalize(st);
  return 0;
}

int vecindex_knn(db_handle *db, const vecindex_spec *s, const float *q, int dim,
                 int k, const vecindex_filter *f, int nf, vecindex_hit **out,
                 char *err, size_t errcap) {
  char ebuf[8];
  if (!err || !errcap) { err = ebuf; errcap = sizeof ebuf; }
  err[0] = 0;
  *out = NULL;
  if (!q || dim <= 0 || k <= 0) { snprintf(err, errcap, "bad query"); return -1; }
  /* Validate every filter BEFORE any SQL: a filter naming a column the index
   * does not have must fail, not run unfiltered — an unfiltered answer to a
   * filtered question is the confident wrong answer of CLAUDE.md 4d. */
  const vecindex_col *pcol = NULL;
  const vecindex_filter *pf = NULL;
  for (int i = 0; i < nf; i++) {
    const vecindex_col *c = find_col(s, f[i].col);
    if (!c) { snprintf(err, errcap, "%s has no column '%s'", s->table, f[i].col ? f[i].col : "?"); return -1; }
    int is_in = f[i].op && !strcmp(f[i].op, "IN");
    if (!is_in && !sql_op(f[i].op)) { snprintf(err, errcap, "bad operator on %s", c->name); return -1; }
    if (f[i].nvals < 1 || !f[i].vals || (!is_in && f[i].nvals != 1)) {
      snprintf(err, errcap, "filter on %s needs %s", c->name, is_in ? "one or more values" : "exactly one value");
      return -1;
    }
    if (c->partition) {
      if (pf) { snprintf(err, errcap, "two filters on the partition key %s", c->name); return -1; }
      if (!is_in && strcmp(f[i].op, "=")) {
        snprintf(err, errcap, "partition key %s takes = or IN", c->name);
        return -1;
      }
      pcol = c; pf = &f[i];
    } else if (is_in && c->type == VI_FLOAT) {
      snprintf(err, errcap, "IN is only available on TEXT/INT metadata (%s)", c->name);
      return -1;
    }
  }
  vecindex_hit *h = NULL;
  int n = 0, cap = 0;
  int np = pf ? pf->nvals : 1;
  for (int p = 0; p < np; p++) {
    if (knn_once(db, s, q, dim, k, f, nf, pcol, pf ? &pf->vals[p] : NULL,
                 &h, &n, &cap, err, errcap) != 0) {
      vecindex_hits_free(h, n);
      return -1;
    }
  }
  /* Several partitions each answered their own k: merge and cut back to k. */
  if (np > 1) {
    qsort(h, (size_t)n, sizeof *h, hit_cmp);
    for (int i = k; i < n; i++) free(h[i].key);
    if (n > k) n = k;
  }
  *out = h;
  return n;
}

/* ---- rank fusion --------------------------------------------------------- */

vecindex_cand *vecindex_fused_add(vecindex_fused *f, const char *key,
                                  int vrank, int frank, double distance) {
  vecindex_cand *c = NULL;
  for (int i = 0; i < f->n; i++) if (!strcmp(f->v[i].key, key)) { c = &f->v[i]; break; }
  if (!c) {
    if (f->n == f->cap) {
      int ncap = f->cap ? f->cap * 2 : 128;
      vecindex_cand *nv = realloc(f->v, (size_t)ncap * sizeof *nv);
      if (!nv) return NULL;
      f->v = nv; f->cap = ncap;
    }
    c = &f->v[f->n];
    memset(c, 0, sizeof *c);
    c->key = strdup(key);
    if (!c->key) return NULL;
    f->n++;
  }
  /* An arm credits an item once — its best (first) rank. */
  if (vrank && !c->vrank) {
    c->vrank = vrank; c->distance = distance;
    c->score += 1.0 / (VECINDEX_RRF_K + vrank);
  }
  if (frank && !c->frank) {
    c->frank = frank;
    c->score += 1.0 / (VECINDEX_RRF_K + frank);
  }
  return c;
}

int vecindex_fused_add_hits(vecindex_fused *f, const vecindex_hit *h, int n) {
  int added = 0;
  for (int i = 0; i < n; i++)
    if (vecindex_fused_add(f, h[i].key, i + 1, 0, h[i].distance)) added++;
  return added;
}

int vecindex_fts_arm(db_handle *db, const char *fts_table, const char *key_col,
                     const char *match_expr, int k, vecindex_fused *f) {
  if (!match_expr || !*match_expr || !vecindex_table_exists(db, fts_table)) return -1;
  char sql[300];
  snprintf(sql, sizeof sql, "SELECT %s FROM \"%s\" WHERE \"%s\" MATCH ?1 "
           "ORDER BY bm25(\"%s\") LIMIT ?2", key_col, fts_table, fts_table, fts_table);
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(st, 1, match_expr, -1, SQLITE_STATIC);
  sqlite3_bind_int(st, 2, k);
  int rank = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *key = (const char *)sqlite3_column_text(st, 0);
    if (!key) continue;
    if (!vecindex_fused_add(f, key, 0, ++rank, 0)) break;
  }
  sqlite3_finalize(st);
  return rank;
}

static int cand_cmp(const void *a, const void *b) {
  const vecindex_cand *x = a, *y = b;
  if (x->score > y->score) return -1;
  if (x->score < y->score) return 1;
  /* Deterministic tie-break, so the same query twice lists the same items in
   * the same order: the vector rank, then the key. */
  int xv = x->vrank ? x->vrank : 1 << 20, yv = y->vrank ? y->vrank : 1 << 20;
  if (xv != yv) return xv < yv ? -1 : 1;
  return strcmp(x->key, y->key);
}

void vecindex_fused_sort(vecindex_fused *f) {
  if (f->n > 1) qsort(f->v, (size_t)f->n, sizeof *f->v, cand_cmp);
}

void vecindex_fused_free(vecindex_fused *f) {
  for (int i = 0; i < f->n; i++) free(f->v[i].key);
  free(f->v);
  f->v = NULL; f->n = f->cap = 0;
}
