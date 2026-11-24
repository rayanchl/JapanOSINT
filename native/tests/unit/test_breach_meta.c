/* test_breach_meta.c — the breach catalogue: slugs, counts, the two loaders,
 * and the source list.
 *
 *   - breach_slug is the identity of every breach_items row, so its folding is
 *     pinned: ASCII lower-casing, runs of non-word chars as one '-', NFKD
 *     folding of Latin ("Café" -> "cafe"), a mark acting as a separator
 *     ("Yonéma" -> "yone-ma"), non-Latin names -> "" (unstorable), no leading
 *     or trailing '-', malformed UTF-8 never overruns, and the buffer bound holds;
 *   - parse_count reads "821.1k" / "6.6M" / "2B" / plain digits and answers -1
 *     for anything else, never a guess;
 *   - seed TSV: header, comment, blank, nameless and unsluggable rows are
 *     counted as skipped (not silently lost), a re-read reports existing vs new,
 *     the sample is bounded by `limit`, an unparsable count is stored as NULL
 *     (not 0), and a missing/empty file is -1;
 *   - manifest JSON: upserts on breach_id, keeps NULL where the file says
 *     nothing, rejects a file with no "breaches" array, skips id-less entries;
 *   - breach_meta_sources joins the item counts; display prefers the stored
 *     item count over the catalogue's claim. */
#include "../../core/breach_meta.c"
#include "core/db.h"

#include <assert.h>
#include <unistd.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}
static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_type(s, 0) == SQLITE_NULL
      ? -999 : (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}
static char g_dir[256];
static const char *wr(const char *name, const char *content) {
  static char path[400];
  snprintf(path, sizeof path, "%s/%s", g_dir, name);
  FILE *f = fopen(path, "wb");
  assert(f);
  fwrite(content, 1, strlen(content), f);
  fclose(f);
  return path;
}
static const char *slug(const char *s) {
  static char b[128];
  breach_slug(s, b, sizeof b);
  return b;
}

