/* core/sourceindexapi.c — see sourceindexapi.h. */
#include "sourceindexapi.h"
#include "service_vec.h"
#include "source_card.h"
#include "../source.h"
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
