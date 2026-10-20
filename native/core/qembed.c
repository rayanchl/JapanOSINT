/* core/qembed.c — see qembed.h. */
#include "qembed.h"
#include "embed_pod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int qembed_enabled(void) {
  return vecindex_query_url() != NULL;
}

static int qembed_timeout_ms(void) {
  const char *e = getenv("JO_EMBED_QUERY_TIMEOUT_MS");
  int v = (e && *e) ? atoi(e) : 0;
  return v > 0 ? v : 15000;
}

int qembed_texts(const char *const *texts, int n, float **out, int *dim,
                 llm_status *st) {
  if (out) *out = NULL;
  if (dim) *dim = 0;
  if (st) *st = LLM_ERR_BAD_REQUEST;
  const char *url = vecindex_query_url();
  if (!url || !texts || n <= 0 || !out || !dim) return -1;

  llm_client llm = { .http = NULL, .base_url = url, .interactive = 1,
                     .bound_queue_wait = 1 };
  float *all = NULL;
  int d = 0;
  for (int off = 0; off < n; off += QEMBED_BATCH) {
    int m = n - off < QEMBED_BATCH ? n - off : QEMBED_BATCH;
    char **b = calloc((size_t)m, sizeof *b);
    if (!b) { free(all); return -1; }
    int ok = 1;
    for (int i = 0; i < m && ok; i++) {
      b[i] = embed_bound_text(texts[off + i] ? texts[off + i] : "");
      if (!b[i]) ok = 0;
    }
    float *v = NULL; int vd = 0; llm_status s = LLM_ERR_BAD_REQUEST;
    int rc = ok ? llm_embed(&llm, (const char *const *)b, m, &v, &vd,
                            qembed_timeout_ms(), &s) : -1;
    for (int i = 0; i < m; i++) free(b[i]);
    free(b);
    if (rc != 0 || !v || vd <= 0 || (d && vd != d)) {
      /* A batch that answered in another width than the one before it is
       * two spaces in one array; refuse rather than compare across them. */
      if (st) *st = (rc != 0) ? s : LLM_ERR_EMPTY;
      free(v); free(all);
      return -1;
    }
    if (!all) {
      d = vd;
      all = malloc((size_t)n * (size_t)d * sizeof *all);
      if (!all) { free(v); return -1; }
    }
    memcpy(all + (size_t)off * (size_t)d, v, (size_t)m * (size_t)d * sizeof *v);
    free(v);
  }
  *out = all;
  *dim = d;
  if (st) *st = LLM_OK;
  return 0;
}

int qembed_index_usable(db_handle *db, const vecindex_spec *s, int *dim_out,
                        char *why, size_t cap) {
  if (dim_out) *dim_out = 0;
  if (why && cap) why[0] = 0;
  if (!qembed_enabled()) {
    if (why && cap) snprintf(why, cap, "no embedding server configured (JO_EMBED_URL is unset)");
    return 0;
  }
  if (!db || !db->h || !s || !vecindex_exists(db, s)) {
    if (why && cap) snprintf(why, cap, "the %s index has not been built", s ? s->name : "?");
    return 0;
  }
  if (vecindex_refused(db, s, why, cap)) return 0;
  char model[256];
  if (!vecindex_meta_get(db, s, "model", model, sizeof model) || !model[0]) {
    if (why && cap) snprintf(why, cap, "the %s index records no model", s->name);
    return 0;
  }
  int dim = (int)vecindex_meta_long(db, s, "dim");
  if (dim <= 0) {
    if (why && cap) snprintf(why, cap, "the %s index records no dimension", s->name);
    return 0;
  }
  char live[256];
  if (vecindex_live_query_model(live, sizeof live) != 0) {
    if (why && cap) snprintf(why, cap, "%s", live);
    return 0;
  }
  if (strcmp(model, live) != 0) {
    if (why && cap)
      snprintf(why, cap, "the %s index was built with '%.100s' but the query "
               "server serves '%.100s'", s->name, model, live);
    return 0;
  }
  if (dim_out) *dim_out = dim;
  return 1;
}

int qembed_intel_knn(db_handle *db, const char *tenant, const char *text, int k,
                     vecindex_hit **out, char *why, size_t cap) {
  if (out) *out = NULL;
  if (why && cap) why[0] = 0;
  if (!out || !text || !*text || k <= 0) return -1;
  if (!qembed_enabled()) {
    if (why && cap) snprintf(why, cap, "no embedding server configured");
    return -1;
  }
  /* The intel index keeps model/dim in intel_vec_meta (embed_pod's SPEC);
   * the vec0 table answering queries may be the legacy one mid-migration. */
  const vecindex_spec *vs = embed_query_spec(db);
  if (!embed_table_exists(db)) {
    if (why && cap) snprintf(why, cap, "the intel index has not been built");
    return -1;
  }
  char model[256];
  int dim = embed_index_dim(db, model, sizeof model);
  if (!dim) {
    if (why && cap) snprintf(why, cap, "the intel index records no dimension");
    return -1;
  }
  if (embed_index_refused(db, why, cap)) return -1;
  char live[256];
  if (embed_live_query_model(live, sizeof live) != 0) {
    if (why && cap) snprintf(why, cap, "%s", live);
    return -1;
  }
  if (model[0] && strcmp(model, live) != 0) {
    if (why && cap)
      snprintf(why, cap, "the intel index was built with '%.100s' but the "
               "query server serves '%.100s'", model, live);
    return -1;
  }
  const char *texts[1] = { text };
  float *qv = NULL; int qdim = 0; llm_status st;
  if (qembed_texts(texts, 1, &qv, &qdim, &st) != 0) {
    if (why && cap) snprintf(why, cap, "query embedding failed (%s)", llm_status_code(st));
    return -1;
  }
  if (qdim != dim) {
    free(qv);
    if (why && cap) snprintf(why, cap, "query is %d-d, index is %d-d", qdim, dim);
    return -1;
  }
  /* Tenant + shared corpus, INSIDE the KNN when the index is partitioned
   * (semsearchapi.c explains why a post-filter answers a tenant with too
   * little). The legacy table has no partition; the caller's materialising
   * read applies the tenant filter for it. */
  int partitioned = (vs == &embed_intel_spec) && tenant && *tenant;
  vecindex_val tv[2] = { VI_S(tenant), VI_S("legacy") };
  vecindex_filter tf = { .col = "tenant_id", .op = "IN", .vals = tv,
                         .nvals = (tenant && !strcmp(tenant, "legacy")) ? 1 : 2 };
  char err[256];
  int n = vecindex_knn(db, vs, qv, qdim, k, partitioned ? &tf : NULL,
                       partitioned ? 1 : 0, out, err, sizeof err);
  free(qv);
  if (n < 0 && why && cap) snprintf(why, cap, "%s", err);
  return n;
}
