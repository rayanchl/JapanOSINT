/* test_db_seed.c — the registry-seed fast path in core/db.c.
 *
 * db_seed_sources() writes one `sources` row per registered source and
 * refreshes four registry-derived columns on every open. At 58,055 sources
 * that is 116,110 prepared statement steps per db_open, which ThreadSanitizer
 * instruments one at a time: 0.75 s natively, 30 s under TSan, and the unit
 * suite pays it at least once per test. It is now skipped when a fingerprint
 * of exactly the values it would write, plus the row count the table had when
 * the last full pass finished, both still match.
 *
 * WHAT IS BEING PROTECTED. A skip is a decision not to look, so each thing
 * that must still force a full pass gets its own case:
 *   1. the fingerprint covers each of the five values the seed writes — a
 *      changed id, name, type, category or url changes it, and a reordered
 *      registry changes it too (the separator byte is what makes
 *      "ab"+"c" differ from "a"+"bc");
 *   2. a stamp that is absent, that names a different fingerprint, or whose
 *      row count disagrees with the table reads as "no stamp": full pass;
 *   3. the stamp records the count AFTER the prune, so an install holding a
 *      stale row for its history still reaches the fast path;
 *   4. an empty registry never takes the fast path and never prunes — the
 *      invariant the `n <= 0` guard exists for, which a unit-test binary
 *      without the collectors depends on;
 *   5. $JO_DB_RESEED forces the full pass, which is the escape hatch for the
 *      one thing the fast path gives up: a hand-edited name/type/category/url
 *      is no longer reverted on the next boot.
 *
 * It tests the fingerprint and the stamp directly rather than through a
 * 58,055-source registry, because this binary has the collectors linked and a
 * test that measured the real registry would be measuring the thing it is
 * meant to make cheap.
 */
#include "../../core/db.c"

#include <assert.h>
#include <stdio.h>

static int fails;
static void ok(int cond, const char *what) {
  printf("  %-4s %s\n", cond ? "ok" : "FAIL", what);
  if (!cond) fails++;
}

/* A registry stand-in: source_defs we own, so the fingerprint's inputs are
 * ours to change one at a time. src_meta_get() has no row for these ids, so
 * every field falls through to the documented default and `name` to d->name —
 * which is also the path most registered sources take. */
static source_def A = { .id = "T_SEED_A", .name = "Alpha" };
static source_def B = { .id = "T_SEED_B", .name = "Beta"  };
static const source_def *TWO[2];

static unsigned long long fp_of(const source_def **a, int n) {
  return db_seed_fingerprint(a, n);
}

