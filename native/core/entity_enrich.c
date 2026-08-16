/* core/entity_enrich.c — faithful port of entityExtractor.js (entities scope
 * of llmEnricher.js). SQL/constants verbatim. */
#include "entity_enrich.h"
#include "entitystore.h"
#include "prompts.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int env_int(const char *k, int dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  int n = atoi(v);
  return n > 0 ? n : dflt;
}

/* first balanced {...} -> cJSON (== llmClient completeJSON tolerance). */
static cJSON *extract_json(const char *raw) {
  if (!raw) return NULL;
  const char *s = strchr(raw, '{');
  if (!s) return NULL;
  int d = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '{') d++;
    else if (*p == '}' && --d == 0) {
      char *sub = strndup(s, (size_t)(p - s + 1));
      cJSON *j = sub ? cJSON_Parse(sub) : NULL;
      free(sub);
      return j;
    }
  }
  return NULL;
}

static double conf_of(const char *c) {
  if (c && !strcmp(c, "high"))   return 0.9;
  if (c && !strcmp(c, "medium")) return 0.6;
  if (c && !strcmp(c, "low"))    return 0.3;
  return 0.5;                                  /* CONF[..] ?? 0.5 */
}

static char *dupcol(sqlite3_stmt *s, int i) {
  const char *c = (const char *)sqlite3_column_text(s, i);
  return c ? strdup(c) : NULL;
}

typedef struct { char *uid,*src,*title,*body,*summary,*lang; } prow;

