#include "entityapi.h"
#include "entitystore.h"          /* ES_BREACH_TENANT + the scope migration */
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
static long count1_t(sqlite3 *h, const char *sql, const char *tenant) {
  sqlite3_stmt *s; long n = 0;
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  }
  sqlite3_finalize(s);
  return n;
}

/* ── tenant scoping ────────────────────────────────────────────────────────
 * This file previously had no tenant predicate at all: the only `tenant`
 * reference selected the column without ever filtering on it, and httpd.c
 * reached these routes without resolving a tenant. Siblings (casesapi.c,
 * aoiapi.c, exportapi.c) all filter; this is that same predicate.
 *
 * entities.tenant_id is NULLABLE and NULL for the shared, pre-tenancy graph,
 * so the predicate is exportapi.c's exact shape — "the shared graph, plus
 * anything privately mine" — not a bare equality, which would blank every
 * existing deployment's entity screens. intel_items.tenant_id is instead NOT
 * NULL DEFAULT 'legacy', so its counterpart admits 'legacy'.
 *
 * entity_mentions / entity_relationships / entity_extraction_state carry no
 * tenant column. They hang off `entities`, and every entry point below
 * resolves an entity FIRST, so a caller can only ever walk edges from a node
 * they are allowed to see. Their corpus-wide counters in stats() stay
 * corpus-wide, and are labelled as such rather than being filtered by a
 * column that does not exist.
 *
 * ── breach scope ───────────────────────────────────────────────────────────
 * The tenant predicate above was NOT enough, and the way it failed is the
 * reason the fix below lives where it does. breach_index.c materializes every
 * breached identifier into THIS graph — an entity whose canonical is the
 * cleartext identifier, and a mention whose `surface` is that identifier and
 * whose `source_id` is the breach it came out of. Those rows were written with
 * tenant_id NULL, so "tenant_id IS NULL OR tenant_id = :me" matched all of
 * them and any authenticated viewer of any tenant could:
 *   - GET /api/entities/search?type=email  -> cleartext breached addresses;
 *   - GET /api/entities/:t/:id/breaches    -> WHERE extractor='breach-ingest',
 *     i.e. breach rows exclusively, plus the "breach:<keyid>" uid;
 *   - GET /api/entities/:t/:id/mentions    -> m.surface (the identifier) and
 *     m.source_id (which breach).
 * All of that is the corpus httpd.c's breach_gate() wraps at four other doors.
 * A fifth and sixth door appeared here because the gate is on ROUTES while the
 * data sits in a table everyone may read.
 *
 * So the rows are now stamped ES_BREACH_TENANT at ingest (entitystore.h has
 * the full rationale) and simply do not satisfy the disjunct any more. What
 * this file adds is the operator's way BACK IN: an `is_operator` flag that
 * binds the sentinel as an extra disjunct. The default — every existing
 * caller — is 0, i.e. fail closed.
 *
 * entityapi's functions receive no caller identity, only a resolved tenant id,
 * so the flag is threaded from core/httpd.c, which evaluates opgate_check()
 * once for the whole tenant-resolved entity subtree and passes it to each
 * *_scoped() entry point. The un-suffixed names remain as fail-closed wrappers
 * (is_operator = 0) so any future caller that forgets the flag gets the
 * viewer's view rather than the operator's. */

/* A resolved tenant id can never legitimately equal the sentinel, but if one
 * ever did, "tenant_id = :me" would re-open the whole hole through the
 * ordinary disjunct. Fold it to "no tenant" instead of trusting the caller. */
static const char *vis_tenant(const char *t) {
  if (!t) return "";
  return strcmp(t, ES_BREACH_TENANT) == 0 ? "" : t;
}

/* Bind the breach-scope disjunct: the sentinel for a platform operator, SQL
 * NULL for everyone else — `tenant_id = NULL` is never true, so the parameter
 * is inert rather than "empty string", which a tenant_id could match. */
static void bind_scope(sqlite3_stmt *s, int idx, int is_operator) {
  if (is_operator) sqlite3_bind_text(s, idx, ES_BREACH_TENANT, -1, SQLITE_STATIC);
  else             sqlite3_bind_null(s, idx);
}

/* count1_t with the breach-scope disjunct bound at ?2. */
static long count1_ts(sqlite3 *h, const char *sql, const char *tenant,
                      int is_operator) {
  sqlite3_stmt *s; long n = 0;
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
    bind_scope(s, 2, is_operator);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  }
  sqlite3_finalize(s);
  return n;
}

char *entityapi_stats(db_handle *db, const char *tenant) {
  return entityapi_stats_scoped(db, tenant, 0);
}

