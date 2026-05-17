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
 *  3. found = items.length>0 || !!entityGraph; confidence = found?80:30;
 *     data = JSON.stringify({ query, corpus_hits, items, entity_graph }).
 *     On any error → { success:false, error, confidence:0 }.
 * Emits one osint_service_result intel row (body = the {success,data,
 * confidence(,error)} envelope), exactly like dns_records.c. */
#include "../../../source.h"
#include "../../../core/fts.h"
#include "../../../third_party/cJSON.h"
#include "../../../third_party/sqlite3.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CORPUS_LIMIT  15
#define GRAPH_FANOUT  25     /* getGraph default fanout (jpCorpus passes only depth) */
#define GRAPH_DEPTH    1     /* jpCorpus: getGraph(id,{depth:1}) */

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

/* ── 2a. searchEntities({q,limit:1}) — entityMirror.search, top hit ───────── */
static char *first_entity_id(sqlite3 *db, const char *segq) {
  /* entityMirror.search: snippet + JOIN entities, ORDER BY mention_count DESC,
   * LIMIT 1 (jpCorpus passes limit:1, no type). We only need entity_id. */
  static const char *Q =
    "SELECT entities.entity_id "
    "FROM entities_fts "
    "JOIN entities ON entities.entity_id = entities_fts.uid "
    "WHERE entities_fts MATCH ?1 "
    "ORDER BY entities.mention_count DESC "
    "LIMIT 1";
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db, Q, -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, segq, -1, SQLITE_TRANSIENT);
  char *id = NULL;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *v = (const char *)sqlite3_column_text(s, 0);
    if (v) id = strdup(v);
  }
  sqlite3_finalize(s);
  return id;
}

/* getEntity(id) → { entity_id,type,canonical,mention_count } subset (the only
 * fields jpCorpus reads: id,type,value(=canonical),mention_count, plus the
 * graph-node label uses canonical/mention_count too). NULL if absent. */
typedef struct { char *entity_id, *type, *canonical; long long mention_count; int ok; } ent_row;
static ent_row get_entity(sqlite3 *db, const char *id) {
  ent_row e = {0};
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db,
        "SELECT entity_id,type,canonical,mention_count FROM entities "
        "WHERE entity_id=?1", -1, &s, NULL) != SQLITE_OK)
    return e;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *a = (const char *)sqlite3_column_text(s, 0);
    const char *b = (const char *)sqlite3_column_text(s, 1);
    const char *c = (const char *)sqlite3_column_text(s, 2);
    e.entity_id     = a ? strdup(a) : NULL;
    e.type          = b ? strdup(b) : NULL;
    e.canonical     = c ? strdup(c) : NULL;
    e.mention_count = sqlite3_column_int64(s, 3);
    e.ok = 1;
  }
  sqlite3_finalize(s);
  return e;
}
static void ent_free(ent_row *e) {
  free(e->entity_id); free(e->type); free(e->canonical);
  e->entity_id = e->type = e->canonical = NULL; e->ok = 0;
}

/* ── 2b. getGraph(rootId,{depth:1}) — BFS ego-network (entityStore.js) ─────
 * stmtNeighbours: SELECT src AS a,dst AS b,rel_type,weight FROM
 * entity_relationships WHERE src=@id OR dst=@id ORDER BY weight DESC LIMIT
 * @fanout. nodes Map keyed by entity_id (root first), edges deduped by
 * a|b|rel_type. depth clamped 1..3. Output: {nodes:[{id,type,value,label,
 * mention_count}], edges:[{source,target,relationship,weight}]}. */

/* tiny dynamic string-set for visited / seen-edge / node-id de-dup */
typedef struct { char **v; int n, cap; } sset;
static int sset_has(const sset *s, const char *k) {
  for (int i = 0; i < s->n; i++) if (!strcmp(s->v[i], k)) return 1;
  return 0;
}
static int sset_add(sset *s, const char *k) {        /* 1 if newly added */
  if (sset_has(s, k)) return 0;
  if (s->n == s->cap) {
    int nc = s->cap ? s->cap * 2 : 16;
    char **nv = realloc(s->v, (size_t)nc * sizeof *nv);
    if (!nv) return 0;
    s->v = nv; s->cap = nc;
  }
  s->v[s->n++] = strdup(k);
  return 1;
}
static void sset_free(sset *s) {
  for (int i = 0; i < s->n; i++) free(s->v[i]);
  free(s->v); s->v = NULL; s->n = s->cap = 0;
}