int entity_enrich_extract(db_handle *db, llm_client *llm, int batch) {
  sqlite3 *h = db->h;
  int TIMEOUT = env_int("LLM_TIMEOUT_MS", 30000);
  if (batch <= 0) batch = env_int("ENTITY_BACKFILL_BATCH", 50);

  /* fetch the pending batch first (== JS stmtPending.all), then close it
   * before doing the per-item writes. */
  prow *rows = calloc(batch, sizeof(prow));
  int nr = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT i.uid,i.source_id,i.title,i.body,i.summary,i.language"
      " FROM intel_items i"
      " LEFT JOIN entity_extraction_state st ON st.item_uid=i.uid"
      " WHERE (st.item_uid IS NULL OR (st.extracted_at='' AND st.failed_count<5))"
      "   AND (i.title IS NOT NULL OR i.body IS NOT NULL OR i.summary IS NOT NULL)"
      " ORDER BY i.fetched_at DESC LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, batch);
    while (nr < batch && sqlite3_step(s) == SQLITE_ROW) {
      rows[nr].uid     = dupcol(s, 0);
      rows[nr].src     = dupcol(s, 1);
      rows[nr].title   = dupcol(s, 2);
      rows[nr].body    = dupcol(s, 3);
      rows[nr].summary = dupcol(s, 4);
      rows[nr].lang    = dupcol(s, 5);
      if (rows[nr].uid) nr++;
    }
    sqlite3_finalize(s);
  }

  const char *grammar = grammar_load("entity_extraction");
  int processed = 0, failed = 0;

  for (int r = 0; r < nr; r++) {
    prow *pr = &rows[r];
    char *prompt = prompt_entity_extraction(pr->title, pr->body, pr->summary,
                                            pr->lang, pr->src);
    char *raw = llm_complete(llm, prompt, grammar && *grammar ? grammar : NULL,
                             1024, 0.1, TIMEOUT);
    free(prompt);
    cJSON *out = extract_json(raw);
    free(raw);
    cJSON *list = out ? cJSON_GetObjectItem(out, "entities") : NULL;
    if (!list || !cJSON_IsArray(list)) {
      if (out) cJSON_Delete(out);
      if (sqlite3_prepare_v2(h,
          "INSERT INTO entity_extraction_state"
          " (item_uid,extracted_at,extractor_version,failed_count)"
          " VALUES (?1,'',1,1) ON CONFLICT(item_uid) DO UPDATE SET"
          " failed_count=failed_count+1, extracted_at=''",
          -1, &s, NULL) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, pr->uid, -1, SQLITE_TRANSIENT);
        sqlite3_step(s); sqlite3_finalize(s);
      }
      failed++;
      continue;
    }

    sqlite3_exec(h, "BEGIN", 0, 0, 0);
    char **ids = NULL; int nids = 0, cids = 0;
    cJSON *ent;
    cJSON_ArrayForEach(ent, list) {
      cJSON *ev = cJSON_GetObjectItem(ent, "value");
      cJSON *et = cJSON_GetObjectItem(ent, "type");
      if (!cJSON_IsString(ev) || !ev->valuestring[0] ||
          !cJSON_IsString(et) || !et->valuestring[0]) continue;
      char *id = es_upsert_entity(db, et->valuestring, ev->valuestring);
      if (!id) continue;
      cJSON *ec = cJSON_GetObjectItem(ent, "confidence");
      es_add_mention(db, id, pr->uid, pr->src, ev->valuestring, "body",
                     conf_of(cJSON_IsString(ec) ? ec->valuestring : NULL),
                     "ner-llm");
      int dup = 0;
      for (int k = 0; k < nids; k++) if (!strcmp(ids[k], id)) { dup = 1; break; }
      if (dup) { free(id); continue; }
      /* `ids = realloc(ids, ...)` followed by `ids[nids++]` on the next line
       * leaks the old block and writes through NULL when the grow fails.
       * Realloc into a temp, and on failure stop collecting rather than
       * corrupting: the entity itself is already upserted and its mention
       * already recorded, only the co_mention edges below are affected. */
      if (nids == cids) {
        int ncap = cids ? cids * 2 : 16;
        char **nv = realloc(ids, sizeof(char *) * (size_t)ncap);
        if (!nv) {
          fprintf(stderr, "[entity-enrich] out of memory after %d entit(ies) "
                          "on %s; co_mention edges for the rest are skipped\n",
                  nids, pr->uid);
          free(id);
          break;
        }
        ids = nv; cids = ncap;
      }
      ids[nids++] = id;
    }
    /* co_mention edges among distinct entities in the same item */
    for (int a = 0; a < nids; a++)
      for (int b = a + 1; b < nids; b++)
        es_add_relationship(db, ids[a], ids[b], "co_mention", 1.0, pr->uid);
    for (int k = 0; k < nids; k++) free(ids[k]);
    free(ids);
    if (sqlite3_prepare_v2(h,
        "INSERT OR REPLACE INTO entity_extraction_state"
        " (item_uid,extracted_at,extractor_version,failed_count)"
        " VALUES (?1,datetime('now'),1,0)", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, pr->uid, -1, SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
    }
    /* `processed` is this function's return value and its log line. Everything
     * counted by it — the entities, the mentions, the co_mention edges and the
     * extraction-state row — is inside this transaction, so a discarded COMMIT
     * result meant counting an item whose work rolled back. It will be picked
     * up again next pass (extracted_at was never written), which is correct;
     * claiming it now is not. */
    if (sqlite3_exec(h, "COMMIT", 0, 0, 0) != SQLITE_OK) {
      fprintf(stderr, "[entity-enrich] COMMIT failed for %s: %s — the "
                      "extraction was discarded and will be retried\n",
              pr->uid, sqlite3_errmsg(h));
      sqlite3_exec(h, "ROLLBACK", 0, 0, 0);
      cJSON_Delete(out);
      failed++;
      continue;
    }
    cJSON_Delete(out);
    processed++;
  }

  for (int r = 0; r < nr; r++) {
    free(rows[r].uid); free(rows[r].src); free(rows[r].title);
    free(rows[r].body); free(rows[r].summary); free(rows[r].lang);
  }
  free(rows);
  fprintf(stderr, "[entity-enrich] extract: attempted=%d processed=%d failed=%d\n",
          nr, processed, failed);
  return processed;
}

