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
#include "../../source.h"
#include "../../core/fts.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CORPUS_LIMIT  15

static const char *ctext(sqlite3_stmt *s, int i) {
  return (sqlite3_column_type(s, i) == SQLITE_NULL)
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(o, k, v);
  else   cJSON_AddNullToObject(o, k);
}

/* ── 1. corpus FTS search (ftsMirror.js search() SQL, verbatim) ───────────── */
static cJSON *corpus_items(sqlite3 *db, const char *segq) {
  cJSON *items = cJSON_CreateArray();
  static const char *Q =
    "SELECT intel_items.uid, intel_items.source_id, intel_items.title, "
    "intel_items.summary, intel_items.link, intel_items.published_at, "
    "intel_items.record_type, "
    "snippet(intel_items_fts,-1,'<mark>','</mark>','\xE2\x80\xA6',12) AS _excerpt "
    "FROM intel_items_fts "
    "JOIN intel_items ON intel_items.uid = intel_items_fts.uid "
    "WHERE intel_items_fts MATCH ?1 "
    "LIMIT ?2";
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db, Q, -1, &s, NULL) != SQLITE_OK) {
    cJSON_Delete(items);
    return NULL;                                  /* → JS catch path */
  }
  sqlite3_bind_text(s, 1, segq, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 2, CORPUS_LIMIT);
  int rc;
  while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
    cJSON *r = cJSON_CreateObject();
    add_str_or_null(r, "uid",          ctext(s, 0));
    add_str_or_null(r, "source_id",    ctext(s, 1));
    add_str_or_null(r, "title",        ctext(s, 2));
    add_str_or_null(r, "summary",      ctext(s, 3));
    add_str_or_null(r, "link",         ctext(s, 4));
    add_str_or_null(r, "published_at", ctext(s, 5));
    add_str_or_null(r, "record_type",  ctext(s, 6));
    add_str_or_null(r, "excerpt",      ctext(s, 7));   /* r._excerpt */
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

  /* segmentForFts(q.trim()) — fts_segment is the P2-verified C equivalent. */
  char *segq = fts_segment(entity);                 /* malloc'd; passthrough-safe */
  if (!segq) return 0;

  cJSON *items = corpus_items(db, segq);            /* 1. corpus FTS hits */
  free(segq);
  if (!items) return 0;                             /* query failed → honest empty */

  int emitted = 0, n = cJSON_GetArraySize(items);
  for (int i = 0; i < n; i++)
    emitted += emit_hit(sink, cJSON_GetArrayItem(items, i));
  cJSON_Delete(items);

  (void)emitted;
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
