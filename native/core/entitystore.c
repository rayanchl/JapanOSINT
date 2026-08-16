/* core/entitystore.c — faithful port of server/src/utils/entityStore.js
 * write surface. See header. SQL is verbatim from entityStore.js. */
#include "entitystore.h"
#include "fts.h"
#include "../lib/utf8.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

/* ── breach quarantine: schema + one-time backfill ─────────────────────────
 * See entitystore.h for WHY the scope exists. This is the part that has to
 * reach databases that were written BEFORE the scope existed: every breach
 * ingest that has already run left its entities and mentions at tenant_id
 * NULL, i.e. still readable through /api/entities. Stamping only new writes
 * would fix the leak for future ingests and leave the existing corpus exposed.
 *
 * Shape copied from breach_monitor_migrate(): ensure_column then the index
 * over it, behind a process-wide once-guard, called from every entry point.
 * db_open()'s boot-migration block calls it (core/db.c, beside the other
 * ensure_column calls) so the backfill happens at boot rather than being paid
 * for by the first entity request after a deploy. It is ALSO self-healing on
 * purpose: every entry point below calls it behind the once-guard, so a build
 * that ever loses the boot hook degrades to a lazy migration rather than to an
 * exposed corpus.
 *
 * It CANNOT live in schema.sql: schema.sql is generated from the live DB and
 * every statement is CREATE ... IF NOT EXISTS, which silently no-ops on an
 * existing entity_mentions, so a column appended there would never appear on
 * any deployed database (core/db.h says exactly this). */
static pthread_mutex_t g_bscope_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_bscope_done = 0;

static int has_column(sqlite3 *h, const char *table, const char *col) {
  char q[128]; snprintf(q, sizeof q, "PRAGMA table_info(%s)", table);
  sqlite3_stmt *s; int found = 0;
  if (sqlite3_prepare_v2(h, q, -1, &s, NULL) != SQLITE_OK) return 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const unsigned char *n = sqlite3_column_text(s, 1);
    if (n && strcmp((const char *)n, col) == 0) { found = 1; break; }
  }
  sqlite3_finalize(s);
  return found;
}

