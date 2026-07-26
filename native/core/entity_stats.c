/* core/entity_stats.c — PMI / lift over the entity co-mention graph. The
 * rationale, the DDL, the scheduler wiring and the complexity argument all
 * live in entity_stats.h; this file is the mechanics.
 *
 * Shape of a run: one bounded staleness probe, one corpus-size query, one
 * marginal pass into an in-memory map, then keyset pages of edges. Each page
 * is costed OUTSIDE any transaction (the joint queries are reads and would
 * otherwise hold the write lock for their duration) and written INSIDE one
 * short BEGIN IMMEDIATE. IMMEDIATE rather than deferred because the batch is
 * write-only: taking the write lock up front turns a possible mid-batch
 * upgrade failure against an httpd writer into an ordinary wait. */
#include "entity_stats.h"
#include "../third_party/sqlite3.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* entities.entity_id is "ent_" + 16 hex = 20 chars (entitystore.c
 * new_entity_id). 64 is slack, and truncation here could only ever produce a
 * lookup miss — which degrades to "no stats", never to a wrong number. */
#define EID_MAX 64

static int cancelled(volatile int *c) { return c && *c; }

/* ---- marginal map: entity_id -> distinct-item_uid count ------------------
 * Open addressing, linear probing, power-of-two capacity. A map rather than a
 * per-edge "SELECT COUNT(DISTINCT item_uid) WHERE entity_id=?" because the
 * same hub entity appears on thousands of edges and would otherwise be
 * recounted thousands of times; one grouped pass amortises all of it. */
typedef struct { char *key; long long val; } ms_slot;
typedef struct { ms_slot *slot; size_t cap, used; } ms_map;

static uint64_t ms_hash(const char *s) {
  uint64_t hv = 1469598103934665603ULL;              /* FNV-1a 64 */
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    hv ^= (uint64_t)*p;
    hv *= 1099511628211ULL;
  }
  return hv;
}

static int ms_init(ms_map *m, size_t cap) {
  m->slot = calloc(cap, sizeof *m->slot);
  if (!m->slot) return -1;
  m->cap = cap;
  m->used = 0;
  return 0;
}

static void ms_free(ms_map *m) {
  if (!m->slot) return;
  for (size_t i = 0; i < m->cap; i++) free(m->slot[i].key);
  free(m->slot);
  m->slot = NULL;
  m->cap = m->used = 0;
}

static int ms_grow(ms_map *m) {
  ms_map n;
  if (ms_init(&n, m->cap * 2) != 0) return -1;
  for (size_t i = 0; i < m->cap; i++) {
    if (!m->slot[i].key) continue;
    size_t j = (size_t)(ms_hash(m->slot[i].key) & (uint64_t)(n.cap - 1));
    while (n.slot[j].key) j = (j + 1) & (n.cap - 1);
    n.slot[j] = m->slot[i];              /* moves the key, no realloc/copy */
    n.used++;
  }
  free(m->slot);
  *m = n;
  return 0;
}

static int ms_put(ms_map *m, const char *k, long long v) {
  if ((m->used + 1) * 10 >= m->cap * 7 && ms_grow(m) != 0) return -1;
  size_t j = (size_t)(ms_hash(k) & (uint64_t)(m->cap - 1));
  while (m->slot[j].key) {
    if (strcmp(m->slot[j].key, k) == 0) { m->slot[j].val = v; return 0; }
    j = (j + 1) & (m->cap - 1);
  }
  m->slot[j].key = strdup(k);
  if (!m->slot[j].key) return -1;
  m->slot[j].val = v;
  m->used++;
  return 0;
}

/* -1 == absent, which for a marginal means "this entity has no mentions at
 * all", i.e. a provable joint count of zero. */
static long long ms_get(const ms_map *m, const char *k) {
  size_t j = (size_t)(ms_hash(k) & (uint64_t)(m->cap - 1));
  while (m->slot[j].key) {
    if (strcmp(m->slot[j].key, k) == 0) return m->slot[j].val;
    j = (j + 1) & (m->cap - 1);
  }
  return -1;
}