char *entityapi_stats_scoped(db_handle *db, const char *tenant,
                             int is_operator) {
  es_breach_scope_migrate(db);
  tenant = vis_tenant(tenant);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "intel_items",
    (double)count1_t(db->h, "SELECT COUNT(*) FROM intel_items "
                            "WHERE tenant_id='legacy' OR tenant_id=?1", tenant));
  /* Extraction-pipeline health is a platform-wide property of the corpus, not
   * a per-tenant number; there is no tenant column to filter it by and
   * inventing one here would report a plausible-looking wrong figure. */
  cJSON_AddNumberToObject(o, "extracted", (double)count1(db->h,
    "SELECT COUNT(*) FROM entity_extraction_state WHERE extracted_at != ''"));
  cJSON_AddNumberToObject(o, "failed", (double)count1(db->h,
    "SELECT COUNT(*) FROM entity_extraction_state WHERE failed_count >= 5"));
  cJSON_AddNumberToObject(o, "entities",
    (double)count1_ts(db->h, "SELECT COUNT(*) FROM entities WHERE"
                             " (entities.tenant_id IS NULL"
                             "  OR entities.tenant_id = ?1"
                             "  OR entities.tenant_id = ?2)",
                      tenant, is_operator));
  /* Still corpus-wide across tenants (no tenant column), but the breach scope
   * is excluded: a bulk breach ingest writes one mention per identifier per
   * breach, so a raw total is a live readout of how much breach material the
   * platform holds — the same figure the /api/breach routes are gated to
   * protect. */
  cJSON_AddNumberToObject(o, "mentions",
    (double)count1_ts(db->h, "SELECT COUNT(*) FROM entity_mentions"
                             " WHERE tenant_id IS NOT '" ES_BREACH_TENANT "'"
                             "    OR tenant_id = ?2",
                      tenant, is_operator));
  cJSON_AddNumberToObject(o, "relationships",
    (double)count1(db->h, "SELECT COUNT(*) FROM entity_relationships"));
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* searchEntities({q,type,limit}) — entityMirror.search: pre-segment q like
 * the write path (fts_segment == jpTokenizer.segmentForFts), MATCH joined
 * back to `entities` via entities_fts.uid, mention_count DESC. */
char *entityapi_search(db_handle *db, const char *q, const char *type, int limit,
                       const char *tenant) {
  return entityapi_search_scoped(db, q, type, limit, tenant, 0);
}

/* Operator-only exact-identifier probe over the quarantined nodes.
 *
 * Quarantined entities are deliberately absent from entities_fts (see
 * entitystore.h), so the MATCH above can never return them — for an operator
 * either. That is the right trade for a search index that has no tenant column
 * and hundreds of millions of breach rows to avoid, but it would remove the
 * operator's only way to get from an identifier to an entity_id, i.e. it would
 * delete the breach pivot rather than gate it. This restores it in the shape
 * the pivot actually needs: an EXACT identifier lookup on the unique
 * idx_entities_normkey(type, norm_key), never a substring or prefix scan, so
 * it can neither enumerate the corpus nor cost more than a few seeks.
 *
 * Two probes per type: es_norm_key() collapses whitespace but does NOT
 * lowercase, while breach_index.c lowercases emails and usernames before
 * storing them, so the operator's typed "Foo@x.com" has to be tried folded as
 * well or the pivot misses its own rows. */
static void search_breach_exact(db_handle *db, const char *q, const char *type,
                                int lim, cJSON *results) {
  static const char *BT[] = { "email", "username", "phone" };
  char nk[512]; es_norm_key(q ? q : "", nk, sizeof nk);
  if (!nk[0]) return;
  char lc[512]; snprintf(lc, sizeof lc, "%s", nk);
  for (char *p = lc; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;

  char tl[128] = {0};
  if (type) {
    snprintf(tl, sizeof tl, "%s", type);
    for (char *p = tl; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
  }
  for (unsigned i = 0; i < sizeof BT / sizeof BT[0]; i++) {
    if (tl[0] && strcmp(tl, BT[i]) != 0) continue;
    if (cJSON_GetArraySize(results) >= lim) break;
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
          "SELECT entity_id,type,canonical,mention_count,last_seen_at"
          " FROM entities"
          " WHERE type=?1 AND (norm_key=?2 OR norm_key=?3)"
          "   AND tenant_id='" ES_BREACH_TENANT "'", -1, &s, NULL) != SQLITE_OK)
      continue;
    sqlite3_bind_text(s, 1, BT[i], -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, nk, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, lc, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW &&
           cJSON_GetArraySize(results) < lim) {
      cJSON *r = cJSON_CreateObject();
      cJSON_AddStringToObject(r, "entity_id", (const char *)sqlite3_column_text(s,0));
      cJSON_AddStringToObject(r, "type",      (const char *)sqlite3_column_text(s,1));
      cJSON_AddStringToObject(r, "value",     (const char *)sqlite3_column_text(s,2));
      cJSON_AddNumberToObject(r, "mention_count",
                              (double)sqlite3_column_int64(s, 3));
      add_str_or_null(r, "last_seen_at", ctext(s, 4));
      add_str_or_null(r, "excerpt", NULL);
      /* Labelled, because it is not the same claim as an FTS hit: this node
       * exists only because it appeared in the breach corpus. */
      cJSON_AddStringToObject(r, "scope", "breach");
      cJSON_AddItemToArray(results, r);
    }
    sqlite3_finalize(s);
  }
}

