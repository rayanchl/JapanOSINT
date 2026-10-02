/* test_annotations_case.c — notes about a case itself.
 *
 * The web CaseDetailPage files case-level notes as ref_type="case",
 * ref_id=<case id>, case_id=<case id>. REF_TYPES had no "case", so every one
 * was refused 400 invalid_ref_type and the panel could neither save nor list.
 *
 * Holds: a note about one of the caller's own cases is accepted and listed;
 * a "case" note naming another tenant's case id is refused; "case" stays OUT
 * of the pin vocabulary (annotations_ref_type_valid / casesapi), because a
 * case is the container findings are pinned into, not a finding. */
#include "core/annotationsapi.h"
#include "core/tenantapi.h"
#include "core/db.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  int rc = sqlite3_exec(db->h, sql, NULL, NULL, &e);
  if (rc != SQLITE_OK) fprintf(stderr, "%s: %s\n", sql, e ? e : "?");
  assert(rc == SQLITE_OK);
  sqlite3_free(e);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A'),('tB','tb','B')");
  exec_ok(&db, "INSERT INTO cases(id,tenant_id,name) VALUES ('case-a','tA','a'),('case-b','tB','b')");

  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "tA");
  snprintf(t.user_id, sizeof t.user_id, "u1");
  snprintf(t.role, sizeof t.role, "analyst");

  int st = 0;
  char *b = annotationsapi(&db, &t, "POST", "", "",
    "{\"ref_type\":\"case\",\"ref_id\":\"case-a\",\"case_id\":\"case-a\",\"body_md\":\"lead on X\"}", &st);
  printf("  POST case note on own case -> %d\n", st);
  assert(st == 201 && b && strstr(b, "\"ref_type\":\"case\""));
  free(b);

  b = annotationsapi(&db, &t, "GET", "", "ref_type=case&ref_id=case-a&case_id=case-a", NULL, &st);
  assert(st == 200 && b && strstr(b, "lead on X"));
  free(b);
  printf("  GET ?ref_type=case lists it: ok\n");

  /* another tenant's case id as the ref, with no case_id: still refused */
  b = annotationsapi(&db, &t, "POST", "", "",
    "{\"ref_type\":\"case\",\"ref_id\":\"case-b\",\"body_md\":\"x\"}", &st);
  printf("  POST case note on another tenant's case -> %d\n", st);
  assert(st == 400 && b && strstr(b, "unknown_case_id"));
  free(b);

  /* not pinnable */
  assert(!annotations_ref_type_valid("case"));
  assert(annotations_ref_type_valid("intel_item"));
  printf("  \"case\" is annotatable, not pinnable: ok\n");

  db_close(&db);
  printf("test_annotations_case: ok\n");
  return 0;
}