static void graph_add_node(sqlite3 *db, cJSON *nodes, sset *node_ids,
                           const char *id) {
  if (!id || !sset_add(node_ids, id)) return;       /* nodes Map .has guard */
  ent_row e = get_entity(db, id);
  if (!e.ok) { ent_free(&e); return; }              /* add(getEntity(other)) no-op if null */
  cJSON *n = cJSON_CreateObject();
  add_str_or_null(n, "id",   e.entity_id);
  add_str_or_null(n, "type", e.type);
  add_str_or_null(n, "value", e.canonical);
  add_str_or_null(n, "label", e.canonical);
  cJSON_AddNumberToObject(n, "mention_count", (double)e.mention_count);
  cJSON_AddItemToArray(nodes, n);
  ent_free(&e);
}

static cJSON *get_graph(sqlite3 *db, const char *root_id) {
  cJSON *g = cJSON_CreateObject();
  cJSON *nodes = cJSON_AddArrayToObject(g, "nodes");
  cJSON *edges = cJSON_AddArrayToObject(g, "edges");

  ent_row root = get_entity(db, root_id);
  if (!root.ok) { ent_free(&root); return g; }      /* {nodes:[],edges:[]} */
  ent_free(&root);

  sset node_ids = {0}, seen_edge = {0}, visited = {0};
  graph_add_node(db, nodes, &node_ids, root_id);    /* add(root) */

  /* frontier = [rootId]; visited = {rootId}; */
  sset_add(&visited, root_id);
  char **frontier = malloc(sizeof *frontier);
  frontier[0] = strdup(root_id);
  int fn = 1;

  sqlite3_stmt *nb = NULL;
  sqlite3_prepare_v2(db,
    "SELECT src_entity_id AS a, dst_entity_id AS b, rel_type, weight "
    "FROM entity_relationships "
    "WHERE src_entity_id=@id OR dst_entity_id=@id "
    "ORDER BY weight DESC LIMIT @fanout", -1, &nb, NULL);

  int d = GRAPH_DEPTH; if (d < 1) d = 1; if (d > 3) d = 3;
  for (int hop = 0; hop < d && fn > 0; hop++) {
    char **next = NULL; int nn = 0, ncap = 0;
    for (int i = 0; i < fn; i++) {
      const char *id = frontier[i];
      if (nb) {
        sqlite3_reset(nb);
        sqlite3_clear_bindings(nb);
        sqlite3_bind_text(nb, sqlite3_bind_parameter_index(nb, "@id"),
                          id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int (nb, sqlite3_bind_parameter_index(nb, "@fanout"),
                          GRAPH_FANOUT);
        while (sqlite3_step(nb) == SQLITE_ROW) {
          const char *a  = (const char *)sqlite3_column_text(nb, 0);
          const char *b  = (const char *)sqlite3_column_text(nb, 1);
          const char *rt = (const char *)sqlite3_column_text(nb, 2);
          double w = sqlite3_column_double(nb, 3);
          if (!a || !b || !rt) continue;
          const char *other = !strcmp(a, id) ? b : a;
          char ek[768];
          snprintf(ek, sizeof ek, "%s|%s|%s", a, b, rt);
          if (sset_add(&seen_edge, ek)) {           /* !seenEdge.has(ek) */
            cJSON *e = cJSON_CreateObject();
            cJSON_AddStringToObject(e, "source", a);
            cJSON_AddStringToObject(e, "target", b);
            cJSON_AddStringToObject(e, "relationship", rt);
            cJSON_AddNumberToObject(e, "weight", w);
            cJSON_AddItemToArray(edges, e);
          }
          graph_add_node(db, nodes, &node_ids, other);  /* add(getEntity(other)) */
          if (!sset_has(&visited, other)) {
            sset_add(&visited, other);
            if (nn == ncap) {
              ncap = ncap ? ncap * 2 : 16;
              next = realloc(next, (size_t)ncap * sizeof *next);
            }
            next[nn++] = strdup(other);
          }
        }
      }
    }
    for (int i = 0; i < fn; i++) free(frontier[i]);
    free(frontier);
    frontier = next; fn = nn;                       /* frontier = next */
  }
  for (int i = 0; i < fn; i++) free(frontier[i]);
  free(frontier);
  if (nb) sqlite3_finalize(nb);
  sset_free(&node_ids); sset_free(&seen_edge); sset_free(&visited);
  return g;
}

/* ── envelope emit (== dns_records.c shape; body = JS {success,data,conf}) ── */
static int emit_result(intel_sink *sink, const char *entity, int success,
                        int confidence, char *data_json /*owned, may be NULL*/,
                        const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  /* JS `data` is a JSON *string*; embed it as the parsed value so the
   * pipeline (which JSON.parses service data) sees the same structure. */
  if (data_json) {
    cJSON *dv = cJSON_Parse(data_json);
    cJSON_AddItemToObject(env, "data", dv ? dv : cJSON_CreateNull());
  } else {
    cJSON_AddNullToObject(env, "data");
  }
  if (error) cJSON_AddStringToObject(env, "error", error);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "JP_CORPUS_LOOKUP");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "jpcorpus:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "JP_CORPUS_LOOKUP — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = !success ? (error ? error : "lookup failed")
                                : (confidence >= 80 ? "corpus/entity match"
                                                    : "no corpus match");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"JP_CORPUS_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  free(data_json);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;
  if (!ctx->db || !ctx->db->h)
    return emit_result(sink, entity ? entity : "", 0, 0, NULL, "no_db");

  sqlite3 *db = ctx->db->h;

  /* segmentForFts(q.trim()) — fts_segment is the P2-verified C equivalent.
   * (entity has no leading/trailing space in practice; trim is a no-op for
   * the dispatcher's normalized pivot, matching JS q.trim().) */
  char *segq = fts_segment(entity);                 /* malloc'd; passthrough-safe */
  if (!segq) return emit_result(sink, entity, 0, 0, NULL, "oom");

  /* 1. corpus hits */
  cJSON *items = corpus_items(db, segq);
  if (!items) {                                     /* prepare/step failed → JS catch */
    free(segq);
    return emit_result(sink, entity, 0, 0, NULL, "corpus_query_failed");
  }
  int n_items = cJSON_GetArraySize(items);

  /* 2. resolved entity + 1-hop graph */
  cJSON *entity_graph = NULL;                        /* JS: null unless matched */
  char *eid = first_entity_id(db, segq);
  free(segq);
  if (eid) {
    entity_graph = cJSON_CreateObject();
    ent_row full = get_entity(db, eid);
    if (full.ok) {
      cJSON *en = cJSON_CreateObject();
      add_str_or_null(en, "id",   full.entity_id);
      add_str_or_null(en, "type", full.type);
      add_str_or_null(en, "value", full.canonical);
      cJSON_AddNumberToObject(en, "mention_count", (double)full.mention_count);
      cJSON_AddItemToObject(entity_graph, "entity", en);
    } else {
      cJSON_AddNullToObject(entity_graph, "entity"); /* full ? {...} : null */
    }
    ent_free(&full);
    cJSON_AddItemToObject(entity_graph, "graph", get_graph(db, eid));
    free(eid);
  }

  /* 3. assemble data + confidence (found = items.length>0 || !!entityGraph) */
  int found = (n_items > 0) || (entity_graph != NULL);
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "query", entity);
  cJSON_AddNumberToObject(data, "corpus_hits", n_items);
  cJSON_AddItemToObject(data, "items", items);       /* takes ownership */
  if (entity_graph) cJSON_AddItemToObject(data, "entity_graph", entity_graph);
  else cJSON_AddNullToObject(data, "entity_graph");
  char *dj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  return emit_result(sink, entity, 1, found ? 80 : 30, dj, NULL);
}

static const source_def jp_corpus_lookup_def = {
  .id = "JP_CORPUS_LOOKUP", .collector = "osint",
  .name = "Japan Corpus Lookup", .name_ja = "日本コーパス照会",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(jp_corpus_lookup_def)