char *entityapi_search_scoped(db_handle *db, const char *q, const char *type,
                              int limit, const char *tenant, int is_operator) {
  es_breach_scope_migrate(db);
  tenant = vis_tenant(tenant);
  cJSON *results = cJSON_CreateArray();
  /* q already trimmed/non-empty by caller.
   *
   * fts_query_expr(), not fts_segment(): this is typed end-user input and
   * fts_segment is a TOKENIZER, so its Latin output went to the FTS5
   * expression parser verbatim. Verified against FTS5: `cam-tabi` steps with
   * "no such column: tabi", a lone `"` with "unterminated string" and `tok(yo`
   * with a syntax error — each of which this loop reads as zero rows, so the
   * caller got a cheerful empty result set for an ordinary hyphenated query.
   * intelapi.c and exportapi.c were converted to fts_query_expr for exactly
   * this; entity search is the sibling that was left behind. NULL means "no
   * usable token", which is the same {"results":[]} the caller already emits
   * for an empty q. */
  char *segq = fts_query_expr(q);              /* malloc'd, or NULL */
  int lim = limit > 0 ? limit : 30;
  if (lim > 100) lim = 100;
  if (!segq) {
    /* No usable FTS token — but the operator's exact probe does not go through
     * FTS at all, so an identifier that tokenizes to nothing must still pivot.
     * (This branch used to print the bare array, i.e. `[]`, where every other
     * exit from this function — and httpd.c's empty-q shortcut — emits
     * {"results":[]}. It now has rows to carry, so it uses the documented
     * envelope like the rest.) */
    if (is_operator) search_breach_exact(db, q, type, lim, results);
    cJSON *oe = cJSON_CreateObject();
    cJSON_AddItemToObject(oe, "results", results);
    char *empty = cJSON_PrintUnformatted(oe);
    cJSON_Delete(oe);
    return empty;
  }

  /* The added disjunct is the ONLY way a breach-scoped node can come back out
   * of this query, and it is bound to SQL NULL for everyone but an operator.
   * (Belt and braces: those nodes are also absent from entities_fts, so the
   * JOIN cannot produce them regardless.) */
  const char *sql = type
    ? "SELECT entities.*, snippet(entities_fts,-1,'<mark>','</mark>','…',12) "
      "FROM entities_fts JOIN entities ON entities.entity_id=entities_fts.uid "
      "WHERE entities_fts MATCH ?1 AND entities.type=?2 "
      "AND (entities.tenant_id IS NULL OR entities.tenant_id=?4 "
      "     OR entities.tenant_id=?5) "
      "ORDER BY entities.mention_count DESC LIMIT ?3"
    : "SELECT entities.*, snippet(entities_fts,-1,'<mark>','</mark>','…',12) "
      "FROM entities_fts JOIN entities ON entities.entity_id=entities_fts.uid "
      "WHERE entities_fts MATCH ?1 "
      "AND (entities.tenant_id IS NULL OR entities.tenant_id=?3 "
      "     OR entities.tenant_id=?4) "
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
    sqlite3_bind_text(s, 4, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
    bind_scope(s, 5, is_operator);
  } else {
    sqlite3_bind_int(s, 2, lim);
    sqlite3_bind_text(s, 3, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
    bind_scope(s, 4, is_operator);
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

  if (is_operator) search_breach_exact(db, q, type, lim, results);

  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "results", results);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* getEntity(id) → row, with the entities.js type-guard. Caller frees ret.
 * Every /api/entities/:type/:id route enters through here, so this is the one
 * place the tenant predicate has to hold for the whole subtree. */
static sqlite3_stmt *entity_by_id(db_handle *db, const char *id,
                                  const char *tenant, int is_operator) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT entity_id,type,canonical,norm_key,name_ja,name_romaji,"
        "aliases_json,properties,mention_count,first_seen_at,last_seen_at,"
        "tenant_id FROM entities WHERE entity_id=?1 "
        "AND (tenant_id IS NULL OR tenant_id=?2 OR tenant_id=?3)",
        -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
  bind_scope(s, 3, is_operator);
  if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return NULL; }
  return s;                                      /* positioned on the row */
}

/* ── Breach exposure (roadmap item 23) ──────────────────────────────────────
 * The ingest-side link already exists: breach_index.c upserts an entity and a
 * mention (extractor='breach-ingest', item_uid "breach:<keyid>") for every
 * breach identifier, and es_upsert_entity dedups on (type, norm_key) — so a
 * breach email and an intel-mentioned email are ALREADY the same node. The
 * only thing missing was the reverse read: entity → the breaches it appears
 * in. That is what these two helpers add; no new tables, no new ingest.
 *
 * entity_mentions.source_id IS the breach slug, which is also breach_meta's
 * primary key, so the join is direct. LEFT JOIN because a corpus can be
 * ingested before its catalog manifest is loaded — an unnamed breach must
 * still be reported, not silently dropped. */

/* Append `s` to `arr` if not already present (data-class union). */
static void arr_add_unique(cJSON *arr, const char *s) {
  if (!s || !*s) return;
  cJSON *it;
  cJSON_ArrayForEach(it, arr)
    if (cJSON_IsString(it) && strcmp(it->valuestring, s) == 0) return;
  cJSON_AddItemToArray(arr, cJSON_CreateString(s));
}

/* {breach_count,first_breach_date,last_breach_date,data_classes[]} — attached
 * to the entity profile so exposure is visible without a second round-trip.
 *
 * PLATFORM-OPERATOR ONLY; every caller must check first. It reads exclusively
 * breach-ingest mentions, so its output — even the bare breach_count — is a
 * live oracle over the gated corpus: "is this address in a breach, in how many,
 * since when, and what leaked". Handing a viewer a zero-vs-nonzero answer is
 * the same disclosure as handing them the rows. */
