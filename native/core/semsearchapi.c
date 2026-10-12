/* core/semsearchapi.c — see semsearchapi.h. */
#include "semsearchapi.h"
#include "embed_pod.h"
#include "intelapi.h"
#include "breach_meta.h"
#include "translate.h"
#include "fts.h"
#include "llm.h"
#include "../third_party/cJSON.h"
#include "../third_party/mongoose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RRF_K VECINDEX_RRF_K

/* ---- item shape ----------------------------------------------------------
 * The row is rendered by intelapi_row_to_item(), the builder /api/intel/items
 * uses — this file used to carry a verbatim copy of it, which had already
 * drifted (no cluster_id). Column 20 is the row's date as the index's `ts`
 * column holds it (EMBED_ROW_TS), so a post-filter on the date reads the
 * same rule the KNN filter did. */
#define SEL_ROW "SELECT " INTELAPI_ITEM_COLS ", " EMBED_ROW_TS " FROM intel_items"
enum { C_SRC = 1, C_LANG = 7, C_LAT = 13, C_LON = 14, C_RTYPE = 17, C_TS = 20 };

static char *fail(int *status, int code, const char *err, const char *detail,
                  cJSON *coverage) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", err);
  if (detail) cJSON_AddStringToObject(o, "detail", detail);
  if (coverage) {
    cJSON *meta = cJSON_CreateObject();
    cJSON_AddItemToObject(meta, "coverage", coverage);
    cJSON_AddItemToObject(o, "meta", meta);
  }
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  *status = code;
  return s;
}

static long ms_since(const struct timespec *t0) {
  struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
  return (t1.tv_sec - t0->tv_sec) * 1000L + (t1.tv_nsec - t0->tv_nsec) / 1000000L;
}

/* ---- filters ------------------------------------------------------------- */

void semsearch_rf_free(semsearch_rf *rf) {
  if (!rf) return;
  free(rf->srcbuf);
  memset(rf, 0, sizeof *rf);
}

/* Epoch seconds of any time text SQLite's strftime reads — the parser that
 * produced the index's `ts` column — or -1 when it reads nothing. */
