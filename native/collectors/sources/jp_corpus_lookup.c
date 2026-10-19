/* collectors/osint/sources/jp_corpus_lookup.c
 * OSINT service — port of server/src/osint/services/jpCorpus.js. The always-on
 * Japan adapter: every entity the pipeline touches is also run through this so
 * the OSINT search inherently reaches JapanOSINT's collected corpus + the
 * unified entity graph. On-demand (interval 0); the dispatcher runs it with
 * ctx->entity set; ctx->db is the live SQLite handle.
 *
 * Faithful mirror of jpCorpusLookup(entity):
 *  1. Full-text hits across the whole corpus — intelMirror.search({ q:entity,
 *     limit:15, selectColumns:'intel_items.uid, intel_items.source_id,
 *     intel_items.title, intel_items.summary, intel_items.link,
 *     intel_items.published_at, intel_items.record_type' }). That helper
 *     (ftsMirror.js search()) emits exactly:
 *       SELECT <selectColumns>,
 *              snippet(intel_items_fts,-1,'<mark>','</mark>','…',12) AS _excerpt
 *         FROM intel_items_fts
 *         JOIN intel_items ON intel_items.uid = intel_items_fts.uid
 *        WHERE intel_items_fts MATCH @q
 *        LIMIT @limit
 *     with @q = segmentForFts(q.trim()). fts_segment() == segmentForFts (P2
 *     parity). Rows → { uid, source_id, title, summary, link, published_at,
 *     record_type, excerpt:r._excerpt }.
 *  2. Already-known entity + 1-hop relationship neighbourhood, if resolved —
 *     searchEntities({q:entity,limit:1}); if a hit, getEntity(id) +
 *     getGraph(id,{depth:1}). entityGraph = { entity:{id,type,value,
 *     mention_count}|null, graph:{nodes,edges} }.
 *
 * PER-RECORD EMIT: emits ONE intel_item per corpus FTS hit
 * (remote_key="corpus:<hit-uid>"), body {uid,title,summary,link,excerpt},
 * link = the hit's link. The entity-graph neighbourhood is still computed via
 * ctx->db but no longer surfaced as a row. If nothing matches, emits nothing
 * and returns 0 (honest empty). */
#include "core/dbutil.h"
#include "source.h"
#include "lib/jocore.h"          /* jo_trunc_notice: the in-band "shown N of M" */
#include "core/fts.h"
#include "core/qembed.h"         /* the vector arm (inert without JO_EMBED_URL) */
#include "third_party/cJSON.h"
#include "third_party/sqlite3.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* The slice this service emits per entity. Bounded on purpose (it feeds an
 * LLM prompt), so the shortfall is REPORTED IN THE DATA: the run also counts
 * the whole match set (capped at CORPUS_COUNT_CAP) and emits a
 * collector-truncation-notice "used 15 of N" whenever N > 15. House rule 2. */
#define CORPUS_LIMIT      15
#define CORPUS_COUNT_CAP  100000

/* TENANCY. intel_items.tenant_id is NOT NULL DEFAULT 'legacy' (schema.sql), so
 * the corpus IS tenant-scoped and this read must be too: an OSINT pivot run
 * for tenant A must not surface tenant B's rows. Applied as
 * `tenant_id IN (?,'legacy')` — the same spelling as intelapi.c/exportapi.c —
 * whenever the dispatcher sets ctx->tenant_id; an unscoped run (NULL) reads
 * everything, which is today's single-tenant behaviour. */

static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(o, k, v);
  else   cJSON_AddNullToObject(o, k);
}

