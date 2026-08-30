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

/* Emit ONE intel_item for a single corpus FTS hit. Returns 1 if emitted. */
static int emit_hit(intel_sink *sink, cJSON *hit) {
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

  long total = -1;
  cJSON *items = corpus_items(db, segq, ctx->tenant_id, &total); /* 1. corpus FTS hits */
  free(segq);
  if (!items) return 0;                             /* query failed → honest empty */

  int emitted = 0, n = cJSON_GetArraySize(items);
  for (int i = 0; i < n; i++)
    emitted += emit_hit(sink, cJSON_GetArrayItem(items, i));
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