static long long epoch_of(db_handle *db, const char *t) {
  sqlite3_stmt *s;
  long long v = -1;
  if (sqlite3_prepare_v2(db->h, "SELECT CAST(strftime('%s', ?1) AS INTEGER)",
                         -1, &s, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(s, 1, t, -1, SQLITE_STATIC);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
    v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

int semsearch_resolve(db_handle *db, const semsearch_filters *f,
                      semsearch_rf *rf, char *err, size_t cap) {
  memset(rf, 0, sizeof *rf);
  if (err && cap) err[0] = 0;
  if (!f) return 0;
  if (f->source && *f->source) {
    rf->srcbuf = strdup(f->source);
    if (!rf->srcbuf) { snprintf(err, cap, "server_error"); return -1; }
    for (char *p = rf->srcbuf; p && *p; ) {
      char *c = strchr(p, ',');
      if (c) *c = 0;
      while (*p == ' ') p++;
      size_t L = strlen(p);
      while (L && p[L - 1] == ' ') p[--L] = 0;
      if (*p) {
        /* Refuse rather than drop the 33rd: a source list cut short is a
         * different question. */
        if (rf->nsrc >= SEMSEARCH_MAX_SOURCES) {
          snprintf(err, cap, "too_many_sources");
          semsearch_rf_free(rf);
          return -1;
        }
        rf->src[rf->nsrc++] = p;
      }
      p = c ? c + 1 : NULL;
    }
  }
  if (f->record_type && *f->record_type) rf->rtype = f->record_type;
  if (f->since && *f->since) {
    rf->since_ts = epoch_of(db, f->since);
    if (rf->since_ts < 0) { snprintf(err, cap, "invalid_since"); semsearch_rf_free(rf); return -1; }
    rf->has_since = 1;
  }
  if (f->until && *f->until) {
    rf->until_ts = epoch_of(db, f->until);
    if (rf->until_ts < 0) { snprintf(err, cap, "invalid_until"); semsearch_rf_free(rf); return -1; }
    rf->has_until = 1;
  }
  if (f->lang && *f->lang) rf->lang = f->lang;
  if (f->bbox && *f->bbox) {
    double c[4]; const char *p = f->bbox; char *e;
    for (int i = 0; i < 4; i++) {
      c[i] = strtod(p, &e);
      if (e == p || (i < 3 && *e != ',') || (i == 3 && *e)) {
        snprintf(err, cap, "invalid_bbox"); semsearch_rf_free(rf); return -1;
      }
      p = e + 1;
    }
    /* w,s,e,n — the timeline's rule: an inverted or out-of-range box is a
     * 400, not a box that silently matches nothing. */
    if (c[0] > c[2] || c[1] > c[3] || c[1] < -90 || c[3] > 90 ||
        c[0] < -180 || c[2] > 180) {
      snprintf(err, cap, "invalid_bbox"); semsearch_rf_free(rf); return -1;
    }
    rf->has_bbox = 1; rf->bw = c[0]; rf->bs = c[1]; rf->be = c[2]; rf->bn = c[3];
  }
  return 0;
}

static int rf_any_knn(const semsearch_rf *rf) {
  return rf && (rf->nsrc || rf->rtype || rf->has_since || rf->has_until);
}
static int rf_any(const semsearch_rf *rf) {
  return rf_any_knn(rf) || (rf && (rf->lang || rf->has_bbox));
}

/* Which filter a stepped SEL_ROW row fails, first match wins; -1 = passes. */
enum { D_SOURCE = 0, D_RTYPE, D_DATE, D_LANG, D_BBOX, D_N };
static const char *D_NAMES[D_N] = { "source", "record_type", "date", "lang", "bbox" };
static int row_fails(sqlite3_stmt *s, const semsearch_rf *rf) {
  if (!rf) return -1;
  if (rf->nsrc) {
    const char *v = (const char *)sqlite3_column_text(s, C_SRC);
    int ok = 0;
    for (int i = 0; v && i < rf->nsrc && !ok; i++) ok = !strcmp(v, rf->src[i]);
    if (!ok) return D_SOURCE;
  }
  if (rf->rtype) {
    const char *v = (const char *)sqlite3_column_text(s, C_RTYPE);
    if (!v || strcmp(v, rf->rtype)) return D_RTYPE;
  }
  long long ts = sqlite3_column_int64(s, C_TS);
  if (rf->has_since && ts < rf->since_ts) return D_DATE;
  if (rf->has_until && ts > rf->until_ts) return D_DATE;
  if (rf->lang) {
    const char *v = (const char *)sqlite3_column_text(s, C_LANG);
    if (!v || strcmp(v, rf->lang)) return D_LANG;
  }
  if (rf->has_bbox) {
    if (sqlite3_column_type(s, C_LAT) == SQLITE_NULL ||
        sqlite3_column_type(s, C_LON) == SQLITE_NULL) return D_BBOX;
    double la = sqlite3_column_double(s, C_LAT), lo = sqlite3_column_double(s, C_LON);
    if (la < rf->bs || la > rf->bn || lo < rf->bw || lo > rf->be) return D_BBOX;
  }
  return -1;
}

/* ---- the query vector ---------------------------------------------------- */

char *semsearch_embed(db_handle *db, const char *q, float **qv_out,
                      int *dim_out, long *embed_ms, int *status) {
  *qv_out = NULL; *dim_out = 0;
  if (embed_ms) *embed_ms = 0;
  /* The 503s carry coverage so the caller can tell "not configured" from
   * "configured, index not built yet" from the body alone. */
  const char *base = embed_base_url();
  if (!base)
    return fail(status, 503, "semantic_unavailable",
                "no embedding server configured (JO_EMBED_URL is unset)",
                embed_coverage_json(db));
  char model[256];
  int dim = embed_index_dim(db, model, sizeof model);
  if (!dim || !embed_table_exists(db))
    return fail(status, 503, "semantic_unavailable",
                "the embedding pod has not built an index yet",
                embed_coverage_json(db));

  /* REFUSED MEANS REFUSED HERE TOO. The pod stops writing when the server's
   * model or dimension no longer matches the index, and records why. The
   * query side used to check only the dimension — so after a same-width swap
   * (bge-m3 -> multilingual-e5-large, both 1024-d) it embedded the query in
   * the new space, ranked it against vectors from the old one, and answered
   * 200 with rankings that mean nothing, while the coverage block in that
   * same response said "refused". */
  char why[600];
  if (embed_index_refused(db, why, sizeof why))
    return fail(status, 503, "semantic_unavailable", why, embed_coverage_json(db));
  char live[256];
  if (embed_live_query_model(live, sizeof live) != 0)
    return fail(status, 502, "embedding_model_unverified", live,
                embed_coverage_json(db));
  if (model[0] && strcmp(model, live) != 0) {
    char d[600];
    snprintf(d, sizeof d, "the index was built with model '%.200s' but the "
             "embedding server is now '%.200s'; vectors from two models are not "
             "comparable, so nothing is ranked until the index is rebuilt",
             model, live);
    return fail(status, 503, "semantic_unavailable", d, embed_coverage_json(db));
  }

  /* Embed the query under the same text bound the index was built with,
   * on the interactive lane of the embedding server's own worker. That worker
   * is shared with the backfill pod, whose batches run up to 120 s; the
   * interactive lane only jumps the QUEUE, never a request in flight, so the
   * 15 s budget is made to cover the queue wait too (bound_queue_wait). */
  struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
  char *qb = embed_bound_text(q);
  if (!qb) return fail(status, 500, "server_error", NULL, NULL);
  /* JO_EMBED_QUERY_URL (default JO_EMBED_URL): with a slot of its own the
   * query never meets the backfill's queue at all; the model it serves was
   * checked against the index's just above. */
  llm_client llm = { .http = NULL, .base_url = embed_query_url(),
                     .interactive = 1, .bound_queue_wait = 1 };
  const char *texts[1] = { qb };
  float *qv = NULL; int qdim = 0; llm_status st;
  int erc = llm_embed(&llm, texts, 1, &qv, &qdim, 15000, &st);
  free(qb);
  if (embed_ms) *embed_ms = ms_since(&t0);
  if (erc != 0)
    return fail(status, 502, "embedding_failed", llm_status_code(st),
                embed_coverage_json(db));
  if (qdim != dim) {
    free(qv);
    char d[160];
    snprintf(d, sizeof d, "embedding server answered %d-d but the index is %d-d "
             "(model '%.60s'); refusing to compare", qdim, dim, model);
    return fail(status, 502, "embedding_dim_mismatch", d, embed_coverage_json(db));
  }
  *qv_out = qv; *dim_out = dim;
  return NULL;
}

/* ---- the vector arm ------------------------------------------------------ */

int semsearch_knn(db_handle *db, const char *tenant, const float *qv, int dim,
                  int k, const semsearch_rf *rf, vecindex_hit **out,
                  int *in_knn, char *err, size_t errcap) {
  /* FILTERED INSIDE THE KNN. The index is partitioned on tenant_id, so a
   * tenant's query asks for the k nearest among its own rows and the shared
   * corpus — it used to ask for the k nearest across EVERY tenant and then
   * drop what it could not show, so a tenant whose rows sat below k
   * other-tenant rows was answered with nothing. source/record_type/ts are
   * metadata columns and are filtered the same way. While a migration from
   * the pre-metadata table is pending (embed_query_spec) there is nothing to
   * filter on, and the per-row post-check in materialize() is all there is —
   * `in_knn` says which happened. */
  const vecindex_spec *vs = embed_query_spec(db);
  int meta = (vs == &embed_intel_spec);
  vecindex_val tv[2] = { VI_S(tenant), VI_S("legacy") };
  vecindex_val sv[SEMSEARCH_MAX_SOURCES];
  vecindex_val rv[1], sinv[1], unv[1];
  vecindex_filter f[5];
  int nf = 0;
  if (meta && tenant && *tenant)
    f[nf++] = (vecindex_filter){ .col = "tenant_id", .op = "IN", .vals = tv,
                                 .nvals = !strcmp(tenant, "legacy") ? 1 : 2 };
  if (meta && rf && rf->nsrc) {
    for (int i = 0; i < rf->nsrc; i++) sv[i] = VI_S(rf->src[i]);
    f[nf++] = (vecindex_filter){ .col = "source_id", .op = rf->nsrc > 1 ? "IN" : "=",
                                 .vals = sv, .nvals = rf->nsrc };
  }
  if (meta && rf && rf->rtype) {
    rv[0] = VI_S(rf->rtype);
    f[nf++] = (vecindex_filter){ .col = "record_type", .op = "=", .vals = rv, .nvals = 1 };
  }
  if (meta && rf && rf->has_since) {
    sinv[0] = VI_I(rf->since_ts);
    f[nf++] = (vecindex_filter){ .col = "ts", .op = ">=", .vals = sinv, .nvals = 1 };
  }
  if (meta && rf && rf->has_until) {
    unv[0] = VI_I(rf->until_ts);
    f[nf++] = (vecindex_filter){ .col = "ts", .op = "<=", .vals = unv, .nvals = 1 };
  }
  if (in_knn) *in_knn = meta;
  if (k > SEMSEARCH_KNN_MAX) k = SEMSEARCH_KNN_MAX;
  return vecindex_knn(db, vs, qv, dim, k, f, nf, out, err, errcap);
}

/* ---- materialising a ranked list ---------------------------------------- */

typedef struct {
  int visible, shown, withheld, orphans, excluded, skipped;
  int dropped[D_N];
  int post_dropped;            /* rows the caller's `post` statement rejected */
} sem_counts;

static int in_set(const char *k, const char *const *set, int n) {
  for (int i = 0; i < n; i++) if (!strcmp(k, set[i])) return 1;
  return 0;
}

/* Walk the ranked candidates, nearest/best first: look every one up (a
 * primary-key seek each) so `visible` is the number the caller could page to,
 * not a guess; build items only for [offset, offset+limit). A candidate that
 * is not a visible row is another tenant's (withheld) or a vector whose row
 * is gone (orphan) — only an operator is told either number, because how many
 * of OTHER tenants' rows match is information about those tenants. A
 * candidate in `excl` (the item itself, a case's own items) is counted, not
 * shown. A row a post-filter rejects is counted under that filter. `post`
 * (may be NULL) is a caller's prepared `SELECT 1 … WHERE uid=?1 AND …` with
 * its other parameters already bound — a filter set the KNN cannot express
 * (the feed's own WHERE, for sort=hybrid); a row it does not return is
 * counted in post_dropped. */
static int materialize(db_handle *db, const char *tenant, int is_operator,
                       const vecindex_fused *L, int offset, int limit,
                       const semsearch_rf *rf, const char *const *excl, int nexcl,
                       sqlite3_stmt *post, cJSON *data, sem_counts *c) {
  memset(c, 0, sizeof *c);
  sqlite3_stmt *get;
  const char *sql = (tenant && *tenant)
    ? SEL_ROW " WHERE uid=?1 AND tenant_id IN (?2,'legacy')"
    : SEL_ROW " WHERE uid=?1";
  if (sqlite3_prepare_v2(db->h, sql, -1, &get, NULL) != SQLITE_OK) return -1;
  sqlite3_stmt *exists = NULL;
  if (is_operator &&
      sqlite3_prepare_v2(db->h, "SELECT 1 FROM intel_items WHERE uid=?1",
                         -1, &exists, NULL) != SQLITE_OK)
    exists = NULL;
  for (int i = 0; i < L->n; i++) {
    const vecindex_cand *cd = &L->v[i];
    if (nexcl && in_set(cd->key, excl, nexcl)) { c->excluded++; continue; }
    sqlite3_reset(get);
    sqlite3_bind_text(get, 1, cd->key, -1, SQLITE_STATIC);
    if (tenant && *tenant) sqlite3_bind_text(get, 2, tenant, -1, SQLITE_STATIC);
    if (sqlite3_step(get) != SQLITE_ROW) {
      if (exists) {
        sqlite3_reset(exists);
        sqlite3_bind_text(exists, 1, cd->key, -1, SQLITE_STATIC);
        if (sqlite3_step(exists) == SQLITE_ROW) c->withheld++; else c->orphans++;
      }
      continue;
    }
    int why = row_fails(get, rf);
    if (why >= 0) { c->dropped[why]++; continue; }
    if (post) {
      sqlite3_reset(post);
      sqlite3_bind_text(post, 1, cd->key, -1, SQLITE_STATIC);
      if (sqlite3_step(post) != SQLITE_ROW) { c->post_dropped++; continue; }
    }
    c->visible++;
    if (c->visible <= offset) { c->skipped++; continue; }
    if (c->shown >= limit) continue;
    cJSON *it = intelapi_row_to_item(get, 0);
    cJSON *sem = cJSON_CreateObject();
    cJSON_AddNumberToObject(sem, "score", cd->score);
    cJSON_AddNumberToObject(sem, "rank", offset + ++c->shown);
    if (cd->vrank) { cJSON_AddNumberToObject(sem, "vector_rank", cd->vrank);
                     cJSON_AddNumberToObject(sem, "distance", cd->distance); }
    else           { cJSON_AddNullToObject(sem, "vector_rank");
                     cJSON_AddNullToObject(sem, "distance"); }
    if (cd->frank) cJSON_AddNumberToObject(sem, "fts_rank", cd->frank);
    else           cJSON_AddNullToObject(sem, "fts_rank");
    cJSON_AddItemToObject(it, "semantic", sem);
    cJSON_AddItemToArray(data, it);
  }
  sqlite3_finalize(get);
  sqlite3_finalize(exists);
  return 0;
}

/* The filter block of meta: what was asked, where each filter ran, and how
 * many candidates each post-filter (and, while the index has no metadata
 * columns, each KNN filter's post-check) removed. */
static cJSON *filters_meta(const semsearch_filters *f, const semsearch_rf *rf,
                           int in_knn, const sem_counts *c) {
  cJSON *o = cJSON_CreateObject();
  cJSON *src = cJSON_CreateArray();
  for (int i = 0; rf && i < rf->nsrc; i++)
    cJSON_AddItemToArray(src, cJSON_CreateString(rf->src[i]));
  if (rf && rf->nsrc) cJSON_AddItemToObject(o, "source", src);
  else { cJSON_Delete(src); cJSON_AddNullToObject(o, "source"); }
#define FS(k, v) cJSON_AddItemToObject(o, k, (v) && *(v) ? cJSON_CreateString(v) : cJSON_CreateNull())
  FS("record_type", f ? f->record_type : NULL);
  FS("since", f ? f->since : NULL);
  FS("until", f ? f->until : NULL);
  FS("lang", f ? f->lang : NULL);
  FS("bbox", f ? f->bbox : NULL);
#undef FS
  /* Where each ran. The partition (tenant) is reported as knn_partitioned. */
  cJSON *ik = cJSON_CreateArray(), *pf = cJSON_CreateArray();
  if (rf && rf->nsrc)  cJSON_AddItemToArray(in_knn ? ik : pf, cJSON_CreateString("source"));
  if (rf && rf->rtype) cJSON_AddItemToArray(in_knn ? ik : pf, cJSON_CreateString("record_type"));
  if (rf && (rf->has_since || rf->has_until))
    cJSON_AddItemToArray(in_knn ? ik : pf, cJSON_CreateString("date"));
  if (rf && rf->lang)     cJSON_AddItemToArray(pf, cJSON_CreateString("lang"));
  if (rf && rf->has_bbox) cJSON_AddItemToArray(pf, cJSON_CreateString("bbox"));
  cJSON_AddItemToObject(o, "in_knn", ik);
  cJSON_AddItemToObject(o, "post", pf);
  cJSON *d = cJSON_CreateObject();
  int any = 0;
  for (int i = 0; i < D_N; i++) { cJSON_AddNumberToObject(d, D_NAMES[i], c->dropped[i]); any += c->dropped[i]; }
  cJSON_AddItemToObject(o, "dropped", d);
  /* A post-filter can only remove from the k candidates the KNN returned; it
   * cannot reach past them. Said in-band so "3 results" is never read as "3
   * matching rows exist". */
  if (cJSON_GetArraySize(pf) > 0)
    cJSON_AddStringToObject(o, "post_note",
      "post-filters apply to the k nearest candidates only; rows they drop are "
      "counted in `dropped` and a larger k widens the candidate set");
  cJSON_AddNumberToObject(o, "dropped_total", any);
  return o;
}

/* An FTS arm that honours the same filters AND the caller's tenant. With
 * neither this is vecindex_fts_arm(), unchanged; otherwise the MATCH is joined
 * to its rows so the arm's k slots are spent on rows that can be shown — an
 * unscoped arm spent them on other tenants' rows, which materialize() then
 * hid, and a tenant's own match below k of theirs never surfaced. */
static int fts_arm(db_handle *db, const char *tenant, const char *matchq, int k,
                   const semsearch_rf *rf, vecindex_fused *L) {
  int scoped = tenant && *tenant;
  if (!rf_any(rf) && !scoped)
    return vecindex_fts_arm(db, "intel_items_fts", "uid", matchq, k, L);
  char sql[2048];
  int o = snprintf(sql, sizeof sql,
    "SELECT f.uid FROM (SELECT uid, bm25(intel_items_fts) AS r FROM intel_items_fts "
    "WHERE intel_items_fts MATCH ?1) f JOIN intel_items ON intel_items.uid = f.uid "
    "WHERE 1=1");
#define AP(...) do { if (o > 0 && (size_t)o < sizeof sql) { \
    int _n = snprintf(sql + o, sizeof sql - (size_t)o, __VA_ARGS__); \
    o = (_n < 0 || (size_t)(o + _n) >= sizeof sql) ? -1 : o + _n; } } while (0)
  if (scoped)        AP(" AND intel_items.tenant_id IN (?%d,'legacy')", 11 + SEMSEARCH_MAX_SOURCES);
  if (rf->nsrc) {
    AP(" AND intel_items.source_id IN (");
    for (int i = 0; i < rf->nsrc; i++) AP("%s?%d", i ? "," : "", 3 + i);
    AP(")");
  }
  if (rf->rtype)     AP(" AND intel_items.record_type = ?%d", 3 + SEMSEARCH_MAX_SOURCES);
  /* EMBED_ROW_TS carries strftime's own '%s': an argument, never format text. */
  if (rf->has_since) AP(" AND %s >= ?%d", EMBED_ROW_TS, 4 + SEMSEARCH_MAX_SOURCES);
  if (rf->has_until) AP(" AND %s <= ?%d", EMBED_ROW_TS, 5 + SEMSEARCH_MAX_SOURCES);
  if (rf->lang)      AP(" AND intel_items.language = ?%d", 6 + SEMSEARCH_MAX_SOURCES);
  if (rf->has_bbox)  AP(" AND intel_items.lat BETWEEN ?%d AND ?%d AND intel_items.lon "
                        "BETWEEN ?%d AND ?%d", 7 + SEMSEARCH_MAX_SOURCES,
                        8 + SEMSEARCH_MAX_SOURCES, 9 + SEMSEARCH_MAX_SOURCES,
                        10 + SEMSEARCH_MAX_SOURCES);
  AP(" ORDER BY f.r LIMIT ?2");
#undef AP
  if (o < 0) return -1;
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(st, 1, matchq, -1, SQLITE_STATIC);
  sqlite3_bind_int(st, 2, k);
  for (int i = 0; i < rf->nsrc; i++) sqlite3_bind_text(st, 3 + i, rf->src[i], -1, SQLITE_STATIC);
  const int B = SEMSEARCH_MAX_SOURCES;
  if (rf->rtype)     sqlite3_bind_text(st, 3 + B, rf->rtype, -1, SQLITE_STATIC);
  if (rf->has_since) sqlite3_bind_int64(st, 4 + B, rf->since_ts);
  if (rf->has_until) sqlite3_bind_int64(st, 5 + B, rf->until_ts);
  if (rf->lang)      sqlite3_bind_text(st, 6 + B, rf->lang, -1, SQLITE_STATIC);
  if (rf->has_bbox) {
    sqlite3_bind_double(st, 7 + B, rf->bs); sqlite3_bind_double(st, 8 + B, rf->bn);
    sqlite3_bind_double(st, 9 + B, rf->bw); sqlite3_bind_double(st, 10 + B, rf->be);
  }
  if (scoped) sqlite3_bind_text(st, 11 + B, tenant, -1, SQLITE_STATIC);
  int rank = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *key = (const char *)sqlite3_column_text(st, 0);
    if (!key) continue;
    if (!vecindex_fused_add(L, key, 0, ++rank, 0)) break;
  }
  sqlite3_finalize(st);
  return rank;
}

