/* tests/unit/test_dispatch_pool.c — contract test for the OSINT search
 * dispatch fan-out in core/pipeline.c.
 *
 * WHAT IS BEING PROTECTED. run_tasks() used to be a serial for-loop, so the
 * `results` array was trivially in task order and no state was shared. Fanning
 * it out across worker threads makes both of those things something the code
 * has to actively guarantee, and neither is visible in normal use — a
 * reordered results array does not crash, it silently changes the Phase-2
 * prompt and makes the same query produce different follow-up rounds. That is
 * exactly the kind of defect that survives manual testing, so it gets a test.
 *
 * The three invariants:
 *   1. EVERY task produces exactly one result slot (nothing is dropped when
 *      workers race for the job queue).
 *   2. Results appear in TASK order regardless of completion order, so a
 *      fanned-out run is byte-comparable with a serial one.
 *   3. fanout=1 and fanout=N agree on names, entities and array length.
 *
 * It includes pipeline.c directly so it can reach the static run_tasks() and
 * the task list helpers; the harness therefore links every object EXCEPT
 * obj/core/pipeline.o and obj/main.o. See tests/unit/run.sh. */

#include "../../core/pipeline.c"

#include <assert.h>

/* Local/no-network services so the test is deterministic and fast. Repeated
 * deliberately with different entities: add_task dedups on
 * handlerKey|entityLower, so distinct entities are what produce distinct
 * tasks — and a larger task list is what actually makes workers contend. */
static const char *SERVICES[] = {
  "DATA_EXTRACTOR", "JP_CORPUS_LOOKUP", "REVERSE_DNS", "DNS_RECORDS",
};
static const char *ENTITIES[] = {
  "alpha@example.com", "bravo@example.com", "charlie@example.com",
  "delta@example.com", "echo@example.com", "foxtrot@example.com",
};

static void build_tasks(tasklist *tl, strset *ex) {
  for (unsigned s = 0; s < sizeof SERVICES / sizeof *SERVICES; s++)
    for (unsigned e = 0; e < sizeof ENTITIES / sizeof *ENTITIES; e++)
      add_task(tl, ex, SERVICES[s], ENTITIES[e], "email");
}

/* Runs the whole task list at `fanout` and returns the results array. */
static cJSON *run_at(db_handle *db, llm_client *llm, intel_sink *sink,
                     int fanout, const char *req_id, int *out_ntasks) {
  char fo[16]; snprintf(fo, sizeof fo, "%d", fanout);
  setenv("JO_OSINT_FANOUT", fo, 1);

  osint_request *rp = progress_create(req_id, "unit-test", 1);
  assert(rp && "progress_create failed");

  tasklist tl = {0};
  strset   ex = {0};
  build_tasks(&tl, &ex);
  assert(tl.n > 1 && "need >1 task or the fan-out is not exercised");
  if (out_ntasks) *out_ntasks = tl.n;

  cJSON *results = cJSON_CreateArray();
  run_tasks(rp, db, llm, sink, &tl, results);

  tasklist_free(&tl);
  strset_free(&ex);
  return results;
}

/* Invariant 2, checked against the task list the same way it was built. */
static void assert_task_order(cJSON *results) {
  int i = 0;
  for (unsigned s = 0; s < sizeof SERVICES / sizeof *SERVICES; s++) {
    for (unsigned e = 0; e < sizeof ENTITIES / sizeof *ENTITIES; e++, i++) {
      cJSON *r = cJSON_GetArrayItem(results, i);
      assert(r && "fewer results than tasks");
      const char *nm = cJSON_GetStringValue(cJSON_GetObjectItem(r, "name"));
      const char *en = cJSON_GetStringValue(cJSON_GetObjectItem(r, "entity"));
      assert(en && !strcmp(en, ENTITIES[e]) &&
             "results are in completion order, not task order");
      (void)nm;
    }
  }
}

int main(void) {
  /* The dispatch workers each db_attach() the configured database; point it at
   * a scratch file so the test never touches a real one. */
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }

  db_handle db = {0};
  if (db_open(&db, NULL, NULL) != 0) { fprintf(stderr, "db_open failed\n"); return 2; }

  http_client *http = http_client_new();
  llm_client llm; llm_init(&llm, http);
  intel_sink sink = intel_sink_make(&db, "osint-search", "legacy");

  int n1 = 0, n6 = 0;
  cJSON *serial = run_at(&db, &llm, &sink, 1, "unit-serial", &n1);
  cJSON *fanned = run_at(&db, &llm, &sink, 6, "unit-fanout", &n6);

  int cs = cJSON_GetArraySize(serial), cf = cJSON_GetArraySize(fanned);
  printf("tasks=%d  serial_results=%d  fanout6_results=%d\n", n1, cs, cf);

  /* 1 — nothing dropped by either path */
  assert(cs == n1 && "serial dropped a result");
  assert(cf == n6 && "fan-out dropped a result — workers raced the job queue");

  /* 3 — the two paths agree element for element on the identifying fields */
  assert(cs == cf && "serial and fanned-out result counts differ");
  for (int i = 0; i < cs; i++) {
    cJSON *a = cJSON_GetArrayItem(serial, i), *b = cJSON_GetArrayItem(fanned, i);
    const char *an = cJSON_GetStringValue(cJSON_GetObjectItem(a, "name"));
    const char *bn = cJSON_GetStringValue(cJSON_GetObjectItem(b, "name"));
    const char *ae = cJSON_GetStringValue(cJSON_GetObjectItem(a, "entity"));
    const char *be = cJSON_GetStringValue(cJSON_GetObjectItem(b, "entity"));
    assert(an && bn && !strcmp(an, bn) && "service order diverged under fan-out");
    assert(ae && be && !strcmp(ae, be) && "entity order diverged under fan-out");
  }

  /* 2 — and that shared order is TASK order, not whatever finished first */
  assert_task_order(serial);
  assert_task_order(fanned);

  cJSON_Delete(serial);
  cJSON_Delete(fanned);
  intel_sink_free(&sink);
  http_client_free(http);
  db_close(&db);
  printf("PASS: dispatch fan-out preserves task order and drops nothing\n");
  return 0;
}
