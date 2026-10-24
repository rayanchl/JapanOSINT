/* tests/unit/test_cadence.c — the system-wide cadence policy (core/cadence.h).
 *
 * Pins: a pivot (interval 0) is never touched; the compiled default is a
 * global 6 h and "declared" is a saved choice of zeros;
 * a global interval replaces every declared value; a floor/ceiling clamps;
 * the policy persists and comes back after a re-init; the environment
 * bootstraps only an empty database; bad values are refused by name; the
 * JSON names what the policy costs. */
#include "../../core/cadence.h"
#include "../../core/db.h"
#include "../../third_party/cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  unsetenv("JO_SCHED_INTERVAL_SEC"); unsetenv("JO_SCHED_MIN_INTERVAL_SEC"); unsetenv("JO_SCHED_MAX_INTERVAL_SEC");
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);

  cadence_init(&db);
  assert(!strcmp(cadence_source(), "default"));
  assert(cadence_apply(0) == 0 && cadence_apply(-1) == -1);          /* pivots untouched */
  /* The compiled default is ONE system-wide cadence of six hours (the owner's
   * decision of 2026-10-22): an empty database and a silent environment run
   * every scheduled row every 21600 s, and the pages say so. */
  assert(cadence_apply(3600) == 21600 && cadence_apply(604800) == 21600);
  assert(!strcmp(cadence_mode_for(3600), "global"));
  printf("  empty db, no env: the compiled default is global 21600\n");
  /* "declared" is a CHOICE now: an operator saves zeros, which stores a policy
   * that outranks the compiled default. */
  { const char *e0 = NULL; cadence_policy z = {0, 0, 0};
    assert(cadence_set(&db, &z, "ops@example.test", &e0) == 0); }
  assert(!strcmp(cadence_source(), "db"));
  assert(cadence_apply(3600) == 3600 && !strcmp(cadence_mode_for(3600), "declared"));
  printf("  zeros saved: declared stands, from the db\n");

  const char *err = NULL;
  cadence_policy p = {21600, 0, 0};
  unsigned g0 = cadence_generation();
  assert(cadence_set(&db, &p, "ops@example.test", &err) == 0);
  assert(cadence_generation() == g0 + 1);
  assert(cadence_apply(60) == 21600 && cadence_apply(604800) == 21600 && cadence_apply(0) == 0);
  assert(!strcmp(cadence_mode_for(60), "global") && !strcmp(cadence_mode_for(0), "declared"));
  printf("  global 21600: every scheduled row at 6 h, pivots untouched\n");

  cadence_policy c = {0, 900, 86400};
  assert(cadence_set(&db, &c, "ops@example.test", &err) == 0);
  assert(cadence_apply(60) == 900 && cadence_apply(3600) == 3600 && cadence_apply(604800) == 86400);
  assert(!strcmp(cadence_mode_for(60), "clamped") && !strcmp(cadence_mode_for(3600), "declared"));
  printf("  clamp [900, 86400]: floor and ceiling apply, the middle stands\n");

  /* persistence: a fresh init reads the database back */
  cadence_init(&db);
  assert(!strcmp(cadence_source(), "db"));
  assert(cadence_apply(60) == 900 && cadence_apply(604800) == 86400);
  printf("  persisted: re-init reads the clamp back from app_settings\n");

  /* the environment bootstraps only when the database holds no policy */
  setenv("JO_SCHED_INTERVAL_SEC", "7200", 1);
  cadence_init(&db);
  assert(!strcmp(cadence_source(), "db") && cadence_apply(60) == 900);
  cadence_policy none = {0, 0, 0};
  assert(cadence_set(&db, &none, "ops@example.test", &err) == 0);
  assert(cadence_apply(60) == 60 && !strcmp(cadence_mode_for(60), "declared"));
  printf("  cleared: zeros mean declared again, env ignored while a db row exists\n");

  /* refusals, each by name */
  cadence_policy bad1 = {-5, 0, 0};  assert(cadence_set(&db, &bad1, NULL, &err) != 0 && strstr(err, "negative"));
  cadence_policy bad2 = {5, 0, 0};   assert(cadence_set(&db, &bad2, NULL, &err) != 0 && strstr(err, "under 10"));
  cadence_policy bad3 = {0, 86400, 900}; assert(cadence_set(&db, &bad3, NULL, &err) != 0 && strstr(err, "exceeds"));
  cadence_policy bad4 = {0, 0, 400L * 86400L}; assert(cadence_set(&db, &bad4, NULL, &err) != 0 && strstr(err, "year"));
  printf("  refusals name the field\n");

  /* parse: absent fields keep the base, non-numbers refused */
  cadence_policy base = {0, 900, 0}, out;
  assert(cadence_parse("{\"global_interval_sec\": 3600}", &base, &out, &err) == 0 && out.global_sec == 3600 && out.min_sec == 900);
  assert(cadence_parse("{\"max_interval_sec\": null}", &base, &out, &err) == 0 && out.max_sec == 0);
  assert(cadence_parse("{\"global_interval_sec\": \"1h\"}", &base, &out, &err) != 0 && strstr(err, "numbers"));
  assert(cadence_parse("[1]", &base, &out, &err) != 0);
  printf("  parse: partial bodies merge onto the current policy\n");

  /* the JSON: policy, provenance, cost */
  cadence_policy g = {86400, 0, 0};
  assert(cadence_set(&db, &g, "ops@example.test", &err) == 0);
  char *js = cadence_json(&db);
  cJSON *j = cJSON_Parse(js);
  assert(j && cJSON_GetObjectItem(j, "policy") && cJSON_GetObjectItem(j, "estimate"));
  assert(!strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "policy"), "mode")->valuestring, "global"));
  assert(!strcmp(cJSON_GetObjectItem(j, "source")->valuestring, "db"));
  assert(!strcmp(cJSON_GetObjectItem(j, "updated_by")->valuestring, "ops@example.test"));
  cJSON *est = cJSON_GetObjectItem(j, "estimate");
  double rows = cJSON_GetObjectItem(est, "scheduled_rows")->valuedouble;
  double eff = cJSON_GetObjectItem(est, "runs_per_day_effective")->valuedouble;
  assert(rows > 0 && eff == rows);                /* daily global: one run per row per day */
  printf("  json: %0.f scheduled rows, %0.f runs/day under a daily global policy\n", rows, eff);
  cJSON_Delete(j); free(js);

  assert(cadence_set(&db, &none, "ops@example.test", &err) == 0);
  db_close(&db);
  printf("test_cadence: all passed\n");
  return 0;
}