/* ---- GET /api/intel/semantic --------------------------------------------- */

char *semsearchapi_query_ex(db_handle *db, const char *tenant, const char *q,
                            const char *mode, int limit, int k, int is_operator,
                            const semsearch_filters *flt, int *status) {
  *status = 500;
  if (!q || !*q) return fail(status, 400, "missing_q", "q is required", NULL);
  int hybrid;
  if (!mode || !*mode || !strcmp(mode, "hybrid")) hybrid = 1;
  else if (!strcmp(mode, "vector")) hybrid = 0;
  else return fail(status, 400, "bad_mode", "mode must be hybrid or vector", NULL);
  if (limit <= 0) limit = 50;
  if (limit > 200) limit = 200;
  if (k <= 0) k = 200;
  if (k > 1000) k = 1000;

  semsearch_rf rf;
  char ferr[48];
  if (semsearch_resolve(db, flt, &rf, ferr, sizeof ferr) != 0)
    return fail(status, 400, ferr, "a filter could not be read; nothing was "
                "searched rather than searching without it", NULL);

  struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
  float *qv = NULL; int dim = 0; long embed_ms = 0;
  char *eb = semsearch_embed(db, q, &qv, &dim, &embed_ms, status);
  if (eb) { semsearch_rf_free(&rf); return eb; }

  vecindex_fused L = {0};
  int vector_hits = 0, fts_hits = 0, in_knn = 0;
  const vecindex_spec *vs = embed_query_spec(db);
  int partitioned = (vs == &embed_intel_spec) && tenant && *tenant;
  {
    vecindex_hit *h = NULL;
    char err[256];
    int n = semsearch_knn(db, tenant, qv, dim, k, &rf, &h, &in_knn, err, sizeof err);
    if (n < 0) {
      free(qv); semsearch_rf_free(&rf);
      return fail(status, 500, "vector_query_failed", err, NULL);
    }
    vector_hits = vecindex_fused_add_hits(&L, h, n);
    vecindex_hits_free(h, n);
  }
  /* What an operator is told about the rows a tenant could NOT see: the same
   * k nearest WITHOUT the partition filter (other filters kept); each one the
   * fused list does not hold is another tenant's (or an orphan vector). Only
   * for an operator — how many of OTHER tenants' rows match is itself
   * information about those tenants — and so the extra KNN costs nothing on
   * an ordinary query. */
  vecindex_hit *wh = NULL;
  int nwh = 0;
  if (is_operator && partitioned) {
    nwh = semsearch_knn(db, NULL, qv, dim, k, &rf, &wh, NULL, NULL, 0);
    if (nwh < 0) nwh = 0;
  }
  free(qv);

  /* FTS arm (hybrid only). fts_query_expr() sanitises the user's text the
   * same way /api/intel/items does; a NULL means it yielded no usable token,
   * which is "this arm has nothing to say", not an error. */
  char *matchq = NULL;
  if (hybrid) {
    matchq = fts_query_expr(q);
    if (matchq) {
      int f = fts_arm(db, tenant, matchq, k, &rf, &L);
      if (f > 0) fts_hits = f;
    }
  }

  vecindex_fused_sort(&L);

  cJSON *data = cJSON_CreateArray();
  sem_counts c;
  if (materialize(db, tenant, is_operator, &L, 0, limit, &rf, NULL, 0, NULL, data, &c) != 0) {
    cJSON_Delete(data); free(matchq); vecindex_fused_free(&L);
    vecindex_hits_free(wh, nwh); semsearch_rf_free(&rf);
    return fail(status, 500, "server_error", sqlite3_errmsg(db->h), NULL);
  }
  /* The unfiltered arm's rows the partition kept out (see above). */
  if (nwh > 0) {
    sqlite3_stmt *exists = NULL;
    if (sqlite3_prepare_v2(db->h, "SELECT 1 FROM intel_items WHERE uid=?1",
                           -1, &exists, NULL) == SQLITE_OK) {
      for (int i = 0; i < nwh; i++) {
        int in_list = 0;
        for (int j = 0; j < L.n && !in_list; j++) in_list = !strcmp(L.v[j].key, wh[i].key);
        if (in_list) continue;
        sqlite3_reset(exists);
        sqlite3_bind_text(exists, 1, wh[i].key, -1, SQLITE_STATIC);
        if (sqlite3_step(exists) == SQLITE_ROW) c.withheld++; else c.orphans++;
      }
    }
    sqlite3_finalize(exists);
  }
  vecindex_hits_free(wh, nwh);

  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "mode", hybrid ? "hybrid" : "vector");
  cJSON_AddStringToObject(meta, "q", q);
  cJSON_AddNumberToObject(meta, "k", k);
  cJSON_AddNumberToObject(meta, "limit", limit);
  cJSON_AddNumberToObject(meta, "vector_hits", vector_hits);
  if (hybrid) {
    cJSON_AddNumberToObject(meta, "fts_hits", fts_hits);
    cJSON_AddItemToObject(meta, "fts_expr",
      matchq ? cJSON_CreateString(matchq) : cJSON_CreateNull());
    cJSON_AddNumberToObject(meta, "rrf_k", RRF_K);
  }
  cJSON_AddNumberToObject(meta, "fused", L.n);
  /* Whether the tenant filter ran INSIDE the KNN (false with no tenant, or
   * while a migration of the pre-metadata index is pending). */
  cJSON_AddBoolToObject(meta, "knn_partitioned", partitioned);
  cJSON_AddItemToObject(meta, "filters", filters_meta(flt, &rf, in_knn, &c));
  if (is_operator) {
    cJSON_AddNumberToObject(meta, "tenant_withheld", c.withheld);
    cJSON_AddNumberToObject(meta, "index_orphans", c.orphans);
  }
  /* The bounded view states its bound in-band: shown of total. */
  cJSON_AddNumberToObject(meta, "shown", c.shown);
  cJSON_AddNumberToObject(meta, "total", c.visible);
  cJSON_AddBoolToObject(meta, "truncated", c.visible > c.shown);
  cJSON_AddNumberToObject(meta, "embed_ms", (double)embed_ms);
  cJSON_AddNumberToObject(meta, "total_ms", (double)ms_since(&t0));
  cJSON_AddItemToObject(meta, "coverage", embed_coverage_json(db));
  free(matchq);
  vecindex_fused_free(&L);
  semsearch_rf_free(&rf);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "meta", meta);
  char *out = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  *status = out ? 200 : 500;
  return out;
}