void es_breach_scope_migrate(db_handle *db) {
  if (!db || !db->h) return;
  pthread_mutex_lock(&g_bscope_mu);
  if (g_bscope_done) { pthread_mutex_unlock(&g_bscope_mu); return; }
  sqlite3 *h = db->h;

  ensure_column(db, "entity_mentions", "tenant_id", "TEXT");
  if (!has_column(h, "entity_mentions", "tenant_id")) {
    /* Table missing (schema.sql has not run on this handle yet) or the ALTER
     * failed. Do NOT latch the guard: every writer below binds this column, so
     * latching would turn a transient into permanently dropped mentions. */
    pthread_mutex_unlock(&g_bscope_mu);
    return;
  }

  /* Partial index over exactly the rows the backfill has to find. After the
   * backfill it is empty and stays empty (new breach writes are stamped at
   * INSERT), so the probe below costs one seek per boot instead of a scan of a
   * mention table that a bulk ingest can push into the hundreds of millions.
   * It cannot go in schema.sql for the ordering reason above — the column it
   * references does not exist when schema.sql runs. */
  sqlite3_exec(h,
    "CREATE INDEX IF NOT EXISTS idx_em_breach_unscoped"
    " ON entity_mentions(entity_id)"
    " WHERE extractor='breach-ingest' AND tenant_id IS NULL;",
    NULL, NULL, NULL);

  sqlite3_stmt *s; int pending = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT 1 FROM entity_mentions"
        " WHERE extractor='breach-ingest' AND tenant_id IS NULL LIMIT 1",
        -1, &s, NULL) == SQLITE_OK) {
    pending = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }

  if (pending) {
    /* Order matters, and so does the entity predicate. An entity is
     * quarantined only when breach ingest is its ONLY attestation; one that is
     * also mentioned by the ordinary corpus stays shared, because hiding it
     * would remove intel that has nothing to do with the breach corpus (house
     * rule 2) and would hide nothing — its value is already public. The
     * mention rows are stamped regardless, so "which breach" stays
     * operator-only for shared and quarantined nodes alike. */
    /* SAVEPOINT, not BEGIN. This function is reached from es_upsert_entity,
     * which core/entity_enrich.c calls from INSIDE its own transaction
     * (entity_enrich.c opens BEGIN, then upserts). A nested BEGIN fails, which
     * aborts this multi-statement script — and the ROLLBACK that used to follow
     * was then applied to the CALLER's transaction, silently discarding a whole
     * batch of NER work. A savepoint nests correctly either way, and rolling
     * one back undoes only the statements below. */
    char *err = NULL;
    int rc = sqlite3_exec(h,
      "SAVEPOINT jo_bscope;"
      "UPDATE entities SET tenant_id='" ES_BREACH_TENANT "'"
      " WHERE tenant_id IS NULL"
      "   AND EXISTS (SELECT 1 FROM entity_mentions m"
      "                WHERE m.entity_id=entities.entity_id"
      "                  AND m.extractor='breach-ingest')"
      "   AND NOT EXISTS (SELECT 1 FROM entity_mentions m"
      "                    WHERE m.entity_id=entities.entity_id"
      "                      AND m.extractor<>'breach-ingest');"
      /* Evict the newly quarantined nodes from entity search. Their canonical
       * IS the breached identifier, and entities_fts has no tenant column — a
       * MATCH that forgets to join back to entities.tenant_id would hand it
       * straight back. */
      "DELETE FROM entities_fts WHERE rowid IN"
      " (SELECT rowid FROM entities_fts_uid_map WHERE uid IN"
      "   (SELECT entity_id FROM entities"
      "     WHERE tenant_id='" ES_BREACH_TENANT "'));"
      "DELETE FROM entities_fts_uid_map WHERE uid IN"
      " (SELECT entity_id FROM entities WHERE tenant_id='" ES_BREACH_TENANT "');"
      "UPDATE entity_mentions SET tenant_id='" ES_BREACH_TENANT "'"
      " WHERE extractor='breach-ingest' AND tenant_id IS NULL;"
      "RELEASE jo_bscope;", NULL, NULL, &err);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "[entitystore] breach-scope backfill failed: %s\n",
              err ? err : "?");
      sqlite3_free(err);
      /* Undo only our own statements, then discard the savepoint. Both are
       * needed: ROLLBACK TO rewinds without removing the savepoint from the
       * stack, so omitting the RELEASE would leave the caller's transaction
       * holding a savepoint it never created. */
      sqlite3_exec(h, "ROLLBACK TO jo_bscope", NULL, NULL, NULL);
      sqlite3_exec(h, "RELEASE jo_bscope", NULL, NULL, NULL);
      /* Leave the guard unlatched: the corpus is still exposed, so the next
       * caller must retry rather than run for the life of the process with a
       * half-quarantined graph. */
      pthread_mutex_unlock(&g_bscope_mu);
      return;
    }
    fprintf(stderr, "[entitystore] migrated: breach-derived entities and "
                    "mentions moved to the '%s' scope\n", ES_BREACH_TENANT);
  }

  g_bscope_done = 1;
  pthread_mutex_unlock(&g_bscope_mu);
}

/* nfkcCollapse: \s+ -> ' ', trim. (NFKC decomposition intentionally omitted —
 * documented pragmatic approximation; the LLM resolver judges real sameness.) */
void es_norm_key(const char *v, char *out, size_t n) {
  if (!v || !n) { if (n) out[0] = 0; return; }
  size_t w = 0; int sp = 0, started = 0;
  for (const char *p = v; *p && w + 1 < n; p++) {
    unsigned char c = (unsigned char)*p;
    int ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
              c == '\f' || c == '\v');
    if (ws) { sp = 1; continue; }
    if (sp && started) out[w++] = ' ';
    sp = 0; started = 1;
    if (w + 1 < n) out[w++] = (char)c;
  }
  out[w] = 0;
}

static char *dup_norm(const char *v) {
  size_t L = v ? strlen(v) : 0;
  char *b = malloc(L + 1);
  if (!b) return NULL;
  es_norm_key(v ? v : "", b, L + 1);
  return b;
}

/* entityStore CJK set: /[぀-ヿ㐀-䶿一-鿿豈-﫿]/ over UTF-8. The bounded decode
 * that used to be open-coded here is now lib/utf8.h — this file had the only
 * correct copy of the four, so it became the shared one. */
