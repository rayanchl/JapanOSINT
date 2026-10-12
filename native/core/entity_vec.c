/* core/entity_vec.c — see entity_vec.h. */
#include "entity_vec.h"
#include "entitystore.h"
#include "../source.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define EVEC_SID "_entity_vec"
/* Model input bound. Entity names are short; a 2 KB "address" is a parse
 * accident, and its first 512 bytes still say which address it is. */
#define EVEC_MAX_BYTES 512
/* Vectors of folded-away entities removed per tick. */
#define EVEC_PRUNE_PER_TICK 1000

static const vecindex_col EVEC_COLS[] = {
  { "type",      VI_TEXT, 1 },      /* partition: a company never reads persons */
  { "tenant_id", VI_TEXT, 0 },      /* '' = the shared graph                    */
};
const vecindex_spec entity_vec_spec = {
  .name = "entity", .table = "entity_vec", .key_col = "entity_id",
  .meta_table = "entity_vec_meta", .done_table = "entity_vec_done",
  .fail_table = "entity_vec_failed", .cols = EVEC_COLS, .ncols = 2,
  .rebuild_hint = " (rebuild: DROP TABLE entity_vec, entity_vec_done, "
                  "entity_vec_failed, entity_vec_meta; the pod re-embeds)" };
VECINDEX_REGISTER(entity_vec_spec)

double entity_vec_cos_floor(void) {
  const char *e = getenv("JO_ENTITY_VEC_COS_FLOOR");
  if (e && *e) {
    char *end = NULL;
    double v = strtod(e, &end);
    if (end && end != e && v > 0.0 && v <= 1.0) return v;
  }
  return ENTITY_VEC_COS_FLOOR;
}

char *entity_vec_text(const char *canonical, const char *romaji) {
  if (!canonical) canonical = "";
  size_t n = strlen(canonical) + (romaji ? strlen(romaji) : 0) + 4;
  char *t = malloc(n);
  if (!t) return NULL;
  if (romaji && *romaji && strcasecmp(romaji, canonical) != 0)
    snprintf(t, n, "%s (%s)", canonical, romaji);
  else
    snprintf(t, n, "%s", canonical);
  char *b = vecindex_bound_text(t, EVEC_MAX_BYTES);
  free(t);
  return b;
}

static void iso_now(char *buf, size_t n) {
  time_t t = time(NULL);
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(buf, n, "%Y-%m-%dT%H:%M:%S.000Z", &tm);
}

int entity_vec_ready(db_handle *db) {
  const vecindex_spec *s = &entity_vec_spec;
  if (!db || !vecindex_exists(db, s)) return 0;
  if (vecindex_refused(db, s, NULL, 0)) return 0;
  return vecindex_meta_long(db, s, "dim") > 0;
}

/* Remove vectors whose entity was folded away by a union, or no longer
 * exists for any other reason: a KNN that keeps answering with a deleted id
 * spends the neighbour budget on nothing. */
