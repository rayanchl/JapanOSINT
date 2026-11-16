/* tests/unit/test_url_override.c — core/url_override.c.
 *
 * An operator can redirect a registered collector URL that moved. The danger
 * is the redirect itself: an override table is a way to point the fetch layer
 * at an address the SSRF policy would never let a collector name, so
 * validation must refuse private/metadata/loopback/non-http targets BOTH when
 * an override is created and when one is loaded from the database (a row can
 * be hand-inserted). Also pinned: exact-match application, remove/reset
 * semantics (soft-delete in memory + DELETE in the table), duplicate loads
 * being idempotent and a revived override after remove.
 * Uses IP-literal hosts only, so no DNS is needed. */
#include "../../core/url_override.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

static void ins(db_handle *db, const char *o, const char *n) {
  sqlite3_stmt *s;
  sqlite3_prepare_v2(db->h, "INSERT INTO collector_url_overrides(old_url,new_url) VALUES(?1,?2)", -1, &s, NULL);
  sqlite3_bind_text(s, 1, o, -1, SQLITE_TRANSIENT);
  if (n) sqlite3_bind_text(s, 2, n, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(s, 2);
  sqlite3_step(s); sqlite3_finalize(s);
}
static int rows(db_handle *db) {
  sqlite3_stmt *s; int n = -1;
  sqlite3_prepare_v2(db->h, "SELECT count(*) FROM collector_url_overrides", -1, &s, NULL);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
  sqlite3_finalize(s); return n;
}

int main(void) {
  db_handle db; memset(&db, 0, sizeof db);
  sqlite3_open(":memory:", &db.h);
  sqlite3_exec(db.h, "CREATE TABLE collector_url_overrides (old_url TEXT, new_url TEXT)", 0, 0, 0);

  const char *OLD = "http://8.8.8.8/old/feed.json", *NEW = "https://8.8.4.4/new/feed.json";

  printf("-- validation\n");
  ok(url_override_validate(OLD, NEW) == HG_URL_OK, "public old and new URL accepted");
  ok(url_override_validate(NULL, NEW) != HG_URL_OK && url_override_validate("", NEW) != HG_URL_OK, "missing old URL refused");
  ok(url_override_validate(OLD, NULL) != HG_URL_OK && url_override_validate(OLD, "") != HG_URL_OK, "missing new URL refused");
  ok(url_override_validate(OLD, "http://127.0.0.1/x") == HG_URL_PRIVATE, "redirect to loopback refused");
  ok(url_override_validate(OLD, "http://169.254.169.254/latest/meta-data/") == HG_URL_PRIVATE, "redirect to the cloud metadata address refused");
  ok(url_override_validate(OLD, "http://10.1.2.3/") == HG_URL_PRIVATE, "redirect to RFC1918 refused");
  ok(url_override_validate(OLD, "http://192.168.0.1/") == HG_URL_PRIVATE, "redirect to 192.168/16 refused");
  ok(url_override_validate(OLD, "http://[::1]/") != HG_URL_OK, "redirect to IPv6 loopback refused");
  ok(url_override_validate(OLD, "file:///etc/passwd") == HG_URL_BAD_SCHEME, "file:// refused");
  ok(url_override_validate(OLD, "ftp://8.8.8.8/x") == HG_URL_BAD_SCHEME, "ftp:// refused");
  ok(url_override_validate(OLD, "gopher://8.8.8.8/x") == HG_URL_BAD_SCHEME, "gopher:// refused");
  ok(url_override_validate("file:///etc/passwd", NEW) != HG_URL_OK, "a non-http OLD url is refused too");

  printf("-- load and apply\n");
  ok(url_override_apply(NULL) == NULL, "apply(NULL) -> NULL");
  ok(url_override_apply(OLD) == OLD, "no overrides loaded: apply returns the very same pointer");
  ins(&db, OLD, NEW);
  url_override_reload(&db);
  ok(!strcmp(url_override_apply(OLD), NEW), "loaded override rewrites an exact match");
  ok(!strcmp(url_override_apply("http://8.8.8.8/old/feed.json?x=1"), "http://8.8.8.8/old/feed.json?x=1"),
     "a URL that merely extends the old one is NOT rewritten (exact match only)");
  ok(!strcmp(url_override_apply("http://8.8.8.8/other"), "http://8.8.8.8/other"), "unrelated URL untouched");
  url_override_reload(&db); url_override_reload(&db);
  int cnt = 0; for (ov *p = g_head; p; p = p->next) cnt++;
  ok(cnt == 1, "reloading the same table is idempotent (no duplicate nodes)");

  printf("-- hostile rows in the table\n");
  ins(&db, "http://8.8.8.8/evil", "http://169.254.169.254/latest/meta-data/");
  ins(&db, "http://8.8.8.8/evil2", "file:///etc/shadow");
  ins(&db, "http://8.8.8.8/nullnew", NULL);
  ins(&db, "", "http://8.8.4.4/x");
  url_override_reload(&db);
  ok(!strcmp(url_override_apply("http://8.8.8.8/evil"), "http://8.8.8.8/evil"), "hand-inserted metadata redirect is refused at load, not applied");
  ok(!strcmp(url_override_apply("http://8.8.8.8/evil2"), "http://8.8.8.8/evil2"), "hand-inserted file:// redirect refused at load");
  ok(!strcmp(url_override_apply("http://8.8.8.8/nullnew"), "http://8.8.8.8/nullnew"), "NULL new_url row skipped");
  url_override_reload(NULL);
  { db_handle nodb; memset(&nodb, 0, sizeof nodb); url_override_reload(&nodb); }
  ok(1, "reload with a NULL / handle-less db is a no-op");
  { db_handle bad; memset(&bad, 0, sizeof bad); sqlite3_open(":memory:", &bad.h);
    url_override_reload(&bad); sqlite3_close(bad.h);
    ok(1, "reload when the table does not exist is a no-op"); }

  printf("-- remove / reset\n");
  ok(url_override_remove(&db, "") == 0 && url_override_remove(&db, NULL) == 0, "remove of empty/NULL url -> 0");
  ok(url_override_remove(&db, "http://8.8.8.8/never") == 0, "remove of an unknown url -> 0");
  int before = rows(&db);
  ok(url_override_remove(&db, OLD) == 1, "remove marks the active override dead");
  ok(url_override_apply(OLD) == OLD, "a removed override no longer applies");
  ok(rows(&db) == before - 1, "remove also DELETEs the row so a restart does not resurrect it");
  ok(url_override_remove(&db, OLD) == 0, "removing twice is 0");
  ins(&db, OLD, NEW);
  url_override_reload(&db);
  ok(!strcmp(url_override_apply(OLD), NEW), "re-adding a removed override revives it (no stale dead node shadows it)");
  ins(&db, "http://8.8.8.8/two", "http://8.8.4.4/two");
  url_override_reload(&db);
  ok(url_override_reset(&db) >= 2, "reset kills every active override");
  ok(url_override_apply(OLD) == OLD, "after reset nothing applies");
  ok(rows(&db) == 0, "reset empties the table");
  ok(url_override_reset(&db) == 0 && url_override_reset(NULL) == 0, "reset is repeatable and NULL-safe");
  ok(url_override_remove(NULL, OLD) == 0, "remove with no db is memory-only and safe");

  sqlite3_close(db.h);
  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
