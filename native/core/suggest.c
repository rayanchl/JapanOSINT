/* core/suggest.c — see suggest.h. */
#include "suggest.h"
#include "qembed.h"
#include "service_vec.h"
#include "pipeline.h"
#include "../lib/jpnorm.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- a small process-wide vector cache ----------------------------------
 * A user pausing on "acme sh", "acme ship", "acme shipping" sends the same
 * history texts three times; their vectors do not change between calls.
 * Keyed on (model, text) so a model swap can never serve an old vector. Full
 * = cleared, not evicted piecemeal: it refills from one call. */
#define SUGGEST_CACHE_MAX 4096

typedef struct { unsigned long long h; float *v; } sg_slot;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static sg_slot g_tab[SUGGEST_CACHE_MAX * 2];
static int g_used = 0, g_dim = 0;

static void cache_clear_locked(void) {
  for (int i = 0; i < SUGGEST_CACHE_MAX * 2; i++) { free(g_tab[i].v); g_tab[i].v = NULL; g_tab[i].h = 0; }
  g_used = 0;
}

static int cache_slot(unsigned long long h) {
  int cap = SUGGEST_CACHE_MAX * 2;
  int i = (int)(h % (unsigned)cap);
  for (int k = 0; k < cap; k++, i = (i + 1) % cap)
    if (g_tab[i].h == h || g_tab[i].h == 0) return i;
  return -1;
}

static unsigned long long text_key(const char *model, const char *t) {
  unsigned long long h = vecindex_hash(model) * 1099511628211ULL ^ vecindex_hash(t);
  return h ? h : 1;
}

/* Vectors for texts[0..n) (row-major into *out, caller frees). 0 / -1. */
static int embed_cached(const char *model, const char *const *texts, int n,
                        float **out, int *dim) {
  *out = NULL; *dim = 0;
  const char **miss = malloc((size_t)n * sizeof *miss);
  int *mi = malloc((size_t)n * sizeof *mi);
  if (!miss || !mi) { free(miss); free(mi); return -1; }
  int m = 0, d = 0;
  pthread_mutex_lock(&g_mu);
  d = g_dim;
  for (int i = 0; i < n; i++) {
    int s = cache_slot(text_key(model, texts[i]));
    if (s < 0 || g_tab[s].h == 0) { miss[m] = texts[i]; mi[m] = i; m++; }
  }
  pthread_mutex_unlock(&g_mu);
  float *fresh = NULL;
  if (m > 0) {
    llm_status st;
    int fd = 0;
    if (qembed_texts(miss, m, &fresh, &fd, &st) != 0) { free(miss); free(mi); return -1; }
    pthread_mutex_lock(&g_mu);
    if (g_dim != fd || g_used + m > SUGGEST_CACHE_MAX) { cache_clear_locked(); g_dim = fd; }
    for (int k = 0; k < m; k++) {
      unsigned long long h = text_key(model, miss[k]);
      int s = cache_slot(h);
      if (s < 0 || g_tab[s].h) continue;
      float *v = malloc((size_t)fd * sizeof *v);
      if (!v) continue;
      memcpy(v, fresh + (size_t)k * (size_t)fd, (size_t)fd * sizeof *v);
      g_tab[s].h = h; g_tab[s].v = v; g_used++;
    }
    pthread_mutex_unlock(&g_mu);
    d = fd;
  }
  float *all = d > 0 ? malloc((size_t)n * (size_t)d * sizeof *all) : NULL;
  int ok = all != NULL;
  pthread_mutex_lock(&g_mu);
  for (int i = 0; ok && i < n; i++) {
    int s = cache_slot(text_key(model, texts[i]));
    if (s >= 0 && g_tab[s].h && g_dim == d)
      memcpy(all + (size_t)i * (size_t)d, g_tab[s].v, (size_t)d * sizeof *all);
    else ok = 0;
  }
  pthread_mutex_unlock(&g_mu);
  /* A text the cache could not keep (it filled mid-call) is still answered
   * from this call's own batch. */
  if (!ok && all && fresh && m == n) {
    memcpy(all, fresh, (size_t)n * (size_t)d * sizeof *all);
    ok = 1;
  }
  free(fresh); free(miss); free(mi);
  if (!ok) { free(all); return -1; }
  *out = all; *dim = d;
  return 0;
}

/* ---- candidates ---------------------------------------------------------- */

typedef struct {
  char *text;
  const char *source;        /* "history" | "saved" | "service" | "llm"      */
  char *service_id;          /* service candidates only                     */
  double score;
  int has_score;
} sg_cand;