static cJSON *exposure_summary(db_handle *db, const char *entity_id) {
  cJSON *o = cJSON_CreateObject();
  long n = 0;
  const char *first = NULL, *last = NULL;
  char fbuf[64] = {0}, lbuf[64] = {0};
  cJSON *classes = cJSON_CreateArray();

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT m.source_id,bm.breach_date,bm.data_classes_json "
        "FROM entity_mentions m "
        "LEFT JOIN breach_meta bm ON bm.breach_id=m.source_id "
        "WHERE m.entity_id=?1 AND m.extractor='breach-ingest' "
        "GROUP BY m.source_id", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, entity_id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW) {
      n++;
      const char *bd = ctext(s, 1);
      if (bd && *bd) {
        if (!first || strcmp(bd, fbuf) < 0) { snprintf(fbuf,sizeof fbuf,"%s",bd); first = fbuf; }
        if (!last  || strcmp(bd, lbuf) > 0) { snprintf(lbuf,sizeof lbuf,"%s",bd); last  = lbuf; }
      }
      const char *dc = ctext(s, 2);
      if (dc && *dc) {
        cJSON *a = cJSON_Parse(dc);
        if (a && cJSON_IsArray(a)) {
          cJSON *it;
          cJSON_ArrayForEach(it, a)
            if (cJSON_IsString(it)) arr_add_unique(classes, it->valuestring);
        }
        cJSON_Delete(a);
      }
    }
    sqlite3_finalize(s);
  }

  cJSON_AddNumberToObject(o, "breach_count", (double)n);
  add_str_or_null(o, "first_breach_date", first);
  add_str_or_null(o, "last_breach_date", last);
  cJSON_AddItemToObject(o, "data_classes", classes);
  return o;
}

/* GET /api/entities/:type/:id/breaches — the breaches this entity appears in.
 * NULL if the entity is missing or the type does not match (caller → 404). */
char *entityapi_breaches(db_handle *db, const char *type, const char *id,
                         int limit, int offset, const char *tenant) {
  return entityapi_breaches_scoped(db, type, id, limit, offset, tenant, 0);
}

char *entityapi_breaches_scoped(db_handle *db, const char *type, const char *id,
                                int limit, int offset, const char *tenant,
                                int is_operator) {
  es_breach_scope_migrate(db);
  tenant = vis_tenant(tenant);
  /* DENY for a non-operator — not an empty list.
   *
   * Every row this endpoint can return is breach-derived by construction
   * (WHERE m.extractor='breach-ingest'), so there is no partial view to serve.
   * The alternative, "return {data:[],exposure:{breach_count:0}}", is worse
   * than a refusal in two ways. It is indistinguishable from the truthful
   * answer "this identifier appears in no breach", which is a false negative an
   * analyst would act on — and house rule 1 says a failure degrades to an
   * explicit error, never to content that reads as a finding. And it is not
   * even safe: if clean entities answered 0 while breached ones refused, the
   * difference between the two responses IS the exposure oracle. So the denial
   * is uniform across every entity, breached or not.
   *
   * core/httpd.c's /breaches branch replies 403 for a non-operator before it
   * ever reaches this function, matching breach_gate's other four doors. This
   * NULL is the belt to that braces: a caller that forgets the check gets a
   * denial rather than data. */
  if (!is_operator) return NULL;
  sqlite3_stmt *chk = entity_by_id(db, id, tenant, is_operator);
  if (!chk) return NULL;
  const char *etype = (const char *)sqlite3_column_text(chk, 1);
  if (!etype || strcmp(etype, type) != 0) { sqlite3_finalize(chk); return NULL; }
  sqlite3_finalize(chk);

  int lim = limit > 0 ? limit : 50;
  if (lim > 200) lim = 200;
  int off = offset > 0 ? offset : 0;

  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT m.source_id,m.item_uid,bm.name,bm.title,bm.domain,"
        "bm.breach_date,bm.pwn_count,bm.data_classes_json,bm.verified,"
        "bm.sensitive "
        "FROM entity_mentions m "
        "LEFT JOIN breach_meta bm ON bm.breach_id=m.source_id "
        "WHERE m.entity_id=?1 AND m.extractor='breach-ingest' "
        "GROUP BY m.source_id "
        "ORDER BY COALESCE(bm.breach_date,'') DESC, m.source_id ASC "
        "LIMIT ?2 OFFSET ?3", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 2, lim);
    sqlite3_bind_int(s, 3, off);
    while (sqlite3_step(s) == SQLITE_ROW) {
      cJSON *b = cJSON_CreateObject();
      cJSON_AddStringToObject(b, "breach_id", (const char *)sqlite3_column_text(s,0));
      /* The synthetic intel uid, so the client can open the breach record
       * through the normal intel detail route (breach_adapter serves it). */
      add_str_or_null(b, "item_uid", ctext(s,1));
      add_str_or_null(b, "name", ctext(s,2));
      add_str_or_null(b, "title", ctext(s,3));
      add_str_or_null(b, "domain", ctext(s,4));
      add_str_or_null(b, "breach_date", ctext(s,5));
      cJSON_AddItemToObject(b, "pwn_count",
        sqlite3_column_type(s,6)==SQLITE_NULL ? cJSON_CreateNull()
          : cJSON_CreateNumber((double)sqlite3_column_int64(s,6)));
      cJSON *dc = NULL;
      const char *dcs = ctext(s,7);
      if (dcs && *dcs) dc = cJSON_Parse(dcs);
      cJSON_AddItemToObject(b, "data_classes",
        (dc && cJSON_IsArray(dc)) ? dc : (cJSON_Delete(dc), cJSON_CreateArray()));
      cJSON_AddItemToObject(b, "verified",
        sqlite3_column_type(s,8)==SQLITE_NULL ? cJSON_CreateNull()
          : cJSON_CreateBool(sqlite3_column_int(s,8) != 0));
      cJSON_AddItemToObject(b, "sensitive",
        sqlite3_column_type(s,9)==SQLITE_NULL ? cJSON_CreateNull()
          : cJSON_CreateBool(sqlite3_column_int(s,9) != 0));
      cJSON_AddItemToArray(arr, b);
    }
    sqlite3_finalize(s);
  }

  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "data", arr);
  cJSON_AddItemToObject(o, "exposure", exposure_summary(db, id));
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

