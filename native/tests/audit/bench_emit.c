/* tests/audit/bench_emit.c — READ-ONLY INVESTIGATION ARTIFACT.
 *
 * Reproduces core/intel.c emit()'s exact SQLite statement pattern against the
 * real schema, and compares it with the same writes done with cached prepared
 * statements + a batched transaction. Nothing in the tree is modified; this
 * builds standalone against third_party/sqlite3.c.
 *
 *   cc -O2 -o bench_emit bench_emit.c ../../third_party/sqlite3.c -lpthread -ldl -lm
 */
#include "../../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

/* Verbatim from core/intel.c:34 */
static const char *SQL_UPSERT =
 "INSERT INTO intel_items"
 " (uid,source_id,title,body,summary,link,author,language,published_at,"
 "  fetched_at,tags,properties,lat,lon,geom_source,geom_at,record_type,"
 "  sub_source_id,geometry,tenant_id)"
 " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20)"
 " ON CONFLICT(uid) DO UPDATE SET"
 "  title=excluded.title, body=excluded.body, summary=excluded.summary,"
 "  link=excluded.link, author=excluded.author, language=excluded.language,"
 "  published_at=excluded.published_at, fetched_at=excluded.fetched_at,"
 "  tags=excluded.tags, properties=excluded.properties,"
 "  lat=CASE WHEN excluded.lat IS NOT NULL THEN excluded.lat"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.lat"
 "          ELSE excluded.lat END,"
 "  lon=CASE WHEN excluded.lon IS NOT NULL THEN excluded.lon"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.lon"
 "          ELSE excluded.lon END,"
 "  geom_source=CASE WHEN excluded.geom_source IS NOT NULL THEN excluded.geom_source"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.geom_source"
 "          ELSE excluded.geom_source END,"
 "  geom_at=CASE WHEN excluded.geom_source IS NOT NULL THEN excluded.geom_at"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.geom_at"
 "          ELSE excluded.geom_at END,"
 "  geometry=COALESCE(excluded.geometry,intel_items.geometry),"
 "  record_type=COALESCE(excluded.record_type,intel_items.record_type),"
 "  sub_source_id=COALESCE(excluded.sub_source_id,intel_items.sub_source_id);";

static const char *DDL =
  "CREATE TABLE IF NOT EXISTS intel_items ("
  " uid TEXT PRIMARY KEY, source_id TEXT NOT NULL, title TEXT, body TEXT,"
  " summary TEXT, link TEXT, author TEXT, language TEXT, published_at TEXT,"
  " fetched_at TEXT NOT NULL, tags TEXT, properties TEXT NOT NULL DEFAULT '{}',"
  " keywords TEXT, keywords_at TEXT, keywords_failed INTEGER DEFAULT 0,"
  " lat REAL, lon REAL, geom_source TEXT, geom_at TEXT, record_type TEXT,"
  " sub_source_id TEXT, geometry TEXT, geom_failed INTEGER DEFAULT 0,"
  " tenant_id TEXT NOT NULL DEFAULT 'legacy', simhash INTEGER, cluster_id TEXT);"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_fetched ON intel_items(fetched_at DESC);"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_geom ON intel_items(lat,lon) WHERE lat IS NOT NULL;"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_geom_pending ON intel_items(source_id) WHERE lat IS NULL AND geom_source IS NULL;"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_source ON intel_items(source_id,published_at DESC);"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_subsrc ON intel_items(source_id,sub_source_id);"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_tenant_fetched ON intel_items(tenant_id,fetched_at DESC);"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_tenant_source ON intel_items(tenant_id,source_id);"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_type ON intel_items(record_type,source_id);"
  "CREATE INDEX IF NOT EXISTS idx_intel_items_cluster ON intel_items(cluster_id);"
  "CREATE VIRTUAL TABLE IF NOT EXISTS intel_items_fts USING fts5("
  " uid UNINDEXED,title,body,summary,keywords,link,author,tags,props,"
  " tokenize='unicode61 remove_diacritics 1');"
  "CREATE TABLE IF NOT EXISTS intel_items_fts_uid_map ("
  " uid TEXT NOT NULL PRIMARY KEY, rowid INTEGER NOT NULL) WITHOUT ROWID;";

static const char *TITLE = "Tokyo Metropolitan Police traffic accident record";
static const char *BODY  = "Intersection collision involving two vehicles, minor injuries reported, "
                           "investigation ongoing by the local precinct traffic division.";