typedef struct { sg_cand *v; int n, cap; } sg_list;

static int folded_eq(const char *a, const char *b) {
  char *fa = jpnorm_fold_dup(a), *fb = jpnorm_fold_dup(b);
  int eq = fa && fb && !strcmp(fa, fb);
  free(fa); free(fb);
  return eq;
}

static int list_has(const sg_list *l, const char *t) {
  for (int i = 0; i < l->n; i++) if (folded_eq(l->v[i].text, t)) return 1;
  return 0;
}

/* Add unless it is empty, the query itself, or already listed. */
static void list_add(sg_list *l, const char *query, const char *t,
                     const char *source, const char *sid) {
  if (!t || !*t || folded_eq(t, query) || list_has(l, t)) return;
  if (l->n == l->cap) {
    int nc = l->cap ? l->cap * 2 : 32;
    sg_cand *nv = realloc(l->v, (size_t)nc * sizeof *nv);
    if (!nv) return;
    l->v = nv; l->cap = nc;
  }
  sg_cand *c = &l->v[l->n];
  memset(c, 0, sizeof *c);
  c->text = strdup(t);
  c->source = source;
  c->service_id = sid ? strdup(sid) : NULL;
  if (!c->text) return;
  l->n++;
}

static void list_free(sg_list *l) {
  for (int i = 0; i < l->n; i++) { free(l->v[i].text); free(l->v[i].service_id); }
  free(l->v);
}

/* The text a stored search was about: params.q, else params.query. */
static const char *params_text(cJSON *p) {
  const cJSON *x = cJSON_GetObjectItem(p, "q");
  if (!cJSON_IsString(x) || !x->valuestring[0]) x = cJSON_GetObjectItem(p, "query");
  return (cJSON_IsString(x) && x->valuestring[0]) ? x->valuestring : NULL;
}

static int read_rows(db_handle *db, const char *sql, const char *tenant,
                      int limit, const char *query, const char *source,
                      sg_list *l) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, tenant, -1, SQLITE_STATIC);
  sqlite3_bind_int(s, 2, limit);
  int rows = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    rows++;
    const char *pj = (const char *)sqlite3_column_text(s, 0);
    cJSON *p = pj ? cJSON_Parse(pj) : NULL;
    list_add(l, query, params_text(p), source, NULL);
    cJSON_Delete(p);
  }
  sqlite3_finalize(s);
  return rows;
}

/* A read-only view of the router's table, as the dispatcher's resolver has. */
static const vecindex_spec sg_svec_view = {
  .name = "service", .table = SVEC_TABLE, .key_col = "id",
  .meta_table = SVEC_META_TABLE };

static int by_score(const void *a, const void *b) {
  const sg_cand *x = a, *y = b;
  if (x->score > y->score) return -1;
  if (x->score < y->score) return 1;
  return strcmp(x->text, y->text);
}

static double min_cos(void) {
  const char *e = getenv("JO_SUGGEST_MIN_COS");
  double v = (e && *e) ? atof(e) : 0;
  return v > 0 && v < 1 ? v : SUGGEST_MIN_COS;
}