char *semsearchapi_query(db_handle *db, const char *tenant, const char *q,
                         const char *mode, int limit, int k, int is_operator,
                         int *status) {
  return semsearchapi_query_ex(db, tenant, q, mode, limit, k, is_operator, NULL,
                               status);
}

/* ---- the raw query string ------------------------------------------------ */

/* mg_http_get_var: -3 = the value would not fit. A clipped filter is a
 * DIFFERENT filter, so that is a 414, never half of what was asked
 * (httpd.c qvar() is the same rule). */
static int qs_get(const char *qs, const char *k, char *out, size_t cap,
                  int *too_long) {
  struct mg_str s = mg_str(qs ? qs : "");
  int n = mg_http_get_var(&s, k, out, cap);
  if (n == -3) *too_long = 1;
  if (n <= 0) out[0] = 0;
  return n > 0;
}

typedef struct {
  char q[1024], mode[16], source[1024], rtype[64], since[48], until[48],
       lang[16], bbox[96], limit[16], k[16];
} sem_params;

/* 0, or -1 when a value was too long (the caller answers 414). */
static int parse_params(const char *qs, sem_params *p, semsearch_filters *f) {
  int tl = 0;
  memset(p, 0, sizeof *p);
  qs_get(qs, "q", p->q, sizeof p->q, &tl);
  qs_get(qs, "mode", p->mode, sizeof p->mode, &tl);
  qs_get(qs, "limit", p->limit, sizeof p->limit, &tl);
  qs_get(qs, "k", p->k, sizeof p->k, &tl);
  qs_get(qs, "source", p->source, sizeof p->source, &tl);
  qs_get(qs, "record_type", p->rtype, sizeof p->rtype, &tl);
  qs_get(qs, "since", p->since, sizeof p->since, &tl);
  qs_get(qs, "until", p->until, sizeof p->until, &tl);
  qs_get(qs, "lang", p->lang, sizeof p->lang, &tl);
  qs_get(qs, "bbox", p->bbox, sizeof p->bbox, &tl);
  *f = (semsearch_filters){ p->source, p->rtype, p->since, p->until, p->lang,
                            p->bbox };
  return tl ? -1 : 0;
}