char *entityapi_get(db_handle *db, const char *type, const char *id,
                    const char *tenant) {
  return entityapi_get_scoped(db, type, id, tenant, 0);
}

char *entityapi_get_scoped(db_handle *db, const char *type, const char *id,
                           const char *tenant, int is_operator) {
  es_breach_scope_migrate(db);
  tenant = vis_tenant(tenant);
  sqlite3_stmt *s = entity_by_id(db, id, tenant, is_operator);
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
  /* Exposure rides on the profile so the client renders "seen in N breaches"
   * without a second round-trip; the full list stays behind /breaches.
   *
   * For a non-operator the block is null and SAYS SO, rather than being a
   * zeroed summary. The count is the disclosure (see exposure_summary), and a
   * silent {breach_count:0} would be a fabricated finding — an analyst reading
   * "not in any breach" off a field that was withheld, not measured. No count
   * of what was withheld either: that count is exactly the secret. */
  if (is_operator) {
    cJSON_AddItemToObject(o, "exposure", exposure_summary(db, id));
  } else {
    cJSON_AddNullToObject(o, "exposure");
    cJSON_AddStringToObject(o, "exposure_withheld", "platform_operator_required");
  }
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
static int fetch_node(db_handle *db, const char *id, const char *tenant,
                      int is_operator, gnode *out) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT entity_id,type,canonical,mention_count FROM entities "
        "WHERE entity_id=?1 "
        "AND (tenant_id IS NULL OR tenant_id=?2 OR tenant_id=?3)",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
  bind_scope(s, 3, is_operator);
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

/* Is rel_type in the caller's comma-separated allowlist? NULL/empty = allow
 * all. Lets the graph canvas separate analyst-asserted edges from statistical
 * co-mention noise — they are not the same claim and should not be one blob. */
static int rel_allowed(const char *csv, const char *rt) {
  if (!csv || !*csv) return 1;
  if (!rt) return 0;
  size_t rl = strlen(rt);
  const char *p = csv;
  while (*p) {
    while (*p == ',' || *p == ' ') p++;
    const char *q = p;
    while (*q && *q != ',') q++;
    size_t n = (size_t)(q - p);
    while (n && p[n-1] == ' ') n--;
    if (n == rl && strncmp(p, rt, rl) == 0) return 1;
    p = q;
  }
  return 0;
}

/* Relationship degree of an entity (indexed by idx_er_src / idx_er_dst).
 * Used for the hub guard: a prefecture co-occurs with everything, so
 * expanding it turns a 2-hop ego-network into the whole graph. */
static long entity_degree(db_handle *db, const char *eid) {
  sqlite3_stmt *s; long n = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT (SELECT COUNT(*) FROM entity_relationships WHERE src_entity_id=?1)"
        "     + (SELECT COUNT(*) FROM entity_relationships WHERE dst_entity_id=?1)",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, eid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  }
  sqlite3_finalize(s);
  return n;
}

char *entityapi_graph(db_handle *db, const char *type, const char *id, int depth,
                      const char *rel_types, int exclude_hubs, int max_nodes,
                      const char *tenant) {
  return entityapi_graph_scoped(db, type, id, depth, rel_types, exclude_hubs,
                                max_nodes, tenant, 0);
}

