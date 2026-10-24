/* core/breach_store.c — see breach_store.h. */
#include "breach_store.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Rows per batch transaction. Large enough to amortize commit/fsync cost, small
 * enough to bound WAL growth and rollback-on-crash cost. */
#define BREACH_BATCH 50000

struct breach_store {
  sqlite3      *db;    /* borrowed (owned by the caller's db_handle) */
  sqlite3_stmt *ins;
  long long     pending;   /* rows stepped into the OPEN batch (not yet durable) */
  long long     total;     /* rows this store attempted to insert */
  long long     committed; /* rows that made it through a successful COMMIT */
  int           broken;    /* a batch failed to commit; refuse further writes */
};

static int exec1(sqlite3 *db, const char *sql) {
  char *err = NULL;
  int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[breach_store] \"%s\": %s\n", sql, err ? err : "?");
    sqlite3_free(err);
  }
  return rc;
}

/* Commit the open batch, and when `reopen` start the next one. 0 on success.
 *
 * The COMMIT return is NOT optional. On SQLITE_BUSY or SQLITE_FULL the
 * transaction stays OPEN, and discarding the rc had three consequences at
 * once: `pending` was reset anyway, so the row count this store reports
 * counted a batch that was never durable; the BEGIN that follows failed
 * silently on the still-open transaction; and that same transaction then went
 * on absorbing another BREACH_BATCH rows per "batch" for the rest of the
 * ingest — unbounded WAL growth on a corpus of this size, all of it discarded
 * together if anything later failed. Roll the batch back and latch the store
 * broken instead: the on-disk shard corpus is the source of truth, so the
 * honest outcome is a failed materialize that is re-run, never a partial
 * table reported as a complete one. */
static int store_flush(breach_store *s, int reopen) {
  if (exec1(s->db, "COMMIT") != SQLITE_OK) {
    exec1(s->db, "ROLLBACK");   /* leave the connection usable, not mid-txn */
    s->pending = 0;
    s->broken  = 1;
    return -1;
  }
  s->committed += s->pending;
  s->pending    = 0;
  if (reopen && exec1(s->db, "BEGIN") != SQLITE_OK) {
    s->broken = 1;
    return -1;
  }
  return 0;
}

void breach_store_migrate(db_handle *db) {
  if (!db || !db->h) return;
  /* Column before index — the index cannot live in schema.sql for the reason
   * breach_monitor_migrate() gives: schema.sql runs before ensure_column, and
   * a CREATE INDEX on a missing column aborts the whole script. */
  ensure_column(db, "breach_items", "lookup_hash", "TEXT");
  exec1(db->h, "CREATE INDEX IF NOT EXISTS idx_breach_items_lookup "
               "ON breach_items(lookup_hash) WHERE lookup_hash IS NOT NULL");
}

breach_store *breach_store_open(db_handle *db) {
  if (!db || !db->h) return NULL;
  breach_store_migrate(db);
  breach_store *s = calloc(1, sizeof *s);
  if (!s) return NULL;
  s->db = db->h;

  /* Bulk-load pragmas. A one-shot admin ingest owns this connection, and the
   * on-disk shard corpus is the source of truth, so trading durability for
   * throughput here is safe (a crashed materialize is simply re-run). */
  exec1(s->db, "PRAGMA synchronous=OFF");
  exec1(s->db, "PRAGMA temp_store=MEMORY");
  exec1(s->db, "PRAGMA cache_size=-262144");   /* ~256 MB page cache */

  static const char *SQL =
    "INSERT INTO breach_items(keyid,type,value,source_id,hash,has_secret,count,"
    "lookup_hash) VALUES(?1,?2,?3,?4,?5,?6,?7,?8) "
    "ON CONFLICT(keyid) DO UPDATE SET "
    "count = breach_items.count + excluded.count, "
    "has_secret = MAX(breach_items.has_secret, excluded.has_secret), "
    "lookup_hash = COALESCE(excluded.lookup_hash, breach_items.lookup_hash)";
  if (sqlite3_prepare_v2(s->db, SQL, -1, &s->ins, NULL) != SQLITE_OK) {
    fprintf(stderr, "[breach_store] prepare failed: %s\n", sqlite3_errmsg(s->db));
    free(s);
    return NULL;
  }
  if (exec1(s->db, "BEGIN") != SQLITE_OK) {
    sqlite3_finalize(s->ins);
    free(s);
    return NULL;
  }
  return s;
}