static char *too_long(int *status) {
  return fail(status, 414, "filter_too_long", "a query parameter exceeded its "
              "maximum length; half a query cannot be honoured", NULL);
}

char *semsearchapi_http(db_handle *db, const char *tenant, const char *id,
                        const char *qs, int is_operator, int *status) {
  sem_params p;
  semsearch_filters f;
  if (parse_params(qs, &p, &f) != 0) return too_long(status);
  return semsearchapi_query_ex(db, tenant, p.q, p.mode, atoi(p.limit),
                               atoi(p.k), is_operator, &f, status);
}

/* ---- GET /api/intel/items?q=&sort=hybrid --------------------------------- */

/* An envelope's JSON goes out through ?lang_view= exactly like the feed's
 * (httpd.c intel_lang_view): translate_shape_items() on the same body. */
static char *lang_view(db_handle *db, const char *qs, char *body) {
  char lv[16];
  int tl = 0;
  if (!body || !qs_get(qs, "lang_view", lv, sizeof lv, &tl)) return body;
  char *sh = translate_shape_items(db, body, translate_view_parse(lv));
  if (!sh) return body;
  free(body);
  return sh;
}

char *semsearchapi_items_hybrid(db_handle *db, const char *tenant,
                                const char *id, const char *qs, int is_operator,
                                int *status) {
  *status = 500;
  char src[160], q[256], qalt[256], lang[16], since[40], until[40], rt[48],
       ssid[120], hg[8], tag[120], cur[2048], lim[16], kk[16];
  int tl = 0, ctl = 0;
  intel_items_query Q = {0};
#define QV(name, buf, field) if (qs_get(qs, name, buf, sizeof buf, &tl)) Q.field = buf
  QV("source", src, source); QV("q", q, q); QV("qAlt", qalt, q_alt);
  QV("lang", lang, lang); QV("since", since, since); QV("until", until, until);
  QV("record_type", rt, record_type); QV("sub_source_id", ssid, sub_source_id);
  QV("has_geom", hg, has_geom); QV("tag", tag, tag);
#undef QV
  /* A cursor too long even for this buffer is not one of this sort's
   * ("hybrid:<offset>"): ignored and disclosed below, as the feed does. */
  int cur_given = qs_get(qs, "cursor", cur, sizeof cur, &ctl);
  if (ctl) { cur[0] = 0; cur_given = 1; }
  qs_get(qs, "limit", lim, sizeof lim, &tl);
  qs_get(qs, "k", kk, sizeof kk, &tl);
  if (tl) return too_long(status);
  Q.tenant = tenant;
  if (!Q.q || !*Q.q)
    return fail(status, 400, "sort_requires_q", "sort=hybrid ranks full-text "
                "and semantic hits; there is nothing to rank without q", NULL);
  if (Q.source && *Q.source && breach_meta_is_source(db, Q.source))
    return fail(status, 400, "sort_not_supported_for_breach_source",
                "breach sources are served by the breach adapter, which orders "
                "by record id only; drop sort= for this source", NULL);
  int limit = atoi(lim);
  if (limit <= 0) limit = 50;
  if (limit > 200) limit = 200;
  int k = atoi(kk);
  if (k <= 0) k = 200;
  if (k > 1000) k = 1000;
  long off = 0;
  int cur_used = 0;
  if (cur_given && !strncmp(cur, "hybrid:", 7)) {
    off = strtol(cur + 7, NULL, 10);
    if (off < 0) off = 0;
    cur_used = 1;
  }
  struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);

  /* BM25 arm: exactly the list sort=relevance would page through, every feed
   * filter applied (intelapi_relevance_uids). */
  vecindex_fused L = {0};
  char **fu = NULL;
  int nf = intelapi_relevance_uids(db, &Q, k, &fu);
  int fts_hits = nf > 0 ? nf : 0;
  for (int i = 0; i < nf; i++) {
    vecindex_fused_add(&L, fu[i], 0, i + 1, 0);
    free(fu[i]);
  }
  free(fu);

  /* Vector arm. source/record_type and the dates run INSIDE the KNN (the
   * feed's since/until compare text, the index's ts is epoch — the post-check
   * below applies the feed's own rule, so the two cannot disagree on what is
   * shown). A vector arm that cannot answer (no server, no index, a swapped
   * model) does not fail the request: the list is the BM25 arm alone, and
   * meta.hybrid.vector_arm says so with the reason and coverage. */
  semsearch_filters sf = { .source = Q.source, .record_type = Q.record_type,
                           .since = Q.since, .until = Q.until };
  semsearch_rf rf;
  char ferr[48];
  int rf_ok = semsearch_resolve(db, &sf, &rf, ferr, sizeof ferr) == 0;
  if (!rf_ok) {
    /* the feed reads since/until as text; an epoch the index cannot read
     * them as just leaves the dates to the post-check */
    sf.since = sf.until = NULL;
    rf_ok = semsearch_resolve(db, &sf, &rf, ferr, sizeof ferr) == 0;
  }
  int vector_hits = 0, in_knn = 0;
  long embed_ms = 0;
  cJSON *varm_err = NULL;
  float *qv = NULL; int dim = 0, est = 0;
  char *eb = rf_ok ? semsearch_embed(db, Q.q, &qv, &dim, &embed_ms, &est) : NULL;
  if (eb) { varm_err = cJSON_Parse(eb); free(eb); }
  else if (qv) {
    vecindex_hit *h = NULL;
    char err[256];
    int n = semsearch_knn(db, tenant, qv, dim, k, &rf, &h, &in_knn, err, sizeof err);
    if (n >= 0) {
      vector_hits = n;
      for (int i = 0; i < n; i++)
        vecindex_fused_add(&L, h[i].key, i + 1, 0, h[i].distance);
      vecindex_hits_free(h, n);
    } else {
      varm_err = cJSON_CreateObject();
      cJSON_AddStringToObject(varm_err, "error", "vector_query_failed");
      cJSON_AddStringToObject(varm_err, "detail", err);
    }
  }
  free(qv);
  vecindex_fused_sort(&L);

  /* The feed's WHERE as the post-check, tenant left to materialize(). */
  intel_items_query Q2 = Q;
  Q2.tenant = NULL;
  char wbuf[4096];
  const char *bnd[24];
  int nb = intelapi_where(&Q2, wbuf, sizeof wbuf, bnd, 24);
  char psql[4400];
  snprintf(psql, sizeof psql, "SELECT 1 FROM intel_items WHERE intel_items.uid=?1%s", wbuf);
  sqlite3_stmt *post = NULL;
  if (sqlite3_prepare_v2(db->h, psql, -1, &post, NULL) != SQLITE_OK) {
    vecindex_fused_free(&L); if (rf_ok) semsearch_rf_free(&rf);
    cJSON_Delete(varm_err);
    return fail(status, 500, "server_error", sqlite3_errmsg(db->h), NULL);
  }
  for (int i = 0; i < nb; i++)
    sqlite3_bind_text(post, 2 + i, bnd[i], -1, SQLITE_TRANSIENT);

  cJSON *data = cJSON_CreateArray();
  sem_counts c;
  /* rf NULL: the feed's WHERE (post) is the one rule for what is shown; the
   * index's epoch dates only narrowed the KNN. */
  int mr = materialize(db, tenant, is_operator, &L, (int)off, limit,
                       NULL, NULL, 0, post, data, &c);
  sqlite3_finalize(post);
  if (mr != 0) {
    cJSON_Delete(data); vecindex_fused_free(&L); if (rf_ok) semsearch_rf_free(&rf);
    cJSON_Delete(varm_err);
    return fail(status, 500, "server_error", sqlite3_errmsg(db->h), NULL);
  }

  cJSON *page = cJSON_CreateObject();
  if (c.visible > off + c.shown) {
    char nc[48];
    snprintf(nc, sizeof nc, "hybrid:%ld", off + c.shown);
    cJSON_AddStringToObject(page, "next_cursor", nc);
  } else cJSON_AddNullToObject(page, "next_cursor");
  cJSON_AddNumberToObject(page, "limit", limit);
  cJSON_AddNumberToObject(page, "total", c.visible);

  cJSON *filters = cJSON_CreateObject();