char *suggest_build(db_handle *db, llm_client *llm, const char *tenant,
                    const char *query, int expand) {
  if (!qembed_enabled() || !db || !db->h || !query || !*query) return NULL;
  char model[256];
  if (vecindex_live_query_model(model, sizeof model) != 0) return NULL;

  sg_list l = {0};
  int scanned_h = 0, scanned_s = 0;
  if (tenant && *tenant) {
    /* The newest SUGGEST_HISTORY_SCAN rows, said in meta.history_scanned: a
     * typeahead reads a window, never the whole history, and says so. */
    scanned_h = read_rows(db, "SELECT params_json FROM search_history "
                              "WHERE tenant_id=?1 ORDER BY ts DESC, id DESC LIMIT ?2",
                          tenant, SUGGEST_HISTORY_SCAN, query, "history", &l);
    scanned_s = read_rows(db, "SELECT params_json FROM saved_searches "
                              "WHERE tenant_id=?1 "
                              "ORDER BY pinned DESC, created_at DESC LIMIT ?2",
                          tenant, SUGGEST_SAVED_SCAN, query, "saved", &l);
  }
  int n_text = l.n;

  /* The router's nearest services, by its own public query. */
  char **ids = NULL; int nids = 0;
  char *cat = service_vec_catalogue(db, query, SUGGEST_SERVICE_K, NULL, &ids, &nids);
  free(cat);
  for (int i = 0; i < nids; i++) {
    const source_def *d = registry_get(ids[i]);
    if (d) list_add(&l, query, d->name ? d->name : d->id, "service", d->id);
  }
  service_vec_free_ids(ids, nids);

  /* Score: the query and every text candidate in one cached batch; services
   * against the router's stored vectors. */
  const char **texts = malloc((size_t)(n_text + 1) * sizeof *texts);
  if (!texts) { list_free(&l); return NULL; }
  texts[0] = query;
  for (int i = 0; i < n_text; i++) texts[i + 1] = l.v[i].text;
  float *v = NULL; int d = 0;
  int rc = embed_cached(model, texts, n_text + 1, &v, &d);
  free(texts);
  if (rc != 0) { list_free(&l); return NULL; }    /* → generation-only path */
  for (int i = 0; i < l.n; i++) {
    if (i < n_text) {
      l.v[i].score = vec_cosine(v, v + (size_t)(i + 1) * (size_t)d, d);
      l.v[i].has_score = 1;
    } else {
      float *sv = NULL; int sd = 0;
      if (vecindex_load(db, &sg_svec_view, l.v[i].service_id, &sv, &sd) == 0 && sd == d) {
        l.v[i].score = vec_cosine(v, sv, d);
        l.v[i].has_score = 1;
      }
      free(sv);
    }
  }
  free(v);

  /* Keep what clears the floor, best first. */
  double floor = min_cos();
  int keep = 0;
  for (int i = 0; i < l.n; i++) {
    if (l.v[i].has_score && l.v[i].score >= floor) {
      sg_cand t = l.v[keep]; l.v[keep] = l.v[i]; l.v[i] = t;
      keep++;
    }
  }
  qsort(l.v, (size_t)keep, sizeof *l.v, by_score);
  for (int i = keep; i < l.n; i++) { free(l.v[i].text); free(l.v[i].service_id); }
  l.n = keep;                     /* below the floor: not suggestions at all */
  int candidates = keep;

  /* The LLM, only when asked: after the retrieved suggestions. */
  int llm_n = 0;
  const char *llm_state = "not_requested (expand=1 asks the model)";
  if (expand) {
    char *arr = llm ? osint_suggest_db(db, llm, query) : NULL;
    cJSON *a = arr ? cJSON_Parse(arr) : NULL;
    free(arr);
    cJSON *s;
    if (cJSON_IsArray(a))
      cJSON_ArrayForEach(s, a) {
        if (!cJSON_IsString(s)) continue;
        int before = l.n;
        list_add(&l, query, s->valuestring, "llm", NULL);  /* after the retrieved */
        if (l.n > before) llm_n++;
      }
    keep = l.n;
    cJSON_Delete(a);
    llm_state = llm_n ? "included" : "requested, nothing usable returned";
    candidates += llm_n;
  }

  cJSON *o = cJSON_CreateObject();
  cJSON *sug = cJSON_AddArrayToObject(o, "suggestions");
  cJSON *items = cJSON_AddArrayToObject(o, "items");
  int shown = keep < SUGGEST_MAX ? keep : SUGGEST_MAX;
  for (int i = 0; i < shown; i++) {
    sg_cand *c = &l.v[i];
    cJSON_AddItemToArray(sug, cJSON_CreateString(c->text));
    cJSON *it = cJSON_CreateObject();
    cJSON_AddStringToObject(it, "text", c->text);
    cJSON_AddStringToObject(it, "source", c->source);
    if (c->service_id) cJSON_AddStringToObject(it, "service_id", c->service_id);
    if (c->has_score)
      cJSON_AddNumberToObject(it, "similarity", (double)(long)(c->score * 1000) / 1000.0);
    cJSON_AddItemToArray(items, it);
  }
  cJSON *meta = cJSON_AddObjectToObject(o, "meta");
  cJSON_AddStringToObject(meta, "retrieval", "embedding");
  cJSON_AddNumberToObject(meta, "candidates", candidates);
  cJSON_AddNumberToObject(meta, "shown", shown);
  cJSON_AddNumberToObject(meta, "cut_at", SUGGEST_MAX);
  cJSON_AddBoolToObject(meta, "truncated", candidates > shown);
  cJSON_AddNumberToObject(meta, "history_scanned", scanned_h);
  cJSON_AddNumberToObject(meta, "saved_scanned", scanned_s);
  cJSON_AddNumberToObject(meta, "min_similarity", floor);
  cJSON_AddStringToObject(meta, "llm", llm_state);
  char *out = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  list_free(&l);
  return out;
}
