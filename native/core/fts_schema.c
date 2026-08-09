#include "fts_schema.h"
#include "fts.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* ── JSON → indexable text ────────────────────────────────────────────── */

typedef struct { char *p; size_t n, cap; } sbuf;

static void sb_raw(sbuf *b, const char *s, size_t len) {
  if (b->n >= FTS_FLATTEN_MAX_TOTAL) return;
  if (len > FTS_FLATTEN_MAX_TOTAL - b->n) len = FTS_FLATTEN_MAX_TOTAL - b->n;
  if (b->n + len + 1 > b->cap) {
    size_t nc = b->cap ? b->cap : 256;
    while (nc < b->n + len + 1) nc *= 2;
    char *np = realloc(b->p, nc);
    if (!np) return;                       /* keep what we have, stop growing */
    b->p = np; b->cap = nc;
  }
  if (b->n + len + 1 > b->cap) return;
  memcpy(b->p + b->n, s, len);
  b->n += len;
  b->p[b->n] = '\0';
}

/* One value, space-separated from the previous. Oversized strings are dropped
 * whole rather than truncated: a 40 KB base64 thumbnail contributes no real
 * tokens, and half of one is worse than none. */
static void sb_word(sbuf *b, const char *s) {
  if (!s || !*s) return;
  size_t len = strlen(s);
  if (len > FTS_FLATTEN_MAX_VALUE) return;
  if (b->n) sb_raw(b, " ", 1);
  sb_raw(b, s, len);
}

static void flatten_val(sbuf *b, const cJSON *v, int depth) {
  if (!v || depth > 6 || b->n >= FTS_FLATTEN_MAX_TOTAL) return;
  if (cJSON_IsObject(v) || cJSON_IsArray(v)) {
    /* Members, not keys: nobody searches for the word "discovery_channel". */
    const cJSON *it;
    cJSON_ArrayForEach(it, v) flatten_val(b, it, depth + 1);
    return;
  }
  if (cJSON_IsString(v)) { sb_word(b, v->valuestring); return; }
  if (cJSON_IsNumber(v)) {
    char t[40];
    double d = v->valuedouble;
    if (d == (double)(long long)d) snprintf(t, sizeof t, "%lld", (long long)d);
    else                           snprintf(t, sizeof t, "%g", d);
    sb_word(b, t);
    return;
  }
  /* true/false/null carry nothing a human would type into a search box. */
}

char *fts_flatten_json(const char *json) {
  sbuf b = {NULL, 0, 0};
  if (json && *json) {
    cJSON *j = cJSON_Parse(json);
    if (j) { flatten_val(&b, j, 0); cJSON_Delete(j); }
    else     sb_word(&b, json);          /* not JSON after all: index verbatim */
  }
  if (!b.p) { b.p = malloc(1); if (b.p) b.p[0] = '\0'; }
  return b.p;
}

/* ── schema probes ───────────────────────────────────────────────────── */

static int table_exists(sqlite3 *h, const char *name) {
  sqlite3_stmt *s; int found = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT 1 FROM sqlite_master WHERE type IN ('table','view') AND name=?1",
        -1, &s, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(s, 1, name, -1, SQLITE_TRANSIENT);
  found = (sqlite3_step(s) == SQLITE_ROW);
  sqlite3_finalize(s);
  return found;
}

/* PRAGMA table_info works on fts5 virtual tables and lists the declared
 * columns, which is what tells v1 from v2 apart. */
static int has_col(sqlite3 *h, const char *table, const char *col) {
  char sql[128];
  snprintf(sql, sizeof sql, "PRAGMA table_info(%s)", table);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) != SQLITE_OK) return 0;
  int found = 0;
  while (!found && sqlite3_step(s) == SQLITE_ROW) {
    const char *n = (const char *)sqlite3_column_text(s, 1);
    if (n && strcmp(n, col) == 0) found = 1;
  }
  sqlite3_finalize(s);
  return found;
}

/* ── the live index shape, as the WRITE path needs it ─────────────────── */