#define FS(k, v) cJSON_AddItemToObject(filters, k, (v) ? cJSON_CreateString(v) : cJSON_CreateNull())
  FS("source", Q.source); FS("q", Q.q); FS("q_alt", Q.q_alt); FS("lang", Q.lang);
  FS("record_type", Q.record_type); FS("sub_source_id", Q.sub_source_id);
  FS("has_geom", Q.has_geom);
#undef FS
  cJSON *meta = cJSON_CreateObject();
  { char ts[40]; time_t now = time(NULL); struct tm tmv; gmtime_r(&now, &tmv);
    strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    cJSON_AddStringToObject(meta, "fetched_at", ts); }
  cJSON_AddItemToObject(meta, "filters", filters);
  cJSON_AddStringToObject(meta, "sort", "hybrid");
  cJSON_AddBoolToObject(meta, "q_applied", 1);
  cJSON *hy = cJSON_CreateObject();
  cJSON_AddNumberToObject(hy, "k", k);
  cJSON_AddNumberToObject(hy, "rrf_k", RRF_K);
  cJSON_AddNumberToObject(hy, "fts_hits", fts_hits);
  cJSON_AddNumberToObject(hy, "vector_hits", vector_hits);
  cJSON_AddNumberToObject(hy, "fused", L.n);
  cJSON_AddStringToObject(hy, "vector_arm", varm_err ? "unavailable" : "ok");
  if (varm_err) cJSON_AddItemToObject(hy, "vector_arm_error", varm_err);
  cJSON_AddBoolToObject(hy, "vector_filters_in_knn", in_knn);
  /* Rows a filter removed after the arms answered: the feed's own WHERE
   * (lang, tag, sub_source_id, has_geom, and the text date rule) on the
   * vector arm's candidates. */
  cJSON_AddNumberToObject(hy, "filtered_out", c.post_dropped);
  if (is_operator) {
    cJSON_AddNumberToObject(hy, "tenant_withheld", c.withheld);
    cJSON_AddNumberToObject(hy, "index_orphans", c.orphans);
  }
  /* page.total is the fused list, which is a BOUNDED VIEW: each arm's top k. */
  cJSON_AddBoolToObject(hy, "bounded", fts_hits >= k || vector_hits >= k);
  cJSON_AddStringToObject(hy, "note",
    "page.total counts the fused top-k of each arm (BM25 over the feed's "
    "filters, vector KNN inside the tenant's partitions), not every match; "
    "raise k to widen it");
  cJSON_AddNumberToObject(hy, "embed_ms", (double)embed_ms);
  cJSON_AddNumberToObject(hy, "total_ms", (double)ms_since(&t0));
  cJSON_AddItemToObject(hy, "coverage", embed_coverage_json(db));
  cJSON_AddItemToObject(meta, "hybrid", hy);
  if (cur_given && !cur_used) {
    cJSON *notes = cJSON_CreateArray();
    cJSON_AddItemToArray(notes, cJSON_CreateString("cursor_ignored"));
    cJSON_AddItemToObject(meta, "notes", notes);
  }
  vecindex_fused_free(&L);
  if (rf_ok) semsearch_rf_free(&rf);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *out = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  *status = out ? 200 : 500;
  return lang_view(db, qs, out);
}

