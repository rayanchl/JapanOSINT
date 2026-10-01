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

breach_store *breach_store_open(db_handle *db) {
  if (!db || !db->h) return NULL;
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
    "INSERT INTO breach_items(keyid,type,value,source_id,hash,has_secret,count) "
    "VALUES(?1,?2,?3,?4,?5,?6,?7) "
    "ON CONFLICT(keyid) DO UPDATE SET "
    "count = breach_items.count + excluded.count, "
    "has_secret = MAX(breach_items.has_secret, excluded.has_secret)";
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
                     int has_secret, long long count) {
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

char *breach_search(db_handle *db, const char *q, const char *type, int limit) {
  if (!db || !db->h) return NULL;
  int has_q = (q && *q), has_type = (type && *type);
  if (!has_q && !has_type) return NULL;
  if (limit <= 0) limit = 50;
  if (limit > 500) limit = 500;

  char sql[512];
  if (has_q) {
    snprintf(sql, sizeof sql,
      "SELECT b.keyid,b.type,b.value,b.source_id,b.has_secret,b.count "
      "FROM breach_items b JOIN breach_fts ON breach_fts.rowid=b.id "
      "WHERE breach_fts MATCH ?1%s LIMIT ?3",
      has_type ? " AND b.type=?2" : "");
  } else {
    snprintf(sql, sizeof sql,
      "SELECT keyid,type,value,source_id,has_secret,count "
      "FROM breach_items WHERE type=?2 LIMIT ?3");
  }

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return NULL;
  if (has_q) {
    char mq[600]; fts_phrase(q, mq, sizeof mq);
    sqlite3_bind_text(st, 1, mq, -1, SQLITE_TRANSIENT);
  }
  if (has_type) sqlite3_bind_text(st, 2, type, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(st, 3, limit);

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

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", has_q ? q : "");
  if (has_type) cJSON_AddStringToObject(root, "type", type);
  cJSON_AddNumberToObject(root, "count", n);
  cJSON_AddItemToObject(root, "results", arr);
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}
