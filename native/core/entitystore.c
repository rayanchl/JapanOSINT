/* core/entitystore.c — faithful port of server/src/utils/entityStore.js
 * write surface. See header. SQL is verbatim from entityStore.js. */
#include "entitystore.h"
#include "fts.h"
#include "../lib/jpnorm.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

/* The tier-1 identity key. Used to be whitespace collapse only ("NFKC
 * intentionally omitted"), so 「ＮＴＴドコモ㈱」 and 「NTTドコモ株式会社」 were
 * two entities with two mention counts and two graphs. Now the full
 * jpnorm_fold(): width, kana script, Latin case, whitespace. Kanji variants
 * are still NOT folded (see jpnorm.h) — that remains the resolver's job. */
void es_norm_key(const char *v, char *out, size_t n) {
  jpnorm_fold(v ? v : "", out, n);
}

static char *dup_norm(const char *v) { return jpnorm_fold_dup(v ? v : ""); }

/* The DISPLAY form: whitespace collapsed and trimmed, nothing else. The
 * canonical must stay what the source wrote — folding it would show an
 * analyst "ntt docomo" for an entity the corpus calls "NTT Docomo". */
static char *dup_display(const char *v) {
  size_t L = v ? strlen(v) : 0;
  char *out = malloc(L + 1);
  if (!out) return NULL;
  size_t w = 0; int sp = 0, started = 0;
  for (const char *p = v ? v : ""; *p; p++) {
    unsigned char c = (unsigned char)*p;
    int ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
              c == '\f' || c == '\v');
    if (ws) { sp = 1; continue; }
    if (sp && started) out[w++] = ' ';
    sp = 0; started = 1;
    out[w++] = (char)c;
  }
  out[w] = 0;
  return out;
}

/* entityStore CJK set: /[぀-ヿ㐀-䶿一-鿿豈-﫿]/ (plus half-width katakana,
 * which folds into that set). Shared with lib/jpnorm.c. */
static int has_cjk(const char *s) { return jpnorm_has_cjk(s); }

/* ── readings ────────────────────────────────────────────────────────────
 *
 * For a CJK canonical, name_ja is its HIRAGANA reading (MeCab-IPADIC 読み,
 * kana-folded) and name_romaji its Hepburn romaji in jpnorm_hepburn()'s one
 * canonical form — both space-separated per morpheme, so 山田太郎 yields
 * 「やまだ たろう」 and "yamada taro". Both are indexed in entities_fts, which
 * is what lets `yamada` find 山田 and `やまだ` find ヤマダ. Before this the
 * two columns held a copy of the canonical, which indexed nothing new.
 *
 * For a non-CJK canonical, name_ja is NULL and name_romaji is the display
 * canonical, as before. Readings are what MeCab said, never a guess; a
 * morpheme it cannot read passes through as its surface (fts_reading). */
static void compute_readings(const char *canon, char **ja, char **ro) {
  *ja = NULL; *ro = NULL;
  if (!has_cjk(canon)) { *ro = strdup(canon); return; }
  char *kata = fts_reading(canon);
  if (!kata) return;
  *ja = jpnorm_fold_dup(kata);            /* katakana → hiragana */
  *ro = jpnorm_hepburn_dup(kata);
  free(kata);
  if (*ja && !**ja) { free(*ja); *ja = NULL; }
  if (*ro && !**ro) { free(*ro); *ro = NULL; }
}

/* Japanese person names are written family-first and typed either way. For
 * a `person` whose romaji is exactly two tokens, the swapped order is also
 * indexed — as a second string in the keywords column, not a second entity.
 * Returns malloc'd "given family" or NULL when the rule does not apply. */