/* ---- GET /api/intel/items/:uid/similar ----------------------------------- */

/* 1 when `uid` is a row the caller may read (tenant + shared corpus). */
static int item_visible(db_handle *db, const char *tenant, const char *uid) {
  sqlite3_stmt *s;
  const char *sql = (tenant && *tenant)
    ? "SELECT 1 FROM intel_items WHERE uid=?1 AND tenant_id IN (?2,'legacy')"
    : "SELECT 1 FROM intel_items WHERE uid=?1";
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_STATIC);
  if (tenant && *tenant) sqlite3_bind_text(s, 2, tenant, -1, SQLITE_STATIC);
  int ok = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return ok;
}

/* Why the pod has no vector for `uid`: the fail table's reason, or NULL. */
static char *failed_reason(db_handle *db, const char *uid) {
  if (!vecindex_table_exists(db, EMBED_FAIL_TABLE)) return NULL;
  sqlite3_stmt *s;
  char *r = NULL;
  if (sqlite3_prepare_v2(db->h, "SELECT reason FROM " EMBED_FAIL_TABLE
                         " WHERE uid=?1", -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_STATIC);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0))
    r = strdup((const char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);
  return r;
}

/* A stored vector that the index cannot answer about is told apart from "no
 * such item": 503 (no index at all), or 409 not_embedded with the coverage
 * block and, when the server refused the row, the reason it gave. */
