#include "entityapi.h"
#include "fts.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
static long count1(sqlite3 *h, const char *sql) {
  sqlite3_stmt *s; long n = 0;
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) == SQLITE_OK &&
      sqlite3_step(s) == SQLITE_ROW)
    n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

char *entityapi_stats(db_handle *db) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "intel_items",
    (double)count1(db->h, "SELECT COUNT(*) FROM intel_items"));
  cJSON_AddNumberToObject(o, "extracted", (double)count1(db->h,
    "SELECT COUNT(*) FROM entity_extraction_state WHERE extracted_at != ''"));
  cJSON_AddNumberToObject(o, "failed", (double)count1(db->h,
    "SELECT COUNT(*) FROM entity_extraction_state WHERE failed_count >= 5"));
  cJSON_AddNumberToObject(o, "entities",
    (double)count1(db->h, "SELECT COUNT(*) FROM entities"));
  cJSON_AddNumberToObject(o, "mentions",
    (double)count1(db->h, "SELECT COUNT(*) FROM entity_mentions"));
  cJSON_AddNumberToObject(o, "relationships",
    (double)count1(db->h, "SELECT COUNT(*) FROM entity_relationships"));
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* searchEntities({q,type,limit}) — entityMirror.search: pre-segment q like
 * the write path (fts_segment == jpTokenizer.segmentForFts), MATCH joined
 * back to `entities` via entities_fts.uid, mention_count DESC. */
char *entityapi_search(db_handle *db, const char *q, const char *type, int limit) {
  cJSON *results = cJSON_CreateArray();
  /* q already trimmed/non-empty by caller */
  char *segq = fts_segment(q);                 /* malloc'd; passthrough-safe */
  int lim = limit > 0 ? limit : 30;
  if (lim > 100) lim = 100;

  const char *sql = type
    ? "SELECT entities.*, snippet(entities_fts,-1,'<mark>','</mark>','…',12) "
      "FROM entities_fts JOIN entities ON entities.entity_id=entities_fts.uid "
      "WHERE entities_fts MATCH ?1 AND entities.type=?2 "
      "ORDER BY entities.mention_count DESC LIMIT ?3"
    : "SELECT entities.*, snippet(entities_fts,-1,'<mark>','</mark>','…',12) "
      "FROM entities_fts JOIN entities ON entities.entity_id=entities_fts.uid "
      "WHERE entities_fts MATCH ?1 "
      "ORDER BY entities.mention_count DESC LIMIT ?2";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    free(segq); cJSON_Delete(results); return NULL;          /* → 500 */
  }
  sqlite3_bind_text(s, 1, segq, -1, SQLITE_TRANSIENT);
  char tl[128];
  if (type) {
    snprintf(tl, sizeof tl, "%s", type);
    for (char *p = tl; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    sqlite3_bind_text(s, 2, tl, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 3, lim);
  } else {
    sqlite3_bind_int(s, 2, lim);
  }
  /* entities.* column order = entities table DDL; _excerpt is the last col */
  int rc;
  while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
    int n = sqlite3_column_count(s);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "entity_id", (const char *)sqlite3_column_text(s, 0));
    cJSON_AddStringToObject(r, "type",      (const char *)sqlite3_column_text(s, 1));
    cJSON_AddStringToObject(r, "value",     (const char *)sqlite3_column_text(s, 2));
    cJSON_AddNumberToObject(r, "mention_count",
                            (double)sqlite3_column_int64(s, 8));
    add_str_or_null(r, "last_seen_at", ctext(s, 10));
    add_str_or_null(r, "excerpt", ctext(s, n - 1));
    cJSON_AddItemToArray(results, r);
  }
  int failed = (rc != SQLITE_DONE);
  sqlite3_finalize(s);
  free(segq);
  if (failed) { cJSON_Delete(results); return NULL; }        /* → 500 */

  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "results", results);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* getEntity(id) → row, with the entities.js type-guard. Caller frees ret. */
static sqlite3_stmt *entity_by_id(db_handle *db, const char *id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT entity_id,type,canonical,norm_key,name_ja,name_romaji,"
        "aliases_json,properties,mention_count,first_seen_at,last_seen_at,"
        "tenant_id FROM entities WHERE entity_id=?1", -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return NULL; }
  return s;                                      /* positioned on the row */
}