/* ---- schema ------------------------------------------------------------- */

/* 1 present, 0 absent, -1 unable to tell. `table` is always a literal here, so
 * the unbindable PRAGMA table name is not an injection surface. */
static int has_col(sqlite3 *h, const char *table, const char *col) {
  char sql[128];
  sqlite3_stmt *s;
  int found = 0;
  snprintf(sql, sizeof sql, "PRAGMA table_info(%s)", table);
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) != SQLITE_OK) return -1;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *n = (const char *)sqlite3_column_text(s, 1);
    if (n && strcmp(n, col) == 0) { found = 1; break; }
  }
  sqlite3_finalize(s);
  return found;
}

int entity_stats_ensure_columns(db_handle *db) {
  static const char *const col[] = { "pmi", "lift", "co_count", "stats_at" };
  static const char *const ddl[] = {
    "ALTER TABLE entity_relationships ADD COLUMN pmi REAL",
    "ALTER TABLE entity_relationships ADD COLUMN lift REAL",
    "ALTER TABLE entity_relationships ADD COLUMN co_count INTEGER",
    "ALTER TABLE entity_relationships ADD COLUMN stats_at TEXT"
  };
  if (!db || !db->h) return -1;
  for (size_t i = 0; i < sizeof col / sizeof *col; i++) {
    int hc = has_col(db->h, "entity_relationships", col[i]);
    char *e = NULL;
    if (hc < 0) return -1;
    if (hc) continue;
    if (sqlite3_exec(db->h, ddl[i], NULL, NULL, &e) != SQLITE_OK) {
      fprintf(stderr, "[entity-stats] ADD COLUMN %s failed: %s\n",
              col[i], e ? e : "?");
      sqlite3_free(e);
      return -1;
    }
    sqlite3_free(e);
    fprintf(stderr, "[entity-stats] added entity_relationships.%s\n", col[i]);
  }
  return 0;
}

/* ---- recompute ---------------------------------------------------------- */

/* Anything with IFNULL(stats_at,'') below this string is due for scoring. ''
 * sorts before every datetime, so never-scored edges qualify. Resolved once
 * per run and bound as a literal: datetime('now',...) inline in the page query
 * is non-deterministic, so SQLite may re-evaluate it per row. */
static int ttl_horizon(sqlite3 *h, char *out, size_t n) {
  sqlite3_stmt *s;
  char mod[48];
  int ok = 0;
  snprintf(mod, sizeof mod, "-%d seconds", ENTITY_STATS_TTL_SEC);
  if (sqlite3_prepare_v2(h, "SELECT datetime('now', ?1)", -1, &s, NULL)
      != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, mod, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *v = (const char *)sqlite3_column_text(s, 0);
    if (v && *v) { snprintf(out, n, "%s", v); ok = 1; }
  }
  sqlite3_finalize(s);
  return ok;
}

typedef struct {
  sqlite3_int64 rowid;
  char src[EID_MAX], dst[EID_MAX];
  long long co;
  double pmi, lift;
  int have_stats;                        /* pmi/lift survived the floor    */
  int have_co;                           /* co_count is known (may be 0)   */
} er_row;

static const char *SQL_STALE =
  "SELECT 1 FROM entity_relationships"
  " WHERE rel_type<>'asserted' AND IFNULL(stats_at,'')<?1 LIMIT 1";

static const char *SQL_N =
  "SELECT COUNT(DISTINCT item_uid) FROM entity_mentions";

static const char *SQL_MARGINALS =
  "SELECT entity_id, COUNT(DISTINCT item_uid) FROM entity_mentions"
  " GROUP BY entity_id";