int breach_store_put(breach_store *s, const char *keyid, const char *type,
                     const char *value, const char *source_id, const char *hash,
                     const char *lookup_hash, int has_secret, long long count) {
  if (!s) return -1;
  /* Once a batch has failed to commit there is no open transaction to write
   * into; stepping on regardless would dribble rows out in autocommit at a
   * fraction of the throughput and leave a half-loaded table that still looks
   * finished. Refuse instead. */
  if (s->broken) return -1;
  sqlite3_stmt *st = s->ins;

  sqlite3_bind_text(st, 1, keyid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, type,  -1, SQLITE_TRANSIENT);
  if (value) sqlite3_bind_text(st, 3, value, -1, SQLITE_TRANSIENT);
  else       sqlite3_bind_null(st, 3);
  sqlite3_bind_text(st, 4, source_id ? source_id : "?", -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 5, hash ? hash : "", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (st, 6, has_secret ? 1 : 0);
  sqlite3_bind_int64(st, 7, count > 0 ? count : 1);
  if (lookup_hash && *lookup_hash) sqlite3_bind_text(st, 8, lookup_hash, -1, SQLITE_TRANSIENT);
  else                             sqlite3_bind_null(st, 8);

  int rc = sqlite3_step(st);
  sqlite3_reset(st);
  sqlite3_clear_bindings(st);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "[breach_store] insert: %s\n", sqlite3_errmsg(s->db));
    return -1;
  }

  s->total++;
  if (++s->pending >= BREACH_BATCH) return store_flush(s, 1);
  return 0;
}

int breach_store_finish(breach_store *s) {
  if (!s) return -1;
  /* A broken store already rolled its batch back and holds no transaction, so
   * there is nothing left to commit — only the final batch of a healthy store
   * is flushed here. Either way the return says whether every attempted row
   * is durable, rather than the unconditional 0 it used to report. */
  int rc = s->broken ? -1 : store_flush(s, 0);
  if (s->ins) sqlite3_finalize(s->ins);
  /* Deferred bulk FTS build over the freshly-loaded content table — far cheaper
   * than maintaining the index incrementally during the load. Run even after a
   * failed batch: breach_fts is external-content, so an index left describing
   * rows the table no longer has would answer searches with phantom hits. */
  exec1(s->db, "INSERT INTO breach_fts(breach_fts) VALUES('rebuild')");
  /* Restore durable behavior for normal operation. */
  exec1(s->db, "PRAGMA synchronous=NORMAL");
  /* Report what was COMMITTED, not what was attempted. */
  fprintf(stderr, "[breach_store] materialized %lld rows into breach_items\n",
          s->committed);
  if (s->committed != s->total)
    fprintf(stderr, "[breach_store] INCOMPLETE: %lld of %lld attempted rows were "
                    "not committed (transaction failure) — re-run the materialize\n",
            s->total - s->committed, s->total);
  free(s);
  return rc;
}

/* Wrap an arbitrary user term as a single quoted FTS5 phrase so it can never be
 * interpreted as FTS query syntax (doubles embedded quotes). out must hold
 * 2*len+3 bytes. */
static void fts_phrase(const char *q, char *out, size_t cap) {
  size_t o = 0;
  if (cap < 3) { if (cap) out[0] = 0; return; }
  out[o++] = '"';
  for (const char *p = q; *p && o + 2 < cap - 1; p++) {
    if (*p == '"') out[o++] = '"';   /* escape by doubling */
    out[o++] = *p;
  }
  out[o++] = '"';
  out[o] = 0;
}

/* Exact COUNT up to this many matches; past it the total is reported as a
 * floor. A substring term over a corpus of "millions–billions of rows"
 * (breach_store.h) can match a large fraction of it, and an unbounded COUNT
 * is the same full walk the page itself was bounded to avoid. */
#define BREACH_COUNT_CAP 100000