static void prune(db_handle *db) {
  const vecindex_spec *s = &entity_vec_spec;
  if (!vecindex_exists(db, s)) return;
  sqlite3_stmt *q, *dv, *dd;
  char **ids = NULL;
  int n = 0;
  if (sqlite3_prepare_v2(db->h,
      "SELECT d.entity_id FROM entity_vec_done d"
      " WHERE NOT EXISTS (SELECT 1 FROM entities e WHERE e.entity_id=d.entity_id"
      "   AND (e.tenant_id IS NULL OR e.tenant_id<>'" ES_BREACH_TENANT "'))"
      " LIMIT ?1", -1, &q, NULL) != SQLITE_OK) return;
  sqlite3_bind_int(q, 1, EVEC_PRUNE_PER_TICK);
  ids = calloc(EVEC_PRUNE_PER_TICK, sizeof *ids);
  while (ids && n < EVEC_PRUNE_PER_TICK && sqlite3_step(q) == SQLITE_ROW)
    ids[n++] = strdup((const char *)sqlite3_column_text(q, 0));
  sqlite3_finalize(q);
  if (n > 0 && vecindex_begin(db) == 0) {
    dv = dd = NULL;
    int ok = sqlite3_prepare_v2(db->h, "DELETE FROM entity_vec WHERE entity_id=?1",
                                -1, &dv, NULL) == SQLITE_OK &&
             sqlite3_prepare_v2(db->h, "DELETE FROM entity_vec_done WHERE entity_id=?1",
                                -1, &dd, NULL) == SQLITE_OK;
    for (int i = 0; ok && i < n; i++) {
      if (!ids[i]) continue;
      sqlite3_reset(dv); sqlite3_reset(dd);
      sqlite3_bind_text(dv, 1, ids[i], -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(dd, 1, ids[i], -1, SQLITE_TRANSIENT);
      if (sqlite3_step(dv) != SQLITE_DONE || sqlite3_step(dd) != SQLITE_DONE) ok = 0;
    }
    sqlite3_finalize(dv); sqlite3_finalize(dd);
    vecindex_end(db, ok);
    if (ok)
      fprintf(stderr, "[entity-vec] pruned %d vector(s) of entities no longer in "
                      "the shared graph\n", n);
  }
  for (int i = 0; i < n; i++) free(ids[i]);
  free(ids);
}

typedef struct { char *id, *type, *tenant, *text; } erow;

int entity_vec_tick(db_handle *db, struct http_client *http, int max_walk) {
  const vecindex_spec *s = &entity_vec_spec;
  const char *base = vecindex_base_url();
  if (!base) return 0;
  if (max_walk <= 0) {
    const char *e = getenv("JO_ENTITY_VEC_MAX_PER_RUN");
    max_walk = (e && atoi(e) > 0) ? atoi(e) : 2000;
  }
  vecindex_ensure_meta(db, s);
  vecindex_ensure_side_tables(db, s);
  char now[40];
  iso_now(now, sizeof now);

  char model[256];
  if (vecindex_detect_model(http, base, model, sizeof model) != 0) {
    char msg[400];
    snprintf(msg, sizeof msg, "%s: cannot identify the embedding model (%.200s); "
             "tick skipped", now, model);
    vecindex_meta_set(db, s, "last_error", msg);
    fprintf(stderr, "[entity-vec] %s\n", msg);
    return -1;
  }

  char after[256] = "";
  vecindex_meta_get(db, s, "walk_after", after, sizeof after);
  erow *rows = calloc((size_t)max_walk, sizeof *rows);
  vecindex_item *items = calloc((size_t)max_walk, sizeof *items);
  vecindex_val *vals = calloc((size_t)max_walk * 2, sizeof *vals);
  if (!rows || !items || !vals) { free(rows); free(items); free(vals); return -1; }
  int n = 0;
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "SELECT entity_id,type,COALESCE(tenant_id,''),canonical,name_romaji"
      " FROM entities WHERE entity_id>?1"
      "   AND (tenant_id IS NULL OR tenant_id<>'" ES_BREACH_TENANT "')"
      " ORDER BY entity_id LIMIT ?2", -1, &q, NULL) != SQLITE_OK) {
    vecindex_sql_error(db, s, now, "selecting entities");
    free(rows); free(items); free(vals);
    return -1;
  }
  sqlite3_bind_text(q, 1, after, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(q, 2, max_walk);
  while (n < max_walk && sqlite3_step(q) == SQLITE_ROW) {
    erow *r = &rows[n];
    r->id     = strdup((const char *)sqlite3_column_text(q, 0));
    r->type   = strdup((const char *)sqlite3_column_text(q, 1));
    r->tenant = strdup((const char *)sqlite3_column_text(q, 2));
    r->text   = entity_vec_text((const char *)sqlite3_column_text(q, 3),
                                (const char *)sqlite3_column_text(q, 4));
    if (!r->id || !r->type || !r->tenant || !r->text) {
      free(r->id); free(r->type); free(r->tenant); free(r->text);
      memset(r, 0, sizeof *r);
      break;
    }
    vals[n * 2 + 0] = VI_S(r->type);
    vals[n * 2 + 1] = VI_S(r->tenant);
    items[n].key = r->id;
    items[n].text = r->text;
    items[n].vals = &vals[n * 2];
    n++;
  }
  sqlite3_finalize(q);

  llm_client llm = { .http = http, .base_url = base, .interactive = 0 };
  vecindex_stats st = {0};
  int rc = n > 0 ? vecindex_sync(db, s, &llm, model, items, n, 32, now, now, &st) : 0;
  if (rc == 0) {
    /* a short page is the end of the lap: the next tick starts over, which
     * is what picks up a renamed or newly added entity below the cursor */
    vecindex_meta_set(db, s, "walk_after", n < max_walk ? "" : rows[n - 1].id);
    prune(db);
  }
  if (rc == -2)
    fprintf(stderr, "[entity-vec] index refused (model or dimension changed); "
                    "see entity_vec_meta.refused\n");
  for (int i = 0; i < n; i++) {
    free(rows[i].id); free(rows[i].type); free(rows[i].tenant); free(rows[i].text);
  }
  free(rows); free(items); free(vals);
  if (rc != 0) return -1;
  fprintf(stderr, "[entity-vec] walked=%ld embedded=%ld unchanged=%ld failed=%ld\n",
          st.walked, st.embedded, st.unchanged, st.failed);
  return (int)st.embedded;
}