/* Cached because it is a property of the DATABASE, not of a connection, and
 * intel.c asks once per emitted item. -1 = not probed yet. */
static pthread_mutex_t g_shape_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_shape = -1;

int fts_index_is_v2(sqlite3 *h) {
  pthread_mutex_lock(&g_shape_mu);
  if (g_shape < 0 && h)
    g_shape = has_col(h, "intel_items_fts", FTS_SCHEMA_SENTINEL_COL);
  int v = (g_shape > 0);
  pthread_mutex_unlock(&g_shape_mu);
  return v;
}

const char *fts_insert_sql(sqlite3 *h) {
  /* The v1 statement is not a fallback nobody will hit: it is the state of
   * every database that has not yet completed the rebuild, including one
   * booted with JO_FTS_REBUILD=0. Naming nine columns there fails to prepare,
   * which is precisely how the write path turned into a pure delete. */
  return fts_index_is_v2(h)
    ? "INSERT INTO intel_items_fts"
      "(uid,title,body,summary,keywords,link,author,tags,props)"
      " VALUES(?1,?2,?3,?4,'',?5,?6,?7,?8)"
    : "INSERT INTO intel_items_fts(uid,title,body,summary,keywords)"
      " VALUES(?1,?2,?3,?4,'')";
}

static long long scalar_i64(sqlite3 *h, const char *sql) {
  sqlite3_stmt *s; long long v = 0;
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) != SQLITE_OK) return 0;
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

/* FNV-1a over columns+tokenizer — _fts_meta.fingerprint, so a future version
 * can tell "same v2 number, different columns" from a real match. */
static void schema_fingerprint(char out[17]) {
  const char *src = FTS_SCHEMA_COLUMNS "|" FTS_SCHEMA_TOKENIZER;
  unsigned long long hsh = 1469598103934665603ULL;
  for (const unsigned char *p = (const unsigned char *)src; *p; p++) {
    hsh ^= *p; hsh *= 1099511628211ULL;
  }
  snprintf(out, 17, "%016llx", hsh);
}

static double now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/* ── the rebuild ─────────────────────────────────────────────────────── */

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_ran = 0;