static const char *PROPS = "{\"record_type\":\"accident\",\"pref\":\"13\",\"year\":\"2024\","
                           "\"severity\":\"minor\",\"road\":\"national\",\"weather\":\"clear\"}";

static void pragmas(sqlite3 *h) {
  sqlite3_exec(h, "PRAGMA journal_mode=WAL;",   NULL, NULL, NULL);
  sqlite3_exec(h, "PRAGMA foreign_keys=ON;",    NULL, NULL, NULL);
  sqlite3_exec(h, "PRAGMA busy_timeout=5000;",  NULL, NULL, NULL);
  sqlite3_exec(h, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
}

static void bind_row(sqlite3_stmt *s, const char *uid, int i) {
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, "npa-traffic-accidents", -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 3, TITLE, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 4, BODY,  -1, SQLITE_STATIC);
  sqlite3_bind_null(s, 5);
  sqlite3_bind_text(s, 6, "https://example.jp/x", -1, SQLITE_STATIC);
  sqlite3_bind_null(s, 7); sqlite3_bind_null(s, 8); sqlite3_bind_null(s, 9);
  sqlite3_bind_text(s, 10, "2026-07-31T16:00:00.000Z", -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 11, "[]", -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 12, PROPS, -1, SQLITE_STATIC);
  sqlite3_bind_double(s, 13, 35.0 + (i % 900) / 100.0);
  sqlite3_bind_double(s, 14, 135.0 + (i % 900) / 100.0);
  sqlite3_bind_text(s, 15, "native", -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 16, "2026-07-31T16:00:00.000Z", -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 17, "accident", -1, SQLITE_STATIC);
  sqlite3_bind_null(s, 18); sqlite3_bind_null(s, 19);
  sqlite3_bind_text(s, 20, "legacy", -1, SQLITE_STATIC);
}