int entity_vec_neighbours(db_handle *db, const char *entity_id, const char *type,
                          const char *tenant, int k, double floor,
                          entity_nb **out) {
  const vecindex_spec *s = &entity_vec_spec;
  *out = NULL;
  if (!entity_vec_ready(db) || !entity_id || !type || k <= 0) return -1;
  float *qv = NULL; int dim = 0;
  if (vecindex_load(db, s, entity_id, &qv, &dim) != 0) return -1;
  vecindex_val tv = VI_S(type), nv = VI_S(tenant ? tenant : "");
  vecindex_filter f[2] = {
    { .col = "type",      .op = "=", .vals = &tv, .nvals = 1 },
    { .col = "tenant_id", .op = "=", .vals = &nv, .nvals = 1 },
  };
  vecindex_hit *h = NULL;
  char err[256];
  int nh = vecindex_knn(db, s, qv, dim, k + 1, f, 2, &h, err, sizeof err);
  if (nh < 0) { free(qv); return -1; }
  entity_nb *v = calloc((size_t)(nh > 0 ? nh : 1), sizeof *v);
  int n = 0;
  for (int i = 0; v && i < nh && n < k; i++) {
    if (!strcmp(h[i].key, entity_id)) continue;
    /* The angle, from the stored vectors: an L2 distance only maps to cosine
     * for unit vectors, and no server is obliged to return those. */
    float *hv = NULL; int hd = 0;
    if (vecindex_load(db, s, h[i].key, &hv, &hd) != 0 || hd != dim) { free(hv); continue; }
    double c = vec_cosine(qv, hv, dim);
    free(hv);
    if (c < floor) continue;
    v[n].entity_id = strdup(h[i].key);
    v[n].cos = c;
    if (v[n].entity_id) n++;
  }
  vecindex_hits_free(h, nh);
  free(qv);
  *out = v;
  return n;
}

void entity_nb_free(entity_nb *v, int n) {
  for (int i = 0; v && i < n; i++) free(v[i].entity_id);
  free(v);
}

/* ---- the pod ------------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                         /* the index is the output, not intel */
  if (!vecindex_base_url()) {
    fprintf(stderr, "[entity-vec] disabled: JO_EMBED_URL is unset; the entity "
                    "resolver uses norm_key prefixes only\n");
    return 0;
  }
  if (!ctx || !ctx->db) return -1;
  db_handle own;
  db_handle *db = db_worker_open(&own, ctx->db);
  int n = entity_vec_tick(db, ctx->http, 0);
  db_worker_close(&own);
  return n < 0 ? -1 : 0;
}

static const source_def entity_vec_pod_def = {
  .id = EVEC_SID, .collector = "_maint",
  .name = "Entity Name Index",
  .name_ja = "エンティティ名ベクトル索引",
  .description = "Embeds each entity's canonical name and romaji reading so the "
                 "entity resolver can put cross-script duplicates to its LLM "
                 "judge. Never merges. Inert unless JO_EMBED_URL is set.",
  .url = "internal://entity-index",
  .update_interval_sec = 600, .run = run,
  .category = "maintenance" };
REGISTER_SOURCE(entity_vec_pod_def)
