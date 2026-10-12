/* core/story.c — see story.h. */
#include "story.h"
#include "embed_pod.h"
#include "../lib/vecindex.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Pending rows drained per hook call, at most (each costs two KNNs). */
#define STORY_DRAIN_MAX 64

static double env_double(const char *k, double dflt, double lo, double hi) {
  const char *e = getenv(k);
  if (!e || !*e) return dflt;
  char *end = NULL;
  double v = strtod(e, &end);
  return (end && end != e && v >= lo && v <= hi) ? v : dflt;
}
static int env_int(const char *k, int dflt) {
  const char *e = getenv(k);
  int v = (e && *e) ? atoi(e) : 0;
  return v > 0 ? v : dflt;
}

double story_tau(void)        { return env_double("JO_STORY_TAU", STORY_TAU_DEFAULT, 0.01, 1.0); }
int    story_max_members(void){ return env_int("JO_STORY_MAX_MEMBERS", STORY_MAX_MEMBERS_DEFAULT); }
int    novelty_days(void)     { return env_int("JO_NOVELTY_DAYS", NOVELTY_DAYS_DEFAULT); }
static int budget_ms(void)    { return env_int("JO_STORY_BUDGET_MS", STORY_BUDGET_MS_DEFAULT); }

void story_migrate(db_handle *db) {
  ensure_column(db, "intel_items", "story_id", "TEXT");
  ensure_column(db, "intel_items", "novelty", "REAL");
  sqlite3_exec(db->h,
    "CREATE INDEX IF NOT EXISTS idx_intel_items_story ON intel_items(story_id)"
    " WHERE story_id IS NOT NULL;"
    "CREATE TABLE IF NOT EXISTS story_pending (uid TEXT PRIMARY KEY,"
    " tenant_id TEXT NOT NULL, ts INTEGER NOT NULL, queued_at TEXT NOT NULL"
    " DEFAULT (datetime('now')));"
    "CREATE TABLE IF NOT EXISTS story_meta (k TEXT PRIMARY KEY, v TEXT);",
    NULL, NULL, NULL);
}

void story_id_for(const char *seed_uid, char *out, size_t cap) {
  snprintf(out, cap, "st-%016llx", vecindex_hash(seed_uid ? seed_uid : ""));
}

static long long mono_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}

/* Exact cosine to a stored neighbour (no unit-norm assumption). -2 if the
 * neighbour's vector cannot be read. */
static double cos_to(db_handle *db, const char *key, const float *q, int dim) {
  float *v = NULL; int d = 0;
  if (vecindex_load(db, &embed_intel_spec, key, &v, &d) != 0 || d != dim) {
    free(v);
    return -2;
  }
  double c = vec_cosine(q, v, dim);
  free(v);
  return c;
}

static char *col_text(db_handle *db, const char *sql, const char *uid) {
  sqlite3_stmt *s; char *v = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
      v = strdup((const char *)sqlite3_column_text(s, 0));
    sqlite3_finalize(s);
  }
  return v;
}

static long story_size(db_handle *db, const char *sid) {
  sqlite3_stmt *s; long n = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT count(*) FROM intel_items WHERE story_id=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, sid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  return n;
}

/* 1 when the row was written (it was NULL). */
static int set_story(db_handle *db, const char *uid, const char *sid) {
  sqlite3_stmt *s; int ch = 0;
  if (sqlite3_prepare_v2(db->h, "UPDATE intel_items SET story_id=?2"
                         " WHERE uid=?1 AND story_id IS NULL", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, sid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_DONE) ch = sqlite3_changes(db->h) > 0;
    sqlite3_finalize(s);
  }
  return ch;
}

