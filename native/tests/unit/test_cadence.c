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
#include "../../core/scheduler.h"
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
  unsetenv("JO_SCHED_REALTIME_MAX_SEC");
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

  /* ENG-17: a row DECLARED at or below sched.realtime_max_sec (default 300) is
   * exempt from the GLOBAL interval - wolfx-eew at 10 s, jma-tsunami at 60 s -
   * and the exemption is decided on the declared value, not on a measurement. */
  assert(cadence_get().realtime_max_sec == 300);
  assert(cadence_apply(10) == 10 && cadence_apply(60) == 60 && cadence_apply(300) == 300);
  assert(cadence_apply(301) == 21600 && "301 s is not real-time");
  assert(!strcmp(cadence_mode_for(60), "realtime") && !strcmp(cadence_mode_for(301), "global"));
  /* an adaptive row's measurement does not make it real-time, nor stop it being so */
  assert(cadence_apply_row(3600, 60) == 21600 && "a feed that merely went quiet is not real-time");
  assert(cadence_apply_row(60, 3600) == 3600 && !strcmp(cadence_mode_for_row(60, 3600), "realtime"));
  printf("  realtime: declared <= 300 s keeps its interval under the global policy\n");
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
  cadence_policy bad5 = {0, 0, 1};   assert(cadence_set(&db, &bad5, NULL, &err) != 0 && strstr(err, "max_interval_sec under 10"));
  cadence_policy ok10 = {0, 0, 10};  assert(cadence_set(&db, &ok10, NULL, &err) == 0 && cadence_set(&db, &none, NULL, &err) == 0);
  printf("  refusals name the field\n");

  /* parse: absent fields keep the base, non-numbers refused */
  cadence_policy base = {0, 900, 0}, out;
  assert(cadence_parse("{\"global_interval_sec\": 3600}", &base, &out, &err) == 0 && out.global_sec == 3600 && out.min_sec == 900);
  assert(cadence_parse("{\"max_interval_sec\": null}", &base, &out, &err) == 0 && out.max_sec == 0);
  assert(cadence_parse("{\"global_interval_sec\": \"1h\"}", &base, &out, &err) != 0 && strstr(err, "numbers"));
  assert(cadence_parse("[1]", &base, &out, &err) != 0);
  assert(cadence_parse("{\"min_interval_sec\": 0.5}", &base, &out, &err) != 0 && strstr(err, "whole"));
  assert(cadence_parse("{\"max_interval_sec\": 1e999}", &base, &out, &err) != 0 && strstr(err, "range"));
  assert(cadence_parse("{\"global_interval_sec\": 1e12}", &base, &out, &err) != 0 && strstr(err, "range"));
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
  /* daily global: one run per row per day — except a maintenance pod, which
   * the policy does not reach and which keeps the interval it declares */
  double want = 0; const source_def **all = registry_all();
  for (int i = 0; i < registry_count(); i++) {
    long d = all[i]->update_interval_sec;
    if (d > 0) want += sched_is_pod(all[i]) ? 86400.0 / (double)d : 1.0;
  }
  assert(rows > 0 && eff == (double)(long)(want + 0.5) && eff >= rows);
  printf("  json: %0.f scheduled rows, %0.f runs/day under a daily global policy\n", rows, eff);
  cJSON_Delete(j); free(js);

  /* a stored policy no PUT would accept is not honoured at boot: the next
   * source is (the env set above), and an invalid env falls to the default */
  assert(db_exec(&db, "UPDATE app_settings SET value='1' WHERE key='sched.max_interval_sec'", NULL) == 0);
  assert(db_exec(&db, "UPDATE app_settings SET value='0' WHERE key='sched.global_interval_sec'", NULL) == 0);
  cadence_init(&db);
  assert(!strcmp(cadence_source(), "env") && cadence_apply(3600) == 7200 && cadence_apply(60) == 60);
  unsetenv("JO_SCHED_INTERVAL_SEC"); setenv("JO_SCHED_MAX_INTERVAL_SEC", "5", 1);
  cadence_init(&db);
  assert(!strcmp(cadence_source(), "default") && cadence_apply(3600) == 21600 && cadence_apply(60) == 60);
  unsetenv("JO_SCHED_MAX_INTERVAL_SEC");

  /* ---- sched.realtime_max_sec as a policy field (ENG-17) -------------------
   * persisted, validated, parsed, shown in the JSON, counted by the estimate. */
  {
    const char *e = NULL;
    cadence_policy g6 = {21600, 0, 0, 300};
    assert(cadence_set(&db, &g6, "ops@example.test", &e) == 0);
    assert(cadence_apply(60) == 60 && cadence_apply(3600) == 21600);
    /* the clamps still apply to a real-time row */
    cadence_policy cl = {21600, 120, 0, 300};
    assert(cadence_set(&db, &cl, "ops@example.test", &e) == 0);
    assert(cadence_apply(10) == 120 && "the floor reaches a real-time row");
    assert(cadence_apply(3600) == 21600);
    /* 0 switches the exemption off: every row at the global interval */
    cadence_policy off = {21600, 0, 0, 0};
    assert(cadence_set(&db, &off, "ops@example.test", &e) == 0);
    assert(cadence_apply(10) == 21600 && !strcmp(cadence_mode_for(10), "global"));
    /* a raised limit exempts more rows */
    cadence_policy wide = {21600, 0, 0, 3600};
    assert(cadence_set(&db, &wide, "ops@example.test", &e) == 0);
    assert(cadence_apply(3600) == 3600 && cadence_apply(3601) == 21600);
    cadence_policy bad = {0, 0, 0, -1};
    assert(cadence_set(&db, &bad, NULL, &e) != 0 && strstr(e, "negative"));
    cadence_policy bad2 = {0, 0, 0, 400L * 86400L};
    assert(cadence_set(&db, &bad2, NULL, &e) != 0 && strstr(e, "year"));
    printf("  realtime: persisted, clamps still apply, 0 switches it off, refusals by name\n");

    /* it survives a re-init, including the stored 0 (an operator's choice) */
    assert(cadence_set(&db, &off, "ops@example.test", &e) == 0);
    cadence_init(&db);
    assert(cadence_get().realtime_max_sec == 0 && cadence_apply(10) == 21600);
    cadence_set(&db, &wide, "ops@example.test", &e);
    cadence_init(&db);
    assert(cadence_get().realtime_max_sec == 3600);
    /* a policy saved BEFORE the key existed reads as the default, not as off */
    assert(db_exec(&db, "DELETE FROM app_settings WHERE key='sched.realtime_max_sec'", NULL) == 0);
    cadence_init(&db);
    assert(cadence_get().realtime_max_sec == 300 && !strcmp(cadence_source(), "db"));
    printf("  realtime: re-init reads it back; a pre-existing policy keeps the default\n");

    /* PUT body: partial merge keeps the rest, the field is a number */
    cadence_policy cur = cadence_get(), out;
    assert(cadence_parse("{\"realtime_max_sec\": 600}", &cur, &out, &e) == 0 &&
           out.realtime_max_sec == 600 && out.global_sec == cur.global_sec);
    assert(cadence_parse("{\"global_interval_sec\": 7200}", &cur, &out, &e) == 0 &&
           out.realtime_max_sec == cur.realtime_max_sec);
    assert(cadence_parse("{\"realtime_max_sec\": \"5m\"}", &cur, &out, &e) != 0 && strstr(e, "numbers"));
    assert(cadence_parse("{\"realtime_max_sec\": 0}", &cur, &out, &e) == 0 && out.realtime_max_sec == 0);

    /* GET: the field, the exempt-row count, and the estimate counting it */
    cadence_policy gl = {21600, 0, 0, 300};
    assert(cadence_set(&db, &gl, "ops@example.test", &e) == 0);
    char *js = cadence_json(&db);
    cJSON *j = cJSON_Parse(js);
    assert(j);
    cJSON *pol = cJSON_GetObjectItem(j, "policy");
    assert(cJSON_GetObjectItem(pol, "realtime_max_sec")->valuedouble == 300);
    cJSON *est = cJSON_GetObjectItem(j, "estimate");
    double rt = cJSON_GetObjectItem(est, "realtime_exempt_rows")->valuedouble;
    double eff = cJSON_GetObjectItem(est, "runs_per_day_effective")->valuedouble;
    double want = 0, want_rt = 0; const source_def **all = registry_all();
    for (int i = 0; i < registry_count(); i++) {
      long d = all[i]->update_interval_sec;
      if (d <= 0) continue;
      int exempt = sched_is_pod(all[i]) || d <= 300;
      want += exempt ? 86400.0 / (double)d : 86400.0 / 21600.0;
      if (!sched_is_pod(all[i]) && d <= 300) want_rt++;
    }
    assert(eff == (double)(long)(want + 0.5) && "the estimate does not count the exemption");
    assert(rt == want_rt);
    printf("  json: realtime_max_sec and %.0f exempt row(s); %.0f runs/day counted with them\n", rt, eff);
    cJSON_Delete(j); free(js);

    /* env bootstrap, and 0 from the environment disables it */
    assert(db_exec(&db, "DELETE FROM app_settings WHERE key LIKE 'sched.%'", NULL) == 0);
    setenv("JO_SCHED_REALTIME_MAX_SEC", "0", 1);
    cadence_init(&db);
    assert(!strcmp(cadence_source(), "default") && cadence_get().realtime_max_sec == 0 && cadence_apply(10) == 21600);
    setenv("JO_SCHED_REALTIME_MAX_SEC", "abc", 1);
    cadence_init(&db);
    assert(cadence_get().realtime_max_sec == 300 && "an unparsable value is the default");
    unsetenv("JO_SCHED_REALTIME_MAX_SEC");
    cadence_init(&db);
    printf("  realtime: JO_SCHED_REALTIME_MAX_SEC bootstraps an empty db; 0 = off; junk = default\n");
  }

  printf("  boot: an invalid stored or env policy is ignored, not honoured\n");

  assert(cadence_set(&db, &none, "ops@example.test", &err) == 0);
  db_close(&db);
  printf("test_cadence: all passed\n");
  return 0;
}
