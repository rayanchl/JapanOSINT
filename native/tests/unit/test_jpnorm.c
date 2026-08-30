/* tests/unit/test_jpnorm.c — contract test for lib/jpnorm.c and the two
 * places that use it: fts_segment()/fts_query_expr() (write/query symmetry)
 * and entitystore.c (readings, name-order alias, re-key merge).
 *
 * WHAT IS BEING PROTECTED. A document written as 「ＮＴＴドコモ㈱」 must be
 * found by the query `ntt`, `NTTドコモ` and `どこも`; an entity written as
 * 山田太郎 must be found by `yamada`, `taro yamada` and `やまだ`; and two
 * entities that differ only in width/kana script must be ONE entity. Each of
 * those was false before jpnorm existed, and none of them shows up as an
 * error anywhere — search just returns nothing.
 *
 * Includes entitystore.c directly to reach its statics (compute_readings,
 * swapped_name); the harness links every object EXCEPT
 * obj/core/entitystore.o and obj/main.o. See tests/unit/run.sh. */

#include "../../core/entitystore.c"
#include "../../core/fts.h"
#include "../../core/fts_schema.h"

#include <assert.h>

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}

static void eq(const char *got, const char *want, const char *what) {
  int c = got && strcmp(got, want) == 0;
  printf("%s  %s", c ? "  ok  " : "FAIL  ", what);
  if (!c) printf("  (got \"%s\", want \"%s\")", got ? got : "(null)", want);
  printf("\n");
  if (!c) g_fail++;
}

/* fold(in) == want, and fold is idempotent on it. */
static void fold_case(const char *in, const char *want, const char *what) {
  char *f = jpnorm_fold_dup(in);
  eq(f, want, what);
  char *ff = jpnorm_fold_dup(f);
  if (strcmp(f, ff) != 0) { printf("FAIL  not idempotent: %s\n", what); g_fail++; }
  free(f); free(ff);
}

static void test_fold_table(void) {
  printf("-- fold table\n");
  fold_case("ＮＴＴドコモ㈱", "nttどこも株式会社", "full-width Latin + ㈱ + katakana→hiragana");
  fold_case("ｴﾇﾃｨｰﾃｨｰ ﾄﾞｺﾓ", "えぬてぃーてぃー どこも", "half-width kana with dakuten composed, ｰ→ー");
  fold_case("ﾊﾟﾅｿﾆｯｸ", "ぱなそにっく", "handakuten composition, small ッ");
  fold_case("ｳﾞｨｸﾀｰ", "ゔぃくたー", "ｳﾞ → ヴ → ゔ");
  fold_case("東京　都\t渋谷区  ", "東京 都 渋谷区", "ideographic space + tabs collapse, trim");
  fold_case("ＡＢＣ１２３（株）", "abc123(株)", "full-width digits/parens, case fold");
  fold_case("㈲山田〜Ⅻ№①⑩", "有限会社山田~xiino110", "㈲, wave dash, roman, №, circled");
  fold_case("Café ÀÉ Ā", "café àé ā", "Latin-1 / Extended-A lower-cased, diacritics kept");
  fold_case("澤田 沢田", "澤田 沢田", "kanji variants NOT folded");
  fold_case("ヷ・ー", "ヷ・ー", "ヷ, middle dot, long-vowel mark untouched");
  fold_case("", "", "empty");
  fold_case("   ", "", "whitespace only");
  /* the buffer contract: n bytes never overrun, never splits a sequence */
  char small[8];
  size_t w = jpnorm_fold("株式会社", small, sizeof small);
  ok(w == 6 && small[6] == 0, "bounded write stops at a codepoint boundary");
  /* compat keeps kana script, and fold(compat(x)) == fold(x) */
  char *c = jpnorm_compat_dup("ＮＴＴドコモ㈱");
  eq(c, "nttドコモ株式会社", "compat: width + case, katakana kept");
  char *fc = jpnorm_fold_dup(c), *fx = jpnorm_fold_dup("ＮＴＴドコモ㈱");
  ok(strcmp(fc, fx) == 0, "fold(compat(x)) == fold(x)");
  free(c); free(fc); free(fx);
}