static char *swapped_name(const char *type, const char *romaji) {
  if (!type || strcmp(type, "person") != 0 || !romaji) return NULL;
  const char *sp = strchr(romaji, ' ');
  if (!sp || sp == romaji || !sp[1] || strchr(sp + 1, ' ')) return NULL;
  size_t a = (size_t)(sp - romaji), b = strlen(sp + 1);
  char *out = malloc(a + b + 2);
  if (!out) return NULL;
  memcpy(out, sp + 1, b); out[b] = ' ';
  memcpy(out + b + 1, romaji, a); out[a + b + 1] = 0;
  return out;
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
static void write_entity_fts(sqlite3 *h, const char *uid, const char *type,
                             const char *canonical,
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
  /* keywords = aliases + the given-family order of a person's romaji. */
  char *sw = swapped_name(type, name_romaji);
  if (sw) {
    size_t a = strlen(jk), b = strlen(sw);
    char *j2 = malloc(a + b + 2);
    if (j2) {
      memcpy(j2, jk, a);
      if (a) j2[a++] = ' ';
      memcpy(j2 + a, sw, b + 1);
      free(jk); jk = j2;
    }
    free(sw);
  }
  char *sc = fts_segment(canonical ? canonical : "");
  char *sj = fts_segment(name_ja ? name_ja : "");
  char *sr = fts_segment(name_romaji ? name_romaji : "");
  char *sk = fts_segment(jk);
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

char *es_upsert_entity(db_handle *db, const char *type, const char *value) {
  char *surface = dup_display(value);       /* what the source wrote, ws-collapsed */
  char *key     = dup_norm(value);          /* jpnorm_fold: the identity */
  if (!surface || !key || !*key) { free(surface); free(key); return NULL; }
  char t[64]; size_t i = 0;
  for (const char *p = type && *type ? type : "unknown"; *p && i < 63; p++)
    t[i++] = (char)tolower((unsigned char)*p);
  t[i] = 0;
  char *canon = strdup(surface);            /* canonical||surface, collapsed */
  sqlite3 *h = db->h; sqlite3_stmt *s;

  /* SELECT * FROM entities WHERE type=? AND norm_key=? */
  char *ex_id = NULL, *ex_canon = NULL, *ex_ja = NULL, *ex_ro = NULL, *ex_al = NULL;
  if (sqlite3_prepare_v2(h,
      "SELECT entity_id,canonical,name_ja,name_romaji,aliases_json"
      " FROM entities WHERE type=?1 AND norm_key=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, t, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, key, -1, SQLITE_TRANSIENT);
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
    /* Readings are a function of the canonical, so an entity that already
     * has them keeps them; one written before readings existed (or whose
     * MeCab call failed) gets them on its next mention. */
    char *nja = NULL, *nro = NULL;
    if (!ex_ja || !ex_ro) compute_readings(ex_canon, &nja, &nro);
    if (sqlite3_prepare_v2(h,
        "UPDATE entities SET last_seen_at=datetime('now'), aliases_json=?2,"
        " name_ja=COALESCE(name_ja,?3), name_romaji=COALESCE(name_romaji,?4)"
        " WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, ex_id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, aliases, -1, SQLITE_TRANSIENT);
      if (nja) sqlite3_bind_text(s, 3, nja, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(s, 3);
      if (nro) sqlite3_bind_text(s, 4, nro, -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(s, 4);
      sqlite3_step(s); sqlite3_finalize(s);
    }
    const char *fja = ex_ja ? ex_ja : nja;
    const char *fro = ex_ro ? ex_ro : nro;
    write_entity_fts(h, ex_id, t, ex_canon, fja, fro, aliases);
    char *ret = strdup(ex_id);
    free(aliases); free(ex_id); free(ex_canon); free(ex_ja); free(ex_ro);
    free(nja); free(nro);
    free(surface); free(key); free(canon);
    return ret;
  }

  char *ja = NULL, *ro = NULL;
  compute_readings(canon, &ja, &ro);
  char *eid = new_entity_id();
  if (sqlite3_prepare_v2(h,
      "INSERT INTO entities (entity_id,type,canonical,norm_key,name_ja,"
      "name_romaji,aliases_json,properties,mention_count,first_seen_at,"
      "last_seen_at,tenant_id) VALUES (?1,?2,?3,?4,?5,?6,'[]','{}',0,"
      "datetime('now'),datetime('now'),NULL)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, eid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, t, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, canon, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, key, -1, SQLITE_TRANSIENT);       /* norm_key */
    if (ja) sqlite3_bind_text(s, 5, ja, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(s, 5);
    if (ro) sqlite3_bind_text(s, 6, ro, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(s, 6);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  write_entity_fts(h, eid, t, canon, ja, ro, "[]");
  free(ja); free(ro);
  free(surface); free(key); free(canon);
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
                   char **ro, char **al, char type[64]) {
  sqlite3_stmt *s; int ok = 0;
  if (sqlite3_prepare_v2(h, "SELECT canonical,name_ja,name_romaji,aliases_json,type"
      " FROM entities WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *c;
      c = (const char *)sqlite3_column_text(s, 0); *canon = strdup(c ? c : "");
      c = (const char *)sqlite3_column_text(s, 1); *ja = c ? strdup(c) : NULL;
      c = (const char *)sqlite3_column_text(s, 2); *ro = c ? strdup(c) : NULL;
      c = (const char *)sqlite3_column_text(s, 3); *al = strdup(c ? c : "[]");
      c = (const char *)sqlite3_column_text(s, 4); snprintf(type, 64, "%s", c ? c : "");
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
  char *s_can=NULL,*s_ja=NULL,*s_ro=NULL,*s_al=NULL; char s_ty[64];
  char *l_can=NULL,*l_ja=NULL,*l_ro=NULL,*l_al=NULL; char l_ty[64];
  if (!get_ent(h, survivor, &s_can,&s_ja,&s_ro,&s_al,s_ty) ||
      !get_ent(h, loser, &l_can,&l_ja,&l_ro,&l_al,l_ty)) {
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
  write_entity_fts(h, survivor, s_ty, s_can, s_ja, s_ro, aliases);
  sqlite3_exec(h, "COMMIT", 0, 0, 0);
  (void)l_ty;
  free(aliases);
  free(s_can);free(s_ja);free(s_ro);free(s_al);
  free(l_can);free(l_ja);free(l_ro);free(l_al);
  return 1;
}

/* ── boot re-key ─────────────────────────────────────────────────────────
 *
 * norm_key, name_ja and name_romaji are derived columns, and the derivation
 * changed (jpnorm_fold, MeCab readings). Every row written by the old
 * derivation is recomputed once, when ENTITY_NORM_VERSION is newer than the
 * version recorded in entity_norm_state. Two entities whose canonicals now
 * fold to the SAME key are the same tier-1 identity, and are merged through
 * es_union_entities — mentions and relationships re-pointed, the loser's
 * canonical kept as an alias, nothing deleted but the duplicate row.
 *
 * Transactions: the per-entity updates run in batches of 500 inside one
 * transaction each; es_union_entities owns its own transaction, so the batch
 * is committed around every merge. The version stamp is written LAST, so an
 * interrupted run simply re-runs next boot — every step is idempotent
 * (recomputing an already-recomputed row is a no-op, and a collision that
 * was already merged no longer exists). Cost is one MeCab call per CJK
 * entity plus one FTS row rewrite per entity: tens of thousands per minute.
 * JO_ENTITY_REKEY=0 defers it for one boot; the stamp stays old, so it
 * re-arms on the next boot without the flag. */

static long long es_scalar(sqlite3 *h, const char *sql) {
  sqlite3_stmt *s; long long v = 0;
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) != SQLITE_OK) return 0;
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

/* The OTHER entity (if any) already holding (type, key). malloc'd or NULL. */
static char *find_collision(sqlite3 *h, const char *type, const char *key,
                            const char *self) {
  sqlite3_stmt *s; char *o = NULL;
  if (sqlite3_prepare_v2(h, "SELECT entity_id FROM entities"
      " WHERE type=?1 AND norm_key=?2 AND entity_id<>?3", -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, type, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, self, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *c = (const char *)sqlite3_column_text(s, 0);
    if (c) o = strdup(c);
  }
  sqlite3_finalize(s);
  return o;
}

int es_norm_migrate(db_handle *db) {
  if (!db || !db->h) return 0;
  sqlite3 *h = db->h;
  sqlite3_exec(h, "CREATE TABLE IF NOT EXISTS entity_norm_state("
                  "k TEXT PRIMARY KEY, v TEXT NOT NULL)", 0, 0, 0);
  long long ver = es_scalar(h, "SELECT CAST(v AS INTEGER) FROM entity_norm_state"
                               " WHERE k='norm_version'");
  if (ver >= ENTITY_NORM_VERSION) return 0;

  const char *flag = getenv("JO_ENTITY_REKEY");
  if (flag && (strcmp(flag, "0") == 0 || strcmp(flag, "false") == 0)) {
    fprintf(stderr, "[entity] norm_key is v%lld (< v%d) and JO_ENTITY_REKEY=0 — "
                    "skipping re-key; width/kana variants of one name stay "
                    "separate entities and have no readings until a boot "
                    "without the flag\n", ver, ENTITY_NORM_VERSION);
    return 0;
  }

  long long total = es_scalar(h, "SELECT COUNT(*) FROM entities");
  struct timeval tv0; gettimeofday(&tv0, NULL);
  fprintf(stderr, "[entity] re-keying %lld entities v%lld -> v%d (norm_key, "
                  "readings, entities_fts; collisions merged via union)\n",
          total, ver, ENTITY_NORM_VERSION);

  /* Snapshot the id list first: the walk deletes rows (merges) and rewrites
   * the column the UNIQUE index is on, neither of which a live cursor over
   * `entities` should see. */
  size_t nids = 0, cap = total > 0 ? (size_t)total : 16;
  char **ids = malloc(cap * sizeof *ids);
  if (!ids) return 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "SELECT entity_id FROM entities ORDER BY entity_id",
                         -1, &s, NULL) == SQLITE_OK) {
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *c = (const char *)sqlite3_column_text(s, 0);
      if (!c) continue;
      if (nids == cap) {
        char **ni = realloc(ids, cap * 2 * sizeof *ids);
        if (!ni) break;
        ids = ni; cap *= 2;
      }
      ids[nids++] = strdup(c);
    }
    sqlite3_finalize(s);
  }

  long long rekeyed = 0, merged = 0, batch = 0;
  int in_txn = (sqlite3_exec(h, "BEGIN IMMEDIATE", 0, 0, 0) == SQLITE_OK);
  for (size_t i = 0; i < nids; i++) {
    char *target = strdup(ids[i]);
    char *canon = NULL, *ja = NULL, *ro = NULL, *al = NULL; char ty[64];
    if (!get_ent(h, target, &canon, &ja, &ro, &al, ty)) { free(target); continue; } /* merged away */
    free(ja); free(ro); ja = ro = NULL;
    char *key = dup_norm(canon);

    /* Collision: the folded key is already held by another entity of the
     * same type. Merge, then carry on with the survivor — whose canonical
     * (and therefore key) may be the other one's, so recompute. Bounded:
     * each pass removes one entity, so it cannot loop. */
    for (int guard = 0; guard < 16; guard++) {
      char *other = key ? find_collision(h, ty, key, target) : NULL;
      if (!other) break;
      if (in_txn) { sqlite3_exec(h, "COMMIT", 0, 0, 0); in_txn = 0; }
      const char *survivor = strcmp(target, other) < 0 ? target : other;
      char *surv = strdup(survivor);
      if (es_union_entities(db, target, other)) merged++;
      free(other);
      free(target); target = surv;
      free(canon); free(al); free(key);
      canon = al = key = NULL;
      if (!get_ent(h, target, &canon, &ja, &ro, &al, ty)) break;
      free(ja); free(ro); ja = ro = NULL;
      key = dup_norm(canon);
    }
    if (!in_txn) in_txn = (sqlite3_exec(h, "BEGIN IMMEDIATE", 0, 0, 0) == SQLITE_OK);
    if (!canon || !key) { free(target); free(canon); free(al); free(key); continue; }

    compute_readings(canon, &ja, &ro);
    if (sqlite3_prepare_v2(h, "UPDATE entities SET norm_key=?2, name_ja=?3,"
        " name_romaji=?4 WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, target, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, key, -1, SQLITE_TRANSIENT);
      if (ja) sqlite3_bind_text(s, 3, ja, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 3);
      if (ro) sqlite3_bind_text(s, 4, ro, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 4);
      if (sqlite3_step(s) == SQLITE_DONE) rekeyed++;
      else fprintf(stderr, "[entity] re-key of %s failed: %s\n", target, sqlite3_errmsg(h));
      sqlite3_finalize(s);
    }
    write_entity_fts(h, target, ty, canon, ja, ro, al);
    free(target); free(canon); free(ja); free(ro); free(al); free(key);

    if (++batch % 500 == 0 && in_txn) {
      sqlite3_exec(h, "COMMIT", 0, 0, 0);
      in_txn = (sqlite3_exec(h, "BEGIN IMMEDIATE", 0, 0, 0) == SQLITE_OK);
    }
  }
  for (size_t i = 0; i < nids; i++) free(ids[i]);
  free(ids);

  if (!in_txn) in_txn = (sqlite3_exec(h, "BEGIN IMMEDIATE", 0, 0, 0) == SQLITE_OK);
  if (sqlite3_prepare_v2(h, "INSERT INTO entity_norm_state(k,v) VALUES('norm_version',?1)"
      " ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &s, NULL) == SQLITE_OK) {
    char v[16]; snprintf(v, sizeof v, "%d", ENTITY_NORM_VERSION);
    sqlite3_bind_text(s, 1, v, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
  if (in_txn) sqlite3_exec(h, "COMMIT", 0, 0, 0);

  struct timeval tv1; gettimeofday(&tv1, NULL);
  double secs = (double)(tv1.tv_sec - tv0.tv_sec) + (double)(tv1.tv_usec - tv0.tv_usec) / 1e6;
  fprintf(stderr, "[entity] re-key done: %lld re-keyed, %lld merged (of %lld) in %.1fs\n",
          rekeyed, merged, total, secs);
  return (int)merged;
}
