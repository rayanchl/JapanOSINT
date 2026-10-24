/* tests/unit/seed_template.c — open $JO_DB once, so run.sh can copy the
 * result instead of making every test derive it again.
 *
 * NOT a test: the glob in run.sh is `tests/unit/test_*.c`, so this file is
 * compiled and run exactly once per suite, by name.
 *
 * db_open() applies core/schema.sql, runs every boot migration and seeds the
 * `sources` table from the registry. The seed is 58,055 sources x two prepared
 * statement steps, which ThreadSanitizer instruments one by one: measured
 * 2026-10-24 at 0.75 s natively and **30 s under TSan**, paid by every one of
 * the suite's 88 databases. Doing it once and copying the file leaves each
 * test with the database it would have built for itself — and with core/db.c's
 * registry-seed fingerprint in `_registry_seed`, its own db_open() then
 * matches that stamp and skips the pass in 0.07 s rather than redoing it.
 *
 * Nothing else is written here: no items, no rules, no settings. A test that
 * starts from this file sees exactly what it saw before — schema, migrations
 * and a seeded sources table — because that is all its own first db_open()
 * produced. */
#include "../../core/db.h"
#include <stdio.h>

int main(void) {
  db_handle db = {0};
  if (db_open(&db, NULL, NULL) != 0) {
    fprintf(stderr, "seed_template: db_open failed\n");
    return 1;
  }
  db_close(&db);
  return 0;
}
