/* test_report_template.c — the writer for tenant_report_templates
 * (core/report_template.c) and the route bodies behind
 * GET/PUT /api/reports/template.
 *
 * WHAT IS BEING PROTECTED.
 *   1. the gate: any member reads; only owner/admin writes (403 for analyst
 *      and viewer, and their refusal changes nothing and audits nothing);
 *   2. validation mirrors the READER: each field's byte bound is reportapi's
 *      buffer less the NUL — so the bound itself is accepted and one byte more
 *      is a 400 — control characters are refused in the one-line fields,
 *      newlines allowed in the footer, logo_url must be https:// with a host
 *      and pass reportapi's safe_url() character rule; unknown keys and
 *      non-string values are refused by name;
 *   3. the round trip, END TO END: a saved template is what report_run()
 *      prints (the banner, header note, footer and the logo <img> in HTML);
 *   4. partial PUT keeps the fields it does not name; clearing every field
 *      deletes the row, so the report is the unbranded document again;
 *   5. tenancy: one workspace's template is not another's;
 *   6. every successful write is one audit row in the workspace's chain.
 */
#include "../../core/report_template.c"

#include <assert.h>

static db_handle g_db;

static void sql(const char *q) {
  char *err = NULL;
  if (sqlite3_exec(g_db.h, q, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", err ? err : "?", q);
    assert(0);
  }
}
static long count_sql(const char *q) {
  sqlite3_stmt *s; long v = -1;
  assert(sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

static tenant_ctx ctx(const char *tenant, const char *role) {
  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "%s", tenant);
  snprintf(t.user_id, sizeof t.user_id, "user-%s", role);
  snprintf(t.role, sizeof t.role, "%s", role);
  return t;
}

static void put(const tenant_ctx *t, const char *body, int want, const char *needle) {
  int st = 0;
  char *b = report_template_api_put(&g_db, t, body, body ? strlen(body) : 0, &st);
  assert(b);
  if (st != want || (needle && !strstr(b, needle))) {
    fprintf(stderr, "PUT %s -> %d (want %d, needle %s): %s\n", body ? body : "",
            st, want, needle ? needle : "-", b);
    assert(0);
  }
  free(b);
}

static char g_rep[65536];
static size_t g_repn = 0;
static int rep_write(void *c, const char *buf, size_t n) {
  (void)c;
  if (g_repn + n >= sizeof g_rep) n = sizeof g_rep - 1 - g_repn;
  memcpy(g_rep + g_repn, buf, n);
  g_repn += n;
  g_rep[g_repn] = 0;
  return 0;
}
static void render(const tenant_ctx *t, const char *body) {
  int st = 0; long bytes = 0;
  g_repn = 0; g_rep[0] = 0;
  assert(report_run(&g_db, t, "case-rt", body, rep_write, NULL, &bytes, &st) == 0);
  assert(st == 200);
}

/* A JSON body {"<field>": "<n x c>"}. */
static char *body_of_len(const char *field, size_t n, char c) {
  size_t cap = n + strlen(field) + 16;
  char *b = malloc(cap);
  assert(b);
  int k = snprintf(b, cap, "{\"%s\":\"", field);
  memset(b + k, c, n);
  snprintf(b + k + n, cap - k - n, "\"}");
  return b;
}

int main(void) {
  assert(db_open(&g_db, NULL, NULL) == 0);
  sql("INSERT OR IGNORE INTO tenants(id,slug,name) VALUES "
      "('rt-t1','rt-t1','RT one'),('rt-t2','rt-t2','RT two')");
  sql("INSERT OR IGNORE INTO cases(id,tenant_id,name) VALUES ('case-rt','rt-t1','harbour')");
  sql("DELETE FROM tenant_report_templates WHERE tenant_id IN ('rt-t1','rt-t2')");
  long a0 = count_sql("SELECT count(*) FROM audit_events WHERE action='report_template.put'");
  tenant_ctx owner = ctx("rt-t1", "owner"), admin = ctx("rt-t1", "admin");
  tenant_ctx analyst = ctx("rt-t1", "analyst"), viewer = ctx("rt-t1", "viewer");
  tenant_ctx other = ctx("rt-t2", "owner");

  /* 1. gate */
  int st = 0;
  char *b = report_template_api_get(&g_db, &viewer, &st);
  assert(b && st == 200 && strstr(b, "\"present\":false") && strstr(b, "\"can_write\":false"));
  free(b);
  put(&analyst, "{\"classification\":\"X\"}", 403, "forbidden");
  put(&viewer,  "{\"classification\":\"X\"}", 403, "forbidden");
  assert(count_sql("SELECT count(*) FROM tenant_report_templates WHERE tenant_id='rt-t1'") == 0);
  printf("  members read, analyst/viewer refused 403: ok\n");

  /* 2. validation */
  put(&owner, "{\"classfication\":\"X\"}", 400, "unknown_field");
  put(&owner, "{\"classification\":5}", 400, "bad_request");
  put(&owner, "[1]", 400, "bad_request");
  put(&owner, NULL, 400, "bad_request");
  put(&owner, "{\"classification\":\"A\\nB\"}", 400, "control character");
  put(&owner, "{\"header_note\":\"A\\u0007B\"}", 400, "control character");
  put(&owner, "{\"logo_url\":\"http://example.org/l.png\"}", 400, "https");
  put(&owner, "{\"logo_url\":\"https://\"}", 400, "https");
  put(&owner, "{\"logo_url\":\"https:///x.png\"}", 400, "https");
  put(&owner, "{\"logo_url\":\"javascript:alert(1)\"}", 400, "https");
  put(&owner, "{\"logo_url\":\"https://e.org/a b.png\"}", 400, "invalid_field");
  put(&owner, "{\"logo_url\":\"https://e.org/\\\"x.png\"}", 400, "invalid_field");
  size_t cmax = sizeof(((report_template *)0)->classification) - 1;
  char *ok = body_of_len("classification", cmax, 'C');
  char *over = body_of_len("classification", cmax + 1, 'C');
  put(&owner, over, 400, "at most");
  assert(count_sql("SELECT count(*) FROM tenant_report_templates WHERE tenant_id='rt-t1'") == 0);
  put(&owner, ok, 200, "\"present\":true");             /* the bound itself fits */
  free(ok); free(over);
  size_t fmax = sizeof(((report_template *)0)->footer_md) - 1;
  over = body_of_len("footer_md", fmax + 1, 'f');
  put(&owner, over, 400, "at most");
  free(over);
  printf("  closed keys, types, one-line fields, https logo, reader-sized bounds: ok\n");

  /* 3. round trip, through the report renderer */
  put(&admin, "{\"classification\":\"CONFIDENTIAL // TLP:AMBER\","
              "\"header_note\":\"Prepared for the harbour desk\","
              "\"footer_md\":\"Line one\\nLine two\","
              "\"logo_url\":\"https://example.org/logo.png\"}", 200,
      "\"classification\":\"CONFIDENTIAL // TLP:AMBER\"");
  render(&owner, NULL);
  assert(strstr(g_rep, "CONFIDENTIAL // TLP:AMBER"));
  assert(strstr(g_rep, "Prepared for the harbour desk"));
  assert(strstr(g_rep, "Line two"));
  render(&owner, "{\"format\":\"html\"}");
  assert(strstr(g_rep, "https://example.org/logo.png"));
  printf("  saved template is what report_run prints (md + html logo): ok\n");

  /* 4. partial PUT keeps the rest; clearing all deletes the row */
  put(&owner, "{\"header_note\":null}", 200, "\"header_note\":\"\"");
  b = report_template_api_get(&g_db, &analyst, &st);
  assert(b && strstr(b, "CONFIDENTIAL // TLP:AMBER") && strstr(b, "Line one"));
  free(b);
  put(&owner, "{\"classification\":\"\",\"footer_md\":null,\"logo_url\":\"\"}", 200,
      "\"present\":false");
  assert(count_sql("SELECT count(*) FROM tenant_report_templates WHERE tenant_id='rt-t1'") == 0);
  render(&owner, NULL);
  assert(!strstr(g_rep, "CONFIDENTIAL // TLP:AMBER"));
  printf("  partial PUT keeps unnamed fields; clearing all = unbranded: ok\n");

  /* 5. tenancy */
  put(&other, "{\"classification\":\"OTHER-WS\"}", 200, NULL);
  b = report_template_api_get(&g_db, &owner, &st);
  assert(b && !strstr(b, "OTHER-WS") && strstr(b, "\"present\":false"));
  free(b);
  printf("  one workspace's template is not another's: ok\n");

  /* 6. audit: ok-bound, admin full, header clear, clear-all, other = 5 */
  assert(count_sql("SELECT count(*) FROM audit_events WHERE action='report_template.put'")
         - a0 == 5);
  assert(count_sql("SELECT count(*) FROM audit_events WHERE action='report_template.put' "
                   "AND tenant_id='rt-t1' AND user_id='user-admin' "
                   "AND payload_json LIKE '%TLP:AMBER%'") == 1);
  printf("  one audit row per accepted write, in the workspace chain: ok\n");

  db_close(&g_db);
  printf("test_report_template: all passed\n");
  return 0;
}
