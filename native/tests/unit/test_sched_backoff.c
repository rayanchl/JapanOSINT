/* tests/unit/test_sched_backoff.c — the health-driven scheduling arithmetic in
 * core/scheduler.c, driven with a fake clock and no network.
 *
 * WHAT IS BEING PROTECTED. The state machine is small and every transition is
 * the kind of thing that fails quietly: an off-by-one in the quarantine
 * threshold benches a source one failure early, a cap that is applied before
 * the doubling stalls the backoff at cap/2, a success that forgets to clear
 * backoff_until leaves a recovered source waiting out an outage that is over,
 * an honest empty counted as a failure re-creates the very "quarantine on
 * success" bug run_status() was written to end. None of those crash; all of
 * them make the map stale in a way that looks like an upstream problem.
 *
 * The invariants:
 *   1. failures double effective_interval from the declared value, capped.
 *   2. a cap below the declared interval never SHORTENS the cadence.
 *   3. a success resets to declared in one step and clears backoff.
 *   4. an honest empty (ok, 0 records) is not a failure: no backoff, and it
 *      is counted in consecutive_empties, which a non-empty run resets.
 *   5. exactly quarantine_after consecutive failures quarantine; the probe
 *      cadence takes over; the first successful probe releases.
 *   6. persistence round-trips through source_sched_state.
 *   7. priority: healthy < backed-off < quarantined, better score first,
 *      more overdue first within a band.
 *
 * Includes scheduler.c directly to reach its statics (env_long, the policy),
 * so run.sh links every object EXCEPT obj/core/scheduler.o and obj/main.o. */

#include "../../core/scheduler.c"

#include <assert.h>

static void test_backoff_doubling(void) {
  sched_policy p = { .backoff_cap_sec = 86400, .quarantine_after = 12,
                     .quarantine_probe_sec = 7 * 86400 };
  sched_state st = {0};
  time_t now = 1000000;
  long expect = 60;
  for (int i = 1; i <= 11; i++) {
    sched_state_apply(&st, &p, 60, "error", 0, now);
    expect = expect * 2 > 86400 ? 86400 : expect * 2;
    assert(st.consecutive_failures == i);
    assert(st.effective_interval == expect);
    assert(st.backoff_until == now + expect);
    assert(!st.quarantined);
    assert(sched_state_interval(&st, &p, 60) == expect);
    now += expect;
  }
  /* 60·2^11 = 122880 > cap: the 11th failure must have landed on the cap. */
  assert(st.effective_interval == 86400);
  /* one success: declared again, backoff gone, failures 0 */
  sched_state_apply(&st, &p, 60, "ok", 5, now);
  assert(st.consecutive_failures == 0);
  assert(st.effective_interval == 60);
  assert(st.backoff_until == 0);
  assert(sched_state_interval(&st, &p, 60) == 60);
}

static void test_cap_below_declared(void) {
  sched_policy p = { .backoff_cap_sec = 3600, .quarantine_after = 12,
                     .quarantine_probe_sec = 7 * 86400 };
  sched_state st = {0};
  sched_state_apply(&st, &p, 86400, "error", 0, 1);
  /* the cap may not make a daily source run hourly */
  assert(st.effective_interval == 86400);
  assert(sched_state_interval(&st, &p, 86400) == 86400);
}

static void test_empties_are_not_failures(void) {
  sched_policy p = { .backoff_cap_sec = 86400, .quarantine_after = 3,
                     .quarantine_probe_sec = 7 * 86400 };
  sched_state st = {0};
  for (int i = 1; i <= 50; i++) {
    sched_state_apply(&st, &p, 60, "ok", 0, 100 + i);
    assert(st.consecutive_failures == 0);
    assert(st.consecutive_empties == i);
    assert(st.effective_interval == 60);
    assert(st.backoff_until == 0);
    assert(!st.quarantined);
  }
  sched_state_apply(&st, &p, 60, "ok", 1, 200);
  assert(st.consecutive_empties == 0);
  /* a failure does not disturb the empties counter, and vice versa */
  sched_state_apply(&st, &p, 60, "ok", 0, 201);
  sched_state_apply(&st, &p, 60, "error", 0, 202);
  assert(st.consecutive_empties == 1 && st.consecutive_failures == 1);
}