static void test_hepburn(void) {
  printf("-- hepburn\n");
  char *r;
  r = jpnorm_hepburn_dup("ヤマダ タロウ"); eq(r, "yamada taro", "ou → o, per token"); free(r);
  r = jpnorm_hepburn_dup("トウキョウ");     eq(r, "tokyo", "toukyou → tokyo"); free(r);
  r = jpnorm_hepburn_dup("オオサカ");       eq(r, "osaka", "oo → o"); free(r);
  r = jpnorm_hepburn_dup("シブヤ");         eq(r, "shibuya", "shi"); free(r);
  r = jpnorm_hepburn_dup("チャ ジャ シャ キャ"); eq(r, "cha ja sha kya", "youon"); free(r);
  r = jpnorm_hepburn_dup("マッチャ ガッコウ ニッポン"); eq(r, "matcha gakko nippon", "sokuon: tch, kk, pp"); free(r);
  r = jpnorm_hepburn_dup("シンブン");       eq(r, "shinbun", "n plain before b"); free(r);
  r = jpnorm_hepburn_dup("ファ ティ ウィ シェ"); eq(r, "fa ti wi she", "small vowels"); free(r);
  r = jpnorm_hepburn_dup("コーヒー");       eq(r, "kohi", "ー dropped"); free(r);
  r = jpnorm_hepburn_dup("ニイガタ センセイ"); eq(r, "niigata sensei", "ii and ei kept"); free(r);
  r = jpnorm_hepburn_dup("ツヅキ ヲ");      eq(r, "tsuzuki o", "づ zu, を o"); free(r);
  r = jpnorm_hepburn_dup("NTT ドコモ");     eq(r, "ntt dokomo", "Latin passes through"); free(r);
  r = jpnorm_hepburn_dup("山 ヤマ");        eq(r, "山 yama", "unread kanji passes through"); free(r);
}

static void test_readings(void) {
  printf("-- readings (MeCab)\n");
  char *k = fts_reading("山田太郎");
  eq(k, "ヤマダ タロウ", "reading 山田太郎");
  free(k);
  k = fts_reading("ＮＴＴドコモ㈱");
  eq(k, "ntt ドコモ カブシキガイシャ", "reading ＮＴＴドコモ㈱ (compat-folded first: width, case, ㈱)");
  free(k);
  k = fts_reading("東京都渋谷区");
  eq(k, "トウキョウ ト シブヤ ク", "reading 東京都渋谷区");
  free(k);
  char *ja, *ro;
  compute_readings("山田太郎", &ja, &ro);
  eq(ja, "やまだ たろう", "name_ja = hiragana reading");
  eq(ro, "yamada taro", "name_romaji = Hepburn");
  char *sw = swapped_name("person", ro);
  eq(sw, "taro yamada", "person name order swapped");
  free(sw);
  ok(swapped_name("organization", ro) == NULL, "swap only for person");
  ok(swapped_name("person", "yamada") == NULL, "swap only for exactly two tokens");
  free(ja); free(ro);
  compute_readings("NTT Docomo", &ja, &ro);
  ok(ja == NULL && ro && strcmp(ro, "NTT Docomo") == 0, "non-CJK: name_ja NULL, romaji = canonical");
  free(ja); free(ro);
}

static void test_symmetry(void) {
  printf("-- write/query symmetry\n");
  char *seg = fts_segment("ＮＴＴドコモ㈱");
  printf("      segment(ＮＴＴドコモ㈱) = \"%s\"\n", seg);
  ok(strstr(seg, "ntt") == seg, "segmented text starts with folded ntt");
  ok(strstr(seg, "株式会社") != NULL, "㈱ expanded before MeCab");
  free(seg);
  char *a = fts_segment("ヤマダ"), *b = fts_segment("やまだ");
  eq(a, b, "katakana/hiragana pair segments identically");
  printf("      segment(ヤマダ) = \"%s\"\n", a);
  free(a); free(b);
  char *q = fts_query_expr("ＮＴＴ");
  eq(q, "\"ntt\"*", "query folds like the write path");
  free(q);
}

/* End to end through the real tables: a document written full-width is
 * matched by a half-width query, and an entity by its readings. */