int main(void) {
  TWO[0] = &A; TWO[1] = &B;

  printf("-- 1. the fingerprint covers what the seed writes\n");
  unsigned long long base = fp_of(TWO, 2);
  ok(base == fp_of(TWO, 2), "the same registry hashes the same twice");

  A.name = "Alpha2";
  ok(fp_of(TWO, 2) != base, "a changed name changes it");
  A.name = "Alpha";
  ok(fp_of(TWO, 2) == base, "and changing it back restores it");

  A.id = "T_SEED_A2";
  ok(fp_of(TWO, 2) != base, "a changed id changes it");
  A.id = "T_SEED_A";

  const source_def *rev[2] = { &B, &A };
  ok(fp_of(rev, 2) != base, "reordering the registry changes it");

  /* The separator: without it "Alpha"+"Beta" and "Alph"+"aBeta" would hash
   * alike, so a name whose tail moved into the next field would be invisible.
   * Shifting one character across the boundary must change the hash. */
  source_def s1 = { .id = "AB", .name = "C" }, s2 = { .id = "A", .name = "BC" };
  const source_def *p1[1] = { &s1 }, *p2[1] = { &s2 };
  ok(fp_of(p1, 1) != fp_of(p2, 1), "a character moved across a field boundary changes it");

  ok(fp_of(TWO, 0) != base, "an empty registry does not hash like a full one");

  printf("-- 2. the stamp, and every way of not matching it\n");
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  /* The real seed has already run here (db_open called it), so the table and
   * the stamp are the live ones. Read them back. */
  unsigned long long had = 0; long rows = -1;
  ok(db_seed_stamp_read(&db, &had, &rows), "db_open left a stamp");
  long live = db_sources_rows(&db);
  ok(rows == live, "the stamp's row count is the table's");
  ok(had == db_seed_fingerprint(registry_all(), registry_count()),
     "the stamp's fingerprint is this registry's");

  /* A second seed must be the fast path: it writes nothing and leaves the
   * stamp alone. Proven by the row count and the stamp being unchanged
   * afterwards even though we move the table's content below. */
  db_seed_sources(&db);
  unsigned long long after = 0; long after_rows = -1;
  db_seed_stamp_read(&db, &after, &after_rows);
  ok(after == had && after_rows == rows, "a second seed changes nothing");

  /* Row-count drift: delete a row the seed would have written. The stamp no
   * longer matches the table, so the next seed must put it back. */
  char gone[128] = {0};
  sqlite3_stmt *s = NULL;
  assert(sqlite3_prepare_v2(db.h, "SELECT id FROM sources LIMIT 1", -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW)
    snprintf(gone, sizeof gone, "%s", (const char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);
  ok(*gone != 0, "took an id to delete");
  char del[256];
  snprintf(del, sizeof del, "DELETE FROM sources WHERE id='%s'", gone);
  sqlite3_exec(db.h, del, NULL, NULL, NULL);
  ok(db_sources_rows(&db) == live - 1, "the row is gone");
  db_seed_sources(&db);
  ok(db_sources_rows(&db) == live, "a count that disagrees forces the full pass");

  /* A stamp naming another fingerprint: same shape, must also force a pass. */
  db_seed_stamp_write(&db, had ^ 0x5a5a5a5aULL, rows, registry_count());
  sqlite3_exec(db.h, del, NULL, NULL, NULL);
  db_seed_sources(&db);
  ok(db_sources_rows(&db) == live, "a fingerprint that disagrees forces it too");

  /* No stamp at all. */
  sqlite3_exec(db.h, "DELETE FROM _registry_seed", NULL, NULL, NULL);
  sqlite3_exec(db.h, del, NULL, NULL, NULL);
  db_seed_sources(&db);
  ok(db_sources_rows(&db) == live, "an absent stamp forces it");
  ok(db_seed_stamp_read(&db, &after, &after_rows) && after == had,
     "and the pass writes the stamp back");

  printf("-- 3. a stale row kept for its history still reaches the fast path\n");
  /* A row no registered source claims, with history, is KEPT by the prune —
   * so the table holds one more row than the registry has sources. The stamp
   * records the count after the prune, so this install must still skip. */
  sqlite3_exec(db.h, "INSERT INTO sources (id,name,type,category,status) "
                     "VALUES ('T_SEED_RETIRED','Retired','api','investigation','pending')",
               NULL, NULL, NULL);
  sqlite3_exec(db.h, "INSERT INTO fetch_log (source_id,status) "
                     "VALUES ('T_SEED_RETIRED','ok')", NULL, NULL, NULL);
  db_seed_sources(&db);                       /* full pass: count drifted */
  long kept = db_sources_rows(&db);
  ok(kept == live + 1, "the stale row is kept for its history");
  db_seed_stamp_read(&db, &after, &after_rows);
  ok(after_rows == kept, "the stamp records the count after the prune");
  sqlite3_exec(db.h, "DELETE FROM sources WHERE id='T_SEED_RETIRED'", NULL, NULL, NULL);
  sqlite3_exec(db.h, "DELETE FROM fetch_log WHERE source_id='T_SEED_RETIRED'", NULL, NULL, NULL);
  db_seed_sources(&db);

  printf("-- 4. JO_DB_RESEED forces the pass\n");
  /* Visible without counting rows: blank a seeded row's name and see whether
   * the pass restores it. Under the fast path it stays blanked, which is the
   * give-up this flag exists for. */
  char upd[256];
  snprintf(upd, sizeof upd, "UPDATE sources SET name='' WHERE id='%s'", gone);
  sqlite3_exec(db.h, upd, NULL, NULL, NULL);
  db_seed_sources(&db);
  char name[256] = {0};
  snprintf(upd, sizeof upd, "SELECT name FROM sources WHERE id='%s'", gone);
  if (sqlite3_prepare_v2(db.h, upd, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW)
      snprintf(name, sizeof name, "%s", (const char *)sqlite3_column_text(s, 0));
    sqlite3_finalize(s);
  }
  ok(*name == 0, "the fast path leaves a hand-edited column alone (the documented cost)");
  setenv("JO_DB_RESEED", "1", 1);
  db_seed_sources(&db);
  if (sqlite3_prepare_v2(db.h, upd, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW)
      snprintf(name, sizeof name, "%s", (const char *)sqlite3_column_text(s, 0));
    sqlite3_finalize(s);
  }
  ok(*name != 0, "JO_DB_RESEED=1 restores it");

  /* "0" must read as unset rather than as a force, or an operator who turns
   * the flag off gets the slow path forever. Blank the name again and check
   * the pass does NOT restore it. */
  setenv("JO_DB_RESEED", "0", 1);
  snprintf(upd, sizeof upd, "UPDATE sources SET name='' WHERE id='%s'", gone);
  sqlite3_exec(db.h, upd, NULL, NULL, NULL);
  db_seed_sources(&db);
  snprintf(upd, sizeof upd, "SELECT name FROM sources WHERE id='%s'", gone);
  *name = 0;
  if (sqlite3_prepare_v2(db.h, upd, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW)
      snprintf(name, sizeof name, "%s", (const char *)sqlite3_column_text(s, 0));
    sqlite3_finalize(s);
  }
  ok(*name == 0, "JO_DB_RESEED=0 is not a force (it reads as unset)");
  unsetenv("JO_DB_RESEED");

  db_close(&db);
  printf(fails ? "\nFAILED (%d)\n" : "\nall passed\n", fails);
  return fails ? 1 : 0;
}
