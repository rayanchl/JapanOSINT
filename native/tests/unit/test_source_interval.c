/* tests/unit/test_source_interval.c — the per-source interval override
 * (core/source_interval.h) and its place in the ONE composition
 * (core/scheduler.c sched_row_interval).
 *
 * Pins: an unknown id, an entity pivot, a value under 10 s and over a year
 * are refused by name; an enabled override IS the row's cadence even under
 * a global system-wide policy (specific beats general); disabling keeps the
 * number and hands the row back to the policy; clearing deletes; the row
 * persists across a re-init; the generation moves on every change; the
 * JSON names what decided the figure. No network. */
#include "../../core/source_interval.h"
#include "../../core/cadence.h"
#include "../../core/scheduler.h"
#include "../../core/db.h"
#include "../../source.h"
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
  source_interval_init(&db);

  /* one scheduled row and one entity pivot, whatever the registry holds */
  const source_def **all = registry_all(); int n = registry_count();
  const source_def *sched = NULL, *pivot = NULL;
  for (int i = 0; i < n && (!sched || !pivot); i++) {
    if (!sched && all[i]->update_interval_sec > 0 && all[i]->collector && all[i]->collector[0] != '_') sched = all[i];
    if (!pivot && all[i]->update_interval_sec == 0 && registry_is_entity_pivot(all[i])) pivot = all[i];
  }
  assert(sched && pivot);
  printf("  rows: scheduled %s (declared %d), pivot %s\n", sched->id, sched->update_interval_sec, pivot->id);

  const char *err = NULL;
  assert(source_interval_for(sched->id) == 0 && source_interval_count() == 0);
  assert(source_interval_set(&db, "NO_SUCH_SOURCE_XYZ", 3600, 1, "ops@example.test", &err) != 0 && strstr(err, "no registered source"));
  assert(source_interval_set(&db, pivot->id, 3600, 1, "ops@example.test", &err) != 0 && strstr(err, "no schedule"));
  assert(source_interval_set(&db, sched->id, 5, 1, "ops@example.test", &err) != 0 && strstr(err, "under 10 s"));
  assert(source_interval_set(&db, sched->id, 400L * 86400L, 1, "ops@example.test", &err) != 0 && strstr(err, "longer than a year"));
  assert(source_interval_count() == 0);
  printf("  refusals: unknown id, entity pivot, < 10 s, > 1 year — each by name\n");

  /* a global policy of 6 h holds every row ... */
  cadence_policy g = {21600, 0, 0};
  assert(cadence_set(&db, &g, "ops@example.test", &err) == 0);
  sched_state st = {0};
  assert(sched_row_interval(&st, sched) == 21600);
  /* ... except the one the operator set by hand */
  unsigned g0 = source_interval_generation();
  assert(source_interval_set(&db, sched->id, 900, 1, "ops@example.test", &err) == 0);
  assert(source_interval_generation() == g0 + 1 && source_interval_count() == 1);
  assert(source_interval_for(sched->id) == 900);
  assert(sched_row_interval(&st, sched) == 900);
  assert(sched_row_interval(&st, pivot) == 0);          /* a pivot stays on demand */
  printf("  override 900 under a global 21600: the row runs every 900 s, the pivot stays on demand\n");

  /* disabled: the number is kept, the policy decides again */
  assert(source_interval_set(&db, sched->id, 900, 0, "ops@example.test", &err) == 0);
  source_interval ov;
  assert(source_interval_get(&db, sched->id, &ov) == 1 && ov.present && !ov.enabled && ov.interval_sec == 900);
  assert(source_interval_for(sched->id) == 0 && sched_row_interval(&st, sched) == 21600);
  printf("  disabled: number kept (900), cadence back to the policy (21600)\n");

  /* persistence: re-enable, re-init from the database, still there */
  assert(source_interval_set(&db, sched->id, 1800, 1, NULL, &err) == 0);
  source_interval_init(&db);
  assert(source_interval_for(sched->id) == 1800 && sched_row_interval(&st, sched) == 1800);
  assert(source_interval_get(&db, sched->id, &ov) == 1 && ov.updated_by[0] == 0 && ov.updated_at[0]);
  printf("  persisted: re-init reads 1800 back from source_interval_override\n");

  /* the JSON names the composition */
  char *j = source_interval_json(&db, sched->id, 1);
  assert(j);
  cJSON *o = cJSON_Parse(j);
  assert(o);
  assert(cJSON_GetObjectItem(o, "effective_interval_sec")->valuedouble == 1800);
  assert(!strcmp(cJSON_GetObjectItem(o, "effective_basis")->valuestring, "override"));
  assert(cJSON_GetObjectItem(o, "policy_interval_sec")->valuedouble == 21600);
  assert(!strcmp(cJSON_GetObjectItem(o, "policy")->valuestring, "global"));
  assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "can_write")));
  assert(cJSON_GetObjectItem(cJSON_GetObjectItem(o, "override"), "interval_sec")->valuedouble == 1800);
  cJSON_Delete(o); free(j);
  assert(source_interval_json(&db, "NO_SUCH_SOURCE_XYZ", 0) == NULL);
  printf("  json: effective 1800, basis override, policy 21600 global, can_write echoed\n");

  /* clear: the row is gone, the policy decides, the generation moved */
  unsigned g1 = source_interval_generation();
  assert(source_interval_clear(&db, sched->id, &err) == 0);
  assert(source_interval_generation() == g1 + 1);
  assert(source_interval_get(&db, sched->id, &ov) == 0 && !ov.present);
  assert(source_interval_for(sched->id) == 0 && sched_row_interval(&st, sched) == 21600 && source_interval_count() == 0);
  j = source_interval_json(&db, sched->id, 0);
  o = cJSON_Parse(j);
  assert(!strcmp(cJSON_GetObjectItem(o, "effective_basis")->valuestring, "policy") && cJSON_IsNull(cJSON_GetObjectItem(o, "override")));
  cJSON_Delete(o); free(j);
  printf("  cleared: row deleted, basis policy, generation moved\n");

  cadence_policy none = {0, 0, 0};
  assert(cadence_set(&db, &none, NULL, &err) == 0);
  printf("all passed\n");
  return 0;
}