void fts_schema_migrate(db_handle *db) {
  if (!db || !db->h) return;
  pthread_mutex_lock(&g_mu);
  if (g_ran) { pthread_mutex_unlock(&g_mu); return; }
  g_ran = 1;
  pthread_mutex_unlock(&g_mu);

  sqlite3 *h = db->h;
  if (!table_exists(h, "intel_items_fts")) return;   /* schema.sql owns create */
  if (has_col(h, "intel_items_fts", FTS_SCHEMA_SENTINEL_COL)) return;  /* v2 */

  const char *flag = getenv("JO_FTS_REBUILD");
  if (flag && (strcmp(flag, "0") == 0 || strcmp(flag, "false") == 0)) {
    fprintf(stderr, "[fts] intel_items_fts is v1 and JO_FTS_REBUILD=0 — skipping "
                    "rebuild; link/author/tags/properties stay UNSEARCHABLE "
                    "(ingest writes the five v1 columns and does NOT drain the "
                    "index; re-run without the flag to widen it)\n");
    return;
  }

  long long nrows = scalar_i64(h, "SELECT COUNT(*) FROM intel_items");
  double t0 = now_ms();
  fprintf(stderr, "[fts] rebuilding intel_items_fts -> v%d over %lld items "
                  "(one transaction; an interrupted run rolls back to the "
                  "existing index and retries next boot)\n",
          FTS_SCHEMA_VERSION, nrows);

  sqlite3_stmt *sel = NULL, *ins = NULL, *map = NULL, *met = NULL;
  char *e = NULL;
  int in_txn = 0;

#define EXEC(sql) do {                                                        \
    if (sqlite3_exec(h, (sql), NULL, NULL, &e) != SQLITE_OK) {                \
      fprintf(stderr, "[fts] rebuild failed (%s): %s\n", (sql), e ? e : "?"); \
      sqlite3_free(e); e = NULL; goto fail;                                   \
    }                                                                         \
  } while (0)

  EXEC("BEGIN IMMEDIATE");
  in_txn = 1;

  /* keywords is NOT derivable from intel_items: the ingest path writes '' and
   * the real content is pushed in later by translate.c (machine English) and
   * media.c (OCR text). Dropping the index without saving it would silently
   * un-translate the corpus, repairable only by re-running the LLM over every
   * row. Snapshot the non-empty ones first — on an untranslated corpus this
   * temp table is empty and costs nothing. */
  EXEC("CREATE TEMP TABLE _fts_kw AS "
       "SELECT m.uid AS uid, f.keywords AS keywords "
       "  FROM intel_items_fts_uid_map m "
       "  JOIN intel_items_fts f ON f.rowid = m.rowid "
       " WHERE f.keywords IS NOT NULL AND f.keywords <> ''");
  EXEC("CREATE UNIQUE INDEX temp._fts_kw_uid ON _fts_kw(uid)");

  /* One INSERT per corpus row into uid_map is cheaper without this index;
   * it is recreated below, so either ordering against translate_migrate()
   * (which also creates it) ends with the index present. */
  EXEC("DROP INDEX IF EXISTS idx_intel_items_fts_uid_map_rowid");

  EXEC("DROP TABLE intel_items_fts");
  EXEC("CREATE VIRTUAL TABLE intel_items_fts USING fts5("
       FTS_SCHEMA_COLUMNS ", tokenize='" FTS_SCHEMA_TOKENIZER "')");
  EXEC("DELETE FROM intel_items_fts_uid_map");

  if (sqlite3_prepare_v2(h,
        "SELECT i.uid,i.title,i.body,i.summary,i.link,i.author,i.tags,"
        "       i.properties,k.keywords"
        "  FROM intel_items i"
        "  LEFT JOIN _fts_kw k ON k.uid = i.uid", -1, &sel, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(h,
        "INSERT INTO intel_items_fts"
        "(uid,title,body,summary,keywords,link,author,tags,props)"
        " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)", -1, &ins, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(h,
        "INSERT INTO intel_items_fts_uid_map(uid,rowid) VALUES(?1,?2)",
        -1, &map, NULL) != SQLITE_OK) {
    fprintf(stderr, "[fts] rebuild prepare failed: %s\n", sqlite3_errmsg(h));
    goto fail;
  }

  long long done = 0;
  while (sqlite3_step(sel) == SQLITE_ROW) {
    const char *uid = (const char *)sqlite3_column_text(sel, 0);
    if (!uid || !*uid) continue;

    /* Everything goes through fts_segment for MeCab parity with the write
     * path; Latin (URLs, ids, English) passes straight through, so this is
     * only a real cost on Japanese text. */
    char *v_title = fts_segment((const char *)sqlite3_column_text(sel, 1));
    char *v_body  = fts_segment((const char *)sqlite3_column_text(sel, 2));
    char *v_summ  = fts_segment((const char *)sqlite3_column_text(sel, 3));
    char *v_link  = fts_segment((const char *)sqlite3_column_text(sel, 4));
    char *v_auth  = fts_segment((const char *)sqlite3_column_text(sel, 5));
    char *f_tags  = fts_flatten_json((const char *)sqlite3_column_text(sel, 6));
    char *f_prop  = fts_flatten_json((const char *)sqlite3_column_text(sel, 7));
    char *v_tags  = fts_segment(f_tags);
    char *v_prop  = fts_segment(f_prop);
    const char *kw = (const char *)sqlite3_column_text(sel, 8);

    sqlite3_reset(ins);
    sqlite3_bind_text(ins, 1, uid,     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 2, v_title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 3, v_body,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 4, v_summ,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 5, kw ? kw : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 6, v_link,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 7, v_auth,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 8, v_tags,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 9, v_prop,  -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(ins);

    free(v_title); free(v_body); free(v_summ); free(v_link); free(v_auth);
    free(f_tags); free(f_prop); free(v_tags); free(v_prop);

    if (rc != SQLITE_DONE) {
      fprintf(stderr, "[fts] rebuild insert failed at %s: %s\n",
              uid, sqlite3_errmsg(h));
      goto fail;
    }

    sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
    sqlite3_reset(map);
    sqlite3_bind_text(map, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(map, 2, rid);
    if (sqlite3_step(map) != SQLITE_DONE) {
      fprintf(stderr, "[fts] rebuild uid_map failed at %s: %s\n",
              uid, sqlite3_errmsg(h));
      goto fail;
    }

    if (++done % 25000 == 0)
      fprintf(stderr, "[fts]   %lld/%lld items reindexed (%.0fs)\n",
              done, nrows, (now_ms() - t0) / 1000.0);
  }
  sqlite3_finalize(sel); sel = NULL;
  sqlite3_finalize(ins); ins = NULL;
  sqlite3_finalize(map); map = NULL;

  EXEC("DROP TABLE temp._fts_kw");
  EXEC("CREATE INDEX IF NOT EXISTS idx_intel_items_fts_uid_map_rowid"
       "  ON intel_items_fts_uid_map(rowid)");

  /* Every rowid in the index is new, so translate.c's resync watermark now
   * points into the old numbering. Re-point it at the new MAX: keywords were
   * carried over, so nothing above it is stale and the next pass correctly
   * scans nothing. */
  if (table_exists(h, "translate_state"))
    EXEC("INSERT INTO translate_state(k,v) VALUES('fts_rowid_watermark',"
         " (SELECT COALESCE(MAX(rowid),0) FROM intel_items_fts_uid_map))"
         " ON CONFLICT(k) DO UPDATE SET v=excluded.v");

  /* _fts_meta has been in schema.sql (inherited from the Node database) with
   * zero readers and zero writers since the C port. This is its first writer:
   * what the live index actually is, and when it was built. */
  if (table_exists(h, "_fts_meta") &&
      sqlite3_prepare_v2(h,
        "INSERT INTO _fts_meta(name,fingerprint,columns,tokenizer,version,rebuilt_at)"
        " VALUES('intel_items_fts',?1,?2,?3,?4,"
        "        strftime('%Y-%m-%dT%H:%M:%SZ','now'))"
        " ON CONFLICT(name) DO UPDATE SET fingerprint=excluded.fingerprint,"
        "   columns=excluded.columns, tokenizer=excluded.tokenizer,"
        "   version=excluded.version, rebuilt_at=excluded.rebuilt_at",
        -1, &met, NULL) == SQLITE_OK) {
    char fp[17]; schema_fingerprint(fp);
    sqlite3_bind_text(met, 1, fp, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(met, 2, FTS_SCHEMA_COLUMNS, -1, SQLITE_STATIC);
    sqlite3_bind_text(met, 3, FTS_SCHEMA_TOKENIZER, -1, SQLITE_STATIC);
    sqlite3_bind_int(met, 4, FTS_SCHEMA_VERSION);
    sqlite3_step(met);
    sqlite3_finalize(met); met = NULL;
  }

  EXEC("COMMIT");
  in_txn = 0;
  /* The index the write path binds against just changed shape underneath it. */
  pthread_mutex_lock(&g_shape_mu); g_shape = -1; pthread_mutex_unlock(&g_shape_mu);
  fprintf(stderr, "[fts] intel_items_fts rebuilt: %lld items indexed in %.1fs\n",
          done, (now_ms() - t0) / 1000.0);
#undef EXEC
  return;

fail:
  if (sel) sqlite3_finalize(sel);
  if (ins) sqlite3_finalize(ins);
  if (map) sqlite3_finalize(map);
  if (met) sqlite3_finalize(met);
  if (in_txn) sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
  fprintf(stderr, "[fts] rebuild rolled back — the existing index is intact "
                  "and unchanged; will retry on next boot\n");
}