int story_novelty(db_handle *db, const char *uid, const char *tenant,
                  long long ts, const float *vec, int dim, double *out) {
  {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, "SELECT novelty FROM intel_items WHERE uid=?1",
                           -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      int have = sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL;
      if (have) *out = sqlite3_column_double(s, 0);
      sqlite3_finalize(s);
      if (have) return 0;
    }
  }
  if (!vec || dim <= 0) return -1;
  int legacy = !tenant || !*tenant || !strcmp(tenant, "legacy");
  vecindex_val tv[2] = { VI_S(legacy ? "legacy" : tenant), VI_S("legacy") };
  vecindex_val lo = VI_I(ts - (long long)novelty_days() * 86400LL), hi = VI_I(ts);
  vecindex_filter f[3] = {
    { .col = "tenant_id", .op = "IN", .vals = tv, .nvals = legacy ? 1 : 2 },
    { .col = "ts", .op = ">=", .vals = &lo, .nvals = 1 },
    { .col = "ts", .op = "<=", .vals = &hi, .nvals = 1 },
  };
  vecindex_hit *h = NULL;
  int n = vecindex_knn(db, &embed_intel_spec, vec, dim, 2, f, 3, &h, NULL, 0);
  if (n < 0) return -1;
  double best = -1;
  for (int i = 0; i < n; i++) {
    if (!strcmp(h[i].key, uid)) continue;
    double c = cos_to(db, h[i].key, vec, dim);
    if (c > best) best = c;
  }
  vecindex_hits_free(h, n);
  double nov = best < -1.5 ? 1.0 : 1.0 - best;
  if (nov < 0) nov = 0;
  if (nov > 1) nov = 1;
  *out = nov;
  return 0;
}

typedef struct { char *uid; double cos; char *sid; } nbr;

static int nbr_cmp(const void *a, const void *b) {
  const nbr *x = a, *y = b;
  return x->cos > y->cos ? -1 : x->cos < y->cos ? 1 : strcmp(x->uid, y->uid);
}

/* Story for one row; returns 1 when it joined or seeded a story. */
static int story_row(db_handle *db, const char *uid, const char *tenant,
                     long long ts, const float *vec, int dim) {
  char *have = col_text(db, "SELECT story_id FROM intel_items WHERE uid=?1", uid);
  if (have) { free(have); return 0; }             /* a story is kept */
  vecindex_val tv = VI_S(tenant && *tenant ? tenant : "legacy");
  vecindex_val lo = VI_I(ts - STORY_WINDOW_SEC), hi = VI_I(ts + STORY_WINDOW_SEC);
  vecindex_filter f[3] = {
    { .col = "tenant_id", .op = "=", .vals = &tv, .nvals = 1 },
    { .col = "ts", .op = ">=", .vals = &lo, .nvals = 1 },
    { .col = "ts", .op = "<=", .vals = &hi, .nvals = 1 },
  };
  vecindex_hit *h = NULL;
  int n = vecindex_knn(db, &embed_intel_spec, vec, dim, STORY_K + 1, f, 3, &h, NULL, 0);
  if (n <= 0) { vecindex_hits_free(h, n > 0 ? n : 0); return 0; }
  double tau = story_tau();
  nbr *k = calloc((size_t)n, sizeof *k);
  int nk = 0;
  for (int i = 0; k && i < n; i++) {
    if (!strcmp(h[i].key, uid)) continue;
    double c = cos_to(db, h[i].key, vec, dim);
    if (c < tau) continue;
    k[nk].uid = strdup(h[i].key);
    k[nk].cos = c;
    k[nk].sid = col_text(db, "SELECT story_id FROM intel_items WHERE uid=?1", h[i].key);
    if (k[nk].uid) nk++;
  }
  vecindex_hits_free(h, n);
  int joined = 0;
  if (nk > 0) {
    qsort(k, (size_t)nk, sizeof *k, nbr_cmp);
    int maxm = story_max_members();
    char target[40] = "";
    for (int i = 0; i < nk && !target[0]; i++)
      if (k[i].sid && story_size(db, k[i].sid) < maxm)
        snprintf(target, sizeof target, "%s", k[i].sid);
    if (!target[0]) story_id_for(uid, target, sizeof target);
    long size = story_size(db, target);
    if (size < maxm && set_story(db, uid, target)) { joined = 1; size++; }
    /* pull in the singletons above tau; never another story's members */
    for (int i = 0; joined && i < nk && size < maxm; i++)
      if (!k[i].sid && set_story(db, k[i].uid, target)) size++;
  }
  for (int i = 0; i < nk; i++) { free(k[i].uid); free(k[i].sid); }
  free(k);
  return joined;
}

static void set_novelty(db_handle *db, const char *uid, double v) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "UPDATE intel_items SET novelty=?2 WHERE uid=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(s, 2, v);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
}

static int process_row(db_handle *db, const char *uid, const char *tenant,
                       long long ts, const float *vec, int dim) {
  int j = story_row(db, uid, tenant, ts, vec, dim);
  sqlite3_stmt *s;
  int need = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT novelty IS NULL FROM intel_items WHERE uid=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    need = sqlite3_step(s) == SQLITE_ROW && sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
  }
  double nv;
  if (need && story_novelty(db, uid, tenant, ts, vec, dim, &nv) == 0) set_novelty(db, uid, nv);
  return j;
}

