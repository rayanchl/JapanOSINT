/* test_source_settings.c — the per-source operator knobs, and the four ways a
 * setting must refuse to be what it is not.
 *
 * `ods-public-geonames` needs a response-body ceiling above the 64 MB default
 * (171,176 records in a ~111 MB export it cannot page). That started as a
 * #define in the collector; it is now an operator setting, which means the
 * value arrives from a database and every read has to distrust it. Pinned:
 *   1. a key outside the whitelist is refused, and resolving it returns the
 *      caller's default rather than anything stored;
 *   2. a value outside the key's declared range is refused on WRITE and
 *      ignored on READ (a row can arrive from an older build or a hand-edited
 *      database, so the bound is enforced on the way out too);
 *   3. DISABLED is not UNSET: a switched-off knob resolves to the default
 *      while keeping the number the operator tuned, so a re-enable does not
 *      start from a guess;
 *   4. a setting for a source nobody registers is refused — a knob that can
 *      never apply would read as configured.
 * Plus: the JSON the settings page reads states bounds, the stored value, and
 * the EFFECTIVE value, so the page needs no second opinion about any of it. */
#include "../../core/source_settings.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SID "test_srcset_source"
#define BIG (192L * 1024 * 1024)

static void exec_ok(db_handle *db, const char *sql) {
  char *err = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "exec failed: %s\n  %s\n", err ? err : "?", sql);
    assert(0);
  }
}

/* The stored row, read straight out of SQL — so the test checks what is in the
 * table rather than what the module says is in the table. */
static int stored(db_handle *db, long *value, int *enabled) {
  sqlite3_stmt *s = NULL;
  assert(sqlite3_prepare_v2(db->h,
    "SELECT value,enabled FROM source_settings WHERE source_id=?1 AND key=?2",
    -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, SID, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, SRCSET_BODY_CEILING, -1, SQLITE_TRANSIENT);
  int have = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    have = 1;
    *value = (long)sqlite3_column_int64(s, 0);
    *enabled = sqlite3_column_int(s, 1);
  }
  sqlite3_finalize(s);
  return have;
}

static int json_has(const char *j, const char *needle) {
  return j && strstr(j, needle) != NULL;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db,
    "INSERT OR IGNORE INTO sources(id,name,type,category) VALUES"
    " ('" SID "','settings test','api','test');");

  /* ---- 4. a source nobody registers cannot hold a setting --------------- */
  char big[32]; snprintf(big, sizeof big, "%ld", BIG);
  assert(source_settings_put(&db, "test_srcset_absent", SRCSET_BODY_CEILING,
                             big, 1, "unit") == -3);

  /* ---- 1. an unknown key is refused, by name --------------------------- */
  assert(source_settings_put(&db, SID, "http_body_celing_bytes", big, 1,
                             "unit") == -1);           /* note the typo */
  assert(source_settings_put(&db, SID, "", big, 1, "unit") == -1);
  assert(source_settings_put(&db, SID, NULL, big, 1, "unit") == -1);
  /* …and reading one never invents a value. */
  assert(source_settings_int(&db, SID, "http_body_celing_bytes", 7) == 7);

  /* ---- 2a. a value outside the declared range is refused on WRITE ------ */
  assert(source_settings_put(&db, SID, SRCSET_BODY_CEILING,
                             "1048576", 1, "unit") == -2);   /* below the min */
  assert(source_settings_put(&db, SID, SRCSET_BODY_CEILING,
                             "1073741824", 1, "unit") == -2); /* 1 GB, > max */
  /* A number with a tail is not a number: "64x" must not read as 64. */
  assert(source_settings_put(&db, SID, SRCSET_BODY_CEILING,
                             "67108864x", 1, "unit") == -2);
  assert(source_settings_put(&db, SID, SRCSET_BODY_CEILING, "", 1, "unit") == -2);
  assert(source_settings_put(&db, SID, SRCSET_BODY_CEILING, NULL, 1, "unit") == -2);
  /* None of those wrote anything. */
  { long v = 0; int en = 0; assert(stored(&db, &v, &en) == 0); }

  /* ---- the happy path -------------------------------------------------- */
  assert(source_settings_put(&db, SID, SRCSET_BODY_CEILING, big, 1,
                             "unit") == 0);
  { long v = 0; int en = 0;
    assert(stored(&db, &v, &en) == 1);
    assert(v == BIG && en == 1); }
  assert(source_settings_int(&db, SID, SRCSET_BODY_CEILING, 0) == BIG);

  /* ---- 3. DISABLED is not UNSET ---------------------------------------- */
  assert(source_settings_put(&db, SID, SRCSET_BODY_CEILING, big, 0,
                             "unit") == 0);
  /* resolves to the caller's default… */
  assert(source_settings_int(&db, SID, SRCSET_BODY_CEILING, 0) == 0);
  assert(source_settings_int(&db, SID, SRCSET_BODY_CEILING, 123) == 123);
  /* …while the tuned number is still there to re-enable. */
  { long v = 0; int en = 0;
    assert(stored(&db, &v, &en) == 1);
    assert(v == BIG && "a disabled knob must keep the operator's value");
    assert(en == 0); }
  assert(source_settings_put(&db, SID, SRCSET_BODY_CEILING, big, 1,
                             "unit") == 0);
  assert(source_settings_int(&db, SID, SRCSET_BODY_CEILING, 0) == BIG);

  /* ---- 2b. and the bound is enforced on the way OUT -------------------- */
  /* A row from an older build, a restored backup or a hand-edited database:
   * written straight to SQL, past source_settings_put() entirely. */
  exec_ok(&db,
    "UPDATE source_settings SET value='999999999999' WHERE source_id='" SID "';");
  assert(source_settings_int(&db, SID, SRCSET_BODY_CEILING, 55) == 55 &&
         "an out-of-range stored value must resolve to the default");
  exec_ok(&db,
    "UPDATE source_settings SET value='not-a-number' WHERE source_id='" SID "';");
  assert(source_settings_int(&db, SID, SRCSET_BODY_CEILING, 55) == 55);
  exec_ok(&db, "UPDATE source_settings SET value='" "201326592" "' "
               "WHERE source_id='" SID "';");       /* 192 MB, back in range */
  assert(source_settings_int(&db, SID, SRCSET_BODY_CEILING, 0) == BIG);

  /* ---- the JSON the settings page reads -------------------------------- */
  char *j = source_settings_json(&db, SID, 1);
  assert(j);
  assert(json_has(j, SRCSET_BODY_CEILING));
  assert(json_has(j, "\"min\":") && json_has(j, "\"max\":") &&
         json_has(j, "\"default\":"));
  assert(json_has(j, "\"enabled\":true") && json_has(j, "\"configured\":true"));
  /* The page is told the EFFECTIVE value, not left to combine the two. */
  assert(json_has(j, "\"effective\":201326592"));
  assert(json_has(j, "\"updated_by\":\"unit\""));
  assert(json_has(j, "\"can_write\":true"));
  free(j);

  /* A source with no row at all still gets the whole key set, so the page can
   * offer a knob that has never been touched. */
  j = source_settings_json(&db, "test_srcset_untouched", 0);
  assert(j);
  assert(json_has(j, SRCSET_BODY_CEILING));
  assert(json_has(j, "\"configured\":false"));
  assert(json_has(j, "\"enabled\":false"));
  assert(json_has(j, "\"effective\":67108864"));   /* the 64 MB default */
  assert(json_has(j, "\"can_write\":false"));
  free(j);

  db_close(&db);
  printf("test_source_settings: ok\n");
  return 0;
}