static char *not_embedded(db_handle *db, const char *what, const char *reason,
                          int *status) {
  char d[600];
  if (reason)
    snprintf(d, sizeof d, "%s has no vector: the embedding server refused its "
             "text (%.300s); it is retried when the text changes", what, reason);
  else
    snprintf(d, sizeof d, "%s has no vector yet: the embedding pod has not "
             "reached it (see coverage.backlog); nothing is ranked against a "
             "vector that does not exist", what);
  return fail(status, 409, "not_embedded", d, embed_coverage_json(db));
}

char *semsearchapi_similar(db_handle *db, const char *tenant, const char *uid,
                           const char *qs, int is_operator, int *status) {
  *status = 500;
  if (!uid || !*uid) return fail(status, 400, "missing_uid", NULL, NULL);
  sem_params p;
  semsearch_filters flt;
  if (parse_params(qs, &p, &flt) != 0) return too_long(status);
  int k = atoi(p.k);
  if (k <= 0) k = 20;
  if (k > 200) k = 200;
  /* Not visible and not existing answer the same 404: whether another
   * tenant's uid exists is information about that tenant. */
  if (!item_visible(db, tenant, uid))
    return fail(status, 404, "not_found", NULL, NULL);
  if (!embed_table_exists(db))
    return fail(status, 503, "semantic_unavailable",
                "the embedding pod has not built an index yet",
                embed_coverage_json(db));
  semsearch_rf rf;
  char ferr[48];
  if (semsearch_resolve(db, &flt, &rf, ferr, sizeof ferr) != 0)
    return fail(status, 400, ferr, "a filter could not be read; nothing was "
                "searched rather than searching without it", NULL);

  /* The item's OWN stored vector: no embedding call, so no server is needed
   * and a refused/swapped server does not matter — every stored vector is in
   * the index's one space. */
  struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
  const vecindex_spec *vs = embed_query_spec(db);
  float *v = NULL; int dim = 0;
  int lr = vecindex_load(db, vs, uid, &v, &dim);
  if (lr != 0) {
    semsearch_rf_free(&rf);
    if (lr < 0) return fail(status, 500, "vector_read_failed",
                            sqlite3_errmsg(db->h), NULL);
    char *why = failed_reason(db, uid);
    char *b = not_embedded(db, "this item", why, status);
    free(why);
    return b;
  }
  /* k + 1: the item is its own nearest neighbour and is excluded. */
  vecindex_hit *h = NULL;
  char err[256];
  int in_knn = 0;
  int n = semsearch_knn(db, tenant, v, dim, k + 1, &rf, &h, &in_knn, err, sizeof err);
  free(v);
  if (n < 0) { semsearch_rf_free(&rf);
               return fail(status, 500, "vector_query_failed", err, NULL); }
  vecindex_fused L = {0};
  int vector_hits = vecindex_fused_add_hits(&L, h, n);
  vecindex_hits_free(h, n);
  /* RRF with one arm: the score is 1/(60+rank), the order is the KNN's. */
  vecindex_fused_sort(&L);
  cJSON *data = cJSON_CreateArray();
  sem_counts c;
  const char *self[1] = { uid };
  if (materialize(db, tenant, is_operator, &L, 0, k, &rf, self, 1, NULL, data, &c) != 0) {
    cJSON_Delete(data); vecindex_fused_free(&L); semsearch_rf_free(&rf);
    return fail(status, 500, "server_error", sqlite3_errmsg(db->h), NULL);
  }
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "mode", "similar");
  cJSON_AddStringToObject(meta, "uid", uid);
  cJSON_AddNumberToObject(meta, "k", k);
  cJSON_AddNumberToObject(meta, "vector_hits", vector_hits);
  cJSON_AddNumberToObject(meta, "excluded_self", c.excluded);
  cJSON_AddBoolToObject(meta, "knn_partitioned",
                        (vs == &embed_intel_spec) && tenant && *tenant);
  cJSON_AddItemToObject(meta, "filters", filters_meta(&flt, &rf, in_knn, &c));
  if (is_operator) {
    cJSON_AddNumberToObject(meta, "tenant_withheld", c.withheld);
    cJSON_AddNumberToObject(meta, "index_orphans", c.orphans);
  }
  cJSON_AddNumberToObject(meta, "shown", c.shown);
  cJSON_AddNumberToObject(meta, "total", c.visible);
  cJSON_AddBoolToObject(meta, "truncated", c.visible > c.shown);
  cJSON_AddStringToObject(meta, "note",
    "the k nearest rows by the item's stored vector (no embedding call); "
    "total counts what that KNN returned, not every row with any similarity");
  cJSON_AddNumberToObject(meta, "total_ms", (double)ms_since(&t0));
  cJSON_AddItemToObject(meta, "coverage", embed_coverage_json(db));
  vecindex_fused_free(&L);
  semsearch_rf_free(&rf);
  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "meta", meta);
  char *out = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  *status = out ? 200 : 500;
  return out;
}