int entity_enrich_resolve(db_handle *db, llm_client *llm, int batch) {
  sqlite3 *h = db->h;
  int TIMEOUT = env_int("LLM_TIMEOUT_MS", 30000);
  if (batch <= 0) batch = env_int("ENTITY_RESOLVE_BATCH", 40);

  /* candidate pairs (== entityStore.js stmtCandidatePairs), limit batch*4 */
  typedef struct { char *a,*b,*ty,*ca,*cb; } pair;
  pair *ps = NULL; int np = 0, cp = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT e1.entity_id,e1.type,e1.canonical,e2.entity_id,e2.canonical"
      " FROM entities e1 JOIN entities e2"
      " ON e1.type=e2.type AND e1.entity_id<e2.entity_id"
      " AND substr(e1.norm_key,1,4)=substr(e2.norm_key,1,4)"
      " WHERE e1.type IN ('person','company','address') LIMIT ?1",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, batch * 4);
    int scan_rc;
    while ((scan_rc = sqlite3_step(s)) == SQLITE_ROW) {
      /* Self-assigning realloc dereferenced two lines later: on failure the
       * old block leaks and ps[np].a writes through NULL. */
      if (np == cp) {
        int ncap = cp ? cp * 2 : 64;
        pair *nv = realloc(ps, sizeof(pair) * (size_t)ncap);
        if (!nv) {
          fprintf(stderr, "[entity-enrich] out of memory after %d candidate "
                          "pair(s); resolving only those\n", np);
          break;
        }
        ps = nv; cp = ncap;
      }
      ps[np].a = dupcol(s, 0); ps[np].ty = dupcol(s, 1); ps[np].ca = dupcol(s, 2);
      ps[np].b = dupcol(s, 3); ps[np].cb = dupcol(s, 4);
      np++;
    }
    /* `while (step() == ROW)` cannot tell DONE from IOERR/CORRUPT/BUSY/
     * INTERRUPT. A short candidate list is not wrong output — the next pass
     * re-runs the same query — but it is silently less work than claimed. */
    if (scan_rc != SQLITE_DONE)
      fprintf(stderr, "[entity-enrich] candidate pair scan interrupted after "
                      "%d pair(s): %s\n", np, sqlite3_errmsg(h));
    sqlite3_finalize(s);
  }

  int attempted = 0, decided = 0;
  for (int i = 0; i < np && attempted < batch; i++) {
    if (es_merge_pair_exists(db, ps[i].a, ps[i].b)) continue;
    attempted++;
    char *pr = prompt_entity_dedup(ps[i].ty, ps[i].ca, ps[i].cb);
    char *raw = llm_complete(llm, pr, NULL, 512, 0.2, TIMEOUT);
    free(pr);
    cJSON *out = extract_json(raw);
    free(raw);
    cJSON *sm = out ? cJSON_GetObjectItem(out, "same") : NULL;
    cJSON *cf = out ? cJSON_GetObjectItem(out, "confidence") : NULL;
    if (sm && cJSON_IsBool(sm) && cf && cJSON_IsNumber(cf)) {
      cJSON *rs = cJSON_GetObjectItem(out, "reason");
      es_record_merge(db, ps[i].a, ps[i].b, cJSON_IsTrue(sm),
                      cf->valuedouble,
                      (rs && cJSON_IsString(rs)) ? rs->valuestring : NULL);
      decided++;
    }
    if (out) cJSON_Delete(out);
  }
  for (int i = 0; i < np; i++) {
    free(ps[i].a); free(ps[i].b); free(ps[i].ty); free(ps[i].ca); free(ps[i].cb);
  }
  free(ps);

  /* union pass: fold same=1 & confidence>=0.7 (RESOLVE_THRESHOLD) */
  int merged = 0;
  if (sqlite3_prepare_v2(h, "SELECT entity_a,entity_b FROM entity_merges"
      " WHERE same=1 AND confidence>=0.7", -1, &s, NULL) == SQLITE_OK) {
    while (sqlite3_step(s) == SQLITE_ROW) {
      char *ea = strdup((const char *)sqlite3_column_text(s, 0));
      char *eb = strdup((const char *)sqlite3_column_text(s, 1));
      /* skip if either side already folded away this tick */
      int ok = 1;
      for (int w = 0; w < 2; w++) {
        sqlite3_stmt *q;
        if (sqlite3_prepare_v2(h, "SELECT 1 FROM entities WHERE entity_id=?1",
                               -1, &q, NULL) == SQLITE_OK) {
          sqlite3_bind_text(q, 1, w ? eb : ea, -1, SQLITE_TRANSIENT);
          if (sqlite3_step(q) != SQLITE_ROW) ok = 0;
          sqlite3_finalize(q);
        }
      }
      if (ok && es_union_entities(db, ea, eb)) merged++;
      free(ea); free(eb);
    }
    sqlite3_finalize(s);
  }
  fprintf(stderr, "[entity-enrich] resolve: attempted=%d decided=%d merged=%d\n",
          attempted, decided, merged);
  return merged;
}

int entity_enrich_run(db_handle *db, llm_client *llm) {
  entity_enrich_extract(db, llm, 0);
  entity_enrich_resolve(db, llm, 0);
  return 0;
}