static const char *SQL_PAGE =
  "SELECT rowid, src_entity_id, dst_entity_id FROM entity_relationships"
  " WHERE rowid>?1 AND rel_type<>'asserted' AND IFNULL(stats_at,'')<?2"
  " ORDER BY rowid LIMIT ?3";

/* Joint count. ?1 MUST be bound to the lower-marginal entity: the outer scan
 * walks ?1's mentions and probes ?2 through the PK autoindex, so the cost is
 * min(d_a,d_b) probes rather than a symmetric INTERSECT of both sides. */
static const char *SQL_JOINT =
  "SELECT COUNT(DISTINCT a.item_uid) FROM entity_mentions a"
  " WHERE a.entity_id=?1 AND EXISTS (SELECT 1 FROM entity_mentions b"
  " WHERE b.entity_id=?2 AND b.item_uid=a.item_uid)";

static const char *SQL_UPDATE =
  "UPDATE entity_relationships SET pmi=?1, lift=?2, co_count=?3,"
  " stats_at=datetime('now') WHERE rowid=?4";

long entity_stats_recompute(db_handle *db, volatile int *cancel) {
  sqlite3 *h;
  sqlite3_stmt *s = NULL, *page_q = NULL, *joint_q = NULL, *upd_q = NULL;
  er_row *page = NULL;
  ms_map marg;
  char horizon[64];
  long long N = 0;
  sqlite3_int64 after = 0;
  long total = 0;
  int fail = 0, stale = 0;

  if (!db || !db->h) return -1;
  h = db->h;
  if (entity_stats_ensure_columns(db) != 0) return -1;
  if (!ttl_horizon(h, horizon, sizeof horizon)) return -1;

  /* Pre-flight. The pod ticks every few minutes but a full sweep is only due
   * once per TTL, so the common case must cost one bounded probe and not a
   * full pass over entity_mentions (which is the large table). */
  if (sqlite3_prepare_v2(h, SQL_STALE, -1, &s, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(s, 1, horizon, -1, SQLITE_TRANSIENT);
  stale = (sqlite3_step(s) == SQLITE_ROW);
  sqlite3_finalize(s);
  if (!stale) return 0;

  /* N — the corpus size the probabilities are relative to. Documents with no
   * mention at all are excluded on purpose: they are outside the observation
   * space of this estimator, and counting them would deflate every P() by the
   * same unextracted-backlog factor. */
  if (sqlite3_prepare_v2(h, SQL_N, -1, &s, NULL) != SQLITE_OK) return -1;
  if (sqlite3_step(s) == SQLITE_ROW) N = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  if (N <= 0) return 0;

  if (ms_init(&marg, 1024) != 0) return -1;
  if (sqlite3_prepare_v2(h, SQL_MARGINALS, -1, &s, NULL) != SQLITE_OK) {
    ms_free(&marg);
    return -1;
  }
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *id = (const char *)sqlite3_column_text(s, 0);
    if (cancelled(cancel)) { fail = 2; break; }
    if (!id || !*id) continue;
    if (ms_put(&marg, id, sqlite3_column_int64(s, 1)) != 0) { fail = 1; break; }
  }
  sqlite3_finalize(s);
  if (fail) {                            /* 1 = OOM, 2 = cancelled early   */
    ms_free(&marg);
    return fail == 1 ? -1 : 0;
  }

  page = malloc(sizeof *page * ENTITY_STATS_BATCH);
  if (!page ||
      sqlite3_prepare_v2(h, SQL_PAGE,   -1, &page_q,  NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(h, SQL_JOINT,  -1, &joint_q, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(h, SQL_UPDATE, -1, &upd_q,   NULL) != SQLITE_OK)
    fail = 1;

  while (!fail && !cancelled(cancel)) {
    int n = 0, i;
    long wrote = 0;

    sqlite3_reset(page_q);
    sqlite3_clear_bindings(page_q);
    sqlite3_bind_int64(page_q, 1, after);
    sqlite3_bind_text(page_q, 2, horizon, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(page_q, 3, ENTITY_STATS_BATCH);
    while (n < ENTITY_STATS_BATCH && sqlite3_step(page_q) == SQLITE_ROW) {
      const char *a = (const char *)sqlite3_column_text(page_q, 1);
      const char *b = (const char *)sqlite3_column_text(page_q, 2);
      page[n].rowid = sqlite3_column_int64(page_q, 0);
      snprintf(page[n].src, EID_MAX, "%s", a ? a : "");
      snprintf(page[n].dst, EID_MAX, "%s", b ? b : "");
      n++;
    }
    sqlite3_reset(page_q);
    if (n == 0) break;
    after = page[n - 1].rowid;           /* ORDER BY rowid: last is highest */

    /* Cost the page with reads only — no transaction open yet. */
    for (i = 0; i < n; i++) {
      er_row *r = &page[i];
      long long ca, cb;
      const char *small, *large;
      double lift;

      r->co = 0;
      r->pmi = r->lift = 0.0;
      r->have_stats = 0;
      r->have_co = 0;
      if (cancelled(cancel)) break;
      if (!r->src[0] || !r->dst[0] || strcmp(r->src, r->dst) == 0) continue;

      ca = ms_get(&marg, r->src);
      cb = ms_get(&marg, r->dst);
      r->have_co = 1;                    /* absent marginal => joint is 0   */
      if (ca <= 0 || cb <= 0) continue;

      small = (ca <= cb) ? r->src : r->dst;
      large = (ca <= cb) ? r->dst : r->src;
      sqlite3_reset(joint_q);
      sqlite3_clear_bindings(joint_q);
      sqlite3_bind_text(joint_q, 1, small, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(joint_q, 2, large, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(joint_q) == SQLITE_ROW)
        r->co = sqlite3_column_int64(joint_q, 0);
      sqlite3_reset(joint_q);

      /* Support floor: below it the estimate is noise, so say nothing rather
       * than something large and wrong. co_count still gets written. */
      if (r->co < ENTITY_STATS_MIN_CO_COUNT) continue;

      lift = ((double)r->co * (double)N) / ((double)ca * (double)cb);
      if (!isfinite(lift) || lift <= 0.0) continue;
      r->lift = lift;
      r->pmi = log(lift);
      r->have_stats = 1;
    }

    /* A cancel mid-page discards that page's reads rather than committing a
     * half-costed batch — the edges simply stay stale for the next run. */
    if (cancelled(cancel)) break;
    if (sqlite3_exec(h, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
      fail = 1;
      break;
    }
    for (i = 0; i < n; i++) {
      er_row *r = &page[i];
      sqlite3_reset(upd_q);
      sqlite3_clear_bindings(upd_q);
      if (r->have_stats) {
        sqlite3_bind_double(upd_q, 1, r->pmi);
        sqlite3_bind_double(upd_q, 2, r->lift);
      } else {
        sqlite3_bind_null(upd_q, 1);
        sqlite3_bind_null(upd_q, 2);
      }
      if (r->have_co) sqlite3_bind_int64(upd_q, 3, r->co);
      else            sqlite3_bind_null(upd_q, 3);
      sqlite3_bind_int64(upd_q, 4, r->rowid);
      if (sqlite3_step(upd_q) == SQLITE_DONE) wrote++;
      sqlite3_reset(upd_q);
    }
    if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
      sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
      fail = 1;
      break;
    }
    total += wrote;
    if (n < ENTITY_STATS_BATCH) break;   /* short page => sweep complete    */
  }

  sqlite3_finalize(page_q);
  sqlite3_finalize(joint_q);
  sqlite3_finalize(upd_q);
  free(page);
  fprintf(stderr, "[entity-stats] N=%lld entities=%zu edges_scored=%ld%s\n",
          N, marg.used, total, cancelled(cancel) ? " (cancelled)" : "");
  ms_free(&marg);
  return fail ? -1 : total;
}