char *entityapi_get(db_handle *db, const char *type, const char *id) {
  sqlite3_stmt *s = entity_by_id(db, id);
  if (!s) return NULL;
  const char *etype = (const char *)sqlite3_column_text(s, 1);
  if (!etype || strcmp(etype, type) != 0) { sqlite3_finalize(s); return NULL; }

  cJSON *aliases = cJSON_Parse(ctext(s, 6) ? ctext(s, 6) : "[]");
  if (!aliases) aliases = cJSON_CreateArray();
  cJSON *props = cJSON_Parse(ctext(s, 7) ? ctext(s, 7) : "{}");
  if (!props) props = cJSON_CreateObject();

  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "entity_id", (const char *)sqlite3_column_text(s, 0));
  cJSON_AddStringToObject(o, "type", etype);
  cJSON_AddStringToObject(o, "value", (const char *)sqlite3_column_text(s, 2));
  add_str_or_null(o, "name_ja", ctext(s, 4));
  add_str_or_null(o, "name_romaji", ctext(s, 5));
  cJSON_AddItemToObject(o, "aliases", aliases);
  cJSON_AddItemToObject(o, "properties", props);
  cJSON_AddNumberToObject(o, "mention_count", (double)sqlite3_column_int64(s, 8));
  add_str_or_null(o, "first_seen_at", ctext(s, 9));
  add_str_or_null(o, "last_seen_at", ctext(s, 10));
  sqlite3_finalize(s);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* ── getGraph(): BFS ego-network, fan-out 25/node, depth 1..3 ───────────── */
typedef struct { char *id, *type, *canon; long mc; } gnode;

static int gn_find(gnode *v, int n, const char *id) {
  for (int i = 0; i < n; i++) if (strcmp(v[i].id, id) == 0) return i;
  return -1;
}
/* fetch a node's display fields; returns 0 if entity_id absent */
static int fetch_node(db_handle *db, const char *id, gnode *out) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT entity_id,type,canonical,mention_count FROM entities "
        "WHERE entity_id=?1", -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  int ok = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    out->id    = strdup((const char *)sqlite3_column_text(s, 0));
    out->type  = strdup((const char *)sqlite3_column_text(s, 1));
    out->canon = strdup((const char *)sqlite3_column_text(s, 2));
    out->mc    = sqlite3_column_int64(s, 3);
    ok = 1;
  }
  sqlite3_finalize(s);
  return ok;
}

