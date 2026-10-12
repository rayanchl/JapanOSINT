/* core/entity_enrich.c — faithful port of entityExtractor.js (entities scope
 * of llmEnricher.js). SQL/constants verbatim. */
#include "entity_enrich.h"
#include "entitystore.h"
#include "entity_vec.h"
#include "prompts.h"
#include "llm_fault.h"
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
      /* The pipeline's own synthesis row: entities the MODEL wrote, which
       * extracted here would enter the entity graph as mentions in a
       * collected record. The run's real entities are already linked to it
       * by pipeline.c with their own provenance. */
      "   AND (i.record_type IS NULL OR i.record_type <> 'osint_search_run')"
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
  int processed = 0, failed = 0, deferred = 0;

  for (int r = 0; r < nr; r++) {
    prow *pr = &rows[r];
    char *prompt = prompt_entity_extraction(pr->title, pr->body, pr->summary,
                                            pr->lang, pr->src);
    long t0 = llm_fault_now_ms();
    char *raw = llm_complete(llm, prompt, grammar && *grammar ? grammar : NULL,
                             1024, 0.1, TIMEOUT);
    long elapsed = llm_fault_now_ms() - t0;
    free(prompt);
    /* failed_count is a verdict on the ITEM, and an item at 5 is never
     * retried. A server that is down or timing out says nothing about the
     * item — counting it used to retire every item a llama-server restart
     * happened to be asked about — so a transport failure leaves this item
     * and the rest of the batch untouched for the next tick, and only an
     * answer the model actually gave (empty, unparseable, no entities array)
     * is held against the item. */
    if (!raw && llm_fault_is_transport(llm, elapsed, TIMEOUT)) {
      deferred = nr - r;
      fprintf(stderr, "[entity-enrich] LLM unreachable or timed out; %d item(s) "
                      "left for the next tick, not counted as failures\n",
              deferred);
      break;
    }
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
  fprintf(stderr, "[entity-enrich] extract: attempted=%d processed=%d failed=%d"
                  " deferred=%d\n", nr - deferred, processed, failed, deferred);
  return processed;
}

/* ── candidate pairs ─────────────────────────────────────────────────────
 *
 * The port's candidate SQL was a bare self-join on substr(norm_key,1,4) with
 * LIMIT and no ORDER BY and no exclusion of pairs already decided. Three
 * consequences, all silent:
 *   - every tick re-read the SAME first batch*4 pairs (SQLite's join order),
 *     and es_merge_pair_exists() then skipped the decided ones — so once those
 *     were decided the tick attempted nothing, forever, while undecided pairs
 *     further down the join were never reached;
 *   - which pairs came first was whatever the planner chose, not the entities
 *     that matter (the most-mentioned);
 *   - a pair the model could not answer stayed at the head and was re-asked
 *     every tick.
 *
 * Now: the walk is driven by e1 in (mention_count DESC, entity_id) order and,
 * within one e1, by e2.entity_id; decided pairs are excluded IN the query
 * (NOT EXISTS entity_merges); and a keyset (e1's mention_count + id, the last
 * e2 id) persisted in entity_resolve_state advances past every pair ATTEMPTED,
 * answered or not, and wraps to the top when the walk reaches the end. An
 * unanswered pair is therefore re-asked on the next lap, not the next tick.
 * mention_count moves between ticks, so the order is a lap's order, not an
 * invariant: an entity whose count crosses the cursor is met next lap.
 *
 * Each unordered pair is met once per lap, from the side the walk reaches
 * first (e2 is restricted to entities later in the walk order), and is
 * oriented (min id, max id) — the orientation entity_merges has always been
 * written in. */
#define RESOLVE_TYPES "('person','company','address')"
/* e1 rows looked at per tick, paired or not, so a tick over a graph where
 * nothing pairs is bounded; the cursor keeps the remainder for next tick. */
#define RESOLVE_MAX_E1_WALK 5000

/* cos is the vector arm's similarity (-1 for a prefix pair); vsave is the
 * vector-arm cursor that is safe to persist once this pair was attempted. */
typedef struct { char *a,*b,*ty,*ca,*cb; double cos; char *vsave; } pair;
typedef struct { pair *v; int n, cap; } pairs;

