/* core/semsearchapi.c — see semsearchapi.h. */
#include "semsearchapi.h"
#include "embed_pod.h"
#include "fts.h"
#include "llm.h"
#include "source_registry.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RRF_K 60.0

/* ---- item shape ----------------------------------------------------------
 * DUPLICATED from core/intelapi.c (ITEM_COLS / row_to_item, list shape,
 * full=0). intelapi.h exports no row builder — row_to_item is static — and
 * intelapi.c is owned by another change in flight, so the column list and
 * key order are copied here verbatim. If /api/intel/items grows a column,
 * this SELECT and item_from_row() must follow it; intelapi.c is the source of
 * truth. */
#define ITEM_COLS \
  "uid,source_id,title,body,summary,link,author,language," \
  "published_at,fetched_at,tags,properties,keywords,lat,lon,geom_source," \
  "geom_at,record_type,sub_source_id"

static const char *ctext(sqlite3_stmt *s, int i) {
  return (sqlite3_column_type(s, i) == SQLITE_NULL)
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
static cJSON *parse_or(cJSON *fallback, const char *txt) {
  if (txt && *txt) { cJSON *j = cJSON_Parse(txt); if (j) { cJSON_Delete(fallback); return j; } }
  return fallback;
}

static cJSON *item_from_row(sqlite3_stmt *s) {
  const char *c_uid=ctext(s,0), *c_src=ctext(s,1), *c_title=ctext(s,2),
    *c_summary=ctext(s,4), *c_link=ctext(s,5),
    *c_author=ctext(s,6), *c_lang=ctext(s,7), *c_pub=ctext(s,8),
    *c_fetch=ctext(s,9), *c_tags=ctext(s,10), *c_props=ctext(s,11),
    *c_kw=ctext(s,12), *c_gsrc=ctext(s,15), *c_gat=ctext(s,16),
    *c_rt=ctext(s,17), *c_sub=ctext(s,18);
  int latnull = sqlite3_column_type(s,13)==SQLITE_NULL;
  int lonnull = sqlite3_column_type(s,14)==SQLITE_NULL;
  double lat = sqlite3_column_double(s,13), lon = sqlite3_column_double(s,14);

  cJSON *tags = parse_or(cJSON_CreateArray(), c_tags);
  cJSON *props = parse_or(cJSON_CreateObject(), c_props);
  cJSON *kw = (c_kw && *c_kw) ? cJSON_Parse(c_kw) : NULL;

  const src_meta *m = src_meta_get(c_src);
  cJSON *lic = cJSON_GetObjectItem(props, "license");
  if (!lic || cJSON_IsNull(lic)) lic = cJSON_GetObjectItem(props, "license_id");
  if (!lic || cJSON_IsNull(lic)) lic = cJSON_GetObjectItem(props, "license_name");
  const char *licstr = (lic && cJSON_IsString(lic)) ? lic->valuestring
                       : (m && m->license ? m->license : NULL);
  char licbuf[128] = {0};
  if (!licstr && lic && cJSON_IsNumber(lic)) { snprintf(licbuf,sizeof licbuf,"%g",lic->valuedouble); licstr=licbuf; }
  cJSON *conf = cJSON_GetObjectItem(props, "confidence");
  if (!conf) conf = cJSON_GetObjectItem(props, "_confidence");

  cJSON *pv = cJSON_CreateObject();
  add_str_or_null(pv,"source_id", c_src);
  cJSON_AddItemToObject(pv,"source_name", cJSON_CreateString(m&&m->name?m->name:(c_src?c_src:"")));
  add_str_or_null(pv,"source_name_ja", m?m->name_ja:NULL);
  add_str_or_null(pv,"category", m?m->category:NULL);
  add_str_or_null(pv,"collection_method", m?m->type:NULL);
  add_str_or_null(pv,"source_url", m?m->url:NULL);
  add_str_or_null(pv,"item_url", c_link);
  add_str_or_null(pv,"sub_source_id", c_sub);
  add_str_or_null(pv,"fetched_at", c_fetch);
  add_str_or_null(pv,"published_at", c_pub);
  add_str_or_null(pv,"license", licstr);
  cJSON_AddItemToObject(pv,"confidence",
     (conf && cJSON_IsNumber(conf)) ? cJSON_CreateNumber(conf->valuedouble) : cJSON_CreateNull());

  cJSON *out = cJSON_CreateObject();
  add_str_or_null(out,"uid", c_uid);
  add_str_or_null(out,"source_id", c_src);
  add_str_or_null(out,"title", c_title);
  add_str_or_null(out,"summary", c_summary);
  add_str_or_null(out,"link", c_link);
  add_str_or_null(out,"author", c_author);
  add_str_or_null(out,"language", c_lang);
  add_str_or_null(out,"published_at", c_pub);
  add_str_or_null(out,"fetched_at", c_fetch);
  cJSON_AddItemToObject(out,"tags", tags);
  cJSON_AddItemToObject(out,"lat", latnull?cJSON_CreateNull():cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(out,"lon", lonnull?cJSON_CreateNull():cJSON_CreateNumber(lon));
  add_str_or_null(out,"geom_source", c_gsrc);
  add_str_or_null(out,"geom_at", c_gat);
  add_str_or_null(out,"record_type", c_rt);
  add_str_or_null(out,"sub_source_id", c_sub);
  cJSON_AddItemToObject(out,"provenance", pv);
  if (cJSON_GetArraySize(props) > 0) cJSON_AddItemToObject(out,"properties", props);
  else cJSON_Delete(props);
  if (kw) cJSON_AddItemToObject(out,"keywords", kw);
  return out;
}

/* ---- candidates ---------------------------------------------------------- */

typedef struct {
  char  *uid;
  int    vrank, frank;      /* 1-based, 0 = not returned by that arm */
  double distance;          /* vector arm's cosine distance (vrank>0) */
  double score;             /* RRF */
} cand;

typedef struct { cand *v; int n, cap; } candlist;

static cand *cand_find(candlist *l, const char *uid) {
  for (int i = 0; i < l->n; i++) if (!strcmp(l->v[i].uid, uid)) return &l->v[i];
  return NULL;
}
static cand *cand_add(candlist *l, const char *uid) {
  cand *c = cand_find(l, uid);
  if (c) return c;
  if (l->n == l->cap) {
    int ncap = l->cap ? l->cap * 2 : 256;
    cand *nv = realloc(l->v, (size_t)ncap * sizeof *nv);
    if (!nv) return NULL;
    l->v = nv; l->cap = ncap;
  }
  c = &l->v[l->n++];
  memset(c, 0, sizeof *c);
  c->uid = strdup(uid);
  return c->uid ? c : NULL;
}
static void cand_free(candlist *l) {
  for (int i = 0; i < l->n; i++) free(l->v[i].uid);
  free(l->v);
}
static int cand_cmp(const void *a, const void *b) {
  const cand *x = a, *y = b;
  if (x->score != y->score) return x->score < y->score ? 1 : -1;
  return strcmp(x->uid, y->uid);
}

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

char *semsearchapi_query(db_handle *db, const char *tenant, const char *q,
                         const char *mode, int limit, int k, int *status) {
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

  /* Embed the query under the same text bound the index was built with,
   * on the interactive lane of the embedding server's own worker. */
  struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
  char *qb = embed_bound_text(q);
  if (!qb) return fail(status, 500, "server_error", NULL, NULL);
  llm_client llm = { .http = NULL, .base_url = base, .interactive = 1 };
  const char *texts[1] = { qb };
  float *qv = NULL; int qdim = 0; llm_status st;
  int erc = llm_embed(&llm, texts, 1, &qv, &qdim, 15000, &st);
  free(qb);
  long embed_ms = ms_since(&t0);
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

  candlist L = {0};
  int vector_hits = 0, fts_hits = 0;

  /* Vector arm. */
  {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
          "SELECT uid, distance FROM " EMBED_VEC_TABLE
          " WHERE embedding MATCH ?1 AND k = ?2 ORDER BY distance",
          -1, &s, NULL) != SQLITE_OK) {
      free(qv); cand_free(&L);
      return fail(status, 500, "vector_query_failed", sqlite3_errmsg(db->h), NULL);
    }
    sqlite3_bind_blob(s, 1, qv, (int)(dim * sizeof(float)), SQLITE_STATIC);
    sqlite3_bind_int(s, 2, k);
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *uid = (const char *)sqlite3_column_text(s, 0);
      if (!uid) continue;
      cand *c = cand_add(&L, uid);
      if (!c) break;
      c->vrank = ++vector_hits;
      c->distance = sqlite3_column_double(s, 1);
      c->score += 1.0 / (RRF_K + c->vrank);
    }
    sqlite3_finalize(s);
  }
  free(qv);

  /* FTS arm (hybrid only). fts_query_expr() sanitises the user's text the
   * same way /api/intel/items does; a NULL means it yielded no usable token,
   * which is "this arm has nothing to say", not an error. */
  char *matchq = NULL;
  if (hybrid) {
    matchq = fts_query_expr(q);
    if (matchq) {
      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h,
            "SELECT uid FROM intel_items_fts WHERE intel_items_fts MATCH ?1 "
            "ORDER BY bm25(intel_items_fts) LIMIT ?2", -1, &s, NULL) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, matchq, -1, SQLITE_STATIC);
        sqlite3_bind_int(s, 2, k);
        while (sqlite3_step(s) == SQLITE_ROW) {
          const char *uid = (const char *)sqlite3_column_text(s, 0);
          if (!uid) continue;
          cand *c = cand_add(&L, uid);
          if (!c) break;
          c->frank = ++fts_hits;
          c->score += 1.0 / (RRF_K + c->frank);
        }
        sqlite3_finalize(s);
      }
    }
  }

  qsort(L.v, (size_t)L.n, sizeof *L.v, cand_cmp);

  /* Materialise. Every fused candidate is looked up (a primary-key seek
   * each) so `total` is the number the caller could page to, not a guess;
   * items are only built for the first `limit` visible ones. */
  sqlite3_stmt *get;
  const char *sql = (tenant && *tenant)
    ? "SELECT " ITEM_COLS " FROM intel_items WHERE uid=?1 AND tenant_id IN (?2,'legacy')"
    : "SELECT " ITEM_COLS " FROM intel_items WHERE uid=?1";
  if (sqlite3_prepare_v2(db->h, sql, -1, &get, NULL) != SQLITE_OK) {
    free(matchq); cand_free(&L);
    return fail(status, 500, "server_error", sqlite3_errmsg(db->h), NULL);
  }
  cJSON *data = cJSON_CreateArray();
  int shown = 0, visible = 0, withheld = 0;
  for (int i = 0; i < L.n; i++) {
    cand *c = &L.v[i];
    sqlite3_reset(get);
    sqlite3_bind_text(get, 1, c->uid, -1, SQLITE_STATIC);
    if (tenant && *tenant) sqlite3_bind_text(get, 2, tenant, -1, SQLITE_STATIC);
    if (sqlite3_step(get) != SQLITE_ROW) { withheld++; continue; }
    visible++;
    if (shown >= limit) continue;
    cJSON *it = item_from_row(get);
    cJSON *sem = cJSON_CreateObject();
    cJSON_AddNumberToObject(sem, "score", c->score);
    cJSON_AddNumberToObject(sem, "rank", ++shown);
    if (c->vrank) { cJSON_AddNumberToObject(sem, "vector_rank", c->vrank);
                    cJSON_AddNumberToObject(sem, "distance", c->distance); }
    else          { cJSON_AddNullToObject(sem, "vector_rank");
                    cJSON_AddNullToObject(sem, "distance"); }
    if (c->frank) cJSON_AddNumberToObject(sem, "fts_rank", c->frank);
    else          cJSON_AddNullToObject(sem, "fts_rank");
    cJSON_AddItemToObject(it, "semantic", sem);
    cJSON_AddItemToArray(data, it);
  }
  sqlite3_finalize(get);

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
  cJSON_AddNumberToObject(meta, "tenant_withheld", withheld);
  /* The bounded view states its bound in-band: shown of total. */
  cJSON_AddNumberToObject(meta, "shown", shown);
  cJSON_AddNumberToObject(meta, "total", visible);
  cJSON_AddBoolToObject(meta, "truncated", visible > shown);
  cJSON_AddNumberToObject(meta, "embed_ms", (double)embed_ms);
  cJSON_AddNumberToObject(meta, "total_ms", (double)ms_since(&t0));
  cJSON_AddItemToObject(meta, "coverage", embed_coverage_json(db));
  free(matchq);
  cand_free(&L);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "meta", meta);
  char *out = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  *status = out ? 200 : 500;
  return out;
}