/* ── 1. corpus FTS search (ftsMirror.js search() SQL, verbatim) ───────────── */
static cJSON *corpus_items(sqlite3 *db, const char *segq, const char *tenant,
                           long *total_out) {
  cJSON *items = cJSON_CreateArray();
  const int tf = tenant && *tenant;
  /* bm25-ordered, title-weighted like /api/intel/items?sort=relevance: the 15
   * we hand the prompt are the 15 BEST hits, not the 15 SQLite happened to
   * visit first. */
#define CORPUS_SEL \
    "SELECT intel_items.uid, intel_items.source_id, intel_items.title, " \
    "intel_items.summary, intel_items.link, intel_items.published_at, " \
    "intel_items.record_type, " \
    "snippet(intel_items_fts,-1,'<mark>','</mark>','\xE2\x80\xA6',12) AS _excerpt " \
    "FROM intel_items_fts " \
    "JOIN intel_items ON intel_items.uid = intel_items_fts.uid " \
    "WHERE intel_items_fts MATCH ?1 "
#define CORPUS_ORD \
    "ORDER BY bm25(intel_items_fts,0,10,1,3,2,1,1,2,1), intel_items.uid LIMIT ?2"
#define CORPUS_CNT \
    "SELECT COUNT(*) FROM (SELECT 1 FROM intel_items_fts " \
    "JOIN intel_items ON intel_items.uid = intel_items_fts.uid " \
    "WHERE intel_items_fts MATCH ?1 "
#define CORPUS_TEN "AND intel_items.tenant_id IN (?3,'legacy') "
  static const char *Q_ANY    = CORPUS_SEL CORPUS_ORD;
  static const char *Q_TENANT = CORPUS_SEL CORPUS_TEN CORPUS_ORD;
  static const char *C_ANY    = CORPUS_CNT "LIMIT ?2)";
  static const char *C_TENANT = CORPUS_CNT CORPUS_TEN "LIMIT ?2)";

  /* Total first (capped): -1 = "we never found out", which the notice prints
   * as such rather than guessing. */
  *total_out = -1;
  sqlite3_stmt *c = NULL;
  if (sqlite3_prepare_v2(db, tf ? C_TENANT : C_ANY, -1, &c, NULL) == SQLITE_OK) {
    sqlite3_bind_text(c, 1, segq, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (c, 2, CORPUS_COUNT_CAP + 1);
    if (tf) sqlite3_bind_text(c, 3, tenant, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(c) == SQLITE_ROW) *total_out = (long)sqlite3_column_int64(c, 0);
    sqlite3_finalize(c);
  }

  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db, tf ? Q_TENANT : Q_ANY, -1, &s, NULL) != SQLITE_OK) {
    cJSON_Delete(items);
    return NULL;                                  /* → JS catch path */
  }
  sqlite3_bind_text(s, 1, segq, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 2, CORPUS_LIMIT);
  if (tf) sqlite3_bind_text(s, 3, tenant, -1, SQLITE_TRANSIENT);
  int rc;
  while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
    cJSON *r = cJSON_CreateObject();
    add_str_or_null(r, "uid",          db_ctext(s, 0));
    add_str_or_null(r, "source_id",    db_ctext(s, 1));
    add_str_or_null(r, "title",        db_ctext(s, 2));
    add_str_or_null(r, "summary",      db_ctext(s, 3));
    add_str_or_null(r, "link",         db_ctext(s, 4));
    add_str_or_null(r, "published_at", db_ctext(s, 5));
    add_str_or_null(r, "record_type",  db_ctext(s, 6));
    add_str_or_null(r, "excerpt",      db_ctext(s, 7));   /* r._excerpt */
    cJSON_AddItemToArray(items, r);
  }
  int failed = (rc != SQLITE_DONE);
  sqlite3_finalize(s);
  if (failed) { cJSON_Delete(items); return NULL; }
  return items;
}

/* Emit ONE intel_item for a single corpus FTS hit. Returns 1 if emitted.
 * `retrieval` (may be NULL; borrowed) is the hybrid path's account of which
 * arms found this hit; NULL leaves the properties exactly as the FTS-only
 * path has always written them. */