static void resolve_state_ensure(sqlite3 *h) {
  sqlite3_exec(h, "CREATE TABLE IF NOT EXISTS entity_resolve_state"
                  " (k TEXT PRIMARY KEY, v TEXT)", 0, 0, 0);
  /* The walk's ORDER BY; without it every tick sorts the whole table. */
  sqlite3_exec(h, "CREATE INDEX IF NOT EXISTS idx_entities_resolve_walk"
                  " ON entities(mention_count DESC, entity_id)", 0, 0, 0);
}

static char *state_get(sqlite3 *h, const char *k) {
  sqlite3_stmt *s; char *v = NULL;
  if (sqlite3_prepare_v2(h, "SELECT v FROM entity_resolve_state WHERE k=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, k, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) v = dupcol(s, 0);
    sqlite3_finalize(s);
  }
  return v;
}

static void state_set(sqlite3 *h, const char *k, const char *v) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "INSERT INTO entity_resolve_state(k,v) VALUES(?1,?2)"
      " ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, k, -1, SQLITE_TRANSIENT);
    if (v) sqlite3_bind_text(s, 2, v, -1, SQLITE_TRANSIENT);
    else   sqlite3_bind_null(s, 2);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
}

/* Self-assigning realloc dereferenced two lines later: on failure the old
 * block leaks and the write goes through NULL. NULL = out of memory. */
static pair *pairs_push(pairs *p) {
  if (p->n == p->cap) {
    int ncap = p->cap ? p->cap * 2 : 64;
    pair *nv = realloc(p->v, sizeof(pair) * (size_t)ncap);
    if (!nv) return NULL;
    p->v = nv; p->cap = ncap;
  }
  pair *x = &p->v[p->n++];
  memset(x, 0, sizeof *x);
  return x;
}

static int pairs_has(const pairs *p, const char *a, const char *b) {
  for (int i = 0; i < p->n; i++)
    if (!strcmp(p->v[i].a, a) && !strcmp(p->v[i].b, b)) return 1;
  return 0;
}

static void pairs_free(pairs *p) {
  for (int i = 0; i < p->n; i++) {
    free(p->v[i].a); free(p->v[i].b); free(p->v[i].ty);
    free(p->v[i].ca); free(p->v[i].cb); free(p->v[i].vsave);
  }
  free(p->v);
  p->v = NULL; p->n = p->cap = 0;
}

/* Where a pair sits in the walk: the cursor value that, once saved, means
 * "everything up to and including this pair was attempted". */
typedef struct { long long mc; char *e1, *after; } walk_pos;
static void pos_free(walk_pos *w) { free(w->e1); free(w->after); w->e1 = w->after = NULL; }

/* Up to `want` undecided prefix-sharing pairs from the cursor on. pos[i] is
 * pair i's walk position. Returns 1 when the walk reached the end of the
 * entities (the caller wraps the cursor), else 0. */