char *entityapi_graph_scoped(db_handle *db, const char *type, const char *id,
                             int depth, const char *rel_types, int exclude_hubs,
                             int max_nodes, const char *tenant,
                             int is_operator) {
  es_breach_scope_migrate(db);
  tenant = vis_tenant(tenant);
  sqlite3_stmt *r = entity_by_id(db, id, tenant, is_operator);
  if (!r) return NULL;
  const char *etype = (const char *)sqlite3_column_text(r, 1);
  if (!etype || strcmp(etype, type) != 0) { sqlite3_finalize(r); return NULL; }
  sqlite3_finalize(r);

  int d = depth < 1 ? 1 : depth > 3 ? 3 : depth;
  int cap_nodes = max_nodes > 0 ? max_nodes : 300;
  if (cap_nodes > 2000) cap_nodes = 2000;
  long dropped_hub = 0, dropped_cap = 0;

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
  if (fetch_node(db, id, tenant, is_operator, &root)) {
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
            /* pmi/lift/co_count are computed by the correlation pod
             * (core/entity_stats.c) and stored on this row. They used to be
             * selected by nobody, so the scores were calculated, persisted and
             * then dropped at this seam — the graph canvas gates both its edge
             * labels and its edge thickness on `lift`, so it always rendered
             * the unscored fallback. Serialising them is the collection half of
             * house rule 2: if we computed it, we hand it over. */
            "SELECT src_entity_id,dst_entity_id,rel_type,weight,"
            "       pmi,lift,co_count "
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
        /* NULL until the pod has scored this edge. Distinguish "not scored
         * yet" from "scored zero" — a fabricated 0.0 would read as
         * "these two never co-occur", which is a different claim. */
        int has_pmi  = sqlite3_column_type(s, 4) != SQLITE_NULL;
        int has_lift = sqlite3_column_type(s, 5) != SQLITE_NULL;
        int has_co   = sqlite3_column_type(s, 6) != SQLITE_NULL;
        double pmi   = sqlite3_column_double(s, 4);
        double lift  = sqlite3_column_double(s, 5);
        long long co = sqlite3_column_int64(s, 6);
        const char *other = strcmp(a, cur) == 0 ? b : a;
        if (!rel_allowed(rel_types, rt)) continue;
        /* entity_relationships carries no tenant column, so visibility of the
         * far endpoint is what bounds the walk. Skipping the EDGE too, not
         * just the node, matters: an edge whose target is invisible would
         * still disclose that entity's id to the canvas. */
        { gnode probe;
          if (gn_find(nodes, nn, other) < 0) {
            if (!fetch_node(db, other, tenant, is_operator, &probe)) continue;
            free(probe.id); free(probe.type); free(probe.canon);
          } }

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
          if (has_pmi)  cJSON_AddNumberToObject(e, "pmi",  pmi);
          else          cJSON_AddNullToObject(e, "pmi");
          if (has_lift) cJSON_AddNumberToObject(e, "lift", lift);
          else          cJSON_AddNullToObject(e, "lift");
          if (has_co)   cJSON_AddNumberToObject(e, "co_count", (double)co);
          else          cJSON_AddNullToObject(e, "co_count");
          cJSON_AddItemToArray(edges, e);
        }
        if (gn_find(nodes, nn, other) < 0) {
          if (nn >= cap_nodes) {
            dropped_cap++;              /* counted, never silently discarded */
          } else {
            gnode g;
            if (fetch_node(db, other, tenant, is_operator, &g)) {
              if (nn == ncap) { ncap = ncap ? ncap*2 : 16;
                nodes = realloc(nodes, ncap*sizeof *nodes); }
              nodes[nn++] = g;
            }
          }
        }
        int seen = 0;
        for (int i = 0; i < nv; i++) if (strcmp(visited[i], other) == 0) { seen = 1; break; }
        if (!seen) {
          /* A hub is shown as a node but never expanded — the edge that found
           * it is real intel; its other 4,000 edges are not. */
          int is_hub = exclude_hubs > 0 &&
                       entity_degree(db, other) > (long)exclude_hubs;
          PUSH(visited, vcap, nv, strdup(other));
          if (is_hub) dropped_hub++;
          else if (nn < cap_nodes) PUSH(next, nextcap, nnext, strdup(other));
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
  /* Truncation is reported, not hidden. A canvas that silently drops half the
   * graph teaches an analyst to trust a picture that is lying to them; the
   * client renders this as "showing N — M hubs collapsed, K nodes over cap". */
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddNumberToObject(meta, "node_count", (double)nn);
  cJSON_AddNumberToObject(meta, "max_nodes", (double)cap_nodes);
  cJSON_AddNumberToObject(meta, "hubs_collapsed", (double)dropped_hub);
  cJSON_AddNumberToObject(meta, "nodes_over_cap", (double)dropped_cap);
  cJSON_AddBoolToObject(meta, "truncated", dropped_cap > 0 || dropped_hub > 0);
  add_str_or_null(meta, "rel_types", (rel_types && *rel_types) ? rel_types : NULL);
  cJSON_AddItemToObject(o, "meta", meta);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
  #undef PUSH
}

char *entityapi_mentions(db_handle *db, const char *type, const char *id,
                         int limit, int offset, const char *tenant) {
  return entityapi_mentions_scoped(db, type, id, limit, offset, tenant, 0);
}

char *entityapi_mentions_scoped(db_handle *db, const char *type, const char *id,
                                int limit, int offset, const char *tenant,
                                int is_operator) {
  es_breach_scope_migrate(db);
  tenant = vis_tenant(tenant);
  sqlite3_stmt *r = entity_by_id(db, id, tenant, is_operator);
  if (!r) return NULL;
  const char *etype = (const char *)sqlite3_column_text(r, 1);
  if (!etype || strcmp(etype, type) != 0) { sqlite3_finalize(r); return NULL; }
  sqlite3_finalize(r);

  int lim = limit > 0 ? limit : 50;
  if (lim > 200) lim = 200;
  if (offset < 0) offset = 0;

  /* The joined side needs its OWN predicate. "resolve the entity first" bounds
   * which NODE you may walk from, and that is enough for the graph — but this
   * query returns intel_items columns, and intel_items IS tenant-owned. Almost
   * every entity is shared (tenant_id NULL, visible to everyone), so without
   * this a member of tenant B reading the mentions of a shared node received
   * the titles, summaries and links of tenant A's private items that happen to
   * mention it. Predicate is intel_items' own form — NOT NULL DEFAULT 'legacy',
   * so the shared corpus is the literal 'legacy' (exportapi.c:461,
   * nearapi.c:158) — not the nullable-column form used for `entities`.
   *
   * `i.uid IS NULL` is kept deliberately: the join is a LEFT JOIN because a
   * mention can point at a synthetic uid with no intel_items row (breach
   * records), and those rows carry no tenant-owned content to leak.
   *
   * ...which was true of the intel_items columns and false of the mention row
   * itself, and that sentence is how the breach corpus walked out of here. A
   * breach mention has no intel_items row, so `i.uid IS NULL` waved it through
   * — and m.surface IS the cleartext breached identifier while m.source_id IS
   * the breach it came from. The mention now carries its own scope stamp, so
   * the predicate on the mention side is the one that matters: everything
   * except the breach scope, plus the breach scope only for an operator.
   * `IS NOT` (not `<>`) because the column is NULL for every ordinary row and
   * `NULL <> 'x'` is NULL, i.e. false — that spelling would have hidden the
   * entire graph. */
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT m.item_uid,m.source_id,m.surface,m.field,m.confidence,"
        "m.extractor,m.created_at,i.title,i.summary,i.link,i.published_at,"
        "i.record_type FROM entity_mentions m "
        "LEFT JOIN intel_items i ON i.uid=m.item_uid "
        "WHERE m.entity_id=?1 "
        "AND (m.tenant_id IS NOT '" ES_BREACH_TENANT "' OR m.tenant_id=?5) "
        "AND (i.uid IS NULL OR i.tenant_id='legacy' OR i.tenant_id=?4) "
        "ORDER BY m.created_at DESC LIMIT ?2 OFFSET ?3",
        -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, lim);
  sqlite3_bind_int(s, 3, offset);
  sqlite3_bind_text(s, 4, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
  bind_scope(s, 5, is_operator);

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
  /* House rule 2: a bounded view says in-band that it is bounded. What it
   * deliberately does NOT say is how many rows were withheld — that number is
   * the count of breaches this identifier appears in, which is the secret
   * itself. "Filtered, and here is the role that unfilters it" is the honest
   * maximum. */
  cJSON *scope = cJSON_CreateObject();
  cJSON_AddBoolToObject(scope, "breach_mentions_included", is_operator ? 1 : 0);
  add_str_or_null(scope, "withheld_reason",
                  is_operator ? NULL : "platform_operator_required");
  cJSON_AddItemToObject(o, "scope", scope);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* ── entity_merges: why two nodes were (or were not) collapsed ─────────────
 * es_record_merge() has been writing a decision row — same, confidence, reason,
 * decided_at — for every candidate pair the resolver judged, and the ONLY
 * reader was es_merge_pair_exists(), which uses it as a memo so the same pair
 * is not re-judged. So the graph acted on these decisions and no analyst could
 * ever see one: "why is this company the same node as that one" had no answer
 * in the product.
 *
 * The breach quarantine applies here exactly as it does to mentions. A merge
 * row names a COUNTERPART entity, and entity_by_id's predicate only bounds the
 * node you entered from — so without a predicate on the other side, a merge
 * decision would hand a viewer the entity_id (and, joined, the canonical
 * value) of a quarantined breach node. The counterpart is therefore joined
 * against `entities` under the same disjunct every other read here uses, and a
 * row whose counterpart is not visible is not returned. Like
 * entityapi_mentions_scoped, the response says the view is filtered and does
 * NOT publish a withheld count — that count is the fact being protected. */
static char *merges_body(db_handle *db, const char *id, const char *tenant,
                         int is_operator, int limit, int offset) {
  static const char *WHERE_VIS =
    " FROM entity_merges m JOIN entities o ON o.entity_id="
    "  (CASE WHEN m.entity_a=?1 THEN m.entity_b ELSE m.entity_a END)"
    " WHERE (m.entity_a=?1 OR m.entity_b=?1)"
    "   AND (o.tenant_id IS NULL OR o.tenant_id=?2 OR o.tenant_id=?3)";

  /* Visible total first, so the bounded view can say how many of how many it
   * is showing (house rule 2) instead of ending at an unexplained page edge. */
  long total = 0;
  {
    char csql[512];
    snprintf(csql, sizeof csql, "SELECT COUNT(*)%s", WHERE_VIS);
    sqlite3_stmt *cs;
    if (sqlite3_prepare_v2(db->h, csql, -1, &cs, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(cs, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(cs, 2, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
    bind_scope(cs, 3, is_operator);
    if (sqlite3_step(cs) == SQLITE_ROW) total = sqlite3_column_int64(cs, 0);
    sqlite3_finalize(cs);
  }

  char sql[768];
  snprintf(sql, sizeof sql,
    "SELECT m.entity_a,m.entity_b,m.same,m.confidence,m.reason,m.decided_at,"
    "o.entity_id,o.type,o.canonical,o.mention_count%s"
    " ORDER BY m.decided_at DESC, o.entity_id ASC LIMIT ?4 OFFSET ?5", WHERE_VIS);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
  bind_scope(s, 3, is_operator);
  sqlite3_bind_int (s, 4, limit);
  sqlite3_bind_int (s, 5, offset);

  cJSON *arr = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *m = cJSON_CreateObject();
    const char *a = (const char *)sqlite3_column_text(s, 0);
    const char *b = (const char *)sqlite3_column_text(s, 1);
    cJSON_AddStringToObject(m, "entity_a", a ? a : "");
    cJSON_AddStringToObject(m, "entity_b", b ? b : "");
    /* The decision, stated as the resolver stated it: `same` is a verdict in
     * BOTH directions. A row with same=false is a recorded "these are NOT the
     * same", which is exactly as useful to an analyst as a merge and would be
     * lost if this only returned the positives. */
    cJSON_AddBoolToObject(m, "same", sqlite3_column_int(s, 2) != 0);
    cJSON_AddNumberToObject(m, "confidence", sqlite3_column_double(s, 3));
    add_str_or_null(m, "reason", ctext(s, 4));
    add_str_or_null(m, "decided_at", ctext(s, 5));
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "entity_id", (const char *)sqlite3_column_text(s, 6));
    cJSON_AddStringToObject(o, "type", (const char *)sqlite3_column_text(s, 7));
    cJSON_AddStringToObject(o, "value", (const char *)sqlite3_column_text(s, 8));
    cJSON_AddNumberToObject(o, "mention_count", (double)sqlite3_column_int64(s, 9));
    cJSON_AddItemToObject(m, "counterpart", o);
    cJSON_AddItemToArray(arr, m);
  }
  sqlite3_finalize(s);

  int shown = cJSON_GetArraySize(arr);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "merges", arr);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddNumberToObject(meta, "returned", shown);
  cJSON_AddNumberToObject(meta, "total", (double)total);
  cJSON_AddNumberToObject(meta, "limit", limit);
  cJSON_AddNumberToObject(meta, "offset", offset);
  cJSON_AddBoolToObject(meta, "has_more", offset + shown < total);
  cJSON_AddItemToObject(root, "meta", meta);
  cJSON *scope = cJSON_CreateObject();
  cJSON_AddBoolToObject(scope, "breach_counterparts_included", is_operator ? 1 : 0);
  add_str_or_null(scope, "withheld_reason",
                  is_operator ? NULL : "platform_operator_required");
  cJSON_AddItemToObject(root, "scope", scope);
  char *js = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return js;
}

char *entityapi_merges(db_handle *db, const char *type, const char *id,
                       int limit, int offset, const char *tenant) {
  return entityapi_merges_scoped(db, type, id, limit, offset, tenant, 0);
}

char *entityapi_merges_scoped(db_handle *db, const char *type, const char *id,
                              int limit, int offset, const char *tenant,
                              int is_operator) {
  es_breach_scope_migrate(db);
  tenant = vis_tenant(tenant);
  sqlite3_stmt *r = entity_by_id(db, id, tenant, is_operator);
  if (!r) return NULL;
  const char *etype = (const char *)sqlite3_column_text(r, 1);
  if (!etype || strcmp(etype, type) != 0) { sqlite3_finalize(r); return NULL; }
  sqlite3_finalize(r);

  int lim = limit > 0 ? limit : 100;
  if (lim > 500) lim = 500;
  if (offset < 0) offset = 0;
  return merges_body(db, id, tenant, is_operator, lim, offset);
}

char *entityapi_item_entities(db_handle *db, const char *uid) {
  return entityapi_item_entities_scoped(db, uid, 0);
}

char *entityapi_item_entities_scoped(db_handle *db, const char *uid,
                                     int is_operator) {
  es_breach_scope_migrate(db);
  cJSON *arr = cJSON_CreateArray();
  if (db && db->h && uid && *uid) {
    sqlite3_stmt *s;
    /* The third door, and the one that is easiest to miss: this route is
     * plain-auth (core/httpd.c ~919) and takes a uid, so GET
     * /api/intel/items/breach:<keyid>/entities returns e.canonical and
     * m.surface for a breach record — the cleartext identifier — even though
     * the sibling route GET /api/intel/items/breach:<keyid> two blocks below
     * it IS breach_gate()d. The scope stamp closes it here without needing to
     * know which uids are breach uids: breach material is excluded unless the
     * caller is an operator, whatever item it hangs off. */
    if (sqlite3_prepare_v2(db->h,
          "SELECT e.entity_id,e.type,e.canonical,m.surface "
          "FROM entity_mentions m JOIN entities e ON e.entity_id=m.entity_id "
          "WHERE m.item_uid=?1 "
          "AND (m.tenant_id IS NOT '" ES_BREACH_TENANT "' OR m.tenant_id=?2) "
          "AND (e.tenant_id IS NOT '" ES_BREACH_TENANT "' OR e.tenant_id=?2) "
          "GROUP BY e.entity_id "
          "ORDER BY e.mention_count DESC, e.canonical ASC", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
      bind_scope(s, 2, is_operator);
      while (sqlite3_step(s) == SQLITE_ROW) {
        const char *eid = (const char *)sqlite3_column_text(s, 0);
        const char *ty  = (const char *)sqlite3_column_text(s, 1);
        const char *val = (const char *)sqlite3_column_text(s, 2);
        const char *surf = (const char *)sqlite3_column_text(s, 3);
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "entity_id", eid ? eid : "");
        cJSON_AddStringToObject(e, "type", ty ? ty : "");
        cJSON_AddStringToObject(e, "value", val ? val : "");
        cJSON_AddStringToObject(e, "label", (surf && *surf) ? surf : (val ? val : ""));
        cJSON_AddItemToArray(arr, e);
      }
      sqlite3_finalize(s);
    }
  }
  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "data", arr);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}
