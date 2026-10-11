/* test_collector_run_gate.c — GET /api/data/<id> charges its per-user run
 * budget exactly where a collector would run, and nowhere else (M13).
 *
 * Any signed-in user could make the server run a collector by naming a cold
 * layer; httpd.c now passes an admission check into dataapi_layer_admit().
 * Pinned here:
 *   - a refusal happens BEFORE the collector runs, and is reported (refused=1,
 *     NULL) rather than served as an empty layer;
 *   - an admitted miss runs the collector once;
 *   - an id that is no collector never consults the budget, so map reads that
 *     cost a query are never rate-limited;
 *   - RL_COLLECTOR_RUN budgets are per key: one user's spend is not another's. */
#include "../../core/dataapi.c"
#include "core/ratelimit.h"

#include <assert.h>

static int g_runs;
static int fake_run(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; (void)sink;
  g_runs++;
  return 0;                                       /* ran, emitted nothing */
}
static const source_def k_src = {
  .id = "test_collector_run_gate_src", .collector = "_test",
  .name = "run-gate test source", .run = fake_run, .category = "test",
};

static int g_asked;
static int deny(void *a)  { (void)a; g_asked++; return 0; }
static int admit(void *a) { (void)a; g_asked++; return 1; }

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  registry_add(&k_src);
  assert(registry_get(k_src.id) == &k_src);

  int refused = -1;
  char *b = dataapi_layer_admit(&db, k_src.id, deny, NULL, &refused);
  assert(!b && refused == 1 && "a refused run is reported, not served empty");
  assert(g_asked == 1 && g_runs == 0 && "refused BEFORE the collector ran");

  b = dataapi_layer_admit(&db, k_src.id, admit, NULL, &refused);
  assert(b && refused == 0 && g_asked == 2 && g_runs == 1);
  free(b);

  /* Not a collector: the budget is never consulted. */
  b = dataapi_layer_admit(&db, "no_such_layer_for_run_gate", deny, NULL, &refused);
  assert(refused == 0 && g_asked == 2);
  free(b);

  /* NULL gate == the old dataapi_layer(). */
  int before = g_runs;
  b = dataapi_layer(&db, k_src.id);
  assert(b);
  free(b);
  assert(g_runs >= before);

  /* Per-user budgets are independent. */
  for (int i = 0; i < 3; i++)
    assert(ratelimit_allow(RL_COLLECTOR_RUN, "data:user-a", 3, 300, NULL));
  int retry = 0;
  assert(!ratelimit_allow(RL_COLLECTOR_RUN, "data:user-a", 3, 300, &retry));
  assert(retry > 0 && retry <= 300);
  assert(ratelimit_allow(RL_COLLECTOR_RUN, "data:user-b", 3, 300, NULL));

  db_close(&db);
  printf("test_collector_run_gate: ok\n");
  return 0;
}