static void test_quarantine_transitions(void) {
  sched_policy p = { .backoff_cap_sec = 600, .quarantine_after = 4,
                     .quarantine_probe_sec = 5000 };
  sched_state st = {0};
  time_t now = 50000;
  for (int i = 1; i < 4; i++) {
    sched_state_apply(&st, &p, 60, "error", 0, now + i);
    assert(!st.quarantined);
  }
  sched_state_apply(&st, &p, 60, "error", 0, now + 4);
  assert(st.quarantined);
  assert(st.consecutive_failures == 4);
  assert(st.quarantined_at == now + 4);
  assert(st.last_probe == now + 4);
  /* while quarantined the cadence is the probe cadence, not the backoff */
  assert(sched_state_interval(&st, &p, 60) == 5000);
  /* a failed probe stays quarantined, moves last_probe, keeps counting */
  sched_state_apply(&st, &p, 60, "error", 0, now + 5004);
  assert(st.quarantined && st.consecutive_failures == 5);
  assert(st.last_probe == now + 5004);
  assert(sched_state_interval(&st, &p, 60) == 5000);
  /* a successful probe — even an EMPTY one; the fetch worked — releases */
  sched_state_apply(&st, &p, 60, "ok", 0, now + 10004);
  assert(!st.quarantined);
  assert(st.quarantined_at == 0 && st.last_probe == 0);
  assert(st.consecutive_failures == 0);
  assert(st.consecutive_empties == 1);
  assert(sched_state_interval(&st, &p, 60) == 60);
}

static void test_persistence(db_handle *db) {
  sched_state st = { .consecutive_failures = 7, .consecutive_empties = 2,
                     .effective_interval = 7680, .backoff_until = 123456,
                     .quarantined = 1, .quarantined_at = 111, .last_probe = 222 };
  assert(sched_state_save(db, "UNIT_SRC", &st, 60, 999) == 0);
  sched_state back;
  assert(sched_state_load_one(db, "UNIT_SRC", &back) == 1);
  assert(back.consecutive_failures == 7 && back.consecutive_empties == 2);
  assert(back.effective_interval == 7680 && back.backoff_until == 123456);
  assert(back.quarantined == 1 && back.quarantined_at == 111 && back.last_probe == 222);
  /* upsert, not insert: a second save overwrites */
  st.quarantined = 0; st.backoff_until = 0;
  assert(sched_state_save(db, "UNIT_SRC", &st, 60, 1000) == 0);
  assert(sched_state_load_one(db, "UNIT_SRC", &back) == 1);
  assert(back.quarantined == 0 && back.backoff_until == 0);
  /* absent row: 0 and a zeroed state, not an error */
  assert(sched_state_load_one(db, "NO_SUCH", &back) == 0);
  assert(back.consecutive_failures == 0 && back.effective_interval == 0);
}

static void test_priority(void) {
  sched_state healthy = {0};
  sched_state backed  = { .consecutive_failures = 2, .effective_interval = 240 };
  sched_state quar    = { .consecutive_failures = 12, .quarantined = 1 };
  /* bands never interleave, whatever the score or overdue ratio */
  assert(sched_priority(&healthy, 0.0, 0.0) < sched_priority(&backed, 1.0, 4.0));
  assert(sched_priority(&backed, 0.0, 0.0)  < sched_priority(&quar, 1.0, 4.0));
  /* better score first inside a band */
  assert(sched_priority(&healthy, 0.9, 1.0) < sched_priority(&healthy, 0.3, 1.0));
  /* more overdue first at equal score */
  assert(sched_priority(&healthy, 0.9, 3.0) < sched_priority(&healthy, 0.9, 0.5));
  /* unrated sits between good and bad, not at the bottom */
  assert(sched_priority(&healthy, 0.9, 1.0) < sched_priority(&healthy, -1.0, 1.0));
  assert(sched_priority(&healthy, -1.0, 1.0) < sched_priority(&healthy, 0.1, 1.0));
}

static void test_policy_env(void) {
  setenv("JO_SCHED_BACKOFF_CAP_SEC", "0", 1);        /* nonsense → clamped */
  setenv("JO_SCHED_QUARANTINE_AFTER", "5", 1);
  unsetenv("JO_SCHED_QUARANTINE_PROBE_SEC");
  sched_policy p; sched_policy_load(&p);
  assert(p.backoff_cap_sec == 1);
  assert(p.quarantine_after == 5);
  assert(p.quarantine_probe_sec == 7L * 86400L);
}

/* 8. freshness: the bucket rule is the one tune_intervals.py applied at
 *    authoring time, the base is the measured bucket only for an adaptive
 *    row that has one, and the measurement reads the newest ISO date and
 *    nothing that merely sits in the column. */
static void test_freshness_bucket(void) {
  assert(sched_freshness_bucket(-1)     == 0);        /* no date: unmeasured */
  assert(sched_freshness_bucket(0)      == 3600);
  assert(sched_freshness_bucket(3)      == 3600);
  assert(sched_freshness_bucket(3.01)   == 21600);
  assert(sched_freshness_bucket(30)     == 21600);
  assert(sched_freshness_bucket(30.5)   == 86400);
  assert(sched_freshness_bucket(365)    == 86400);
  assert(sched_freshness_bucket(366)    == 604800);
  assert(sched_freshness_bucket(4000)   == 604800);
}