static int emit_hit(intel_sink *sink, cJSON *hit, const cJSON *retrieval) {
  if (!hit) return 0;
  cJSON *uid_j     = cJSON_GetObjectItem(hit, "uid");
  cJSON *title_j   = cJSON_GetObjectItem(hit, "title");
  cJSON *summary_j = cJSON_GetObjectItem(hit, "summary");
  cJSON *link_j    = cJSON_GetObjectItem(hit, "link");
  cJSON *excerpt_j = cJSON_GetObjectItem(hit, "excerpt");
  cJSON *srcid_j   = cJSON_GetObjectItem(hit, "source_id");

  const char *uid     = (uid_j     && uid_j->valuestring)     ? uid_j->valuestring     : NULL;
  const char *title   = (title_j   && title_j->valuestring)   ? title_j->valuestring   : NULL;
  const char *summary = (summary_j && summary_j->valuestring) ? summary_j->valuestring : NULL;
  const char *link    = (link_j    && link_j->valuestring)    ? link_j->valuestring    : NULL;

  /* Body = {uid,title,summary,link,excerpt} (the hit's own fields). */
  cJSON *data = cJSON_CreateObject();
  add_str_or_null(data, "uid",     uid);
  add_str_or_null(data, "title",   title);
  add_str_or_null(data, "summary", summary);
  add_str_or_null(data, "link",    link);
  add_str_or_null(data, "excerpt",
                  (excerpt_j && excerpt_j->valuestring) ? excerpt_j->valuestring : NULL);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "JP_CORPUS_LOOKUP");
  add_str_or_null(props, "corpus_uid", uid);
  add_str_or_null(props, "corpus_source_id",
                  (srcid_j && srcid_j->valuestring) ? srcid_j->valuestring : NULL);
  cJSON_AddBoolToObject(props, "success", 1);
  if (retrieval)
    cJSON_AddItemToObject(props, "retrieval", cJSON_Duplicate(retrieval, 1));
  char *pj = cJSON_PrintUnformatted(props);

  char rk[400];
  snprintf(rk, sizeof rk, "corpus:%s", uid ? uid : (title ? title : "?"));

  /* Each hit's originating corpus source becomes the drill-down "source", so
   * DB SEARCH expands into every distinct collector that matched. */
  const char *srcid = (srcid_j && srcid_j->valuestring && *srcid_j->valuestring)
                      ? srcid_j->valuestring : "corpus";

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title ? title : (uid ? uid : "corpus hit");
  it.body            = bj;
  it.summary         = summary ? summary : (title ? title : "corpus hit");
  it.link            = link;
  it.record_type     = "osint_service_result";
  it.sub_source_id   = srcid;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"JP_CORPUS_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* ── 2. hybrid: FTS arm + vector arm, fused by reciprocal rank ────────────
 *
 * WHY. The FTS arm is an AND of every token the entity yields
 * (fts_query_expr), which is the right question for an IP or a domain and the
 * wrong one for "companies supplying the Self-Defense Forces": every word must
 * occur, so a natural-language pivot matches nothing, and an English entity
 * never matches a Japanese record about the same thing. The vector arm asks
 * intel_vec_v2 for the nearest rows by meaning instead — tenant + shared
 * corpus INSIDE the KNN — and the two ranked lists are fused by RRF
 * (VECINDEX_RRF_K, the constant /api/intel/semantic uses), so a row both arms
 * like outranks one only one arm likes, and bm25 and L2 never have to be put
 * on one scale.
 *
 * WHAT IS UNCHANGED. The slice is still CORPUS_LIMIT and still bounded on
 * purpose (it feeds a prompt); the "of N" notice still fires. N is now the
 * union the fusion chose from: every keyword match (counted, capped as
 * before) plus each semantic neighbour that matches no keyword — and the
 * notice SAYS the semantic arm only examined its CORPUS_ARM_K nearest, since
 * a KNN has no "all matches" to count.
 *
 * INERT. When the vector arm cannot answer — no JO_EMBED_URL, no index, a
 * refused or swapped model, an embed failure, zero hits — this returns -1
 * before emitting anything and run() takes the FTS-only path byte for byte.
 * Each hit says which arms found it in properties.retrieval; the FTS-only path
 * writes no such key, exactly as before. */
#define CORPUS_ARM_K 50
/* A KNN always answers k rows, however far: with three keyword matches and a
 * 15 cap, the other twelve slots would be filled by whatever is least
 * dissimilar, which on a small corpus is "unrelated". A neighbour at or past
 * this L2 distance is not a candidate at all. sqrt(2) is cos <= 0 for the unit
 * vectors llama-server returns — orthogonal or opposed, i.e. no shared meaning
 * at all — so with a real model it almost never bites and it never drops a
 * KEYWORD match (those come from the FTS arm). JO_CORPUS_VEC_MAX_DISTANCE
 * tightens it. */
#define CORPUS_VEC_MAX_DISTANCE 1.41421356

static double corpus_vec_max_distance(void) {
  const char *e = getenv("JO_CORPUS_VEC_MAX_DISTANCE");
  double v = (e && *e) ? atof(e) : 0;
  return v > 0 ? v : CORPUS_VEC_MAX_DISTANCE;
}

