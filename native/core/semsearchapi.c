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

#define RRF_K VECINDEX_RRF_K

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
                         const char *mode, int limit, int k, int is_operator,
                         int *status) {
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

  vecindex_fused L = {0};
  int vector_hits = 0, fts_hits = 0;

  /* Vector arm, FILTERED INSIDE THE KNN. The index is partitioned on
   * tenant_id, so a tenant's query asks for the k nearest among its own rows
   * and the shared corpus — it used to ask for the k nearest across EVERY
   * tenant and then drop what it could not show, so a tenant whose rows sat
   * below k other-tenant rows was answered with nothing. While a migration
   * from the pre-metadata table is pending (embed_query_spec) there is no
   * partition to filter on, and the post-filter below is all there is. */
  const vecindex_spec *vs = embed_query_spec(db);
  int partitioned = (vs == &embed_intel_spec) && tenant && *tenant;
  vecindex_val tv[2] = { VI_S(tenant), VI_S("legacy") };
  vecindex_filter tf = { .col = "tenant_id", .op = "IN", .vals = tv,
                         .nvals = (tenant && !strcmp(tenant, "legacy")) ? 1 : 2 };
  {
    vecindex_hit *h = NULL;
    char err[256];
    int n = vecindex_knn(db, vs, qv, dim, k, partitioned ? &tf : NULL,
                         partitioned ? 1 : 0, &h, err, sizeof err);
    if (n < 0) {
      free(qv);
      return fail(status, 500, "vector_query_failed", err, NULL);
    }
    vector_hits = vecindex_fused_add_hits(&L, h, n);
    vecindex_hits_free(h, n);
  }
  /* What an operator is told about the rows a tenant could NOT see: the same
   * k nearest WITHOUT the partition filter; each one the fused list does not
   * hold is another tenant's (or an orphan vector). Only for an operator —
   * how many of OTHER tenants' rows match is itself information about those
   * tenants — and so the extra KNN costs nothing on an ordinary query. */
  vecindex_hit *wh = NULL;
  int nwh = 0;
  if (is_operator && partitioned) {
    nwh = vecindex_knn(db, vs, qv, dim, k, NULL, 0, &wh, NULL, 0);
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
      int f = vecindex_fts_arm(db, "intel_items_fts", "uid", matchq, k, &L);
      if (f > 0) fts_hits = f;
    }
  }

  vecindex_fused_sort(&L);

  /* Materialise. Every fused candidate is looked up (a primary-key seek
   * each) so `total` is the number the caller could page to, not a guess;
   * items are only built for the first `limit` visible ones. */
  sqlite3_stmt *get;
  const char *sql = (tenant && *tenant)
    ? "SELECT " ITEM_COLS " FROM intel_items WHERE uid=?1 AND tenant_id IN (?2,'legacy')"
    : "SELECT " ITEM_COLS " FROM intel_items WHERE uid=?1";
  if (sqlite3_prepare_v2(db->h, sql, -1, &get, NULL) != SQLITE_OK) {
    free(matchq); vecindex_fused_free(&L); vecindex_hits_free(wh, nwh);
    return fail(status, 500, "server_error", sqlite3_errmsg(db->h), NULL);
  }
  /* Candidates that do not resolve to a visible row are one of two things:
   * another tenant's row, or a vector whose intel_items row is gone (an
   * orphan). The split is an extra primary-key seek per such candidate, and
   * only an operator is told either number: how many of OTHER tenants' rows
   * match a query is itself information about those tenants. */
  sqlite3_stmt *exists = NULL;
  if (is_operator &&
      sqlite3_prepare_v2(db->h, "SELECT 1 FROM intel_items WHERE uid=?1",
                         -1, &exists, NULL) != SQLITE_OK)
    exists = NULL;
  cJSON *data = cJSON_CreateArray();
  int shown = 0, visible = 0, withheld = 0, orphans = 0;
  for (int i = 0; i < L.n; i++) {
    vecindex_cand *c = &L.v[i];
    sqlite3_reset(get);
    sqlite3_bind_text(get, 1, c->key, -1, SQLITE_STATIC);
    if (tenant && *tenant) sqlite3_bind_text(get, 2, tenant, -1, SQLITE_STATIC);
    if (sqlite3_step(get) != SQLITE_ROW) {
      if (exists) {
        sqlite3_reset(exists);
        sqlite3_bind_text(exists, 1, c->key, -1, SQLITE_STATIC);
        if (sqlite3_step(exists) == SQLITE_ROW) withheld++; else orphans++;
      }
      continue;
    }
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
  /* The unfiltered arm's rows the partition kept out (see above). */
  for (int i = 0; exists && i < nwh; i++) {
    int in_list = 0;
    for (int j = 0; j < L.n && !in_list; j++) in_list = !strcmp(L.v[j].key, wh[i].key);
    if (in_list) continue;
    sqlite3_reset(exists);
    sqlite3_bind_text(exists, 1, wh[i].key, -1, SQLITE_STATIC);
    if (sqlite3_step(exists) == SQLITE_ROW) withheld++; else orphans++;
  }
  vecindex_hits_free(wh, nwh);
  sqlite3_finalize(get);
  sqlite3_finalize(exists);

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
  if (is_operator) {
    cJSON_AddNumberToObject(meta, "tenant_withheld", withheld);
    cJSON_AddNumberToObject(meta, "index_orphans", orphans);
  }
  /* The bounded view states its bound in-band: shown of total. */
  cJSON_AddNumberToObject(meta, "shown", shown);
  cJSON_AddNumberToObject(meta, "total", visible);
  cJSON_AddBoolToObject(meta, "truncated", visible > shown);
  cJSON_AddNumberToObject(meta, "embed_ms", (double)embed_ms);
  cJSON_AddNumberToObject(meta, "total_ms", (double)ms_since(&t0));
  cJSON_AddItemToObject(meta, "coverage", embed_coverage_json(db));
  free(matchq);
  vecindex_fused_free(&L);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "meta", meta);
  char *out = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  *status = out ? 200 : 500;
  return out;
}