/* ---- A: EXACTLY what core/intel.c emit() does, per row ---- */
static void mode_current(sqlite3 *h, int n) {
  for (int i = 0; i < n; i++) {
    char uid[512];
    snprintf(uid, sizeof uid, "npa-traffic-accidents|%d", i);
    sqlite3_stmt *s;

    /* intel.c:176 — is_new probe */
    if (sqlite3_prepare_v2(h, "SELECT 1 FROM intel_items WHERE uid=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
    }
    /* intel.c:184 — per-row transaction */
    sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
    if (sqlite3_prepare_v2(h, SQL_UPSERT, -1, &s, NULL) == SQLITE_OK) {
      bind_row(s, uid, i);
      sqlite3_step(s); sqlite3_finalize(s);
    }
    /* intel.c:79 fts_write — 3 to 4 more prepare/finalize */
    sqlite3_int64 old = -1;
    if (sqlite3_prepare_v2(h, "SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_ROW) old = sqlite3_column_int64(s, 0);
      sqlite3_finalize(s);
    }
    if (old >= 0 && sqlite3_prepare_v2(h, "DELETE FROM intel_items_fts WHERE rowid=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_int64(s, 1, old); sqlite3_step(s); sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(h,
        "INSERT INTO intel_items_fts(uid,title,body,summary,keywords,link,author,tags,props)"
        " VALUES(?1,?2,?3,?4,'',?5,?6,?7,?8)", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, TITLE, -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 3, BODY,  -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 4, "",    -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 5, "https://example.jp/x", -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 6, "",    -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 7, "",    -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 8, PROPS, -1, SQLITE_STATIC);
      sqlite3_step(s); sqlite3_finalize(s);
    }
    sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
    if (sqlite3_prepare_v2(h,
        "INSERT INTO intel_items_fts_uid_map(uid,rowid) VALUES(?1,?2)"
        " ON CONFLICT(uid) DO UPDATE SET rowid=excluded.rowid", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(s, 2, rid);
      sqlite3_step(s); sqlite3_finalize(s);
    }
    sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
  }
}

/* ---- B: cached prepared statements, one transaction per 1000 rows ---- */
static void mode_fixed(sqlite3 *h, int n, int batch) {
  sqlite3_stmt *ex = NULL, *up = NULL, *mp = NULL, *fd = NULL, *fi = NULL, *mu = NULL;
  sqlite3_prepare_v2(h, "SELECT 1 FROM intel_items WHERE uid=?1", -1, &ex, NULL);
  sqlite3_prepare_v2(h, SQL_UPSERT, -1, &up, NULL);
  sqlite3_prepare_v2(h, "SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1", -1, &mp, NULL);
  sqlite3_prepare_v2(h, "DELETE FROM intel_items_fts WHERE rowid=?1", -1, &fd, NULL);
  sqlite3_prepare_v2(h,
      "INSERT INTO intel_items_fts(uid,title,body,summary,keywords,link,author,tags,props)"
      " VALUES(?1,?2,?3,?4,'',?5,?6,?7,?8)", -1, &fi, NULL);
  sqlite3_prepare_v2(h,
      "INSERT INTO intel_items_fts_uid_map(uid,rowid) VALUES(?1,?2)"
      " ON CONFLICT(uid) DO UPDATE SET rowid=excluded.rowid", -1, &mu, NULL);

  sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL);
  for (int i = 0; i < n; i++) {
    char uid[512];
    snprintf(uid, sizeof uid, "npa-traffic-accidents|%d", i);

    sqlite3_reset(ex); sqlite3_bind_text(ex, 1, uid, -1, SQLITE_TRANSIENT); sqlite3_step(ex);
    sqlite3_reset(up); bind_row(up, uid, i); sqlite3_step(up);

    sqlite3_int64 old = -1;
    sqlite3_reset(mp); sqlite3_bind_text(mp, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(mp) == SQLITE_ROW) old = sqlite3_column_int64(mp, 0);
    if (old >= 0) { sqlite3_reset(fd); sqlite3_bind_int64(fd, 1, old); sqlite3_step(fd); }

    sqlite3_reset(fi);
    sqlite3_bind_text(fi, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(fi, 2, TITLE, -1, SQLITE_STATIC);
    sqlite3_bind_text(fi, 3, BODY,  -1, SQLITE_STATIC);
    sqlite3_bind_text(fi, 4, "",    -1, SQLITE_STATIC);
    sqlite3_bind_text(fi, 5, "https://example.jp/x", -1, SQLITE_STATIC);
    sqlite3_bind_text(fi, 6, "",    -1, SQLITE_STATIC);
    sqlite3_bind_text(fi, 7, "",    -1, SQLITE_STATIC);
    sqlite3_bind_text(fi, 8, PROPS, -1, SQLITE_STATIC);
    sqlite3_step(fi);
    sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
    sqlite3_reset(mu);
    sqlite3_bind_text(mu, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(mu, 2, rid);
    sqlite3_step(mu);

    if (batch > 0 && (i % batch) == batch - 1) {
      sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
      sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    }
  }
  sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
  sqlite3_finalize(ex); sqlite3_finalize(up); sqlite3_finalize(mp);
  sqlite3_finalize(fd); sqlite3_finalize(fi); sqlite3_finalize(mu);
}

/* ---- D: cached statements but STILL a per-row BEGIN/COMMIT.
 * Isolates the transaction cost from the prepare cost. ---- */
static void mode_txn_only(sqlite3 *h, int n) { mode_fixed(h, n, 1); }

/* ---- E: per-row prepare but ONE big transaction.
 * The mirror image: isolates prepare cost at scale. ---- */
static void mode_prep_only_batched(sqlite3 *h, int n) {
  sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL);
  for (int i = 0; i < n; i++) {
    char uid[512];
    snprintf(uid, sizeof uid, "npa-traffic-accidents|%d", i);
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(h, "SELECT 1 FROM intel_items WHERE uid=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT); sqlite3_step(s); sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(h, SQL_UPSERT, -1, &s, NULL) == SQLITE_OK) {
      bind_row(s, uid, i); sqlite3_step(s); sqlite3_finalize(s);
    }
    sqlite3_int64 old = -1;
    if (sqlite3_prepare_v2(h, "SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_ROW) old = sqlite3_column_int64(s, 0);
      sqlite3_finalize(s);
    }
    if (old >= 0 && sqlite3_prepare_v2(h, "DELETE FROM intel_items_fts WHERE rowid=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_int64(s, 1, old); sqlite3_step(s); sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(h,
        "INSERT INTO intel_items_fts(uid,title,body,summary,keywords,link,author,tags,props)"
        " VALUES(?1,?2,?3,?4,'',?5,?6,?7,?8)", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, TITLE, -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 3, BODY,  -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 4, "",    -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 5, "https://example.jp/x", -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 6, "",    -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 7, "",    -1, SQLITE_STATIC);
      sqlite3_bind_text(s, 8, PROPS, -1, SQLITE_STATIC);
      sqlite3_step(s); sqlite3_finalize(s);
    }
    sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
    if (sqlite3_prepare_v2(h,
        "INSERT INTO intel_items_fts_uid_map(uid,rowid) VALUES(?1,?2)"
        " ON CONFLICT(uid) DO UPDATE SET rowid=excluded.rowid", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(s, 2, rid); sqlite3_step(s); sqlite3_finalize(s);
    }
  }
  sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
}

/* ---- C: isolate JUST the prepare cost of SQL_UPSERT ---- */
static void mode_prepare_only(sqlite3 *h, int n) {
  for (int i = 0; i < n; i++) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(h, SQL_UPSERT, -1, &s, NULL) == SQLITE_OK)
      sqlite3_finalize(s);
  }
}

int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 20000;
  const char *dir = argc > 2 ? argv[2] : "/tmp";
  char pa[512], pb[512];
  snprintf(pa, sizeof pa, "%s/bench_a.db", dir);
  snprintf(pb, sizeof pb, "%s/bench_b.db", dir);
  unlink(pa); unlink(pb);
  { char t[600]; snprintf(t, sizeof t, "%s-wal", pa); unlink(t);
    snprintf(t, sizeof t, "%s-shm", pa); unlink(t);
    snprintf(t, sizeof t, "%s-wal", pb); unlink(t);
    snprintf(t, sizeof t, "%s-shm", pb); unlink(t); }

  printf("sqlite %s   rows=%d\n\n", sqlite3_libversion(), n);

  sqlite3 *a, *b;
  sqlite3_open(pa, &a); pragmas(a); sqlite3_exec(a, DDL, NULL, NULL, NULL);
  sqlite3_open(pb, &b); pragmas(b); sqlite3_exec(b, DDL, NULL, NULL, NULL);

  double t0 = now_s();
  mode_prepare_only(a, n);
  double tp = now_s() - t0;
  printf("C  prepare(SQL_UPSERT) x%d          : %8.3f s   %8.1f us/row\n",
         n, tp, tp * 1e6 / n);

  t0 = now_s();
  mode_current(a, n);
  double ta = now_s() - t0;
  printf("A  CURRENT emit() pattern            : %8.3f s   %8.1f us/row  %8.1f rows/s\n",
         ta, ta * 1e6 / n, n / ta);

  t0 = now_s();
  mode_fixed(b, n, 1000);
  double tb = now_s() - t0;
  printf("B  cached stmts + 1000-row txn       : %8.3f s   %8.1f us/row  %8.1f rows/s\n",
         tb, tb * 1e6 / n, n / tb);
  printf("\nspeedup B over A: %.2fx    (prepare overhead alone was %.1f%% of A)\n",
         ta / tb, 100.0 * tp / ta);

  /* decomposition: which half of A is the cost? */
  {
    sqlite3 *c, *d;
    char pc[512], pd[512];
    snprintf(pc, sizeof pc, "%s/bench_c.db", dir);
    snprintf(pd, sizeof pd, "%s/bench_d.db", dir);
    unlink(pc); unlink(pd);
    sqlite3_open(pc, &c); pragmas(c); sqlite3_exec(c, DDL, NULL, NULL, NULL);
    sqlite3_open(pd, &d); pragmas(d); sqlite3_exec(d, DDL, NULL, NULL, NULL);
    t0 = now_s(); mode_txn_only(c, n);          double tc = now_s() - t0;
    t0 = now_s(); mode_prep_only_batched(d, n); double td = now_s() - t0;
    printf("\nDECOMPOSITION of A:\n");
    printf("D  cached stmts, per-row txn         : %8.3f s   %8.1f us/row  -> txn cost = %.1f us/row\n",
           tc, tc * 1e6 / n, (tc - tb) * 1e6 / n);
    printf("E  per-row prepare, ONE txn          : %8.3f s   %8.1f us/row  -> prepare cost = %.1f us/row\n",
           td, td * 1e6 / n, (td - tb) * 1e6 / n);
    sqlite3_close(c); sqlite3_close(d);
  }

  /* second pass = the steady-state re-ingest every scheduled refresh does */
  t0 = now_s(); mode_current(a, n); double ta2 = now_s() - t0;
  t0 = now_s(); mode_fixed(b, n, 1000); double tb2 = now_s() - t0;
  printf("\nRE-INGEST (all rows already exist, the scheduled-refresh case):\n");
  printf("A  CURRENT                           : %8.3f s   %8.1f rows/s\n", ta2, n / ta2);
  printf("B  cached+batched                    : %8.3f s   %8.1f rows/s   %.2fx\n",
         tb2, n / tb2, ta2 / tb2);

  sqlite3_close(a); sqlite3_close(b);
  return 0;
}