static int corpus_hybrid(const source_ctx *ctx, intel_sink *sink, sqlite3 *db,
                         const char *segq, const char *entity) {
  const char *tenant = ctx->tenant_id;
  const int tf = tenant && *tenant;
  if (!qembed_enabled()) return -1;               /* inert: never even try */

  vecindex_hit *vh = NULL;
  char why[300];
  int nv = qembed_intel_knn(ctx->db, tenant, entity, CORPUS_ARM_K, &vh, why,
                            sizeof why);
  if (nv <= 0) {
    if (nv < 0)
      fprintf(stderr, "[corpus] vector arm unavailable (%s) — keyword only\n", why);
    vecindex_hits_free(vh, nv > 0 ? nv : 0);
    return -1;
  }

  /* Keyword total, the same capped count the FTS-only path reports. */
  long fts_total = -1;
  {
    static const char *C_ANY =
      "SELECT COUNT(*) FROM (SELECT 1 FROM intel_items_fts "
      "JOIN intel_items ON intel_items.uid = intel_items_fts.uid "
      "WHERE intel_items_fts MATCH ?1 LIMIT ?2)";
    static const char *C_TEN =
      "SELECT COUNT(*) FROM (SELECT 1 FROM intel_items_fts "
      "JOIN intel_items ON intel_items.uid = intel_items_fts.uid "
      "WHERE intel_items_fts MATCH ?1 "
      "AND intel_items.tenant_id IN (?3,'legacy') LIMIT ?2)";
    sqlite3_stmt *c = NULL;
    if (sqlite3_prepare_v2(db, tf ? C_TEN : C_ANY, -1, &c, NULL) == SQLITE_OK) {
      sqlite3_bind_text(c, 1, segq, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int (c, 2, CORPUS_COUNT_CAP + 1);
      if (tf) sqlite3_bind_text(c, 3, tenant, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(c) == SQLITE_ROW) fts_total = (long)sqlite3_column_int64(c, 0);
      sqlite3_finalize(c);
    }
  }

  vecindex_fused L = {0};
  int fts_n = 0;
  {
    /* The keyword arm: the same bm25 weights as the FTS-only path, so its
     * ranks are the ranks that path would have shown. */
    static const char *F_ANY =
      "SELECT intel_items.uid FROM intel_items_fts "
      "JOIN intel_items ON intel_items.uid = intel_items_fts.uid "
      "WHERE intel_items_fts MATCH ?1 "
      "ORDER BY bm25(intel_items_fts,0,10,1,3,2,1,1,2,1), intel_items.uid LIMIT ?2";
    static const char *F_TEN =
      "SELECT intel_items.uid FROM intel_items_fts "
      "JOIN intel_items ON intel_items.uid = intel_items_fts.uid "
      "WHERE intel_items_fts MATCH ?1 AND intel_items.tenant_id IN (?3,'legacy') "
      "ORDER BY bm25(intel_items_fts,0,10,1,3,2,1,1,2,1), intel_items.uid LIMIT ?2";
    sqlite3_stmt *f = NULL;
    if (sqlite3_prepare_v2(db, tf ? F_TEN : F_ANY, -1, &f, NULL) == SQLITE_OK) {
      sqlite3_bind_text(f, 1, segq, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int (f, 2, CORPUS_ARM_K);
      if (tf) sqlite3_bind_text(f, 3, tenant, -1, SQLITE_TRANSIENT);
      while (sqlite3_step(f) == SQLITE_ROW) {
        const char *u = db_ctext(f, 0);
        if (u && vecindex_fused_add(&L, u, 0, ++fts_n, 0)) continue;
      }
      sqlite3_finalize(f);
    }
  }
  /* Only neighbours inside the distance bound; ranks stay the KNN's own
   * (hits are nearest-first, so the kept ones are a prefix). */
  int nkeep = 0;
  double maxd = corpus_vec_max_distance();
  while (nkeep < nv && vh[nkeep].distance < maxd) nkeep++;
  vecindex_fused_add_hits(&L, vh, nkeep);
  vecindex_hits_free(vh, nv);
  vecindex_fused_sort(&L);

  /* Materialise in fused order. The row read re-applies the tenant filter
   * (the legacy table mid-migration has no partition to filter on), and the
   * snippet read doubles as the "does this semantic neighbour also match the
   * keywords?" test that keeps N an honest union. */
  static const char *G_ANY =
    "SELECT uid, source_id, title, summary, link, published_at, record_type "
    "FROM intel_items WHERE uid=?1";
  static const char *G_TEN =
    "SELECT uid, source_id, title, summary, link, published_at, record_type "
    "FROM intel_items WHERE uid=?1 AND tenant_id IN (?2,'legacy')";
  sqlite3_stmt *get = NULL, *snip = NULL;
  if (sqlite3_prepare_v2(db, tf ? G_TEN : G_ANY, -1, &get, NULL) != SQLITE_OK) {
    vecindex_fused_free(&L);
    return -1;                                   /* → FTS-only path */
  }
  if (sqlite3_prepare_v2(db,
        "SELECT snippet(intel_items_fts,-1,'<mark>','</mark>','\xE2\x80\xA6',12) "
        "FROM intel_items_fts WHERE intel_items_fts MATCH ?1 AND rowid = "
        "(SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?2)",
        -1, &snip, NULL) != SQLITE_OK)
    snip = NULL;

  int emitted = 0, vec_only = 0, vec_n = 0, both = 0;
  for (int i = 0; i < L.n; i++) {
    vecindex_cand *c = &L.v[i];
    if (c->vrank > 0) vec_n++;
    char *excerpt = NULL;
    int kw = c->frank > 0;
    if (snip && (emitted < CORPUS_LIMIT || (c->vrank > 0 && !kw))) {
      sqlite3_reset(snip);
      sqlite3_bind_text(snip, 1, segq, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(snip, 2, c->key, -1, SQLITE_STATIC);
      if (sqlite3_step(snip) == SQLITE_ROW) {
        kw = 1;
        const char *x = db_ctext(snip, 0);
        if (x) excerpt = strdup(x);
      }
    }
    if (c->vrank > 0 && !kw) vec_only++;
    if (emitted >= CORPUS_LIMIT) { free(excerpt); continue; }

    sqlite3_reset(get);
    sqlite3_bind_text(get, 1, c->key, -1, SQLITE_STATIC);
    if (tf) sqlite3_bind_text(get, 2, tenant, -1, SQLITE_STATIC);
    if (sqlite3_step(get) != SQLITE_ROW) { free(excerpt); continue; }  /* gone / not visible */
    cJSON *r = cJSON_CreateObject();
    add_str_or_null(r, "uid",          db_ctext(get, 0));
    add_str_or_null(r, "source_id",    db_ctext(get, 1));
    add_str_or_null(r, "title",        db_ctext(get, 2));
    add_str_or_null(r, "summary",      db_ctext(get, 3));
    add_str_or_null(r, "link",         db_ctext(get, 4));
    add_str_or_null(r, "published_at", db_ctext(get, 5));
    add_str_or_null(r, "record_type",  db_ctext(get, 6));
    add_str_or_null(r, "excerpt",      excerpt);
    free(excerpt);

    cJSON *ret = cJSON_CreateObject();
    cJSON_AddStringToObject(ret, "method", "rrf");
    cJSON *arms = cJSON_AddArrayToObject(ret, "arms");
    if (c->frank > 0) cJSON_AddItemToArray(arms, cJSON_CreateString("fts"));
    if (c->vrank > 0) cJSON_AddItemToArray(arms, cJSON_CreateString("vector"));
    if (c->frank > 0) cJSON_AddNumberToObject(ret, "fts_rank", c->frank);
    else              cJSON_AddNullToObject(ret, "fts_rank");
    if (c->vrank > 0) {
      cJSON_AddNumberToObject(ret, "vector_rank", c->vrank);
      cJSON_AddNumberToObject(ret, "vector_distance", c->distance);
    } else {
      cJSON_AddNullToObject(ret, "vector_rank");
      cJSON_AddNullToObject(ret, "vector_distance");
    }
    cJSON_AddNumberToObject(ret, "rrf_score", c->score);
    if (c->frank > 0 && c->vrank > 0) both++;
    emitted += emit_hit(sink, r, ret);
    cJSON_Delete(ret);
    cJSON_Delete(r);
  }
  sqlite3_finalize(get);
  if (snip) sqlite3_finalize(snip);
  vecindex_fused_free(&L);

  fprintf(stderr, "[corpus] hybrid %s: %d emitted (fts arm %d, vector arm %d, "
                  "both %d, semantic-only %d, keyword total %ld)\n",
          ctx->source_id, emitted, fts_n, vec_n, both, vec_only, fts_total);

  /* N = keyword matches + semantic neighbours that match no keyword. */
  long total = (fts_total < 0 || fts_total > CORPUS_COUNT_CAP)
               ? -1 : fts_total + vec_only;
  if (total > CORPUS_LIMIT || (total < 0 && emitted == CORPUS_LIMIT)) {
    char scope[120];
    snprintf(scope, sizeof scope, "%.100s", entity);
    char reason[400];
    if (total >= 0)
      snprintf(reason, sizeof reason,
               "corpus lookup shows the %d best hits by keyword + semantic rank "
               "fusion of %ld candidates (%ld keyword matches, %d semantic "
               "neighbours matching no keyword; the semantic arm examines only "
               "its %d nearest)", CORPUS_LIMIT, total, fts_total, vec_only,
               CORPUS_ARM_K);
    else
      snprintf(reason, sizeof reason,
               "corpus lookup shows the %d best hits by keyword + semantic rank "
               "fusion; the keyword matches number %s, plus %d semantic "
               "neighbours matching no keyword (the semantic arm examines only "
               "its %d nearest)", CORPUS_LIMIT,
               fts_total > CORPUS_COUNT_CAP ? "more than 100000" : "an uncounted total",
               vec_only, CORPUS_ARM_K);
    jo_trunc_notice_scoped(sink, ctx->source_id, scope,
                           "internal://osint/jp-corpus-lookup", emitted, total,
                           reason,
                           "GET /api/intel/items?q=<entity>&sort=relevance pages the "
                           "keyword set; GET /api/intel/semantic?q=<entity>&mode=hybrid "
                           "pages the fused one");
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;
  if (!ctx->db || !ctx->db->h) return 0;            /* no db → honest empty */

  sqlite3 *db = ctx->db->h;

  /* fts_query_expr(), NOT fts_segment(): the entity is an IP, an email, a URL
   * or a domain far more often than a word, and fts_segment is a tokenizer
   * whose output went to the FTS5 expression parser verbatim — `10.0.0.1`,
   * `a@b.com`, `https://x/y?z` and `foo-bar` are all FTS5 syntax errors
   * ("no such column", unterminated string, NOT), so every such pivot failed
   * the prepare and was reported as an honest-looking empty. fts_query_expr
   * quotes every token (entityapi.c:155 has the same fix and the same
   * reasoning). NULL = no usable token = nothing to search for. */
  char *segq = fts_query_expr(entity);              /* malloc'd, or NULL */
  if (!segq) return 0;

  /* Hybrid first; -1 means "the vector arm cannot answer" and nothing was
   * emitted, so the FTS-only path below runs exactly as it always has. */
  if (corpus_hybrid(ctx, sink, db, segq, entity) == 0) {
    free(segq);
    return 0;
  }

  long total = -1;
  cJSON *items = corpus_items(db, segq, ctx->tenant_id, &total); /* 1. corpus FTS hits */
  free(segq);
  if (!items) return 0;                             /* query failed → honest empty */

  int emitted = 0, n = cJSON_GetArraySize(items);
  for (int i = 0; i < n; i++)
    emitted += emit_hit(sink, cJSON_GetArrayItem(items, i), NULL);
  cJSON_Delete(items);

  /* Shown 15 of N, in-band. The notice is keyed per entity so pivots on
   * different entities do not overwrite each other's disclosure. */
  if (total > CORPUS_LIMIT || (total < 0 && n == CORPUS_LIMIT)) {
    char scope[120];                    /* fits jo_trunc_notice's key buffer */
    snprintf(scope, sizeof scope, "%.100s", entity);
    char reason[200];
    if (total > CORPUS_COUNT_CAP)
      snprintf(reason, sizeof reason,
               "corpus lookup shows the %d best-ranked hits of more than %d",
               CORPUS_LIMIT, CORPUS_COUNT_CAP);
    else if (total >= 0)
      snprintf(reason, sizeof reason,
               "corpus lookup shows the %d best-ranked hits of %ld", CORPUS_LIMIT, total);
    else
      snprintf(reason, sizeof reason,
               "corpus lookup shows its first %d hits; the total could not be counted",
               CORPUS_LIMIT);
    jo_trunc_notice_scoped(sink, ctx->source_id, scope,
                           "internal://osint/jp-corpus-lookup", emitted,
                           total > CORPUS_COUNT_CAP ? -1 : total, reason,
                           "GET /api/intel/items?q=<entity>&sort=relevance pages the full set");
  }
  return 0;                  /* no hits → honest empty, not an error */
}

static const source_def jp_corpus_lookup_def = {
  .id = "JP_CORPUS_LOOKUP", .collector = "osint",
  .name = "Japan Corpus Lookup", .name_ja = "日本コーパス照会",
  .update_interval_sec = 0, .run = run,
  .category = "investigation", .type = "api",
  .url = "internal://osint/jp-corpus-lookup",
  .description = "Search the JapanOSINT corpus + entity graph for an entity.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(jp_corpus_lookup_def)
