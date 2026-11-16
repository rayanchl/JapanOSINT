/* test_intel_redact.c — a credential's VALUE never reaches a stored record.
 *
 * Rows put API keys in request URLs, collectors copy URLs into what they store
 * (a fetch-failure record's `url`, a notice's `failed_page_url`), and those
 * rows live in the shared corpus every user can read. core/intel.c's emit()
 * replaces the value of every credential-named environment variable before a
 * row is written (audit 2026-11-07). This pins the redaction, its control (a
 * non-secret variable and a short value are left alone), and that the row is
 * still stored. */
#include "../../core/intel.c"
#include <assert.h>

static char *col(db_handle *db, const char *uid, const char *c) {
  char sql[160];
  snprintf(sql, sizeof sql, "SELECT %s FROM intel_items WHERE uid=?1", c);
  sqlite3_stmt *s = NULL;
  char *out = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0))
      out = strdup((const char *)sqlite3_column_text(s, 0));
  }
  sqlite3_finalize(s);
  return out;
}

int main(void) {
  db_handle db = {0};
  const char *path = getenv("JO_DB");
  assert(path && "run.sh sets JO_DB to a scratch database");
  if (db_open(&db, path, NULL) != 0) { fprintf(stderr, "db_open failed\n"); return 2; }

  setenv("UNIT_REDACT_API_KEY", "sk-unit-0123456789abcdef", 1);
  setenv("UNIT_REDACT_REGION", "sk-unit-0123456789abcdef-not-a-secret-name", 1);
  setenv("UNIT_SHORT_TOKEN", "abc123", 1);          /* too short to be one */
  intel_secrets_reset();

  intel_sink k = intel_sink_make(&db, "unit-redact", NULL);
  intel_item it = {0};
  it.remote_key = "r1";
  it.title = "fetch failed";
  it.link = "https://api.example.test/v1?api_key=sk-unit-0123456789abcdef&q=x";
  it.body = "token abc123 stays: it is shorter than any key we redact";
  it.record_type = "collector-fetch-failure";
  it.properties_json =
    "{\"url\":\"https://api.example.test/v1/sk-unit-0123456789abcdef/list\"}";
  assert(k.emit(&k, &it) >= 0);

  char *link = col(&db, "unit-redact|r1", "link");
  char *props = col(&db, "unit-redact|r1", "properties");
  char *body = col(&db, "unit-redact|r1", "body");
  assert(link && props && body);
  assert(!strstr(link, "sk-unit-0123456789abcdef"));
  assert(strstr(link, "api_key=[redacted]&q=x"));            /* the rest kept */
  assert(!strstr(props, "sk-unit-0123456789abcdef"));         /* in the path too */
  assert(strstr(props, "/v1/[redacted]/list"));
  assert(strstr(body, "abc123"));                             /* short: untouched */
  printf("  ok   a credential value is redacted from link and properties\n");
  printf("  ok   a short value and the rest of the URL are left alone\n");
  free(link); free(props); free(body);

  /* The control on the NAME: a long value under a name that is not a
   * credential (…_REGION) is not treated as one — the redaction keys on the
   * secret-named variable, so text equal to it is redacted only because the
   * secret's value is a prefix of it. Use a value with no secret inside. */
  unsetenv("UNIT_REDACT_API_KEY");
  setenv("UNIT_REDACT_REGION", "eu-west-unit-region-0001", 1);
  intel_secrets_reset();
  intel_item j = it;
  j.remote_key = "r2";
  j.link = "https://api.example.test/v1?region=eu-west-unit-region-0001";
  j.properties_json = "{}";
  assert(k.emit(&k, &j) >= 0);
  link = col(&db, "unit-redact|r2", "link");
  assert(link && strstr(link, "eu-west-unit-region-0001"));
  printf("  ok   a long value under a non-credential name is not redacted\n");
  free(link);

  intel_sink_free(&k);
  db_close(&db);
  printf("all passed\n");
  return 0;
}