int main(void) {
  /* ── slug ────────────────────────────────────────────────────────────── */
  assert(!strcmp(slug("LinkedIn"), "linkedin"));
  assert(!strcmp(slug("Adobe  Systems, Inc."), "adobe-systems-inc"));
  assert(!strcmp(slug("--Foo__Bar--"), "foo__bar") && "'_' is a word char; edge dashes go");
  assert(!strcmp(slug("2012 Dropbox"), "2012-dropbox"));
  assert(!strcmp(slug("Caf\xc3\xa9 Corp"), "cafe-corp") && "e + a mark that separates");
  assert(!strcmp(slug("Yon\xc3\xa9ma"), "yone-ma") && "the combining mark is a separator");
  assert(!strcmp(slug("\xc2\xbc"), "1-4"));
  assert(!strcmp(slug("\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82"), "") &&
         "Cyrillic has no word chars: unstorable");
  assert(!strcmp(slug("\xe6\x97\xa5\xe6\x9c\xac Foo"), "foo") && "CJK drops, no leading dash");
  assert(!strcmp(slug(""), "") && !strcmp(slug("   "), "") && !strcmp(slug("!!!"), ""));
  assert(!strcmp(slug("A\xc3" "B"), "a-b") && "a truncated sequence is a separator, not a read past the end");
  assert(!strcmp(slug("\xff\xfe" "x"), "x"));
  { char tiny[6]; breach_slug("abcdefghijkl", tiny, sizeof tiny);
    assert(strlen(tiny) == 5 && !strcmp(tiny, "abcde") && "the buffer bound holds");
    char z[1] = {'q'}; breach_slug("abc", z, 0); assert(z[0] == 'q'); }
  printf("  slug folding, separators, unstorable names, bounds: ok\n");

  /* ── parse_count ─────────────────────────────────────────────────────── */
  assert(parse_count("126") == 126);
  assert(parse_count("821.1k") == 821100);
  assert(parse_count("6.6M") == 6600000);
  assert(parse_count("2B") == 2000000000LL);
  assert(parse_count("  12 k \r\n") == 12000);
  assert(parse_count("1.5") == 2 && "rounded, as Math.round");
  assert(parse_count("") == -1 && parse_count("abc") == -1 && parse_count("12x") == -1);
  assert(parse_count("1k2") == -1 && parse_count("k") == -1 && parse_count("-5") == -1);
  assert(parse_count("1e3") == -1 && "no exponent form");
  { char big[200]; memset(big, '9', sizeof big - 1); big[sizeof big - 1] = 0;
    assert(parse_count(big) == -1 && "an absurd field is refused, not overflowed"); }
  printf("  count suffixes and refusals: ok\n");

  /* ── database ────────────────────────────────────────────────────────── */
  const char *tmp = getenv("TMPDIR");
  snprintf(g_dir, sizeof g_dir, "%s/jo-bm-XXXXXX", tmp && *tmp ? tmp : "/tmp");
  assert(mkdtemp(g_dir));
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "DELETE FROM breach_meta");
  exec_ok(&db, "DELETE FROM breach_items");

  const char *tsv = wr("seed.tsv",
    "# comment line\n"
    "name\trecords\tadded\tbreached\n"
    "\n"
    "LinkedIn\t164M\t2016-05-21\t2012-05\n"
    "Adobe\t152.4M\t2013-12-04\t2013-10\n"
    "Weird Count\tmany\t2020-01-01\t2020-01\n"
    "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82\t5k\t2021-01-01\t2021-01\n"
    "\t100\t2021-01-01\t2021-01\n"
    "Extra Cols\t7k\t2022-02-02\t2022-02\tignored\tmore\n"
    "NoDates\t9\n");
  breach_seed_stats stats;
  breach_seed_row *sample = NULL; int ns = 0;
  int parsed = breach_meta_parse_seed(&db, tsv, 2, &sample, &ns, &stats);
  assert(parsed == 5 && ns == 2 && "five usable rows, sample bounded by limit");
  assert(stats.rows_parsed == 5 && stats.rows_new == 5 && stats.rows_existing == 0);
  assert(stats.rows_skipped == 3 && "header + unsluggable + nameless are counted, not lost");
  assert(stats.rows_read == 8 && !strcmp(stats.format, "tsv"));
  assert(!strcmp(sample[0].breach_id, "linkedin") && sample[0].pwn_count == 164000000);
  assert(!strcmp(sample[0].added_date, "2016-05-21") && !strcmp(sample[0].breach_date, "2012-05"));
  free(sample);
  assert(breach_meta_parse_seed(&db, tsv, 0, &sample, &ns, NULL) == 5 && sample == NULL && ns == 0
         && "limit 0 counts without sampling");
  assert(scalar(&db, "SELECT COUNT(*) FROM breach_meta") == 0 && "parse writes nothing");

  assert(breach_meta_load_seed_tsv(&db, tsv, &stats) == 5);
  assert(scalar(&db, "SELECT COUNT(*) FROM breach_meta") == 5);
  assert(breach_meta_is_source(&db, "linkedin") && !breach_meta_is_source(&db, "nope")
         && !breach_meta_is_source(&db, "") && !breach_meta_is_source(&db, NULL)
         && !breach_meta_is_source(NULL, "linkedin"));
  assert(scalar(&db, "SELECT pwn_count FROM breach_meta WHERE breach_id='weird-count'") == -999
         && "an unreadable count is NULL, never a fabricated 0");
  assert(scalar(&db, "SELECT pwn_count FROM breach_meta WHERE breach_id='extra-cols'") == 7000);
  assert(scalar(&db, "SELECT breach_date IS NULL FROM breach_meta WHERE breach_id='nodates'") == 1
         && scalar(&db, "SELECT added_date IS NULL FROM breach_meta WHERE breach_id='nodates'") == 1);
  assert(scalar(&db, "SELECT sensitive FROM breach_meta WHERE breach_id='adobe'") == 0);
  assert(breach_meta_load_seed_tsv(&db, tsv, &stats) == 5 && stats.rows_existing == 5 &&
         stats.rows_new == 0 && "a re-load is an upsert and says so");
  assert(scalar(&db, "SELECT COUNT(*) FROM breach_meta") == 5);
  printf("  seed TSV: skips counted, bounded sample, NULL counts, idempotent: ok\n");

  assert(breach_meta_load_seed_tsv(&db, "/nonexistent/x.tsv", NULL) == -1);
  assert(breach_meta_load_seed_tsv(&db, wr("empty.tsv", ""), NULL) == -1);
  assert(breach_meta_load_seed_tsv(NULL, tsv, NULL) == -1);
  assert(breach_meta_load_seed_tsv(&db, wr("hdr.tsv", "name\trecords\n"), NULL) == 0 &&
         "a header-only file loads nothing and is not an error");
  setenv("JO_BREACH_SEED", "/custom/seed.tsv", 1);
  assert(!strcmp(breach_meta_seed_path(), "/custom/seed.tsv"));
  unsetenv("JO_BREACH_SEED");
  assert(strstr(breach_meta_seed_path(), "breach-corpus.seed.tsv"));
  setenv("JO_BREACH_CORPUS", "", 1);
  assert(strstr(breach_meta_corpus_path(), "breach-corpus.json") && "empty env = default");
  unsetenv("JO_BREACH_CORPUS");

  /* ── manifest ────────────────────────────────────────────────────────── */
  const char *js = wr("m.json",
    "{\"breaches\":["
    "{\"id\":\"linkedin\",\"name\":\"LinkedIn\",\"title\":\"LinkedIn\",\"domain\":\"linkedin.com\","
     "\"breach_date\":\"2012-05-05\",\"added_date\":\"2016-05-21\",\"pwn_count\":164611595,"
     "\"data_classes\":[\"Email addresses\",\"Passwords\"],\"verified\":true,\"sensitive\":false,"
     "\"source_id\":\"hibp\"},"
     "{\"id\":\"brand-new\",\"name\":\"Brand New\"},"
     "{\"name\":\"no id\"},{\"id\":\"\",\"name\":\"empty id\"},{\"id\":5}"
    "]}");
  assert(breach_meta_load_manifest(&db, js) == 2 && "id-less and non-string-id entries are skipped");
  assert(scalar(&db, "SELECT pwn_count FROM breach_meta WHERE breach_id='linkedin'") == 164611595
         && "the manifest upserts over the seed row");
  assert(scalar(&db, "SELECT verified FROM breach_meta WHERE breach_id='linkedin'") == 1);
  assert(scalar(&db, "SELECT data_classes_json LIKE '%Passwords%' FROM breach_meta WHERE breach_id='linkedin'") == 1);
  assert(scalar(&db, "SELECT pwn_count IS NULL AND verified IS NULL AND domain IS NULL"
                     " FROM breach_meta WHERE breach_id='brand-new'") == 1
         && "silence in the file is NULL, not a default");
  assert(scalar(&db, "SELECT COUNT(*) FROM breach_meta") == 6);
  assert(breach_meta_load_manifest(&db, wr("bad.json", "{\"nope\":[]}")) == -1);
  assert(breach_meta_load_manifest(&db, wr("junk.json", "{not json")) == -1);
  assert(breach_meta_load_manifest(&db, "/nonexistent/m.json") == -1);
  assert(breach_meta_load_manifest(NULL, js) == -1);
  assert(scalar(&db, "SELECT COUNT(*) FROM breach_meta") == 6 && "a refused file changes nothing");
  printf("  manifest: upsert, NULL for silence, bad files refused: ok\n");

  /* ── the source list and its display ─────────────────────────────────── */
  exec_ok(&db, "INSERT INTO breach_items(keyid,type,value,source_id,hash,first_seen) VALUES"
    "('email:A','email','a@x','linkedin','A','2026-01-01T00:00:00Z'),"
    "('email:B','email','b@x','linkedin','B','2026-03-01T00:00:00Z'),"
    "('email:C','email','c@x','orphan-source','C','2026-02-01T00:00:00Z')");
  int n = -1;
  breach_src_row *R = breach_meta_sources(&db, &n);
  assert(R && n == 6 && "one row per catalogued breach; items of an unknown source are not rows");
  breach_src_row *li = NULL, *ad = NULL;
  for (int i = 0; i < n; i++) {
    if (!strcmp(R[i].breach_id, "linkedin")) li = &R[i];
    if (!strcmp(R[i].breach_id, "adobe")) ad = &R[i];
  }
  assert(li && ad);
  assert(li->item_count == 2 && li->pwn_count == 164611595 &&
         !strcmp(li->last_seen, "2026-03-01T00:00:00Z") && !strcmp(li->domain, "linkedin.com"));
  assert(ad->item_count == 0 && ad->last_seen[0] == 0 && ad->domain[0] == 0);
  long long cnt = 0; const char *fresh = NULL; char desc[160];
  breach_meta_display(li, &cnt, &fresh, desc, sizeof desc);
  assert(cnt == 2 && "stored items beat the catalogue's claim");
  assert(!strcmp(fresh, "2026-03-01T00:00:00Z") && strstr(desc, "164611595 accounts") &&
         strstr(desc, "breached 2012-05-05"));
  breach_meta_display(ad, &cnt, &fresh, desc, sizeof desc);
  assert(cnt == 152400000 && "no items: the catalogue's count is the fallback");
  assert(!strcmp(fresh, "2013-12-04") && strstr(desc, "breached 2013-10"));
  breach_meta_display(NULL, &cnt, &fresh, desc, sizeof desc);   /* must not crash */
  char tinyd[8];
  breach_meta_display(li, NULL, NULL, tinyd, sizeof tinyd);
  assert(strlen(tinyd) < sizeof tinyd);
  free(R);
  exec_ok(&db, "DELETE FROM breach_meta");
  n = 7; assert(breach_meta_sources(&db, &n) == NULL && n == 0 && "empty catalogue is NULL/0");
  assert(breach_meta_sources(NULL, &n) == NULL);
  printf("  source list joins item counts; display prefers stored items: ok\n");

  db_close(&db);
  { char cmd[300]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_dir); if (system(cmd)) {} }
  printf("test_breach_meta: all passed\n");
  return 0;
}