static void test_db(void) {
  printf("-- database\n");
  db_handle db = {0};
  if (db_open(&db, NULL, NULL) != 0) { printf("  skip: no scratch db\n"); return; }
  sqlite3 *h = db.h;

  /* intel_items_fts, via the same statement intel.c uses */
  sqlite3_stmt *s;
  char *t = fts_segment("ＮＴＴドコモ㈱の決算");
  int rc = sqlite3_prepare_v2(h, fts_insert_sql(h), -1, &s, NULL);
  ok(rc == SQLITE_OK, "fts insert prepares against the live index");
  if (rc == SQLITE_OK) {
    sqlite3_bind_text(s, 1, "t|1", -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, t, -1, SQLITE_STATIC);
    for (int i = 3; i <= 8; i++) sqlite3_bind_text(s, i, "", -1, SQLITE_STATIC);
    ok(sqlite3_step(s) == SQLITE_DONE, "fts row written");
    sqlite3_finalize(s);
  }
  free(t);
  const char *queries[] = { "ntt", "NTTドコモ", "どこも", "ＮＴＴ", "株式会社" };
  for (size_t i = 0; i < sizeof queries / sizeof *queries; i++) {
    char *q = fts_query_expr(queries[i]);
    long n = -1;
    if (q && sqlite3_prepare_v2(h, "SELECT COUNT(*) FROM intel_items_fts WHERE intel_items_fts MATCH ?1",
                                -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, q, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
      sqlite3_finalize(s);
    }
    char what[128]; snprintf(what, sizeof what, "doc 「ＮＴＴドコモ㈱」 found by `%s` (expr %s)", queries[i], q ? q : "NULL");
    ok(n == 1, what);
    free(q);
  }

  /* entities: readings, swapped order, and width variants collapsing */
  char *e1 = es_upsert_entity(&db, "person", "山田太郎");
  char *e2 = es_upsert_entity(&db, "organization", "ＮＴＴドコモ㈱");
  char *e3 = es_upsert_entity(&db, "organization", "NTTドコモ株式会社");
  char *e4 = es_upsert_entity(&db, "organization", "ヤマダ電機");
  ok(e1 && e2 && e3 && e4, "entities written");
  ok(e2 && e3 && strcmp(e2, e3) == 0, "ＮＴＴドコモ㈱ and NTTドコモ株式会社 are ONE entity");
  if (sqlite3_prepare_v2(h, "SELECT name_ja,name_romaji,norm_key FROM entities WHERE entity_id=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, e1, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      eq((const char *)sqlite3_column_text(s, 0), "やまだ たろう", "stored name_ja");
      eq((const char *)sqlite3_column_text(s, 1), "yamada taro", "stored name_romaji");
      eq((const char *)sqlite3_column_text(s, 2), "山田太郎", "stored norm_key");
    } else ok(0, "entity row readable");
    sqlite3_finalize(s);
  }
  struct { const char *q, *want; } eq_cases[] = {
    { "yamada",      "山田太郎" }, { "taro yamada", "山田太郎" },
    { "yamada taro", "山田太郎" }, { "やまだ",       "山田太郎" },
    { "ntt",         "ＮＴＴドコモ㈱" }, { "dokomo",  "ＮＴＴドコモ㈱" },
    { "ヤマダ",       "ヤマダ電機" },
  };
  for (size_t i = 0; i < sizeof eq_cases / sizeof *eq_cases; i++) {
    char *q = fts_query_expr(eq_cases[i].q);
    int hit = 0;
    if (q && sqlite3_prepare_v2(h, "SELECT entities.canonical FROM entities_fts"
          " JOIN entities ON entities.entity_id=entities_fts.uid"
          " WHERE entities_fts MATCH ?1", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, q, -1, SQLITE_TRANSIENT);
      while (sqlite3_step(s) == SQLITE_ROW) {
        const char *c = (const char *)sqlite3_column_text(s, 0);
        if (c && strcmp(c, eq_cases[i].want) == 0) hit = 1;
      }
      sqlite3_finalize(s);
    }
    char what[160]; snprintf(what, sizeof what, "entity search `%s` finds %s", eq_cases[i].q, eq_cases[i].want);
    ok(hit, what);
    free(q);
  }

  /* re-key pass: plant two rows the OLD derivation would have produced
   * (ws-collapse keys, canonical copied into name_ja), mark the version
   * stale, and check they merge into one with readings. */
  sqlite3_exec(h,
    "INSERT INTO entities(entity_id,type,canonical,norm_key,name_ja,aliases_json,properties)"
    " VALUES('ent_old1','organization','ソニー株式会社','ソニー株式会社','ソニー株式会社','[]','{}'),"
    "       ('ent_old2','organization','ｿﾆｰ㈱','ｿﾆｰ㈱','ｿﾆｰ㈱','[]','{}');"
    "INSERT INTO entity_mentions(entity_id,item_uid,source_id,field,confidence,extractor)"
    " VALUES('ent_old1','t|1','x','body',1,'t'),('ent_old2','t|2','x','body',1,'t');"
    "DELETE FROM entity_norm_state;", 0, 0, 0);
  int merged = es_norm_migrate(&db);
  ok(merged == 1, "re-key merged the width variant (1 pair)");
  long left = -1, mentions = -1; const char *ja = NULL;
  if (sqlite3_prepare_v2(h, "SELECT COUNT(*) FROM entities WHERE norm_key='そにー株式会社'", -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) left = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  ok(left == 1, "one entity holds the folded key");
  if (sqlite3_prepare_v2(h, "SELECT COUNT(*) FROM entity_mentions WHERE entity_id='ent_old1'", -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) mentions = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  ok(mentions == 2, "loser's mention re-pointed to the survivor (nothing discarded)");
  if (sqlite3_prepare_v2(h, "SELECT name_ja FROM entities WHERE entity_id='ent_old1'", -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) { ja = (const char *)sqlite3_column_text(s, 0); eq(ja, "そにー かぶしきがいしゃ", "survivor got readings"); }
    sqlite3_finalize(s);
  }
  ok(es_norm_migrate(&db) == 0, "second run is a no-op (version stamped)");

  free(e1); free(e2); free(e3); free(e4);
  db_close(&db);
}

int main(void) {
  test_fold_table();
  test_hepburn();
  test_readings();
  test_symmetry();
  test_db();
  printf("%s (%d failures)\n", g_fail ? "FAILED" : "PASSED", g_fail);
  return g_fail ? 1 : 0;
}