static void test_freshness_base(void) {
  sched_state st = {0};
  assert(sched_state_base(&st, 3600, 1) == 3600);     /* unmeasured: declared */
  st.freshness_interval = 604800;
  assert(sched_state_base(&st, 3600, 1) == 604800);   /* adaptive: the bucket */
  assert(sched_state_base(&st, 3600, 0) == 3600);     /* not adaptive: declared */
  st.freshness_interval = 3600;
  assert(sched_state_base(&st, 604800, 1) == 3600);   /* both directions */
  /* the backoff arithmetic works from the base: a failure doubles the
   * MEASURED cadence, and a success resets to it, not to the declared one */
  sched_policy p = { .backoff_cap_sec = 86400, .quarantine_after = 12,
                     .quarantine_probe_sec = 7 * 86400 };
  st.freshness_interval = 21600; st.effective_interval = 0;
  long base = sched_state_base(&st, 3600, 1);
  sched_state_apply(&st, &p, base, "error", 0, 1000);
  assert(st.effective_interval == 43200);
  sched_state_apply(&st, &p, base, "ok", 5, 2000);
  assert(st.effective_interval == 21600);
  assert(sched_state_interval(&st, &p, base) == 21600);
}

static void test_freshness_measure(db_handle *db) {
  static const char *rows[][3] = {
    /* uid, source, published_at */
    { "fr|1", "UNIT_FRESH_A", "2026-10-18T09:00:00Z" },   /* 2 days: hourly */
    { "fr|2", "UNIT_FRESH_A", "2025-01-01" },
    { "fr|3", "UNIT_FRESH_A", "1760000000" },             /* epoch: not a date */
    { "fr|4", "UNIT_FRESH_B", "2026-09-01T00:00:00Z" },   /* 49 days: daily */
    { "fr|5", "UNIT_FRESH_C", "Tue, 20 Oct 2026 09:00:00 GMT" }, /* RFC 822 only */
    { "fr|6", "UNIT_FRESH_D", "2024-01-01" },             /* > 1 year: weekly */
    { "fr|7", "UNIT_FRESH_E", "2026-10-21T09:00:00Z" },   /* tomorrow: fresh */
  };
  sqlite3_exec(db->h, "DELETE FROM intel_items WHERE uid LIKE 'fr|%'", 0, 0, 0);
  for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db->h,
        "INSERT INTO intel_items(uid,source_id,title,published_at,fetched_at) "
        "VALUES(?1,?2,'t',?3,'2026-10-20T10:00:00Z')", -1, &s, NULL) == SQLITE_OK);
    sqlite3_bind_text(s, 1, rows[i][0], -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, rows[i][1], -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 3, rows[i][2], -1, SQLITE_STATIC);
    assert(sqlite3_step(s) == SQLITE_DONE);
    sqlite3_finalize(s);
  }
  time_t now = 1792407600;   /* 2026-10-20T10:00:00Z: 1792407600 = (2026-10-20 epoch days)*86400 + 36000 */
  {
    /* pin the fake clock against the real calendar rather than trusting the
     * constant above: a wrong constant would make every assert below a
     * statement about the wrong day */
    struct tm tm = {0}; tm.tm_year = 126; tm.tm_mon = 9; tm.tm_mday = 20; tm.tm_hour = 10;
    now = timegm(&tm);
  }
  assert(sched_freshness_measure(db, "UNIT_FRESH_A", now) == 3600);
  assert(sched_freshness_measure(db, "UNIT_FRESH_B", now) == 86400);
  assert(sched_freshness_measure(db, "UNIT_FRESH_C", now) == 0);      /* undated: no change */
  assert(sched_freshness_measure(db, "UNIT_FRESH_D", now) == 604800);
  assert(sched_freshness_measure(db, "UNIT_FRESH_E", now) == 3600);
  assert(sched_freshness_measure(db, "UNIT_FRESH_NONE", now) == 0);
  /* persistence: the measured bucket survives a save/load round trip, and
   * an unmeasured one is NULL, not 0, in the table */
  sched_state st = { .effective_interval = 21600, .freshness_interval = 21600 };
  assert(sched_state_save(db, "UNIT_FRESH_A", &st, 3600, now) == 0);
  sched_state back;
  assert(sched_state_load_one(db, "UNIT_FRESH_A", &back) == 1);
  assert(back.freshness_interval == 21600);
  st.freshness_interval = 0;
  assert(sched_state_save(db, "UNIT_FRESH_A", &st, 3600, now) == 0);
  assert(sched_state_load_one(db, "UNIT_FRESH_A", &back) == 1);
  assert(back.freshness_interval == 0);
  sqlite3_exec(db->h, "DELETE FROM intel_items WHERE uid LIKE 'fr|%'", 0, 0, 0);
}

int main(void) {
  db_handle db = {0};
  const char *path = getenv("JO_DB");
  assert(path && "run.sh sets JO_DB to a scratch database");
  if (db_open(&db, path, NULL) != 0) { fprintf(stderr, "db_open failed\n"); return 2; }

  test_backoff_doubling();
  test_cap_below_declared();
  test_empties_are_not_failures();
  test_quarantine_transitions();
  test_persistence(&db);
  test_priority();
  test_policy_env();
  test_freshness_bucket();
  test_freshness_base();
  test_freshness_measure(&db);

  db_close(&db);
  printf("test_sched_backoff: ok\n");
  return 0;
}