static void meta_add(db_handle *db, const char *k, long d) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "INSERT INTO story_meta(k,v) VALUES(?1,?2)"
      " ON CONFLICT(k) DO UPDATE SET v=CAST(v AS INTEGER)+?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, k, -1, SQLITE_STATIC);
    sqlite3_bind_int64(s, 2, d);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
}

static void enqueue(db_handle *db, const char *uid, const char *tenant, long long ts) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "INSERT OR IGNORE INTO story_pending(uid,tenant_id,ts)"
                         " VALUES(?1,?2,?3)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tenant && *tenant ? tenant : "legacy", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 3, ts);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
}

void story_process_batch(db_handle *db, int n, const char *const *uids,
                         const float *vectors, int dim, const char *const *tenants,
                         const long long *ts) {
  if (!db || n < 0 || dim <= 0) return;      /* n == 0: drain pending only */
  long long deadline = mono_ms() + budget_ms();
  if (vecindex_begin(db) != 0) {
    /* could not take the write lock: nothing is lost, it waits in pending */
    return;
  }
  int done = 0, joined = 0, queued = 0;
  for (int i = 0; i < n; i++) {
    if (mono_ms() >= deadline) { enqueue(db, uids[i], tenants[i], ts[i]); queued++; continue; }
    joined += process_row(db, uids[i], tenants[i], ts[i], vectors + (size_t)i * dim, dim);
    done++;
  }
  /* drain the backlog with what is left of the budget */
  int drained = 0;
  while (mono_ms() < deadline && drained < STORY_DRAIN_MAX) {
    sqlite3_stmt *s;
    char *puid = NULL, *pten = NULL; long long pts = 0;
    if (sqlite3_prepare_v2(db->h, "SELECT uid,tenant_id,ts FROM story_pending"
                           " ORDER BY rowid LIMIT 1", -1, &s, NULL) != SQLITE_OK) break;
    if (sqlite3_step(s) == SQLITE_ROW) {
      puid = strdup((const char *)sqlite3_column_text(s, 0));
      pten = strdup((const char *)sqlite3_column_text(s, 1));
      pts = sqlite3_column_int64(s, 2);
    }
    sqlite3_finalize(s);
    if (!puid || !pten) { free(puid); free(pten); break; }
    float *v = NULL; int d = 0;
    if (vecindex_load(db, &embed_intel_spec, puid, &v, &d) == 0 && d == dim)
      joined += process_row(db, puid, pten, pts, v, d);
    /* a vector that is gone (row deleted) has nothing left to measure */
    free(v);
    if (sqlite3_prepare_v2(db->h, "DELETE FROM story_pending WHERE uid=?1",
                           -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, puid, -1, SQLITE_TRANSIENT);
      sqlite3_step(s);
      sqlite3_finalize(s);
    }
    free(puid); free(pten);
    drained++;
  }
  meta_add(db, "processed", done + drained);
  meta_add(db, "joined", joined);
  meta_add(db, "queued", queued);
  if (vecindex_end(db, 1) != 0)
    fprintf(stderr, "[story] commit failed: %s — %d row(s) will be re-delivered "
                    "only if re-embedded\n", sqlite3_errmsg(db->h), done);
  else if (queued)
    fprintf(stderr, "[story] budget reached: %d row(s) queued in story_pending\n", queued);
}

static void story_hook(db_handle *db, int n, const char *const *uids,
                       const float *vectors, int dim, const embed_row_meta *meta,
                       void *user) {
  (void)user;
  const char **ten = calloc((size_t)n, sizeof *ten);
  long long *ts = calloc((size_t)n, sizeof *ts);
  if (ten && ts) {
    for (int i = 0; i < n; i++) { ten[i] = meta[i].tenant_id; ts[i] = meta[i].ts; }
    story_process_batch(db, n, uids, vectors, dim, ten, ts);
  }
  free(ten); free(ts);
}

__attribute__((constructor)) static void story_register(void) {
  const char *e = getenv("JO_STORY_HOOK");
  if (e && e[0] == '0' && e[1] == 0) return;
  if (embed_hook_register(story_hook, NULL) != 0)
    fprintf(stderr, "[story] could not register the post-batch hook (16 in use)\n");
}