static int collect_prefix_pairs(sqlite3 *h, int want, pairs *out, walk_pos *pos) {
  char *cmc = state_get(h, "prefix_mc"), *cid = state_get(h, "prefix_e1"),
       *caf = state_get(h, "prefix_after");
  long long mc = cmc ? atoll(cmc) : -1;
  int have_cursor = (cmc && cid);
  int reached_end = 0;
  sqlite3_stmt *e1 = NULL, *e2 = NULL;
  if (sqlite3_prepare_v2(h,
      "SELECT entity_id,type,canonical,norm_key,mention_count FROM entities"
      " WHERE type IN " RESOLVE_TYPES
      "   AND (?1=0 OR mention_count<?2 OR (mention_count=?2 AND entity_id>=?3))"
      " ORDER BY mention_count DESC, entity_id LIMIT ?4", -1, &e1, NULL) != SQLITE_OK ||
      sqlite3_prepare_v2(h,
      "SELECT e2.entity_id,e2.canonical FROM entities e2"
      " WHERE e2.type=?1 AND e2.entity_id<>?2"
      /* the range rides idx (type, norm_key); the substr equality keeps the
       * port's exact semantics for a norm_key shorter than 4 characters */
      "   AND e2.norm_key>=substr(?3,1,4) AND e2.norm_key<substr(?3,1,4)||char(1114111)"
      "   AND substr(e2.norm_key,1,4)=substr(?3,1,4)"
      "   AND e2.entity_id>?4"
      /* only partners LATER in the walk order, so an unordered pair is met
       * once per lap, from its higher-ranked side — an unanswered pair met
       * again from the other side would be re-asked within the same lap */
      "   AND (e2.mention_count<?6 OR (e2.mention_count=?6 AND e2.entity_id>?2))"
      "   AND NOT EXISTS (SELECT 1 FROM entity_merges m"
      "        WHERE m.entity_a=min(?2,e2.entity_id) AND m.entity_b=max(?2,e2.entity_id))"
      " ORDER BY e2.entity_id LIMIT ?5", -1, &e2, NULL) != SQLITE_OK) {
    fprintf(stderr, "[entity-enrich] candidate SQL: %s\n", sqlite3_errmsg(h));
    sqlite3_finalize(e1); sqlite3_finalize(e2);
    free(cmc); free(cid); free(caf);
    return 0;
  }
  sqlite3_bind_int(e1, 1, have_cursor);
  sqlite3_bind_int64(e1, 2, mc);
  sqlite3_bind_text(e1, 3, cid ? cid : "", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(e1, 4, RESOLVE_MAX_E1_WALK);
  int walked = 0, rc = SQLITE_ROW;
  while (out->n < want && (rc = sqlite3_step(e1)) == SQLITE_ROW) {
    walked++;
    const char *id = (const char *)sqlite3_column_text(e1, 0);
    long long emc = sqlite3_column_int64(e1, 4);
    /* only the cursor's own e1 resumes after an e2; every later e1 starts
     * from its first partner */
    const char *after = (have_cursor && cid && !strcmp(id, cid) && caf) ? caf : "";
    sqlite3_reset(e2);
    sqlite3_bind_text(e2, 1, (const char *)sqlite3_column_text(e1, 1), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(e2, 2, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(e2, 3, (const char *)sqlite3_column_text(e1, 3), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(e2, 4, after, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(e2, 5, want - out->n);
    sqlite3_bind_int64(e2, 6, emc);
    while (out->n < want && sqlite3_step(e2) == SQLITE_ROW) {
      const char *bid = (const char *)sqlite3_column_text(e2, 0);
      const char *bca = (const char *)sqlite3_column_text(e2, 1);
      const char *eca = (const char *)sqlite3_column_text(e1, 2);
      int lo = strcmp(id, bid) < 0;
      const char *a = lo ? id : bid, *b = lo ? bid : id;
      /* met from both sides in one tick: once is enough, but the position
       * still advances past it */
      int dup = pairs_has(out, a, b);
      pair *p = dup ? NULL : pairs_push(out);
      if (!dup && !p) {
        fprintf(stderr, "[entity-enrich] out of memory after %d candidate "
                        "pair(s); resolving only those\n", out->n);
        goto done;
      }
      if (p) {
        p->a = strdup(a); p->b = strdup(b);
        p->ty = dupcol(e1, 1);
        p->ca = strdup(lo ? eca : bca); p->cb = strdup(lo ? bca : eca);
        p->cos = -1;
        walk_pos *w = &pos[out->n - 1];
        w->mc = emc; w->e1 = strdup(id); w->after = strdup(bid);
      }
    }
  }
  /* `while (step() == ROW)` cannot tell DONE from IOERR/CORRUPT/BUSY/
   * INTERRUPT. A short candidate list is not wrong output — the cursor has
   * not moved past what was not read — but it is less work than claimed. */
  if (out->n < want) {
    if (rc == SQLITE_DONE && walked < RESOLVE_MAX_E1_WALK) reached_end = 1;
    else if (rc != SQLITE_DONE && rc != SQLITE_ROW)
      fprintf(stderr, "[entity-enrich] candidate pair scan interrupted after "
                      "%d pair(s): %s\n", out->n, sqlite3_errmsg(h));
  }
done:
  sqlite3_finalize(e1); sqlite3_finalize(e2);
  free(cmc); free(cid); free(caf);
  return reached_end;
}

static void save_prefix_cursor(sqlite3 *h, const walk_pos *w) {
  char mc[32];
  snprintf(mc, sizeof mc, "%lld", w->mc);
  state_set(h, "prefix_mc", mc);
  state_set(h, "prefix_e1", w->e1);
  state_set(h, "prefix_after", w->after);
}

static void reset_prefix_cursor(sqlite3 *h) {
  sqlite3_exec(h, "DELETE FROM entity_resolve_state WHERE k LIKE 'prefix_%'", 0, 0, 0);
}

/* ── the vector arm ──────────────────────────────────────────────────────
 *
 * A prefix never pairs トヨタ自動車 with "Toyota Motor". entity_vec (core/
 * entity_vec.h) holds each entity's "canonical (romaji)" vector; an entity's
 * stored neighbours of the same type and scope with cosine >=
 * entity_vec_cos_floor() become candidates for the SAME judge. Nothing about
 * the decision changes: a vector neighbour is a question, recorded as same=0
 * when the judge says so, and merged only by the union pass's 0.7 rule.
 *
 * Its own keyset (entity_resolve_state 'vec_after', by entity_id) walks the
 * entities that have a vector. An entity's candidates are taken whole or not
 * at all (except the first of a tick, so a tick always progresses), and the
 * cursor moves past an entity only once its last pair was attempted.
 * Neighbour lists are not symmetric, so an undecided pair can be met from
 * both sides in one lap — at most twice, and only while the judge has not
 * answered it. */
#define RESOLVE_VEC_MAX_WALK 500

static char *canonical_of(sqlite3 *h, const char *id) {
  sqlite3_stmt *s; char *c = NULL;
  if (sqlite3_prepare_v2(h, "SELECT canonical FROM entities WHERE entity_id=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) c = dupcol(s, 0);
    sqlite3_finalize(s);
  }
  return c;
}

/* Appends up to `want` vector pairs to `out`. *walk_last gets the last entity
 * whose candidates were all handed over (malloc'd, NULL if none); returns 1
 * when the walk reached the end of the index. */
static int collect_vector_pairs(db_handle *db, int want, pairs *out,
                                char **walk_last) {
  sqlite3 *h = db->h;
  *walk_last = NULL;
  char *after = state_get(h, "vec_after");
  char *prev = strdup(after ? after : "");
  double floor = entity_vec_cos_floor();
  int reached_end = 0, walked = 0, rc = SQLITE_ROW, start = out->n;
  want += out->n;
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(h,
      "SELECT e.entity_id,e.type,COALESCE(e.tenant_id,''),e.canonical"
      " FROM entity_vec_done d JOIN entities e ON e.entity_id=d.entity_id"
      " WHERE e.type IN " RESOLVE_TYPES " AND d.entity_id>?1"
      " ORDER BY d.entity_id LIMIT ?2", -1, &q, NULL) != SQLITE_OK) {
    free(after); free(prev);
    return 0;
  }
  sqlite3_bind_text(q, 1, after ? after : "", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(q, 2, RESOLVE_VEC_MAX_WALK);
  while (out->n < want && (rc = sqlite3_step(q)) == SQLITE_ROW) {
    walked++;
    const char *id  = (const char *)sqlite3_column_text(q, 0);
    const char *ty  = (const char *)sqlite3_column_text(q, 1);
    const char *eca = (const char *)sqlite3_column_text(q, 3);
    entity_nb *nb = NULL;
    int nn = entity_vec_neighbours(db, id, ty,
                                   (const char *)sqlite3_column_text(q, 2),
                                   ENTITY_VEC_K, floor, &nb);
    int fresh[ENTITY_VEC_K], nf = 0;
    for (int i = 0; i < nn && nf < ENTITY_VEC_K; i++) {
      int lo = strcmp(id, nb[i].entity_id) < 0;
      const char *a = lo ? id : nb[i].entity_id, *b = lo ? nb[i].entity_id : id;
      if (es_merge_pair_exists(db, a, b) || pairs_has(out, a, b)) continue;
      fresh[nf++] = i;
    }
    if (nf > 0 && out->n > start && out->n + nf > want) {
      entity_nb_free(nb, nn > 0 ? nn : 0);
      break;                            /* this entity waits for next tick */
    }
    for (int j = 0; j < nf && out->n < want; j++) {
      entity_nb *x = &nb[fresh[j]];
      int lo = strcmp(id, x->entity_id) < 0;
      char *cb = canonical_of(h, x->entity_id);
      if (!cb) continue;                /* folded away since it was indexed */
      pair *p = pairs_push(out);
      if (!p) { free(cb); break; }
      p->a  = strdup(lo ? id : x->entity_id);
      p->b  = strdup(lo ? x->entity_id : id);
      p->ty = strdup(ty);
      p->ca = lo ? strdup(eca) : cb;
      p->cb = lo ? cb : strdup(eca);
      p->cos = x->cos;
      /* the entity's last pair carries the entity as the cursor; the others
       * carry the cursor from before it */
      p->vsave = strdup((j == nf - 1 || out->n == want) ? id : prev);
    }
    entity_nb_free(nb, nn > 0 ? nn : 0);
    free(prev);
    prev = strdup(id);
    free(*walk_last);
    *walk_last = strdup(id);
  }
  if (out->n < want && rc == SQLITE_DONE && walked < RESOLVE_VEC_MAX_WALK)
    reached_end = 1;
  sqlite3_finalize(q);
  free(after); free(prev);
  return reached_end;
}

int entity_enrich_resolve(db_handle *db, llm_client *llm, int batch) {
  sqlite3 *h = db->h;
  int TIMEOUT = env_int("LLM_TIMEOUT_MS", 30000);
  if (batch <= 0) batch = env_int("ENTITY_RESOLVE_BATCH", 40);
  resolve_state_ensure(h);

  pairs ps = {0};
  walk_pos *pos = calloc((size_t)batch, sizeof *pos);
  if (!pos) return 0;
  /* Vector pairs first, up to half the judge budget, so a graph with a long
   * prefix backlog still gets its cross-script pairs asked; the prefix arm
   * takes everything the vector arm left. pos[] is indexed by pair index. */
  char *vec_walk_last = NULL;
  int vec_end = 0;
  if (entity_vec_ready(db))
    vec_end = collect_vector_pairs(db, batch / 2 > 0 ? batch / 2 : 1, &ps,
                                   &vec_walk_last);
  int n_vec = ps.n;
  int reached_end = collect_prefix_pairs(h, batch, &ps, pos);
  int n_prefix = ps.n - n_vec;

  sqlite3_stmt *s;
  int attempted = 0, decided = 0, deferred = 0, last_attempted = -1;
  for (int i = 0; i < ps.n && attempted < batch; i++) {
    if (es_merge_pair_exists(db, ps.v[i].a, ps.v[i].b)) { last_attempted = i; continue; }
    char *pr = prompt_entity_dedup(ps.v[i].ty, ps.v[i].ca, ps.v[i].cb);
    long t0 = llm_fault_now_ms();
    char *raw = llm_complete(llm, pr, NULL, 512, 0.2, TIMEOUT);
    long elapsed = llm_fault_now_ms() - t0;
    free(pr);
    /* An outage says nothing about the pair: stop, and leave the cursor
     * BEFORE it so the next tick asks it again (extract does the same). */
    if (!raw && llm_fault_is_transport(llm, elapsed, TIMEOUT)) {
      deferred = ps.n - i;
      break;
    }
    attempted++;
    last_attempted = i;
    cJSON *out = extract_json(raw);
    free(raw);
    cJSON *sm = out ? cJSON_GetObjectItem(out, "same") : NULL;
    cJSON *cf = out ? cJSON_GetObjectItem(out, "confidence") : NULL;
    if (sm && cJSON_IsBool(sm) && cf && cJSON_IsNumber(cf)) {
      cJSON *rs = cJSON_GetObjectItem(out, "reason");
      es_record_merge(db, ps.v[i].a, ps.v[i].b, cJSON_IsTrue(sm),
                      cf->valuedouble,
                      (rs && cJSON_IsString(rs)) ? rs->valuestring : NULL);
      decided++;
    }
    if (out) cJSON_Delete(out);
  }
  /* Advance past every pair attempted (answered or not). The walk wraps only
   * when it reached the end AND every pair it handed over was attempted.
   * Pairs are attempted in index order: vector pairs [0,n_vec), then prefix
   * pairs [n_vec, ps.n). */
  if (last_attempted >= n_vec) {
    if (reached_end && last_attempted == ps.n - 1) reset_prefix_cursor(h);
    else save_prefix_cursor(h, &pos[last_attempted]);
  } else if (reached_end && n_prefix == 0) {
    reset_prefix_cursor(h);
  }
  if (n_vec == 0 || last_attempted >= n_vec - 1) {
    /* every vector pair handed over was attempted (or there were none and
     * the walk only passed entities without candidates) */
    if (vec_end) state_set(h, "vec_after", NULL);
    else if (vec_walk_last) state_set(h, "vec_after", vec_walk_last);
  } else if (last_attempted >= 0) {
    state_set(h, "vec_after", ps.v[last_attempted].vsave);
  }
  for (int i = 0; i < batch; i++) pos_free(&pos[i]);
  free(pos);
  free(vec_walk_last);
  pairs_free(&ps);
  if (deferred)
    fprintf(stderr, "[entity-enrich] LLM unreachable or timed out; %d candidate "
                    "pair(s) left for the next tick\n", deferred);
  if (n_vec)
    fprintf(stderr, "[entity-enrich] candidates: %d by name vector (cos >= %.2f), "
                    "%d by norm_key prefix\n", n_vec, entity_vec_cos_floor(),
            n_prefix);

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