char *breach_search(db_handle *db, const char *q, const char *type, int limit,
                    long long offset) {
  if (!db || !db->h) return NULL;
  int has_q = (q && *q), has_type = (type && *type);
  if (!has_q && !has_type) return NULL;
  if (limit <= 0) limit = 50;
  if (limit > 500) limit = 500;      /* exhaustive-ok: page size; total + offset disclose the rest */
  if (offset < 0) offset = 0;

  /* Ordered by rowid so ?offset= pages are stable: without an ORDER BY the
   * order of a JOIN over FTS is whatever the planner picked, and page 2 could
   * repeat or skip page 1's rows. */
  const char *from = has_q
    ? (has_type
        ? "FROM breach_items b JOIN breach_fts ON breach_fts.rowid=b.id "
          "WHERE breach_fts MATCH ?1 AND b.type=?2"
        : "FROM breach_items b JOIN breach_fts ON breach_fts.rowid=b.id "
          "WHERE breach_fts MATCH ?1")
    : "FROM breach_items b WHERE b.type=?2";
  char sql[640];
  snprintf(sql, sizeof sql,
    "SELECT b.keyid,b.type,b.value,b.source_id,b.has_secret,b.count %s "
    "ORDER BY b.id LIMIT ?3 OFFSET ?4", from);
  char csql[640];
  snprintf(csql, sizeof csql,
    "SELECT COUNT(*) FROM (SELECT 1 %s LIMIT %d)", from, BREACH_COUNT_CAP + 1);

  char mq[600];
  if (has_q) fts_phrase(q, mq, sizeof mq);

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return NULL;
  if (has_q) sqlite3_bind_text(st, 1, mq, -1, SQLITE_TRANSIENT);
  if (has_type) sqlite3_bind_text(st, 2, type, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 3, limit);
  sqlite3_bind_int64(st, 4, offset);

  cJSON *arr = cJSON_CreateArray();
  int n = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    cJSON *o = cJSON_CreateObject();
    const unsigned char *keyid = sqlite3_column_text(st, 0);
    const unsigned char *ty    = sqlite3_column_text(st, 1);
    const unsigned char *val   = sqlite3_column_text(st, 2);
    const unsigned char *src   = sqlite3_column_text(st, 3);
    cJSON_AddStringToObject(o, "keyid", keyid ? (const char *)keyid : "");
    cJSON_AddStringToObject(o, "type",  ty ? (const char *)ty : "");
    if (val) cJSON_AddStringToObject(o, "value", (const char *)val);
    else     cJSON_AddNullToObject(o, "value");   /* passwords are hash-only */
    cJSON_AddStringToObject(o, "source_id", src ? (const char *)src : "");
    cJSON_AddBoolToObject(o, "has_secret", sqlite3_column_int(st, 4) != 0);
    cJSON_AddNumberToObject(o, "count", (double)sqlite3_column_int64(st, 5));
    cJSON_AddItemToArray(arr, o);
    n++;
  }
  sqlite3_finalize(st);

  /* The total, so a capped page says how much it is NOT showing (house rule
   * 2). It used to return `count` = rows on this page and nothing else: a
   * query matching 40,000 identifiers answered "count":500 with no way to
   * see, or reach, the other 39,500. */
  long long total = -1;
  if (sqlite3_prepare_v2(db->h, csql, -1, &st, NULL) == SQLITE_OK) {
    if (has_q) sqlite3_bind_text(st, 1, mq, -1, SQLITE_TRANSIENT);
    if (has_type) sqlite3_bind_text(st, 2, type, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) total = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
  }
  int is_floor = total > BREACH_COUNT_CAP;
  if (is_floor) total = BREACH_COUNT_CAP;
  /* A page past the count cap knows the total is at least where it is. */
  if (total >= 0 && total < offset + n) total = offset + n;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", has_q ? q : "");
  if (has_type) cJSON_AddStringToObject(root, "type", type);
  cJSON_AddNumberToObject(root, "count", n);        /* kept: rows on this page */
  cJSON_AddNumberToObject(root, "shown", n);
  cJSON_AddNumberToObject(root, "offset", (double)offset);
  cJSON_AddNumberToObject(root, "limit", limit);
  if (total >= 0) cJSON_AddNumberToObject(root, "total", (double)total);
  else            cJSON_AddNullToObject(root, "total");
  cJSON_AddBoolToObject(root, "total_is_floor", is_floor || total < 0);
  /* A short page is the last one whatever the count says. */
  int more = n == limit && (is_floor || total < 0 || offset + n < total);
  cJSON_AddBoolToObject(root, "has_more", more);
  if (more) cJSON_AddNumberToObject(root, "next_offset", (double)(offset + n));
  cJSON_AddItemToObject(root, "results", arr);
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}
