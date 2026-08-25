#include "entityapi.h"
#include "fts.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

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

/* ── the list envelope (house rule 2) ──────────────────────────────────────
 * Every list in this file capped silently. /api/entities/search took limit=1000
 * and answered 100 rows inside a bare {"results":[…]}; /mentions and /breaches
 * took limit=1000 and answered 200. None of the three said how many rows
 * existed, so "100 results" and "the first 100 of 4,000" were the same
 * response — the silent slicing rule 2 forbids, and the same finding that was
 * fixed on /api/alert-events.
 *
 * The disclosure block is the one /api/intel/items, /api/timeline and
 * /api/alert-events now share:
 *
 *   "page": { "limit": N, "total": M, <paging knob> }
 *   "meta": { "fetched_at": "…", "filters": {…} }
 *
 * `limit` and `total` are always present — `total` a REAL measured COUNT(*)
 * over the identical predicate, never an estimate, and null (not 0) if the
 * count itself failed. The paging knob is whichever one the route actually
 * honours: "next_cursor" on the keyset routes, "offset" on these three, which
 * were already offset-paged (or, for search, are now). Emitting the knob the
 * route does NOT support would be its own lie — a client following a
 * permanently-null next_cursor concludes there is no more data.
 *
 * The row array keeps its existing key ("results"/"mentions"/"data"): the
 * envelope is added ALONGSIDE it, so no existing reader of these three routes
 * breaks. Only /api/sources/:id/logs had to change shape, because a bare JSON
 * array has nowhere to put any of this — see miscapi.c. */

/* Node's new Date().toISOString(), the modulo-clamped spelling used across
 * this tree so -Wformat-truncation can prove the 24 chars fit. */
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
           (unsigned)(tm.tm_year + 1900) % 10000u, (unsigned)(tm.tm_mon + 1) % 100u,
           (unsigned)tm.tm_mday % 100u, (unsigned)tm.tm_hour % 100u,
           (unsigned)tm.tm_min % 100u, (unsigned)tm.tm_sec % 100u,
           (unsigned)(tv.tv_usec / 1000) % 1000u);
}

/* Step-and-finalize a prepared COUNT(*). -1 when the count could not be taken,
 * which the envelope must render as null: rule 1 forbids reporting a number we
 * did not measure, and a fabricated 0 here would read as "nothing exists". */