static int has_cjk(const char *s) {
  if (!s) return 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; ) {
    int len;
    unsigned cp = utf8_decode(p, &len);
    if ((cp >= 0x3040 && cp <= 0x30FF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xF900 && cp <= 0xFAFF))
      return 1;
    p += len;
  }
  return 0;
}

/* NULL on OOM — the caller must not invent an id it did not allocate. */
static char *new_entity_id(void) {
  unsigned char b[8];
  if (RAND_bytes(b, 8) != 1) for (int i = 0; i < 8; i++) b[i] = (unsigned char)rand();
  char *id = malloc(21);
  if (!id) return NULL;                 /* memcpy through NULL, one line down */
  memcpy(id, "ent_", 4);
  for (int i = 0; i < 8; i++) sprintf(id + 4 + i * 2, "%02x", b[i]);
  id[20] = 0;
  return id;
}

/* aliases_json -> space-joined string of its string elements (== JS
 * joinAliasesForFts), then fts_segment for the keywords FTS column. */
static char *join_aliases(const char *aliases_json) {
  cJSON *a = aliases_json ? cJSON_Parse(aliases_json) : NULL;
  if (!a || !cJSON_IsArray(a)) { if (a) cJSON_Delete(a); return strdup(""); }
  /* `buf[0]=0` on an unchecked malloc, and `buf = realloc(buf, cap)` whose
   * result is written to two lines later: both go through NULL on OOM, and the
   * realloc additionally leaks the old block. On failure keep what has been
   * joined so far — this feeds the FTS keywords column, so a short join costs
   * recall on some aliases, which is better than losing the row (and far
   * better than a crash). */
  size_t cap = 64, len = 0; char *buf = malloc(cap);
  if (!buf) { cJSON_Delete(a); return NULL; }
  buf[0] = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, a) {
    if (!cJSON_IsString(e) || !e->valuestring[0]) continue;
    size_t need = len + strlen(e->valuestring) + 2;
    if (need > cap) {
      size_t ncap = cap;
      while (need > ncap) ncap *= 2;
      char *nb = realloc(buf, ncap);
      if (!nb) break;
      buf = nb; cap = ncap;
    }
    if (len) buf[len++] = ' ';
    strcpy(buf + len, e->valuestring); len += strlen(e->valuestring);
  }
  cJSON_Delete(a);
  return buf;
}

/* writeEntityFts: clone of core/intel.c fts_write for entities_fts
 * (uid UNINDEXED, canonical, name_ja, name_romaji, keywords), all segmented. */
