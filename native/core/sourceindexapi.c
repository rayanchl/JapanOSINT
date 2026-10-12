/* core/sourceindexapi.c — see sourceindexapi.h. */
#include "sourceindexapi.h"
#include "service_vec.h"
#include "source_card.h"
#include "source_category.h"
#include "layertab.h"
#include "../source.h"
#include <math.h>
#include <sqlite3.h>
#include "../third_party/cJSON.h"
#include "../third_party/mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* mg_http_get_var: -3 = the value would not fit. A clipped query is a
 * DIFFERENT query, so that is a 414, never half of what was asked
 * (semsearchapi.c qs_get is the same rule). */
static int qs_get(const char *qs, const char *k, char *out, size_t cap,
                  int *too_long) {
  struct mg_str s = mg_str(qs ? qs : "");
  int n = mg_http_get_var(&s, k, out, cap);
  if (n == -3) *too_long = 1;
  if (n <= 0) out[0] = 0;
  return n > 0;
}

static char *err_body(int *status, int code, const char *err, const char *reason,
                      cJSON *coverage) {
  *status = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", err);
  if (reason) cJSON_AddStringToObject(o, "reason", reason);
  if (coverage) cJSON_AddItemToObject(o, "coverage", coverage);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

static cJSON *kinds_array(unsigned mask) {
  cJSON *a = cJSON_CreateArray();
  for (int i = 0; i < SC_NKINDS; i++)
    if (mask & (1u << i))
      cJSON_AddItemToArray(a, cJSON_CreateString(source_kind_name((source_kind)i)));
  return a;
}

/* ---- GET /api/admin/layers/review ---------------------------------------- */

typedef struct {
  const char *id;          /* layertab's static string                     */
  double *sum;             /* sum of members' unit vectors                 */
  int n;
} lr_layer;

typedef struct {
  char *id;
  long rows;
  float *v;                /* unit vector, or NULL (no card vector)        */
} lr_geo;

typedef struct {
  lr_layer *L; int nl, cl;
  lr_geo *G; int ng;
  int dim, bad_dim;
} lr_ctx;

static void unit(float *v, int dim) {
  double s = 0;
  for (int i = 0; i < dim; i++) s += (double)v[i] * v[i];
  if (s <= 0) return;
  float inv = (float)(1.0 / sqrt(s));
  for (int i = 0; i < dim; i++) v[i] *= inv;
}

static void lr_visit(const char *id, const float *vec, int dim, void *u) {
  lr_ctx *x = u;
  if (!x->dim) x->dim = dim;
  if (dim != x->dim) { x->bad_dim++; return; }
  const char *layer = layertab_layer_for_source(id);
  if (layer) {
    lr_layer *l = NULL;
    for (int i = 0; i < x->nl; i++) if (!strcmp(x->L[i].id, layer)) { l = &x->L[i]; break; }
    if (!l) {
      if (x->nl == x->cl) {
        int nc = x->cl ? x->cl * 2 : 64;
        lr_layer *nL = realloc(x->L, (size_t)nc * sizeof *nL);
        if (!nL) return;
        x->L = nL; x->cl = nc;
      }
      l = &x->L[x->nl];
      l->id = layer; l->n = 0;
      l->sum = calloc((size_t)dim, sizeof *l->sum);
      if (!l->sum) return;
      x->nl++;
    }
    double s = 0;
    for (int i = 0; i < dim; i++) s += (double)vec[i] * vec[i];
    double inv = s > 0 ? 1.0 / sqrt(s) : 0;
    for (int i = 0; i < dim; i++) l->sum[i] += vec[i] * inv;
    l->n++;
    return;
  }
  for (int i = 0; i < x->ng; i++) {
    if (strcmp(x->G[i].id, id) || x->G[i].v) continue;
    x->G[i].v = malloc((size_t)dim * sizeof *x->G[i].v);
    if (x->G[i].v) { memcpy(x->G[i].v, vec, (size_t)dim * sizeof *vec); unit(x->G[i].v, dim); }
    break;
  }
}

char *sourceindexapi_layers_review(db_handle *db, const char *tenant,
                                   const char *id, const char *qs,
                                   int is_operator, int *status) {
  (void)tenant; (void)id; (void)qs;
  /* httpd.c routes this inside the /api/admin/ operator gate; refuse again
   * here so a second caller cannot forget it. */
  if (!is_operator) return err_body(status, 403, "operator_required", NULL, NULL);
  lr_ctx x = {0};
  /* GEODATA-EMITTING is MEASURED, not declared: a source with at least one
   * stored geocoded row. A source that declares geometry but has stored none
   * is not listed — it has nothing on the map to place yet. */
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, "SELECT source_id, count(*) FROM intel_items "
                         "WHERE lat IS NOT NULL GROUP BY source_id "
                         "ORDER BY 2 DESC, 1", -1, &s, NULL) != SQLITE_OK)
    return err_body(status, 500, "query_failed", sqlite3_errmsg(db->h), NULL);
  int cg = 0, geo_total = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *sid = (const char *)sqlite3_column_text(s, 0);
    if (!sid || !registry_get(sid)) continue;
    geo_total++;
    if (layertab_layer_for_source(sid)) continue;     /* already on a layer */
    if (x.ng == cg) {
      int nc = cg ? cg * 2 : 64;
      lr_geo *nG = realloc(x.G, (size_t)nc * sizeof *nG);
      if (!nG) break;
      x.G = nG; cg = nc;
    }
    x.G[x.ng].id = strdup(sid);
    x.G[x.ng].rows = sqlite3_column_int64(s, 1);
    x.G[x.ng].v = NULL;
    if (x.G[x.ng].id) x.ng++;
  }
  sqlite3_finalize(s);

  char basis[160];
  int complete = service_vec_ready_all(db, basis, sizeof basis);
  int scanned = service_vec_scan(db, lr_visit, &x);

  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "definition",
    "sources with at least one stored geocoded intel_items row that resolve to "
    "no layer (core/layertab.h step 3), each with the layers whose members' "
    "cards are closest to its own (cosine to the layer centroid). Proposals "
    "only: nothing is assigned.");
  cJSON *arr = cJSON_CreateArray();
  for (int g = 0; g < x.ng; g++) {
    const source_card *c = source_card_get(x.G[g].id);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "id", x.G[g].id);
    if (c && c->def->name) cJSON_AddStringToObject(r, "name", c->def->name);
    else cJSON_AddNullToObject(r, "name");
    cJSON_AddStringToObject(r, "category", c && c->category ? c->category : "");
    cJSON_AddStringToObject(r, "kind", c ? source_kind_name(c->kind) : "");
    cJSON_AddNumberToObject(r, "geocoded_rows", (double)x.G[g].rows);
    cJSON *pr = cJSON_CreateArray();
    if (!x.G[g].v) {
      cJSON_AddStringToObject(r, "no_proposal", scanned < 0
        ? "the service index does not exist" : "this source has no card vector yet");
    } else {
      /* top 3 layers by cosine to the centroid */
      double bs[3] = { -2, -2, -2 };
      int bi[3] = { -1, -1, -1 };
      for (int l = 0; l < x.nl; l++) {
        double dot = 0, nn = 0;
        for (int t = 0; t < x.dim; t++) {
          dot += x.L[l].sum[t] * x.G[g].v[t];
          nn += x.L[l].sum[t] * x.L[l].sum[t];
        }
        double cs = nn > 0 ? dot / sqrt(nn) : -2;
        for (int p = 0; p < 3; p++) {
          if (cs <= bs[p]) continue;
          for (int q = 2; q > p; q--) { bs[q] = bs[q - 1]; bi[q] = bi[q - 1]; }
          bs[p] = cs; bi[p] = l;
          break;
        }
      }
      for (int p = 0; p < 3 && bi[p] >= 0; p++) {
        const lr_layer *L = &x.L[bi[p]];
        const layer_row *lr = layertab_get(L->id);
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "layer", L->id);
        if (lr && lr->name) cJSON_AddStringToObject(e, "name", lr->name);
        else cJSON_AddNullToObject(e, "name");
        if (lr && lr->modality) cJSON_AddStringToObject(e, "modality", lr->modality);
        else cJSON_AddNullToObject(e, "modality");
        cJSON_AddNumberToObject(e, "similarity", bs[p]);
        cJSON_AddNumberToObject(e, "members_with_vectors", L->n);
        cJSON_AddItemToArray(pr, e);
      }
    }
    cJSON_AddItemToObject(r, "proposals", pr);
    cJSON_AddItemToArray(arr, r);
  }
  cJSON_AddNumberToObject(o, "count", x.ng);
  cJSON_AddItemToObject(o, "sources", arr);
  cJSON *cov = cJSON_CreateObject();
  cJSON_AddNumberToObject(cov, "geodata_sources", geo_total);
  cJSON_AddNumberToObject(cov, "geodata_sources_without_layer", x.ng);
  cJSON_AddNumberToObject(cov, "layers_compared", x.nl);
  cJSON_AddNumberToObject(cov, "vectors_scanned", scanned < 0 ? 0 : scanned);
  cJSON_AddBoolToObject(cov, "index_complete", complete);
  if (x.bad_dim) cJSON_AddNumberToObject(cov, "vectors_skipped_other_dim", x.bad_dim);
  cJSON_AddItemToObject(o, "coverage", cov);
  for (int l = 0; l < x.nl; l++) free(x.L[l].sum);
  free(x.L);
  for (int g = 0; g < x.ng; g++) { free(x.G[g].id); free(x.G[g].v); }
  free(x.G);
  char *out = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  *status = 200;
  return out;
}