static long count_step(sqlite3_stmt *s) {
  long n = -1;
  if (s && sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

/* Attach page{limit,offset,total} + meta{fetched_at,filters} to `o`.
 * Takes ownership of `filters` (pass NULL for an empty object). */
static void add_page_meta(cJSON *o, int limit, int offset, long total,
                          cJSON *filters) {
  cJSON *page = cJSON_CreateObject();
  cJSON_AddNumberToObject(page, "limit", limit);
  cJSON_AddNumberToObject(page, "offset", offset);
  if (total < 0) cJSON_AddNullToObject(page, "total");
  else           cJSON_AddNumberToObject(page, "total", (double)total);
  cJSON_AddItemToObject(o, "page", page);

  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddItemToObject(meta, "filters", filters ? filters : cJSON_CreateObject());
  cJSON_AddItemToObject(o, "meta", meta);
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
 * column that does not exist. */

char *entityapi_stats(db_handle *db, const char *tenant) {
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
    (double)count1_t(db->h, "SELECT COUNT(*) FROM entities WHERE"
                            " (entities.tenant_id IS NULL OR entities.tenant_id = ?1)",
                     tenant));
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
char *entityapi_search(db_handle *db, const char *q, const char *type, int limit,
                       int offset, const char *tenant) {
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
  if (!segq) {
    /* The comment above says this is "the same {"results":[]} the caller
     * already emits for an empty q" — it was not: this printed the bare array
     * `[]`, so a client reading body.results got undefined instead of an empty
     * list, on the one path an ordinary unusable query takes. */
    cJSON *o = cJSON_CreateObject();
    cJSON_AddItemToObject(o, "results", results);
    /* Same envelope as the populated answer: a client must not have to write
     * two parsers, one for "we searched and found nothing" and one for "your
     * query had no usable token". total is a truthful 0 — nothing matched
     * because nothing was searched for. */
    cJSON *f0 = cJSON_CreateObject();
    add_str_or_null(f0, "q", q);
    add_str_or_null(f0, "type", type);
    cJSON_AddBoolToObject(f0, "q_applied", 0);
    add_page_meta(o, limit > 0 ? limit : 30, offset > 0 ? offset : 0, 0, f0);
    char *empty = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return empty;
  }
  int lim = limit > 0 ? limit : 30;
  if (lim > 100) lim = 100;
  int off = offset > 0 ? offset : 0;

  /* Lower-cased type, needed by both the count and the page query. */
  char tl[128];
  if (type) {
    snprintf(tl, sizeof tl, "%s", type);
    for (char *p = tl; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
  } else tl[0] = 0;

  /* How many entities the query really matches, before the cap. Same MATCH,
   * same type filter, same tenant predicate — a count taken over anything
   * else would be a number about a different question. */
  long total = -1;
  { const char *csql = type
      ? "SELECT COUNT(*) FROM entities_fts "
        "JOIN entities ON entities.entity_id=entities_fts.uid "
        "WHERE entities_fts MATCH ?1 AND entities.type=?2 "
        "AND (entities.tenant_id IS NULL OR entities.tenant_id=?3)"
      : "SELECT COUNT(*) FROM entities_fts "
        "JOIN entities ON entities.entity_id=entities_fts.uid "
        "WHERE entities_fts MATCH ?1 "
        "AND (entities.tenant_id IS NULL OR entities.tenant_id=?2)";
    sqlite3_stmt *cs = NULL;
    if (sqlite3_prepare_v2(db->h, csql, -1, &cs, NULL) == SQLITE_OK) {
      sqlite3_bind_text(cs, 1, segq, -1, SQLITE_TRANSIENT);
      if (type) {
        sqlite3_bind_text(cs, 2, tl, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(cs, 3, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
      } else {
        sqlite3_bind_text(cs, 2, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
      }
      total = count_step(cs);
    } }

  /* ORDER BY gained the `entities.entity_id ASC` tiebreak. mention_count is
   * emphatically not unique — the corpus is full of entities sharing a count,
   * and most share 0 — so the old single-key ORDER BY left ties in whatever
   * order the query planner happened to produce. That is invisible while the
   * answer is one un-paged page, and becomes lost and duplicated rows the
   * moment a caller walks OFFSET across a page boundary, which is exactly what
   * the paging added here does. */
  const char *sql = type
    ? "SELECT entities.*, snippet(entities_fts,-1,'<mark>','</mark>','…',12) "
      "FROM entities_fts JOIN entities ON entities.entity_id=entities_fts.uid "
      "WHERE entities_fts MATCH ?1 AND entities.type=?2 "
      "AND (entities.tenant_id IS NULL OR entities.tenant_id=?4) "
      "ORDER BY entities.mention_count DESC, entities.entity_id ASC "
      "LIMIT ?3 OFFSET ?5"
    : "SELECT entities.*, snippet(entities_fts,-1,'<mark>','</mark>','…',12) "
      "FROM entities_fts JOIN entities ON entities.entity_id=entities_fts.uid "
      "WHERE entities_fts MATCH ?1 "
      "AND (entities.tenant_id IS NULL OR entities.tenant_id=?3) "
      "ORDER BY entities.mention_count DESC, entities.entity_id ASC "
      "LIMIT ?2 OFFSET ?4";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    free(segq); cJSON_Delete(results); return NULL;          /* → 500 */
  }
  sqlite3_bind_text(s, 1, segq, -1, SQLITE_TRANSIENT);
  if (type) {
    sqlite3_bind_text(s, 2, tl, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 3, lim);
    sqlite3_bind_text(s, 4, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 5, off);
  } else {
    sqlite3_bind_int(s, 2, lim);
    sqlite3_bind_text(s, 3, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 4, off);
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
  cJSON *f = cJSON_CreateObject();
  add_str_or_null(f, "q", q);
  add_str_or_null(f, "type", type);
  cJSON_AddBoolToObject(f, "q_applied", 1);
  add_page_meta(o, lim, off, total, f);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* getEntity(id) → row, with the entities.js type-guard. Caller frees ret.
 * Every /api/entities/:type/:id route enters through here, so this is the one
 * place the tenant predicate has to hold for the whole subtree. */
static sqlite3_stmt *entity_by_id(db_handle *db, const char *id,
                                  const char *tenant) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT entity_id,type,canonical,norm_key,name_ja,name_romaji,"
        "aliases_json,properties,mention_count,first_seen_at,last_seen_at,"
        "tenant_id FROM entities WHERE entity_id=?1 "
        "AND (tenant_id IS NULL OR tenant_id=?2)", -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
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
 * to the entity profile so exposure is visible without a second round-trip. */
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
  sqlite3_stmt *chk = entity_by_id(db, id, tenant);
  if (!chk) return NULL;
  const char *etype = (const char *)sqlite3_column_text(chk, 1);
  if (!etype || strcmp(etype, type) != 0) { sqlite3_finalize(chk); return NULL; }
  sqlite3_finalize(chk);

  int lim = limit > 0 ? limit : 50;
  if (lim > 200) lim = 200;
  int off = offset > 0 ? offset : 0;

  /* COUNT over the GROUPED population, i.e. DISTINCT source_id — the list
   * GROUP BYs source_id, so counting raw entity_mentions rows would report a
   * bigger number than the list can ever return and the disclosure would be
   * wrong in the direction that looks like data is missing. */
  long total = -1;
  { sqlite3_stmt *cs = NULL;
    if (sqlite3_prepare_v2(db->h,
          "SELECT COUNT(DISTINCT m.source_id) FROM entity_mentions m "
          "WHERE m.entity_id=?1 AND m.extractor='breach-ingest'",
          -1, &cs, NULL) == SQLITE_OK) {
      sqlite3_bind_text(cs, 1, id, -1, SQLITE_TRANSIENT);
      total = count_step(cs);
    } }

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
  /* `exposure.breach_count` is a summary statistic about the entity, not a
   * statement about THIS page, and nothing said the two were related — so it
   * could not stand in for the disclosure. page.total is the count of exactly
   * the rows `data` is a slice of. The ORDER BY is already a total order
   * (breach_date DESC, source_id ASC, and source_id is unique after the
   * GROUP BY), so walking offset across pages is safe. */
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "entity_id", id);
  cJSON_AddStringToObject(f, "type", type);
  add_page_meta(o, lim, off, total, f);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

char *entityapi_get(db_handle *db, const char *type, const char *id,
                    const char *tenant) {
  sqlite3_stmt *s = entity_by_id(db, id, tenant);
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
   * without a second round-trip; the full list stays behind /breaches. */
  cJSON_AddItemToObject(o, "exposure", exposure_summary(db, id));
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
                      gnode *out) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT entity_id,type,canonical,mention_count FROM entities "
        "WHERE entity_id=?1 AND (tenant_id IS NULL OR tenant_id=?2)",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
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
  sqlite3_stmt *r = entity_by_id(db, id, tenant);
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
  if (fetch_node(db, id, tenant, &root)) {
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
        if (!rel_allowed(rel_types, rt)) continue;
        /* entity_relationships carries no tenant column, so visibility of the
         * far endpoint is what bounds the walk. Skipping the EDGE too, not
         * just the node, matters: an edge whose target is invisible would
         * still disclose that entity's id to the canvas. */
        { gnode probe;
          if (gn_find(nodes, nn, other) < 0) {
            if (!fetch_node(db, other, tenant, &probe)) continue;
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
          cJSON_AddItemToArray(edges, e);
        }
        if (gn_find(nodes, nn, other) < 0) {
          if (nn >= cap_nodes) {
            dropped_cap++;              /* counted, never silently discarded */
          } else {
            gnode g;
            if (fetch_node(db, other, tenant, &g)) {
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
  for (int i = 0; i < nek; i++) free(ek[i]);
  free(ek);
  for (int i = 0; i < nv; i++) free(visited[i]);
  free(visited);

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
  sqlite3_stmt *r = entity_by_id(db, id, tenant);
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
   * records), and those rows carry no tenant-owned content to leak. */
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT m.item_uid,m.source_id,m.surface,m.field,m.confidence,"
        "m.extractor,m.created_at,i.title,i.summary,i.link,i.published_at,"
        "i.record_type FROM entity_mentions m "
        "LEFT JOIN intel_items i ON i.uid=m.item_uid "
        "WHERE m.entity_id=?1 "
        "AND (i.uid IS NULL OR i.tenant_id='legacy' OR i.tenant_id=?4) "
        /* Tiebreak added for the same reason as entityapi_search: created_at
         * is a datetime('now') to the SECOND and one extraction pass writes a
         * whole item's mentions inside that second, so created_at alone is
         * nowhere near a total order. This route has been offset-paged since
         * it was written, which means every page boundary that landed inside
         * such a batch could already repeat or drop rows. (entity_id,
         * item_uid, field) is the primary key, so with entity_id fixed the
         * remaining two columns make the order total. */
        "ORDER BY m.created_at DESC, m.item_uid ASC, COALESCE(m.field,'') ASC "
        "LIMIT ?2 OFFSET ?3",
        -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, lim);
  sqlite3_bind_int(s, 3, offset);
  sqlite3_bind_text(s, 4, tenant ? tenant : "", -1, SQLITE_TRANSIENT);

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

  /* Counted through the SAME tenant predicate as the list. Counting the raw
   * entity_mentions rows instead would have reported this tenant a total that
   * includes another tenant's private items — the very rows the join above
   * exists to hide — which is a smaller leak than serving them but a leak all
   * the same. */
  long total = -1;
  { sqlite3_stmt *cs = NULL;
    if (sqlite3_prepare_v2(db->h,
          "SELECT COUNT(*) FROM entity_mentions m "
          "LEFT JOIN intel_items i ON i.uid=m.item_uid "
          "WHERE m.entity_id=?1 "
          "AND (i.uid IS NULL OR i.tenant_id='legacy' OR i.tenant_id=?2)",
          -1, &cs, NULL) == SQLITE_OK) {
      sqlite3_bind_text(cs, 1, id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(cs, 2, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
      total = count_step(cs);
    } }

  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "mentions", arr);
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "entity_id", id);
  cJSON_AddStringToObject(f, "type", type);
  add_page_meta(o, lim, offset, total, f);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

char *entityapi_item_entities(db_handle *db, const char *uid) {
  cJSON *arr = cJSON_CreateArray();
  if (db && db->h && uid && *uid) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
          "SELECT e.entity_id,e.type,e.canonical,m.surface "
          "FROM entity_mentions m JOIN entities e ON e.entity_id=m.entity_id "
          "WHERE m.item_uid=?1 "
          "GROUP BY e.entity_id "
          "ORDER BY e.mention_count DESC, e.canonical ASC", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
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