static void write_entity_fts(sqlite3 *h, const char *uid, const char *canonical,
                             const char *name_ja, const char *name_romaji,
                             const char *aliases_json) {
  sqlite3_stmt *s; sqlite3_int64 old = -1;
  if (sqlite3_prepare_v2(h, "SELECT rowid FROM entities_fts_uid_map WHERE uid=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) old = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  char *jk = join_aliases(aliases_json);
  char *sc = fts_segment(canonical ? canonical : "");
  char *sj = fts_segment(name_ja ? name_ja : "");
  char *sr = fts_segment(name_romaji ? name_romaji : "");
  char *sk = fts_segment(jk ? jk : "");    /* join_aliases: NULL on OOM */
  /* The rowid must come from THIS insert — the unfixed twin of the bug
   * core/intel.c:fts_write documents. Reading last_insert_rowid()
   * unconditionally meant that when this INSERT failed or was never prepared,
   * the map recorded whatever rowid the PRECEDING statement produced (the
   * entities upsert, or another entity's fts insert). The delete at the top of
   * the next write for THAT uid then removed a DIFFERENT entity's index row,
   * evicting it from entity search with no error anywhere. */
  int inserted = 0;
  if (sqlite3_prepare_v2(h,
      "INSERT INTO entities_fts(uid,canonical,name_ja,name_romaji,keywords)"
      " VALUES(?1,?2,?3,?4,?5)", -1, &s, NULL) == SQLITE_OK) {
    /* Delete the old row only once the replacement is known to be preparable
     * — same ordering rule as core/intel.c, for the same reason: a committed
     * delete with no insert behind it is an eviction from search that nothing
     * reports and nothing repairs short of a full rebuild. */
    if (old >= 0) {
      sqlite3_stmt *d;
      if (sqlite3_prepare_v2(h, "DELETE FROM entities_fts WHERE rowid=?1",
                             -1, &d, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(d, 1, old); sqlite3_step(d); sqlite3_finalize(d);
      }
    }
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, sc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, sj, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, sr, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 5, sk, -1, SQLITE_TRANSIENT);
    inserted = (sqlite3_step(s) == SQLITE_DONE);
    sqlite3_finalize(s);
  }
  free(jk); free(sc); free(sj); free(sr); free(sk);
  if (!inserted) {
    fprintf(stderr, "[entitystore] entities_fts insert failed for %s: %s\n",
            uid, sqlite3_errmsg(h));
    return;                       /* leave the existing map row alone */
  }
  sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
  if (sqlite3_prepare_v2(h,
      "INSERT INTO entities_fts_uid_map(uid,rowid) VALUES(?1,?2)"
      " ON CONFLICT(uid) DO UPDATE SET rowid=excluded.rowid",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 2, rid); sqlite3_step(s); sqlite3_finalize(s);
  }
}

/* aliases push w/ slice(0,50) + contains, returns malloc'd JSON string. */
static char *aliases_push(const char *cur_json, const char *add) {
  cJSON *a = cur_json ? cJSON_Parse(cur_json) : NULL;
  if (!a || !cJSON_IsArray(a)) { if (a) cJSON_Delete(a); a = cJSON_CreateArray(); }
  int found = 0; cJSON *e;
  cJSON_ArrayForEach(e, a) if (cJSON_IsString(e) && !strcmp(e->valuestring, add)) { found = 1; break; }
  if (!found && add && *add) cJSON_AddItemToArray(a, cJSON_CreateString(add));
  while (cJSON_GetArraySize(a) > 50) cJSON_DeleteItemFromArray(a, 50);
  char *s = cJSON_PrintUnformatted(a);
  cJSON_Delete(a);
  return s ? s : strdup("[]");
}

/* 1 when `scope` names the breach quarantine. NULL/"" == the shared graph. */
static int is_breach_scope(const char *scope) {
  return scope && strcmp(scope, ES_BREACH_TENANT) == 0;
}

char *es_upsert_entity(db_handle *db, const char *type, const char *value) {
  return es_upsert_entity_scoped(db, type, value, NULL);
}

char *es_upsert_entity_scoped(db_handle *db, const char *type,
                              const char *value, const char *scope) {
  es_breach_scope_migrate(db);
  const int breach = is_breach_scope(scope);
  char *surface = dup_norm(value);
  if (!surface || !*surface) { free(surface); return NULL; }
  char t[64]; size_t i = 0;
  for (const char *p = type && *type ? type : "unknown"; *p && i < 63; p++)
    t[i++] = (char)tolower((unsigned char)*p);
  t[i] = 0;
  char *canon = strdup(surface);            /* canonical||surface, collapsed */
  int isja = has_cjk(canon);
  sqlite3 *h = db->h; sqlite3_stmt *s;

  /* SELECT * FROM entities WHERE type=? AND norm_key=? */
  char *ex_id = NULL, *ex_canon = NULL, *ex_ja = NULL, *ex_ro = NULL, *ex_al = NULL;
  int ex_quarantined = 0;
  if (sqlite3_prepare_v2(h,
      "SELECT entity_id,canonical,name_ja,name_romaji,aliases_json,tenant_id"
      " FROM entities WHERE type=?1 AND norm_key=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, t, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, surface, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *c;
      ex_id = strdup((const char *)sqlite3_column_text(s, 0));
      c = (const char *)sqlite3_column_text(s, 1); ex_canon = strdup(c ? c : "");
      c = (const char *)sqlite3_column_text(s, 2); ex_ja = c ? strdup(c) : NULL;
      c = (const char *)sqlite3_column_text(s, 3); ex_ro = c ? strdup(c) : NULL;
      c = (const char *)sqlite3_column_text(s, 4); ex_al = strdup(c ? c : "[]");
      c = (const char *)sqlite3_column_text(s, 5);
      ex_quarantined = (c && strcmp(c, ES_BREACH_TENANT) == 0);
    }
    sqlite3_finalize(s);
  }

  if (ex_id) {
    char *aliases = ex_al;
    /* Promotion, and only in this direction — see the header. A NON-breach
     * writer naming this value means the ordinary corpus attests it
     * independently, so the node rejoins the shared graph and gets its first
     * entities_fts row. A breach writer never touches the scope of a node that
     * already exists, in either direction. */
    if (ex_quarantined && !breach) {
      if (sqlite3_prepare_v2(h, "UPDATE entities SET tenant_id=NULL"
          " WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, ex_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(s); sqlite3_finalize(s);
      }
      ex_quarantined = 0;
    }
    if (strcmp(surface, ex_canon) != 0) {
      char *na = aliases_push(ex_al, surface); free(aliases); aliases = na;
    }
    if (sqlite3_prepare_v2(h,
        "UPDATE entities SET last_seen_at=datetime('now'), aliases_json=?2,"
        " name_ja=COALESCE(name_ja,?3), name_romaji=COALESCE(name_romaji,?4)"
        " WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, ex_id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, aliases, -1, SQLITE_TRANSIENT);
      if (isja) sqlite3_bind_text(s, 3, canon, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(s, 3);
      if (isja) sqlite3_bind_null(s, 4);
      else sqlite3_bind_text(s, 4, canon, -1, SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
    }
    const char *fja = ex_ja ? ex_ja : (isja ? canon : NULL);
    const char *fro = ex_ro ? ex_ro : (isja ? NULL : canon);
    /* A quarantined node stays out of entities_fts: its canonical is the
     * breached identifier and the FTS table has no tenant column to filter on. */
    if (!ex_quarantined)
      write_entity_fts(h, ex_id, ex_canon, fja, fro, aliases);
    char *ret = strdup(ex_id);
    free(aliases); free(ex_id); free(ex_canon); free(ex_ja); free(ex_ro);
    free(surface); free(canon);
    return ret;
  }

  char *eid = new_entity_id();
  if (!eid) { free(surface); free(canon); return NULL; }   /* callers skip NULL */
  if (sqlite3_prepare_v2(h,
      "INSERT INTO entities (entity_id,type,canonical,norm_key,name_ja,"
      "name_romaji,aliases_json,properties,mention_count,first_seen_at,"
      "last_seen_at,tenant_id) VALUES (?1,?2,?3,?4,?5,?6,'[]','{}',0,"
      "datetime('now'),datetime('now'),?7)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, eid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, t, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, canon, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, surface, -1, SQLITE_TRANSIENT);   /* norm_key */
    if (isja) sqlite3_bind_text(s, 5, canon, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(s, 5);
    if (isja) sqlite3_bind_null(s, 6);
    else sqlite3_bind_text(s, 6, canon, -1, SQLITE_TRANSIENT);
    /* The whole fix in one bind: an entity this writer INVENTED from breach
     * material is born outside the shared-graph disjunct instead of inside it. */
    if (breach) sqlite3_bind_text(s, 7, ES_BREACH_TENANT, -1, SQLITE_STATIC);
    else sqlite3_bind_null(s, 7);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  if (!breach)
    write_entity_fts(h, eid, canon, isja ? canon : NULL, isja ? NULL : canon, "[]");
  free(surface); free(canon);
  return eid;
}

void es_add_mention(db_handle *db, const char *eid, const char *item_uid,
                    const char *source_id, const char *surface,
                    const char *field, double confidence,
                    const char *extractor) {
  es_add_mention_scoped(db, eid, item_uid, source_id, surface, field,
                        confidence, extractor, NULL);
}

void es_add_mention_scoped(db_handle *db, const char *eid, const char *item_uid,
                           const char *source_id, const char *surface,
                           const char *field, double confidence,
                           const char *extractor, const char *scope) {
  es_breach_scope_migrate(db);
  if (!eid || !item_uid) return;
  const int breach = is_breach_scope(scope);
  const char *fld = (field && *field) ? field : "body";
  sqlite3 *h = db->h; sqlite3_stmt *s;
  int is_new = 1;
  if (sqlite3_prepare_v2(h, "SELECT 1 FROM entity_mentions"
      " WHERE entity_id=?1 AND item_uid=?2 AND field=?3", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, eid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, item_uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, fld, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) is_new = 0;
    sqlite3_finalize(s);
  }
  char surf[512]; const char *surfp = NULL;
  if (surface) { snprintf(surf, sizeof surf, "%s", surface); surfp = surf; }
  if (sqlite3_prepare_v2(h,
      "INSERT INTO entity_mentions (entity_id,item_uid,source_id,surface,"
      "field,confidence,extractor,created_at,tenant_id) VALUES"
      " (?1,?2,?3,?4,?5,?6,?7,datetime('now'),?8)"
      " ON CONFLICT(entity_id,item_uid,field) DO UPDATE SET"
      " confidence=MAX(confidence,excluded.confidence),"
      " surface=excluded.surface,"
      /* Quarantine is sticky and only ever tightens. `surface` on a breach row
       * is the cleartext identifier, so a later shared write onto the same
       * (entity,item,field) must not be able to un-scope the row it is
       * overwriting — and a breach write onto an existing shared row MUST
       * tighten it, because it has just replaced that surface with breach
       * material. */
      " tenant_id=CASE WHEN entity_mentions.tenant_id='" ES_BREACH_TENANT "'"
      "                THEN entity_mentions.tenant_id"
      "                ELSE excluded.tenant_id END",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, eid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, item_uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, (source_id && *source_id) ? source_id : "unknown", -1, SQLITE_TRANSIENT);
    if (surfp) sqlite3_bind_text(s, 4, surfp, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 4);
    sqlite3_bind_text(s, 5, fld, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(s, 6, (confidence == confidence) ? confidence : 0.5);
    sqlite3_bind_text(s, 7, (extractor && *extractor) ? extractor : "ner-llm", -1, SQLITE_TRANSIENT);
    if (breach) sqlite3_bind_text(s, 8, ES_BREACH_TENANT, -1, SQLITE_STATIC);
    else sqlite3_bind_null(s, 8);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  if (is_new && sqlite3_prepare_v2(h, "UPDATE entities SET"
      " mention_count=mention_count+1, last_seen_at=datetime('now')"
      " WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, eid, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
}

void es_add_relationship(db_handle *db, const char *src, const char *dst,
                         const char *rel_type, double weight,
                         const char *evidence_uid) {
  if (!src || !dst || !strcmp(src, dst)) return;
  sqlite3 *h = db->h; sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "INSERT INTO entity_relationships (src_entity_id,dst_entity_id,rel_type,"
      "weight,evidence_uid,first_seen_at,last_seen_at) VALUES"
      " (?1,?2,?3,?4,?5,datetime('now'),datetime('now'))"
      " ON CONFLICT(src_entity_id,dst_entity_id,rel_type) DO UPDATE SET"
      " weight=entity_relationships.weight+excluded.weight,"
      " last_seen_at=datetime('now'),"
      " evidence_uid=COALESCE(entity_relationships.evidence_uid,excluded.evidence_uid)",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, src, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, dst, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, (rel_type && *rel_type) ? rel_type : "co_mention", -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(s, 4, (weight == weight) ? weight : 1.0);
    if (evidence_uid) sqlite3_bind_text(s, 5, evidence_uid, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(s, 5);
    sqlite3_step(s); sqlite3_finalize(s);
  }
}

int es_merge_pair_exists(db_handle *db, const char *a, const char *b) {
  sqlite3_stmt *s; int yes = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT 1 FROM entity_merges"
      " WHERE entity_a=?1 AND entity_b=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, a, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, b, -1, SQLITE_TRANSIENT);
    yes = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }
  return yes;
}

void es_record_merge(db_handle *db, const char *a, const char *b, int same,
                     double confidence, const char *reason) {
  sqlite3_stmt *s;
  char r[201]; const char *rp = NULL;
  if (reason) { snprintf(r, sizeof r, "%s", reason); rp = r; }
  if (sqlite3_prepare_v2(db->h, "INSERT OR REPLACE INTO entity_merges"
      " (entity_a,entity_b,same,confidence,reason,decided_at)"
      " VALUES (?1,?2,?3,?4,?5,datetime('now'))", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, a, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, b, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 3, same ? 1 : 0);
    sqlite3_bind_double(s, 4, confidence);
    if (rp) sqlite3_bind_text(s, 5, rp, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 5);
    sqlite3_step(s); sqlite3_finalize(s);
  }
}

static int get_ent(sqlite3 *h, const char *id, char **canon, char **ja,
                   char **ro, char **al, int *quarantined) {
  sqlite3_stmt *s; int ok = 0;
  if (sqlite3_prepare_v2(h, "SELECT canonical,name_ja,name_romaji,aliases_json,"
      "tenant_id FROM entities WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *c;
      c = (const char *)sqlite3_column_text(s, 0); *canon = strdup(c ? c : "");
      c = (const char *)sqlite3_column_text(s, 1); *ja = c ? strdup(c) : NULL;
      c = (const char *)sqlite3_column_text(s, 2); *ro = c ? strdup(c) : NULL;
      c = (const char *)sqlite3_column_text(s, 3); *al = strdup(c ? c : "[]");
      c = (const char *)sqlite3_column_text(s, 4);
      if (quarantined) *quarantined = (c && strcmp(c, ES_BREACH_TENANT) == 0);
      ok = 1;
    }
    sqlite3_finalize(s);
  }
  return ok;
}

int es_union_entities(db_handle *db, const char *idA, const char *idB) {
  es_breach_scope_migrate(db);
  /* With idA == idB, survivor and loser name the SAME row: the aliases update
   * would append the entity's own canonical to itself and the DELETE below
   * would then remove the survivor, taking its mentions and relationships with
   * it via ON DELETE CASCADE. Unreachable today — the only caller's candidate
   * query enforces e1.entity_id < e2.entity_id — but that is one query edit
   * away, and es_add_relationship already refuses its own src==dst for the
   * same reason. Nothing to merge, so this is a no-op, not an error path. */
  if (!idA || !idB || strcmp(idA, idB) == 0) return 0;
  const char *survivor = strcmp(idA, idB) < 0 ? idA : idB;
  const char *loser    = strcmp(idA, idB) < 0 ? idB : idA;
  sqlite3 *h = db->h;
  char *s_can=NULL,*s_ja=NULL,*s_ro=NULL,*s_al=NULL;
  char *l_can=NULL,*l_ja=NULL,*l_ro=NULL,*l_al=NULL;
  int s_q = 0, l_q = 0;
  if (!get_ent(h, survivor, &s_can,&s_ja,&s_ro,&s_al,&s_q) ||
      !get_ent(h, loser, &l_can,&l_ja,&l_ro,&l_al,&l_q)) {
    free(s_can);free(s_ja);free(s_ro);free(s_al);
    free(l_can);free(l_ja);free(l_ro);free(l_al);
    return 0;
  }
  /* Cross-scope merges are refused, not "handled". A union appends the loser's
   * canonical to the survivor's aliases and rewrites the survivor's FTS row —
   * so folding a quarantined identifier into a shared node would publish that
   * identifier into entity search, and folding a shared node into a
   * quarantined one would delete a shared node out of every tenant's graph.
   * Both directions are the gate being bypassed by the resolver instead of by
   * a route; sameness across that boundary is not ours to assert. */
  if (s_q != l_q) {
    fprintf(stderr, "[entitystore] refusing cross-scope union %s <-> %s "
                    "(one side is '%s'-scoped)\n", survivor, loser,
            ES_BREACH_TENANT);
    free(s_can);free(s_ja);free(s_ro);free(s_al);
    free(l_can);free(l_ja);free(l_ro);free(l_al);
    return 0;
  }
  sqlite3_stmt *s;
  sqlite3_exec(h, "BEGIN", 0, 0, 0);
  #define REPOINT(sql) if (sqlite3_prepare_v2(h, sql, -1, &s, NULL)==SQLITE_OK){ \
    sqlite3_bind_text(s,1,survivor,-1,SQLITE_TRANSIENT); \
    sqlite3_bind_text(s,2,loser,-1,SQLITE_TRANSIENT); \
    sqlite3_step(s); sqlite3_finalize(s); }
  /* entity_mentions PK is (entity_id,item_uid,field): an IGNOREd row is the
   * survivor already holding the SAME attestation of the same item and field,
   * so dropping the loser's copy loses nothing. */
  REPOINT("UPDATE OR IGNORE entity_mentions SET entity_id=?1 WHERE entity_id=?2");

  /* entity_relationships is different. Its PK is (src,dst,rel_type) and its
   * weight is a RUNNING TOTAL — es_add_relationship's upsert sums it
   * (weight=weight+excluded.weight). So when the survivor already has an edge
   * to the same neighbour, `UPDATE OR IGNORE` skipped the loser's row, the
   * loser's entity row was deleted a few statements later, and ON DELETE
   * CASCADE took the skipped edge with it — the co-mention evidence behind it
   * silently vanished from the graph, and the merged node ended up looking
   * less connected than either input. Fold the weight into the surviving edge
   * FIRST (while the loser's rows still exist), then repoint the rest; what
   * IGNORE now skips has already been accounted for. */
  #define FOLD(sql) if (sqlite3_prepare_v2(h, sql, -1, &s, NULL)==SQLITE_OK){ \
    sqlite3_bind_text(s,1,survivor,-1,SQLITE_TRANSIENT); \
    sqlite3_bind_text(s,2,loser,-1,SQLITE_TRANSIENT); \
    sqlite3_step(s); sqlite3_finalize(s); }
  FOLD("UPDATE entity_relationships AS r SET"
       " weight = r.weight + (SELECT l.weight FROM entity_relationships l"
       "   WHERE l.src_entity_id=?2 AND l.dst_entity_id=r.dst_entity_id"
       "     AND l.rel_type=r.rel_type),"
       " last_seen_at = datetime('now')"
       " WHERE r.src_entity_id=?1 AND EXISTS (SELECT 1 FROM entity_relationships l"
       "   WHERE l.src_entity_id=?2 AND l.dst_entity_id=r.dst_entity_id"
       "     AND l.rel_type=r.rel_type)");
  REPOINT("UPDATE OR IGNORE entity_relationships SET src_entity_id=?1 WHERE src_entity_id=?2");
  FOLD("UPDATE entity_relationships AS r SET"
       " weight = r.weight + (SELECT l.weight FROM entity_relationships l"
       "   WHERE l.dst_entity_id=?2 AND l.src_entity_id=r.src_entity_id"
       "     AND l.rel_type=r.rel_type),"
       " last_seen_at = datetime('now')"
       " WHERE r.dst_entity_id=?1 AND EXISTS (SELECT 1 FROM entity_relationships l"
       "   WHERE l.dst_entity_id=?2 AND l.src_entity_id=r.src_entity_id"
       "     AND l.rel_type=r.rel_type)");
  REPOINT("UPDATE OR IGNORE entity_relationships SET dst_entity_id=?1 WHERE dst_entity_id=?2");
  #undef FOLD
  #undef REPOINT
  char *aliases = aliases_push(s_al, l_can);
  if (sqlite3_prepare_v2(h, "UPDATE entities SET aliases_json=?1"
      " WHERE entity_id=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, aliases, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, survivor, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  if (sqlite3_prepare_v2(h, "DELETE FROM entities WHERE entity_id=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, loser, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  if (sqlite3_prepare_v2(h, "UPDATE entities SET mention_count="
      "(SELECT COUNT(*) FROM entity_mentions WHERE entity_id=?1)"
      " WHERE entity_id=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, survivor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, survivor, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  /* entityMirror.deleteOne(loser): drop its fts row + uid_map entry */
  sqlite3_int64 lrow = -1;
  if (sqlite3_prepare_v2(h, "SELECT rowid FROM entities_fts_uid_map"
      " WHERE uid=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, loser, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) lrow = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  if (lrow >= 0) {
    if (sqlite3_prepare_v2(h, "DELETE FROM entities_fts WHERE rowid=?1",
                           -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_int64(s, 1, lrow); sqlite3_step(s); sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(h, "DELETE FROM entities_fts_uid_map WHERE uid=?1",
                           -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, loser, -1, SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
    }
  }
  /* Both sides are quarantined here (cross-scope was refused above), so a
   * quarantined survivor must not gain an FTS row it never had. */
  if (!s_q) write_entity_fts(h, survivor, s_can, s_ja, s_ro, aliases);
  sqlite3_exec(h, "COMMIT", 0, 0, 0);
  free(aliases);
  free(s_can);free(s_ja);free(s_ro);free(s_al);
  free(l_can);free(l_ja);free(l_ro);free(l_al);
  return 1;
}