char *sourceindexapi_categories(db_handle *db) {
  cJSON *o = source_category_distribution(db);
  cJSON *v = cJSON_CreateArray();
  for (int i = 0; i < SOURCE_CATEGORY_COUNT; i++)
    cJSON_AddItemToArray(v, cJSON_CreateString(SOURCE_CATEGORIES[i]));
  cJSON_AddItemToObject(o, "vocabulary", v);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

char *sourceindexapi_search(db_handle *db, const char *tenant, const char *id,
                            const char *qs, int is_operator, int *status) {
  (void)tenant; (void)id; (void)is_operator;
  char q[1024], kind[16], kk[16];
  int tl = 0;
  qs_get(qs, "q", q, sizeof q, &tl);
  qs_get(qs, "kind", kind, sizeof kind, &tl);
  qs_get(qs, "k", kk, sizeof kk, &tl);
  if (tl) return err_body(status, 414, "query_param_too_long", NULL, NULL);
  if (!q[0]) return err_body(status, 400, "q_required", NULL, NULL);
  if (kind[0] && source_kind_parse(kind) < 0)
    return err_body(status, 400, "unknown_kind",
                    "kind is one of pivot, scheduled, pod, manual", NULL);
  int k_req = kk[0] ? atoi(kk) : SIDX_K_DEF;
  if (k_req <= 0) k_req = SIDX_K_DEF;
  int k = k_req > SIDX_K_MAX ? SIDX_K_MAX : k_req;

  service_vec_filter f = { .kind = kind[0] ? kind : NULL };
  service_vec_hit *hits = NULL;
  service_vec_info info;
  int n = service_vec_search(db, q, &f, k, &hits, &info);

  cJSON *cov = cJSON_CreateObject();
  cJSON_AddItemToObject(cov, "searched_kinds", kinds_array(info.searched_kinds));
  cJSON_AddItemToObject(cov, "unready_kinds", kinds_array(info.unready_kinds));
  cJSON_AddNumberToObject(cov, "cards_searched", info.total);
  cJSON_AddNumberToObject(cov, "semantic_candidates", info.semantic);
  cJSON_AddNumberToObject(cov, "keyword_candidates", info.lexical);
  cJSON_AddItemToObject(cov, "index", service_vec_coverage_json(db));
  if (n < 0)
    return err_body(status, 503, "service_index_unavailable", info.reason, cov);

  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "query", q);
  if (kind[0]) cJSON_AddStringToObject(o, "kind", kind);
  else cJSON_AddNullToObject(o, "kind");
  cJSON_AddNumberToObject(o, "k", k);
  /* A capped k is a bounded view and says so (house rule 2). */
  if (k != k_req) {
    cJSON_AddNumberToObject(o, "k_requested", k_req);
    cJSON_AddStringToObject(o, "k_note", "k is capped at 200 per request");
  }
  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < n; i++) {
    const source_card *c = source_card_get(hits[i].id);
    if (!c) continue;
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "id", hits[i].id);
    if (c->def->name) cJSON_AddStringToObject(r, "name", c->def->name);
    else cJSON_AddNullToObject(r, "name");
    cJSON_AddStringToObject(r, "category", c->category ? c->category : "");
    cJSON_AddStringToObject(r, "kind", source_kind_name(c->kind));
    cJSON_AddNumberToObject(r, "score", hits[i].score);
    if (hits[i].vrank) cJSON_AddNumberToObject(r, "vector_rank", hits[i].vrank);
    else cJSON_AddNullToObject(r, "vector_rank");
    if (hits[i].frank) cJSON_AddNumberToObject(r, "keyword_rank", hits[i].frank);
    else cJSON_AddNullToObject(r, "keyword_rank");
    cJSON_AddItemToArray(arr, r);
  }
  service_vec_hits_free(hits, n);
  cJSON_AddNumberToObject(o, "count", cJSON_GetArraySize(arr));
  cJSON_AddItemToObject(o, "results", arr);
  cJSON_AddItemToObject(o, "coverage", cov);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  *status = 200;
  return s;
}
