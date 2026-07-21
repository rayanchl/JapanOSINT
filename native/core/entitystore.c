/* core/entitystore.c — faithful port of server/src/utils/entityStore.js
 * write surface. See header. SQL is verbatim from entityStore.js. */
#include "entitystore.h"
#include "fts.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

/* entityStore CJK set: /[぀-ヿ㐀-䶿一-鿿豈-﫿]/ over UTF-8. */
static int has_cjk(const char *s) {
  if (!s) return 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; ) {
    unsigned cp, len;
    if (*p < 0x80) { cp = *p; len = 1; }
    else if ((*p >> 5) == 0x6) { cp = *p & 0x1F; len = 2; }
    else if ((*p >> 4) == 0xE) { cp = *p & 0x0F; len = 3; }
    else if ((*p >> 3) == 0x1E) { cp = *p & 0x07; len = 4; }
    else { p++; continue; }
    for (unsigned i = 1; i < len; i++) {
      if ((p[i] & 0xC0) != 0x80) { len = i; break; }
      cp = (cp << 6) | (p[i] & 0x3F);
    }
    if ((cp >= 0x3040 && cp <= 0x30FF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xF900 && cp <= 0xFAFF))
      return 1;
    p += len;
  }
  return 0;
}

static char *new_entity_id(void) {
  unsigned char b[8];
  if (RAND_bytes(b, 8) != 1) for (int i = 0; i < 8; i++) b[i] = (unsigned char)rand();
  char *id = malloc(21);
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
  size_t cap = 64, len = 0; char *buf = malloc(cap); buf[0] = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, a) {
    if (!cJSON_IsString(e) || !e->valuestring[0]) continue;
    size_t need = len + strlen(e->valuestring) + 2;
    if (need > cap) { while (need > cap) cap *= 2; buf = realloc(buf, cap); }
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
  if (old >= 0 && sqlite3_prepare_v2(h, "DELETE FROM entities_fts WHERE rowid=?1",
                                     -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(s, 1, old); sqlite3_step(s); sqlite3_finalize(s);
  }
  char *jk = join_aliases(aliases_json);
  char *sc = fts_segment(canonical ? canonical : "");
  char *sj = fts_segment(name_ja ? name_ja : "");
  char *sr = fts_segment(name_romaji ? name_romaji : "");
  char *sk = fts_segment(jk);
  if (sqlite3_prepare_v2(h,
      "INSERT INTO entities_fts(uid,canonical,name_ja,name_romaji,keywords)"
      " VALUES(?1,?2,?3,?4,?5)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, sc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, sj, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, sr, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 5, sk, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
  free(jk); free(sc); free(sj); free(sr); free(sk);
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

char *es_upsert_entity(db_handle *db, const char *type, const char *value) {
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
  if (sqlite3_prepare_v2(h,
      "SELECT entity_id,canonical,name_ja,name_romaji,aliases_json"
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
    }
    sqlite3_finalize(s);
  }

  if (ex_id) {
    char *aliases = ex_al;
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
    write_entity_fts(h, ex_id, ex_canon, fja, fro, aliases);
    char *ret = strdup(ex_id);
    free(aliases); free(ex_id); free(ex_canon); free(ex_ja); free(ex_ro);
    free(surface); free(canon);
    return ret;
  }

  char *eid = new_entity_id();
  if (sqlite3_prepare_v2(h,
      "INSERT INTO entities (entity_id,type,canonical,norm_key,name_ja,"
      "name_romaji,aliases_json,properties,mention_count,first_seen_at,"
      "last_seen_at,tenant_id) VALUES (?1,?2,?3,?4,?5,?6,'[]','{}',0,"
      "datetime('now'),datetime('now'),NULL)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, eid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, t, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, canon, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, surface, -1, SQLITE_TRANSIENT);   /* norm_key */
    if (isja) sqlite3_bind_text(s, 5, canon, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(s, 5);
    if (isja) sqlite3_bind_null(s, 6);
    else sqlite3_bind_text(s, 6, canon, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  write_entity_fts(h, eid, canon, isja ? canon : NULL, isja ? NULL : canon, "[]");
  free(surface); free(canon);
  return eid;
}

void es_add_mention(db_handle *db, const char *eid, const char *item_uid,
                    const char *source_id, const char *surface,
                    const char *field, double confidence,
                    const char *extractor) {
  if (!eid || !item_uid) return;
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
      "field,confidence,extractor,created_at) VALUES"
      " (?1,?2,?3,?4,?5,?6,?7,datetime('now'))"
      " ON CONFLICT(entity_id,item_uid,field) DO UPDATE SET"
      " confidence=MAX(confidence,excluded.confidence),"
      " surface=excluded.surface", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, eid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, item_uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, (source_id && *source_id) ? source_id : "unknown", -1, SQLITE_TRANSIENT);
    if (surfp) sqlite3_bind_text(s, 4, surfp, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 4);
    sqlite3_bind_text(s, 5, fld, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(s, 6, (confidence == confidence) ? confidence : 0.5);
    sqlite3_bind_text(s, 7, (extractor && *extractor) ? extractor : "ner-llm", -1, SQLITE_TRANSIENT);
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
                   char **ro, char **al) {
  sqlite3_stmt *s; int ok = 0;
  if (sqlite3_prepare_v2(h, "SELECT canonical,name_ja,name_romaji,aliases_json"
      " FROM entities WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *c;
      c = (const char *)sqlite3_column_text(s, 0); *canon = strdup(c ? c : "");
      c = (const char *)sqlite3_column_text(s, 1); *ja = c ? strdup(c) : NULL;
      c = (const char *)sqlite3_column_text(s, 2); *ro = c ? strdup(c) : NULL;
      c = (const char *)sqlite3_column_text(s, 3); *al = strdup(c ? c : "[]");
      ok = 1;
    }
    sqlite3_finalize(s);
  }
  return ok;
}

int es_union_entities(db_handle *db, const char *idA, const char *idB) {
  const char *survivor = strcmp(idA, idB) < 0 ? idA : idB;
  const char *loser    = strcmp(idA, idB) < 0 ? idB : idA;
  sqlite3 *h = db->h;
  char *s_can=NULL,*s_ja=NULL,*s_ro=NULL,*s_al=NULL;
  char *l_can=NULL,*l_ja=NULL,*l_ro=NULL,*l_al=NULL;
  if (!get_ent(h, survivor, &s_can,&s_ja,&s_ro,&s_al) ||
      !get_ent(h, loser, &l_can,&l_ja,&l_ro,&l_al)) {
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
  REPOINT("UPDATE OR IGNORE entity_mentions SET entity_id=?1 WHERE entity_id=?2");
  REPOINT("UPDATE OR IGNORE entity_relationships SET src_entity_id=?1 WHERE src_entity_id=?2");
  REPOINT("UPDATE OR IGNORE entity_relationships SET dst_entity_id=?1 WHERE dst_entity_id=?2");
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
  write_entity_fts(h, survivor, s_can, s_ja, s_ro, aliases);
  sqlite3_exec(h, "COMMIT", 0, 0, 0);
  free(aliases);
  free(s_can);free(s_ja);free(s_ro);free(s_al);
  free(l_can);free(l_ja);free(l_ro);free(l_al);
  return 1;
}