char *entityapi_graph(db_handle *db, const char *type, const char *id, int depth) {
  sqlite3_stmt *r = entity_by_id(db, id);
  if (!r) return NULL;
  const char *etype = (const char *)sqlite3_column_text(r, 1);
  if (!etype || strcmp(etype, type) != 0) { sqlite3_finalize(r); return NULL; }
  sqlite3_finalize(r);

  int d = depth < 1 ? 1 : depth > 3 ? 3 : depth;

  gnode *nodes = NULL; int nn = 0, ncap = 0;
  cJSON *edges = cJSON_CreateArray();
  /* seen-edge keys "a|b|rel" */
  char **ek = NULL; int nek = 0, ekcap = 0;
  char **visited = NULL; int nv = 0, vcap = 0;
  char **frontier = NULL; int nf = 0, fcap = 0;

  #define PUSH(arr, cap, cnt, val) do { \
    if ((cnt) == (cap)) { (cap) = (cap) ? (cap)*2 : 16; \
      (arr) = realloc((arr), (cap)*sizeof(*(arr))); } \
    (arr)[(cnt)++] = (val); } while (0)

  gnode root;
  if (fetch_node(db, id, &root)) {
    if (nn == ncap) { ncap = 16; nodes = realloc(nodes, ncap*sizeof *nodes); }
    nodes[nn++] = root;
  }
  PUSH(visited, vcap, nv, strdup(id));
  PUSH(frontier, fcap, nf, strdup(id));

  for (int hop = 0; hop < d; hop++) {
    char **next = NULL; int nnext = 0, nextcap = 0;
    for (int fi = 0; fi < nf; fi++) {
      const char *cur = frontier[fi];
      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h,
            "SELECT src_entity_id,dst_entity_id,rel_type,weight "
            "FROM entity_relationships "
            "WHERE src_entity_id=?1 OR dst_entity_id=?1 "
            "ORDER BY weight DESC LIMIT 25", -1, &s, NULL) != SQLITE_OK)
        continue;
      sqlite3_bind_text(s, 1, cur, -1, SQLITE_TRANSIENT);
      while (sqlite3_step(s) == SQLITE_ROW) {
        const char *a = (const char *)sqlite3_column_text(s, 0);
        const char *b = (const char *)sqlite3_column_text(s, 1);
        const char *rt = (const char *)sqlite3_column_text(s, 2);
        double w = sqlite3_column_double(s, 3);
        const char *other = strcmp(a, cur) == 0 ? b : a;

        char key[600];
        snprintf(key, sizeof key, "%s|%s|%s", a, b, rt);
        int dup = 0;
        for (int i = 0; i < nek; i++) if (strcmp(ek[i], key) == 0) { dup = 1; break; }
        if (!dup) {
          PUSH(ek, ekcap, nek, strdup(key));
          cJSON *e = cJSON_CreateObject();
          cJSON_AddStringToObject(e, "source", a);
          cJSON_AddStringToObject(e, "target", b);
          cJSON_AddStringToObject(e, "relationship", rt);
          cJSON_AddNumberToObject(e, "weight", w);
          cJSON_AddItemToArray(edges, e);
        }
        if (gn_find(nodes, nn, other) < 0) {
          gnode g;
          if (fetch_node(db, other, &g)) {
            if (nn == ncap) { ncap = ncap ? ncap*2 : 16;
              nodes = realloc(nodes, ncap*sizeof *nodes); }
            nodes[nn++] = g;
          }
        }
        int seen = 0;
        for (int i = 0; i < nv; i++) if (strcmp(visited[i], other) == 0) { seen = 1; break; }
        if (!seen) {
          PUSH(visited, vcap, nv, strdup(other));
          PUSH(next, nextcap, nnext, strdup(other));
        }
      }
      sqlite3_finalize(s);
    }
    for (int i = 0; i < nf; i++) free(frontier[i]);
    free(frontier);
    frontier = next; nf = nnext;
    if (nf == 0) break;
  }
  for (int i = 0; i < nf; i++) free(frontier[i]);
  free(frontier);
  for (int i = 0; i < nek; i++) free(ek[i]); free(ek);
  for (int i = 0; i < nv; i++) free(visited[i]); free(visited);

  cJSON *jnodes = cJSON_CreateArray();
  for (int i = 0; i < nn; i++) {
    cJSON *e = cJSON_CreateObject();
    cJSON_AddStringToObject(e, "id", nodes[i].id);
    cJSON_AddStringToObject(e, "type", nodes[i].type);
    cJSON_AddStringToObject(e, "value", nodes[i].canon);
    cJSON_AddStringToObject(e, "label", nodes[i].canon);
    cJSON_AddNumberToObject(e, "mention_count", (double)nodes[i].mc);
    cJSON_AddItemToArray(jnodes, e);
    free(nodes[i].id); free(nodes[i].type); free(nodes[i].canon);
  }
  free(nodes);

  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "nodes", jnodes);
  cJSON_AddItemToObject(o, "edges", edges);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
  #undef PUSH
}

char *entityapi_mentions(db_handle *db, const char *type, const char *id,
                         int limit, int offset) {
  sqlite3_stmt *r = entity_by_id(db, id);
  if (!r) return NULL;
  const char *etype = (const char *)sqlite3_column_text(r, 1);
  if (!etype || strcmp(etype, type) != 0) { sqlite3_finalize(r); return NULL; }
  sqlite3_finalize(r);

  int lim = limit > 0 ? limit : 50;
  if (lim > 200) lim = 200;
  if (offset < 0) offset = 0;

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT m.item_uid,m.source_id,m.surface,m.field,m.confidence,"
        "m.extractor,m.created_at,i.title,i.summary,i.link,i.published_at,"
        "i.record_type FROM entity_mentions m "
        "LEFT JOIN intel_items i ON i.uid=m.item_uid "
        "WHERE m.entity_id=?1 ORDER BY m.created_at DESC LIMIT ?2 OFFSET ?3",
        -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, lim);
  sqlite3_bind_int(s, 3, offset);

  static const char *K[] = {"item_uid","source_id","surface","field",
    "confidence","extractor","created_at","title","summary","link",
    "published_at","record_type"};
  cJSON *arr = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *m = cJSON_CreateObject();
    for (int i = 0; i < 12; i++) {
      if (sqlite3_column_type(s, i) == SQLITE_NULL)
        cJSON_AddNullToObject(m, K[i]);
      else if (i == 4)   /* confidence REAL */
        cJSON_AddNumberToObject(m, K[i], sqlite3_column_double(s, i));
      else
        cJSON_AddStringToObject(m, K[i],
          (const char *)sqlite3_column_text(s, i));
    }
    cJSON_AddItemToArray(arr, m);
  }
  sqlite3_finalize(s);

  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "mentions", arr);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}
