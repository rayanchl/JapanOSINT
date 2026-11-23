/* test_source_kind.c — what a source IS is declared (`source_def.kind`), not
 * spelled in its collector's first character.
 *
 * A leading '_' in `collector` used to select the maintenance lane, exempt a
 * row from the cadence policy, hide it from the LLM's menu, exempt it from the
 * NEVER-RUNS check and the pivot rule, and make the run routes refuse it. The
 * cadence policy once forgot the spelling and retimed every pod to 6 h. Pinned
 * here, each against a fixture whose collector name is deliberately NOT the
 * convention, so a regression to the prefix fails by construction:
 *   1. source_def_kind_of(): the declared kind wins; the default derives
 *      PIVOT (interval 0 + entity-capable) or FEED.
 *   2. every '_' collector in the REAL registry is POD or PROBE, and no POD or
 *      PROBE lacks one (the registry's own KIND-MISMATCH count agrees).
 *   3. a POD with a non-underscore collector is exempt from the cadence policy,
 *      sits in the maintenance lane, and is internal for the sources API.
 *   4. a '_' collector left at FEED, and a POD without '_', are each reported
 *      as KIND-MISMATCH (and nothing else is).
 *   5. a PROBE is never an entity pivot, never "never-runs".
 *
 * Includes scheduler.c to reach lane_of(); run.sh links everything else. */
#include "../../core/scheduler.c"

#include <assert.h>

static int run_nop(const source_ctx *c, intel_sink *s) { (void)c; (void)s; return 0; }

/* A pod that does NOT follow the naming convention: a mismatch (no '_'). */
static const source_def D_POD_PLAIN = { .id = "UNIT_KIND_POD_PLAIN",
  .collector = "plainpod", .kind = SOURCE_KIND_POD, .update_interval_sec = 120,
  .name = "pod", .run = run_nop, .category = "maintenance" };
/* Underscore spelling but left at the default kind: a mismatch. */
static const source_def D_UNDER_FEED = { .id = "UNIT_KIND_UNDER_FEED",
  .collector = "_oops", .update_interval_sec = 3600,
  .name = "oops", .run = run_nop, .category = "test" };
/* A well-formed probe: not a mismatch. */
static const source_def D_PROBE_OK = { .id = "UNIT_KIND_PROBE_OK",
  .collector = "_probe", .kind = SOURCE_KIND_PROBE, .update_interval_sec = 0,
  .name = "probe", .run = run_nop, .category = "test" };
REGISTER_SOURCE(D_POD_PLAIN)
REGISTER_SOURCE(D_UNDER_FEED)
REGISTER_SOURCE(D_PROBE_OK)

static void test_derivation(void) {
  source_def feed = { .id = "K_FEED", .collector = "x", .update_interval_sec = 600 };
  assert(source_def_kind_of(&feed) == SOURCE_KIND_FEED);
  /* interval 0, non-hpengine, any non-internal collector: an entity pivot */
  source_def piv = { .id = "K_PIVOT", .collector = "x", .update_interval_sec = 0 };
  assert(source_def_kind_of(&piv) == SOURCE_KIND_PIVOT);
  /* the declared kind wins over derivation */
  source_def dp = { .id = "K_DECL", .collector = "x", .kind = SOURCE_KIND_PIVOT,
                    .update_interval_sec = 600 };
  assert(source_def_kind_of(&dp) == SOURCE_KIND_PIVOT);
  assert(source_def_kind_of(&D_POD_PLAIN) == SOURCE_KIND_POD);
  assert(source_def_kind_of(&D_PROBE_OK) == SOURCE_KIND_PROBE);
  assert(source_def_kind_of(NULL) == SOURCE_KIND_FEED);
  /* an interval-0 POD/PROBE is not a pivot whatever its collector says */
  source_def p0 = { .id = "K_P0", .collector = "plain", .kind = SOURCE_KIND_POD };
  assert(!registry_is_entity_pivot(&p0));
  assert(!registry_is_entity_pivot(&D_PROBE_OK));
  assert(source_def_is_internal(&p0) && !source_def_is_internal(&feed));
  assert(!strcmp(source_def_kind_name(SOURCE_KIND_PROBE), "probe"));
}

static void test_real_registry(void) {
  const source_def **a = registry_all();
  int under = 0, pods = 0, probes = 0;
  for (int i = 0; i < registry_count(); i++) {
    const source_def *d = a[i];
    if (!strncmp(d->id, "UNIT_KIND_", 10)) continue;       /* this test's fixtures */
    int u = d->collector && d->collector[0] == '_';
    source_def_kind k = source_def_kind_of(d);
    int internal = k == SOURCE_KIND_POD || k == SOURCE_KIND_PROBE;
    if (u) under++;
    if (k == SOURCE_KIND_POD) pods++;
    if (k == SOURCE_KIND_PROBE) probes++;
    if (u != internal) {
      fprintf(stderr, "kind/collector disagree: %s collector=%s kind=%s\n",
              d->id, d->collector, source_def_kind_name(k));
      assert(0);
    }
  }
  assert(under > 0 && under == pods + probes);
  assert(probes >= 1);                          /* PORT_SCANNER */
  const source_def *ps = registry_get("PORT_SCANNER");
  assert(ps && source_def_kind_of(ps) == SOURCE_KIND_PROBE);
  /* ONLY this test's two deliberate fixtures are mismatches */
  assert(registry_kind_mismatch_count() == 2);
  /* and a probe/pod never counts as never-runs */
  assert(registry_never_runs_count() == 0);
}

static void test_pod_exemptions_by_kind(db_handle *db) {
  const char *err = NULL;
  cadence_policy global = {21600, 0, 0};
  assert(cadence_set(db, &global, "test", &err) == 0);
  sched_state st; memset(&st, 0, sizeof st);
  /* the policy retimes an ordinary feed to 6 h... */
  source_def feed = { .id = "K_FEED2", .collector = "plain", .update_interval_sec = 120 };
  assert(sched_row_interval(&st, &feed) == 21600);
  /* ...and does NOT touch a POD whose collector is not an underscore name */
  assert(sched_is_pod(&D_POD_PLAIN));
  assert(sched_row_interval(&st, &D_POD_PLAIN) == 120);
  assert(lane_of(&D_POD_PLAIN) == LANE_MAINT);
  assert(lane_of(&feed) == LANE_COLLECT);
  /* the reverse: an '_' collector declared FEED is NOT exempt and stays in the
   * collection lane — the spelling carries no behaviour */
  assert(!sched_is_pod(&D_UNDER_FEED));
  assert(lane_of(&D_UNDER_FEED) == LANE_COLLECT);
  assert(sched_row_interval(&st, &D_UNDER_FEED) == 21600);
  cadence_policy none = {0, 0, 0};
  assert(cadence_set(db, &none, "test", &err) == 0);
}

int main(void) {
  db_handle db = {0};
  const char *path = getenv("JO_DB");
  assert(path && "run.sh sets JO_DB to a scratch database");
  if (db_open(&db, path, NULL) != 0) { fprintf(stderr, "db_open failed\n"); return 2; }
  test_derivation();
  test_real_registry();
  test_pod_exemptions_by_kind(&db);
  db_close(&db);
  printf("test_source_kind: all passed\n");
  return 0;
}
