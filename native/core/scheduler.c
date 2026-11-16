/* core/scheduler.c — the collection scheduler: one dispatcher thread that
 * decides WHEN each source is due (sched_next_due — the one rule), and a
 * worker pool per lane that runs them (skip-if-running, health backoff). */
#include "cadence.h"
#include "source_interval.h"
#include "scheduler.h"
#include <limits.h>
#include "../source.h"
#include "intel.h"
#include "evidence.h"
#include "content_change.h"
#include "maint_detect.h"
#include "hostgate.h"
#include "source_trust.h"
#include "source_settings.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include "_credential_notice.inc"  /* jo_needs_credential: the gated notice */
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

/* Counting sink: wraps the real intel_sink and tallies emit() calls so the
 * scheduler knows records_fetched for fetch_log/detection. Both a NEW row
 * (emit==1) and an UPDATE (emit==0) count as records.
 *
 * A `collector-*-notice` record (intel_item_is_notice, core/intel.h) is data
 * about the run, not a record OF the source: it is tallied apart so a source
 * that stored nothing but its own notice still reads records=0.
 *
 * REFUSALS ARE COUNTED TOO. An emit() < 0 is a record the collector fetched
 * and the sink did not store — a ROLLBACK on "database is locked", a failed
 * COMMIT, an item with no uid. This used to drop them on the floor: the run
 * line said records=N for the N that landed and nothing about the ones that
 * did not, so a lock storm that cost a feed half its rows read as a quieter
 * feed. `failed=` on the run line and the fetch_log error column carry them. */
typedef struct { intel_sink *inner; long n; long notices; long failed; } count_sink;
static int count_emit(intel_sink *s, const intel_item *it) {
  count_sink *cs = (count_sink *)s->ctx;
  int r = cs->inner->emit(cs->inner, it);
  if (r < 0) cs->failed++;
  else if (intel_item_is_notice(it)) cs->notices++;
  else cs->n++;
  return r;
}

/* ── run outcome → fetch_log.status ───────────────────────────────────────
 *
 * THIS IS THE ROOT OF "QUARANTINE ON SUCCESS". The line here used to be
 *
 *     const char *status = rc == 0 ? "ok" : "error";
 *
 * and everything downstream is built on it: status != "ok" makes
 * anomaly_detect open a `status_bad` anomaly, the repair pod triages it, and
 * three failed repair cycles trip collector_repair.c's breaker and quarantine
 * the source. Seventeen collectors were individually rewritten to stop
 * returning non-zero on an honest empty result; forty-four still do, and the
 * line that turns their return value into a verdict was never touched. Fixing
 * it here is what makes the per-collector fixes stick and stops the remaining
 * forty-four from benching working sources.
 *
 * `rc` is not, and has never been, a clean success/failure bit. Three
 * conventions are in circulation across the fleet:
 *   `return n >= 0 ? 0 : -1`   the intended one (253 files)
 *   `return n > 0 ? 0 : -1`    "found nothing" reported as failure (44 files)
 *   `return n`                 the row COUNT returned as the status code
 * so a positive rc means "n records", never an error, and is treated as
 * success outright.
 *
 * The genuinely ambiguous case is rc < 0 with zero rows: a quiet hour on a
 * `n > 0 ? 0 : -1` collector and a dead upstream look identical from the
 * return value alone. They do not look identical to the HTTP client the run
 * was handed: it records every distinct host contacted and whether any
 * response from it was 2xx/3xx. If the collector talked to hosts and every one
 * of them answered, the fetch worked and the feed had nothing to give — an
 * empty result, not an outage. If no host answered, or none was contacted at
 * all, we have no evidence the run succeeded and it stays an error.
 *
 * "ok" with records_fetched = 0 is exactly what the seventeen already-fixed
 * collectors produce, so this classifies the other forty-four the same way
 * rather than inventing a new state. A source that normally returns rows and
 * suddenly returns none is still caught — by anomaly_detect's records_drop
 * baseline, which is the check designed for it. */
static const char *run_status(int rc, long records, int hosts, int hosts_ok,
                              const char **why) {
  *why = NULL;
  if (rc >= 0) return "ok";
  if (records > 0) return "ok";        /* rows landed; the code is the count-idiom misfiring */
  if (hosts > 0 && hosts_ok == hosts) return "ok";   /* fetched fine, feed was empty */
  *why = (hosts > 0) ? "run returned non-zero; no host answered"
                     : "run returned non-zero";
  return "error";
}

/* fetch_log.stored — house rule 4b's number, kept next to records_fetched so
 * the two can be compared over history rather than only in the run a human
 * happened to be watching. The column is added by db.c's ensure_column()
 * block; it is NULL on rows written before it existed, and NULL is the honest
 * value there ("not measured"), which is why it has no DEFAULT 0.
 *
 * WHY A FOLLOW-UP UPDATE and not a sixth argument to fetch_log_write().
 * fetch_log_write() is the shared stage-0 API in core/maint_detect.c and its
 * signature is quoted in seven header files as the thing scheduler.c skips for
 * `_maint` collectors; widening it would touch every one of them for a column
 * only this caller can supply. This is one UPDATE by INTEGER PRIMARY KEY on a
 * row inserted microseconds earlier — an rowid seek, still inside the same
 * page cache. If the column is absent (a database that has not booted through
 * db_open, e.g. a unit-test fixture) the prepare fails and we say nothing:
 * the run line already carried the number, and a missing migration is not a
 * reason to make a collector run look failed.
 *
 * A floor (`exact` = 0) is stored NEGATED. There is no room in an INTEGER
 * column for "at least", and writing the floor as though it were the truth
 * would be exactly the quiet over-reporting rule 4b exists to stop; a reader
 * seeing stored < 0 knows the real value is >= -stored. */
static void fetch_log_set_bytes(db_handle *db, long flid, unsigned long long bytes) {
  if (flid <= 0) return;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, "UPDATE fetch_log SET bytes_fetched=?1 WHERE id=?2",
                         -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_int64(s, 1, (sqlite3_int64)bytes);
  sqlite3_bind_int64(s, 2, flid);
  sqlite3_step(s);
  sqlite3_finalize(s);
}
static void fetch_log_set_stored(db_handle *db, long flid, long stored,
                                 int exact) {
  if (!db || !db->h || flid < 0 || stored < 0) return;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "UPDATE fetch_log SET stored=?1 WHERE id=?2",
                         -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_int64(s, 1, (sqlite3_int64)(exact ? stored : -stored));
  sqlite3_bind_int64(s, 2, (sqlite3_int64)flid);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* ── health-driven scheduling: backoff, quarantine, priority ─────────────
 *
 * WHY. The dispatcher used to treat every scheduled source identically: due
 * every `update_interval_sec`, forever, whatever happened last time. A source
 * whose host has been unreachable for a month was still fetched every 60 s,
 * occupying a worker slot and a hostgate token each time, and the only thing
 * that could ever bench it was the LLM repair pod's breaker — which needs an
 * anomaly, a triage call and three failed repair cycles before it acts, and
 * cannot act at all when the LLM is unconfigured.
 *
 * WHAT COUNTS AS A FAILURE. Exactly what run_status() calls "error": rc<0 with
 * no records and no host that answered. An honest empty (status "ok",
 * records=0) is NOT a failure — the fetch worked, the feed had nothing — so
 * it does not back the source off. It is counted separately
 * (consecutive_empties) because a source that has been empty for 200 runs is
 * the EMPTY_RESULTSET class from CLAUDE.md and someone should see the number.
 *
 * THE ARITHMETIC. effective_interval starts at the declared interval. Each
 * failure doubles it, capped at JO_SCHED_BACKOFF_CAP_SEC (86400): a 60 s
 * source that dies is retried at 120, 240, ... 61440, 86400 s. A success
 * resets it to the declared interval in one step — an upstream that has come
 * back does not deserve to be punished for the outage. After
 * JO_SCHED_QUARANTINE_AFTER (12) consecutive failures the source is
 * quarantined: it is probed once per JO_SCHED_QUARANTINE_PROBE_SEC (7 days)
 * and released by the first successful probe. Both counters are persisted so
 * a restart does not turn 11 failures back into 0. */

/* The ONE reader of a numeric JO_SCHED_* knob: unset, empty or not a number
 * is the default (atol used to read "abc" as 0, which for
 * JO_SCHED_DEADLINE_SEC silently switched the watchdog off); a number is
 * clamped into [lo, hi]. */
static long env_long(const char *name, long dflt, long lo, long hi) {
  const char *v = getenv(name);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long x = strtol(v, &end, 10);
  if (end == v) return dflt;
  return x < lo ? lo : (x > hi ? hi : x);
}

void sched_policy_load(sched_policy *p) {
  p->backoff_cap_sec      = env_long("JO_SCHED_BACKOFF_CAP_SEC", 86400, 1, LONG_MAX / 4);
  p->quarantine_after     = (int)env_long("JO_SCHED_QUARANTINE_AFTER", 12, 1, INT_MAX);
  p->quarantine_probe_sec = env_long("JO_SCHED_QUARANTINE_PROBE_SEC",
                                     7L * 86400L, 1, LONG_MAX / 4);
}

void sched_state_apply(sched_state *st, const sched_policy *p, long declared,
                       const char *status, long records, time_t now) {
  if (declared < 1) declared = 1;
  if (st->effective_interval < declared) st->effective_interval = declared;
  int failed = !(status && strcmp(status, "ok") == 0);
  if (st->quarantined) st->last_probe = now;

  if (failed) {
    st->consecutive_failures++;
    /* Double from the CURRENT effective interval, never below declared and
     * never above the cap — unless the declared interval already exceeds the
     * cap, in which case the cap must not SHORTEN a source's cadence. */
    long cap = p->backoff_cap_sec > declared ? p->backoff_cap_sec : declared;
    long next = st->effective_interval > cap / 2 ? cap : st->effective_interval * 2;
    st->effective_interval = next;
    st->backoff_until = now + next;
    if (!st->quarantined && st->consecutive_failures >= p->quarantine_after) {
      st->quarantined = 1;
      st->quarantined_at = now;
      st->last_probe = now;
    }
    return;
  }
  /* "ok": the fetch worked. That is the proof of life quarantine was waiting
   * for, whether or not the feed had rows to give. */
  st->consecutive_failures = 0;
  st->effective_interval = declared;
  st->backoff_until = 0;
  if (st->quarantined) { st->quarantined = 0; st->quarantined_at = 0; st->last_probe = 0; }
  if (records > 0) st->consecutive_empties = 0;
  else             st->consecutive_empties++;
}

long sched_state_interval(const sched_state *st, const sched_policy *p,
                          long declared) {
  if (declared < 1) declared = 1;
  if (st->quarantined) return p->quarantine_probe_sec;
  /* The stored effective_interval is a BACKOFF only while the source is
   * failing. On a healthy row it is whatever base the last run saved, and
   * reading it as a floor held a row at its OLD cadence for one more cycle
   * after the policy, a per-source interval or the freshness measurement
   * made it faster (6 h -> 1 h ran once at 1 h, then waited 6 h). */
  if (st->consecutive_failures > 0 && st->effective_interval > declared)
    return st->effective_interval;
  return declared;
}

/* ── freshness-adaptive cadence ──────────────────────────────────────────
 *
 * WHY. Batches 43–48 (21,000 feeds declared by public bodies and the press)
 * had their intervals SET from a measurement: the age of the newest item on
 * the day each feed was validated, bucketed hourly / 6-hourly / daily /
 * weekly. That reading was true on 2026-10-19 and nothing kept it true. A
 * parish council that posts weekly in autumn and daily before an election,
 * a ministry whose feed broke for a month and came back — every one keeps
 * the cadence of one afternoon forever, which is the same frozen reading as
 * a date written into a URL (hpengine.h's {date} note), on a different axis.
 *
 * WHAT. After every successful run of a source whose def says
 * adaptive_interval, the scheduler reads the newest ISO published_at the
 * source has stored and re-buckets it by the same rule, and that bucket is
 * the base the backoff arithmetic above works from. Both directions: a
 * quiet feed drifts to weekly, a feed that wakes comes back to hourly on
 * its next run (which, at worst, is a week away — the cost of having gone
 * quiet, not of the mechanism). Bounded by the buckets: never faster than
 * hourly, never slower than weekly, whatever the declared interval was.
 *
 * WHAT IT DOES NOT DO. Nothing for a row whose records carry no ISO date
 * (undated feeds keep their declared weekly; no evidence, no change), nothing
 * for a row that is not adaptive, nothing on a failed run (a 403 says
 * nothing about freshness, and the backoff already handles it), and it never
 * reads a date that is not a date: an epoch number or dd/mm/yyyy in
 * published_at is skipped by the GLOB, exactly as embed_pod's eligibility
 * rule skips them. */
long sched_freshness_bucket(double age_days) {
  if (age_days < 0)   return 0;
  if (age_days <= 3)   return 3600;
  if (age_days <= 30)  return 21600;
  if (age_days <= 365) return 86400;
  return 604800;
}

long sched_state_base(const sched_state *st, long declared, int adaptive) {
  /* An entity pivot (interval 0) is never scheduled, whatever its rows'
   * dates say — the composition test caught the measurement giving one a
   * cadence. The adaptive flag refines a schedule; it cannot create one. */
  if (declared <= 0) return declared;
  if (adaptive && st && st->freshness_interval > 0) return st->freshness_interval;
  return declared;
}

/* The ONE composition of the two mechanisms that can move a row's cadence
 * (2026-10-20, when they met in a merge): the row's own measurement inside,
 * the operator's system-wide policy outside.
 *
 *   sched_row_interval = cadence_apply( sched_state_base(st, declared, adaptive) )
 *
 * So a feed whose items say "hourly" runs hourly under no policy, six-hourly
 * under an operator floor of six hours, and at the global cadence when one is
 * set — a person's floor outranks a measurement, a global outranks both, and
 * the measurement is still taken and stored, so a page can say "measured
 * hourly, held at 6 h by policy" rather than hiding one behind the other.
 * The health backoff (sched_state_apply / sched_state_interval) then works
 * from this figure. An entity pivot (interval 0) passes through untouched. */
int sched_is_pod(const source_def *d) {
  return d && d->collector && d->collector[0] == '_';
}

long sched_row_interval(const sched_state *st, const source_def *d) {
  if (!d) return 0;
  /* Third mechanism (2026-10-21, core/source_interval.h): an interval the
   * operator set for THIS source outranks both the measurement and the
   * system-wide policy — specific beats general. It cannot create a
   * schedule: source_interval_set refuses a row with interval 0, and the
   * guard here keeps a stale row from doing so either. */
  if (d->update_interval_sec > 0) {
    long ov = source_interval_for(d->id);
    if (ov > 0) return ov;
  }
  return source_interval_default(st, d);   /* the policy over the row's base; a pod is exempt */
}

long sched_next_delay(const sched_state *st, const sched_policy *p,
                      const source_def *d) {
  long iv = sched_row_interval(st, d);
  return iv > 0 ? sched_state_interval(st, p, iv) : 0;
}

time_t sched_hold_until(const sched_state *st, const sched_policy *p) {
  if (st->quarantined && st->last_probe > 0)
    return st->last_probe + (time_t)p->quarantine_probe_sec;
  return st->backoff_until > 0 ? st->backoff_until : 0;
}

time_t sched_next_due(const sched_state *st, const sched_policy *p,
                      const source_def *d, time_t last) {
  if (!d || d->update_interval_sec <= 0) return 0;
  time_t h = sched_hold_until(st, p);
  if (h > 0) return h;
  return last > 0 ? last + (time_t)sched_next_delay(st, p, d) : 0;
}

long sched_freshness_measure(db_handle *db, const char *id, time_t now) {
  if (!db || !db->h || !id) return 0;
  sqlite3_stmt *s;
  /* ORDER BY on the (source_id, published_at DESC) index rather than max():
   * the GLOB keeps non-ISO values out, and the first row the index yields
   * that passes it is the newest ISO date. */
  if (sqlite3_prepare_v2(db->h,
        "SELECT published_at FROM intel_items WHERE source_id=?1 AND "
        "published_at GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]*' "
        "ORDER BY published_at DESC LIMIT 1", -1, &s, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  long out = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *v = (const char *)sqlite3_column_text(s, 0);
    struct tm tm; memset(&tm, 0, sizeof tm);
    int Y, M, D, h = 0, m = 0, sec = 0;
    if (v && sscanf(v, "%4d-%2d-%2d", &Y, &M, &D) == 3) {
      if (strlen(v) >= 19 && (v[10] == 'T' || v[10] == ' '))
        sscanf(v + 11, "%2d:%2d:%2d", &h, &m, &sec);
      tm.tm_year = Y - 1900; tm.tm_mon = M - 1; tm.tm_mday = D;
      tm.tm_hour = h; tm.tm_min = m; tm.tm_sec = sec;
      time_t t = timegm(&tm);
      double age = t == (time_t)-1 ? -1.0 : (double)(now - t) / 86400.0;
      /* A date in the future (a scheduled post, a clock skew, a feed that
       * stamps items with tomorrow) is "fresh", not "unmeasured". */
      if (age < 0 && t != (time_t)-1) age = 0;
      out = sched_freshness_bucket(age);
    }
  }
  sqlite3_finalize(s);
  return out;
}

/* Lower key runs first. Three bands that never interleave — healthy sources
 * always beat backed-off ones, which always beat quarantine probes — and
 * inside a band: better trust score first, then the MOST overdue first, so a
 * healthy source whose slot was stolen by a slow fleet catches up before a
 * healthy source that just became due. Unrated (score<0) sits in the middle
 * of its band: no evidence is not evidence of badness (source_trust.h). */
double sched_priority(const sched_state *st, double score, double overdue_ratio) {
  double band = st->quarantined ? 20.0 : (st->consecutive_failures > 0 ? 10.0 : 0.0);
  double s = score < 0 ? 0.5 : (score > 1.0 ? 1.0 : score);
  if (overdue_ratio < 0) overdue_ratio = 0;
  if (overdue_ratio > 4) overdue_ratio = 4;   /* bounded so it stays inside the band */
  return band + (1.0 - s) * 4.0 - overdue_ratio;
}

int sched_state_load_one(db_handle *db, const char *id, sched_state *st) {
  memset(st, 0, sizeof *st);
  if (!db || !db->h) return -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT consecutive_failures,consecutive_empties,effective_interval,"
        "backoff_until,quarantined,quarantined_at,last_probe,freshness_interval,"
        "updated_at FROM source_sched_state WHERE source_id=?1", -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  int rc = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    st->consecutive_failures = sqlite3_column_int(s, 0);
    st->consecutive_empties  = sqlite3_column_int(s, 1);
    st->effective_interval   = (long)sqlite3_column_int64(s, 2);
    st->backoff_until        = (time_t)sqlite3_column_int64(s, 3);
    st->quarantined          = sqlite3_column_int(s, 4);
    st->quarantined_at       = (time_t)sqlite3_column_int64(s, 5);
    st->last_probe           = (time_t)sqlite3_column_int64(s, 6);
    st->freshness_interval   = (long)sqlite3_column_int64(s, 7);   /* NULL → 0 */
    st->last_run             = (time_t)sqlite3_column_int64(s, 8);
    rc = 1;
  }
  sqlite3_finalize(s);
  return rc;
}

/* ── WHERE A COLLECTOR STOPPED, kept across runs (source.h's resume_in /
 * resume_out). Deliberately NOT part of `sched_state`: that struct is the
 * scheduler's own health view — failures, backoff, quarantine — and a
 * collector's reading position has nothing to do with whether the source is
 * healthy. Writing them together would also mean every resume update rewrote
 * the backoff columns.
 *
 * The column is on source_sched_state because that table is already one row
 * per scheduled source with the same lifetime. A row that is not scheduled has
 * no resume position by construction: only this path sets the callbacks. */
#define SCHED_RESUME_MAX 8192

static int sched_resume_load(db_handle *db, const char *id, char *out, size_t cap) {
  if (out && cap) out[0] = 0;
  if (!db || !db->h || !id || !out || !cap) return 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT resume_token FROM source_sched_state WHERE source_id=?1",
        -1, &s, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  int got = 0;
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL) {
    const char *v = (const char *)sqlite3_column_text(s, 0);
    if (v && v[0] && strlen(v) < cap) { snprintf(out, cap, "%s", v); got = 1; }
  }
  sqlite3_finalize(s);
  return got;
}

/* The row may not exist yet (a first run that resumes before any save), so this
 * inserts rather than updating — with the defaults the table declares. A NULL
 * or empty token CLEARS the position, which is how a collector says its cycle
 * completed and the next run should start over. */
static void sched_resume_store(db_handle *db, const char *id, const char *token) {
  if (!db || !db->h || !id) return;
  if (token && strlen(token) >= SCHED_RESUME_MAX) {
    fprintf(stderr, "[sched] %s resume token is %zu bytes (max %d); dropped, "
                    "the next run starts over\n",
            id, strlen(token), SCHED_RESUME_MAX - 1);
    token = NULL;
  }
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO source_sched_state(source_id,resume_token,updated_at) "
        "VALUES(?1,?2,?3) ON CONFLICT(source_id) DO UPDATE SET "
        "resume_token=excluded.resume_token,updated_at=excluded.updated_at",
        -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  if (token && token[0]) sqlite3_bind_text(s, 2, token, -1, SQLITE_TRANSIENT);
  else                   sqlite3_bind_null(s, 2);
  sqlite3_bind_int64(s, 3, (sqlite3_int64)time(NULL));
  sqlite3_step(s);
  sqlite3_finalize(s);
}

static void sched_resume_out(const source_ctx *ctx, const char *token) {
  if (ctx) sched_resume_store(ctx->db, ctx->source_id, token);
}

int sched_state_save(db_handle *db, const char *id, const sched_state *st,
                     long declared, time_t now) {
  if (!db || !db->h) return -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO source_sched_state(source_id,consecutive_failures,"
        "consecutive_empties,declared_interval,effective_interval,backoff_until,"
        "quarantined,quarantined_at,last_probe,updated_at,freshness_interval) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11) ON CONFLICT(source_id) DO UPDATE SET "
        "consecutive_failures=excluded.consecutive_failures,"
        "consecutive_empties=excluded.consecutive_empties,"
        "declared_interval=excluded.declared_interval,"
        "effective_interval=excluded.effective_interval,"
        "backoff_until=excluded.backoff_until,quarantined=excluded.quarantined,"
        "quarantined_at=excluded.quarantined_at,last_probe=excluded.last_probe,"
        "updated_at=excluded.updated_at,"
        "freshness_interval=excluded.freshness_interval", -1, &s, NULL) != SQLITE_OK)
    return -1;
  if (st->freshness_interval > 0)
    sqlite3_bind_int64(s, 11, (sqlite3_int64)st->freshness_interval);
  else
    sqlite3_bind_null(s, 11);
  sqlite3_bind_text (s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int  (s, 2, st->consecutive_failures);
  sqlite3_bind_int  (s, 3, st->consecutive_empties);
  sqlite3_bind_int64(s, 4, (sqlite3_int64)declared);
  sqlite3_bind_int64(s, 5, (sqlite3_int64)st->effective_interval);
  if (st->backoff_until) sqlite3_bind_int64(s, 6, (sqlite3_int64)st->backoff_until);
  else                   sqlite3_bind_null(s, 6);
  sqlite3_bind_int  (s, 7, st->quarantined);
  if (st->quarantined_at) sqlite3_bind_int64(s, 8, (sqlite3_int64)st->quarantined_at);
  else                    sqlite3_bind_null(s, 8);
  if (st->last_probe) sqlite3_bind_int64(s, 9, (sqlite3_int64)st->last_probe);
  else                sqlite3_bind_null(s, 9);
  sqlite3_bind_int64(s, 10, (sqlite3_int64)now);
  int rc = sqlite3_step(s) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(s);
  return rc;
}

/* Fold a finished run into the persisted state. Only scheduled runs of real
 * collectors (not entity pivots — a pivot that finds nothing about one entity
 * says nothing about the upstream's health, and not the _maint/_enrich pods).
 * Failure to persist is logged, not fatal: the run itself succeeded or not
 * on its own terms, and a database without the table (a unit-test fixture
 * booted without schema.sql) must not make it look failed. */
static void sched_publish(const source_def *d, const sched_state *st, time_t now);

static void sched_state_record(db_handle *db, const source_def *d,
                               const char *status, long records) {
  if (!d || d->update_interval_sec <= 0) return;
  sched_policy pol; sched_policy_load(&pol);
  sched_state st;
  time_t now = time(NULL);
  if (sched_state_load_one(db, d->id, &st) < 0) return;   /* no table: say nothing */
  int was_q = st.quarantined;
  /* An adaptive row re-measures its cadence from what it has stored — after
   * a run that WORKED, because a refused fetch says nothing about how often
   * the upstream publishes. The bucket replaces the declared interval as the
   * base of the backoff arithmetic; the authored interval stays on the def. */
  if (d->adaptive_interval && status && strcmp(status, "ok") == 0) {
    long fr = sched_freshness_measure(db, d->id, now);
    if (fr != st.freshness_interval)
      fprintf(stderr, "[sched] %s freshness: newest stored item re-buckets the "
                      "cadence %ld -> %ld s (declared %d)\n",
              d->id, st.freshness_interval > 0 ? st.freshness_interval
                                               : (long)d->update_interval_sec,
              fr > 0 ? fr : (long)d->update_interval_sec, d->update_interval_sec);
    st.freshness_interval = fr;
  }
  /* ONE composition, used here and in the due-time loop (sched_row_interval):
   * the row's measured base inside, the operator's cadence policy outside.
   * A person's floor outranks a measurement, a global cadence outranks both,
   * and the health backoff then stretches from the result. */
  long base = sched_row_interval(&st, d);
  sched_state_apply(&st, &pol, base, status, records, now);
  if (sched_state_save(db, d->id, &st, base, now) != 0)
    fprintf(stderr, "[sched] %s: could not persist sched state\n", d->id);
  sched_publish(d, &st, now);
  if (st.consecutive_failures > 0)
    fprintf(stderr, "[sched] %s backoff: %d consecutive failure(s), next in %lds%s\n",
            d->id, st.consecutive_failures, st.effective_interval,
            st.quarantined && !was_q ? " — QUARANTINED (health)" : "");
  else if (was_q)
    fprintf(stderr, "[sched] %s released from health quarantine\n", d->id);
}

/* ── the sources table's operational columns ─────────────────────────────
 *
 * `sources.status / last_check / last_success / response_time_ms /
 * records_count / error_message` are what GET /api/sources, GET /api/status
 * and the iOS dashboard read as "is this source alive". Until 2026-10-12
 * NOTHING wrote them after db_seed_sources() set status='pending': every one of
 * 18,883 rows read pending / 0 records / no response time forever, the status
 * tiles and pie pictured the seed, and /api/status's online/offline summary
 * counted it. The run that just finished knows all five facts, so it writes
 * them here, once, next to fetch_log — one more UPDATE on a row the run
 * already touched. The vocabulary (schema CHECK): online = the run succeeded
 * and stored at least one record; degraded = it succeeded and stored none (an
 * honest empty, or a feed that only re-fetched what it had — not a failure,
 * not health either); offline = the run failed; pending = never run, which
 * only the seed writes. records_count is the DISTINCT uids this run stored
 * (the run line's second number), falling back to emits when the sink could
 * not count. last_success moves only on a successful run. */
static void sources_row_record(db_handle *db, const char *id, const char *status,
                               long records, long duration_ms, const char *why) {
  int ok = status && !strcmp(status, "ok");
  const char *st = !ok ? "offline" : (records > 0 ? "online" : "degraded");
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE sources SET status=?1, last_check=datetime('now'),"
        " last_success=CASE WHEN ?2 THEN datetime('now') ELSE last_success END,"
        " response_time_ms=?3, records_count=?4, error_message=?5 WHERE id=?6",
        -1, &s, NULL) != SQLITE_OK) {
    fprintf(stderr, "[sched] %s: could not record run on sources row: %s\n",
            id, sqlite3_errmsg(db->h));
    return;
  }
  sqlite3_bind_text(s, 1, st, -1, SQLITE_STATIC);
  sqlite3_bind_int(s, 2, ok);
  sqlite3_bind_int64(s, 3, duration_ms);
  sqlite3_bind_int64(s, 4, records < 0 ? 0 : records);
  if (why && *why) sqlite3_bind_text(s, 5, why, -1, SQLITE_TRANSIENT);
  else             sqlite3_bind_null(s, 5);
  sqlite3_bind_text(s, 6, id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_DONE)
    fprintf(stderr, "[sched] %s: could not record run on sources row: %s\n",
            id, sqlite3_errmsg(db->h));
  sqlite3_finalize(s);
}
static volatile int *sched_cancel_slot(const char *id);
static void sched_host_learn(const source_def *d, const char *host);

/* ── house rule 1 for EVERY collector, not only the declarative engine ────
 *
 * `collector-fetch-failure` is lib/hpengine.c's answer to the invisible
 * nothing: before it, a refused fetch stored NOTHING, which is identical in the
 * database to a source that ran and found nothing — `records=0 stored=0 rc=0`
 * either way. That fixed it for the 44,017 rows the engine runs, AND FOR
 * NOTHING ELSE. The generated VJSON/VGEO fleet and the bespoke collectors,
 * about 8,600 rows, kept the old silence. This is the lesson this tree already
 * records twice, hit a third time: the uid-collision guard existed in three
 * record paths and was repaired in one, and the page ceiling lived in three
 * places with two knobs. Look for the other copies of any bug you fix.
 *
 * MEASURED 2026-10-22 by the registry-wide sweep: of its first 3,612 rows, 288
 * came back rc=-1 having emitted nothing and stored no notice — 18 of 18
 * sampled were vsrc rows — against 34 that DID disclose, every one an hp16_*
 * row. Roughly one row in twelve was failing in silence, and the sweep could
 * only report them as `EMITS_NOTHING`, a verdict about a source that fetched
 * and stored none, which is a different defect with a different remedy.
 *
 * The scheduler is the ONE place that sees every collector's run, which is why
 * this lives here rather than in each of the three record paths.
 *
 * WHAT IT MUST NOT DO is claim a cause it cannot see. hpengine knows the HTTP
 * status, whether the body parsed, and whether a 200 was really a rate-limit
 * page or a bot wall; here there is no status at all, because a bespoke
 * collector never reported one. Naming one would be this very notice inventing
 * the cause — the mistake it has already made three times in this tree
 * (Archive-It's rate limit behind a 200, Imperva's bot wall behind a 200, and
 * FDSN's `nodata=404` read back as a refusal when it means "nothing matched").
 * So the record states only what the run itself exposed: the return code, how
 * many hosts were contacted and how many answered, the bytes and request
 * count, the thread's last transport-error class, and the hosts that never
 * answered. The remedy says where to look; it does not say which door is shut.
 *
 * It also must not fire on an HONEST EMPTY, which is the whole point of the
 * rule. The first test is `run_status()`'s verdict — the same classifier
 * fetch_log and the dashboards use, not a second one beside it (rule 4c).
 *
 * AND THAT ALONE LEFT A THIRD OF THE SILENCE IN PLACE, measured 2026-10-23.
 * `run_status()` answers "should this run read red on a dashboard", and for
 * `hosts > 0 && hosts_ok == hosts` it answers "ok" under a comment saying
 * "fetched fine, feed was empty". The sweep's EMITS_NOTHING column says that
 * comment is wrong for 32 of its 80 rows: `il-knesset-dates` returns rc=-1
 * after fetching **131,618 bytes** — the OData service's HTML maintenance page
 * — and `geo-defra-ukair-stations` and `eco-511ny-events` do the same with
 * 2,038 and 9,364 bytes. Every host answered, so the run read "ok", so nothing
 * was disclosed, and the database again could not tell a refusal from an empty
 * source. A refusal wearing a success status, one layer up from the
 * Archive-It case hpengine already names.
 *
 * So the disclosure has a SECOND trigger: the collector returned non-zero and
 * brought back no record and no notice of its own. That is not a second status
 * classifier — `run_status()` is still the only thing that decides red or
 * green, and `fetch_log` and the pages are untouched. It is a different
 * question: not "is this source healthy" but "can the database tell what
 * happened". The two triggers are distinguished in the record, because from
 * here they really are different situations and one of them is ambiguous:
 * when every host answered, this run may be a refusal served as a success OR
 * a collector that returns non-zero on a quiet hour, and the notice says so
 * rather than picking. In the second case the finding is the collector's
 * return code, which is worth surfacing too — a source that is simply quiet
 * should return 0.
 *
 * Why the ambiguity is affordable: the record is upsert-keyed on
 * (source, entity), so a quiet feed with a sloppy return code carries ONE row
 * that updates rather than one per run, and the class is 30 rows in 10,252
 * measured (0.3%). "A failure notice on every quiet feed" would be noise; this
 * is not that number. */
static void sched_disclose_fetch_failure(intel_sink *sink, const source_def *d,
                                         const char *entity, int rc, int hosts,
                                         int hosts_ok, const char *hosts_failed,
                                         unsigned long long bytes, int requests,
                                         const char *why) {
  int te = http_last_transport_error();
  const char *te_name = te == HTTP_TE_CONNECT ? "connect"
                      : te == HTTP_TE_TIMEOUT ? "timeout"
                      : te == HTTP_TE_OTHER   ? "other"
                                              : "none recorded";
  /* EVERY host answered and the collector still reported a failure: the one
   * case this record must not resolve, because from the scheduler's seat a
   * refusal served as a success and a collector with a sloppy return code look
   * identical. Naming both is the honest answer; picking one would be this
   * notice inventing the cause. */
  int answered_all = hosts > 0 && hosts_ok == hosts;
  const char *outcome =
      hosts == 0
        ? "the run made no HTTP request at all — it stopped before reaching the "
          "network, or it could not build the request it needed"
      : hosts_ok == 0
        ? "no host answered: every request this run made failed or was refused"
      : answered_all
        ? "EVERY host answered and the collector still reported a failure, so "
          "this is one of two things and the run cannot tell them apart: the "
          "upstream served a refusal with a success status (a maintenance or "
          "rate-limit page, which is why the byte count below matters), or the "
          "collector returns non-zero on an hour when its source published "
          "nothing"
        : "some of the hosts contacted answered and some did not, and nothing "
          "was stored";
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "source_id", d->id);
  cJSON_AddStringToObject(p, "collector", d->collector ? d->collector : "");
  cJSON_AddStringToObject(p, "query", entity ? entity : "");
  cJSON_AddNumberToObject(p, "run_rc", (double)rc);
  cJSON_AddNumberToObject(p, "hosts_contacted", (double)hosts);
  cJSON_AddNumberToObject(p, "hosts_answered", (double)hosts_ok);
  if (hosts_failed && *hosts_failed)
    cJSON_AddStringToObject(p, "hosts_no_answer", hosts_failed);
  cJSON_AddNumberToObject(p, "bytes_fetched", (double)bytes);
  cJSON_AddNumberToObject(p, "requests", (double)requests);
  cJSON_AddStringToObject(p, "transport_error", te_name);
  /* D1-bis, the same addition hpengine's notice got: "other" covered TLS, and
   * "TLS" covered four findings with four remedies, of which exactly one — a
   * server that sent its leaf without the intermediate — is ours to fix. Same
   * caveat as `transport_error` beside it: this is the LAST attempt on this
   * thread, which for a bespoke collector making several requests is the last
   * one, not necessarily the one that mattered. Named only when there is a TLS
   * verdict at all. */
  {
    int tls = http_last_tls_cause();
    if (tls != HTTP_TLS_NONE) {
      cJSON_AddStringToObject(p, "tls_cause",
        tls == HTTP_TLS_NO_LOCAL_ISSUER   ? "no_local_issuer"
      : tls == HTTP_TLS_EXPIRED           ? "expired"
      : tls == HTTP_TLS_NOT_YET_VALID     ? "not_yet_valid"
      : tls == HTTP_TLS_HOSTNAME_MISMATCH ? "hostname_mismatch"
      : tls == HTTP_TLS_SELF_SIGNED       ? "self_signed"
      : tls == HTTP_TLS_REVOKED           ? "revoked"
      : tls == HTTP_TLS_UNRECOGNIZED_NAME ? "unrecognized_name"
      : tls == HTTP_TLS_HANDSHAKE         ? "handshake"
                                          : "other");
      cJSON_AddStringToObject(p, "tls_detail", http_tls_cause_label(tls));
      cJSON_AddStringToObject(p, "tls_remedy", http_tls_cause_remedy(tls));
    }
  }
  cJSON_AddStringToObject(p, "run_status_reason", why ? why : "");
  cJSON_AddStringToObject(p, "outcome", outcome);
  cJSON_AddStringToObject(p, "note",
    "NOTHING was stored for this run. This record exists so that a refused "
    "fetch cannot be mistaken for a source that ran and found nothing — the "
    "two are identical in the data without it. It is filed by the SCHEDULER, "
    "for a collector that files no disclosure of its own, so it carries no "
    "HTTP status: this collector never reported one, and naming a status "
    "nobody measured would make this notice the fabrication it exists to "
    "prevent.");
  cJSON_AddStringToObject(p, "remedy",
    hosts == 0
      ? "no request was attempted, so this is not an upstream problem: run the "
        "source directly (`--run <id>`) and read its stderr. A row that needs "
        "a credential (key_env) or an entity it was not given stops here"
    : answered_all
      ? "READ THE BODY, not the status: `--run <id>` and look at what came "
        "back for the byte count on the run line. A maintenance or rate-limit "
        "page served as 200 is the common case (il-knesset-dates fetches "
        "131,618 bytes of HTML and reports a failure), and the remedy is to "
        "re-point or pace the row. If the body really is an empty feed, the "
        "finding is the COLLECTOR: a source that published nothing this hour "
        "should return 0, and returning non-zero is what put this record here"
      : "run the source directly (`--run <id>`) and read its stderr for the "
        "status the collector saw. Then: a 401/403 wants a credential or the "
        "per-host agent table in core/host_agents.def; a 429 wants a per-host "
        "gap in core/hostgate.c, measured rather than guessed; a transport "
        "error may be OURS rather than the upstream's — in a container behind "
        "an egress proxy a peer reset can be the tunnel closing, so check "
        "$HTTPS_PROXY/__agentproxy/status before writing the host off");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);
  /* Upsert-keyed on (source, entity) exactly as hpengine's is: a permanently
   * dead source carries ONE row that updates every run, not one row per run. */
  char key[320], title[256];
  snprintf(key, sizeof key, "%.150s|fetch-failure:%.120s", d->id,
           entity ? entity : "");
  if (hosts == 0)
    snprintf(title, sizeof title,
             "%s stored nothing: the run made no request", d->id);
  else if (answered_all)
    snprintf(title, sizeof title,
             "%s stored nothing: every host answered (%llu bytes) and the "
             "collector reported a failure", d->id, bytes);
  else
    snprintf(title, sizeof title,
             "%s stored nothing: %d host(s) contacted, %d answered", d->id,
             hosts, hosts_ok);
  intel_item note = {0};
  note.remote_key      = key;
  note.title           = title;
  note.lang            = "en";
  note.record_type     = "collector-fetch-failure";
  note.properties_json = pj ? pj : "{}";
  note.tags_json       = "[\"osint-search\",\"fetch-failure\"]";
  sink->emit(sink, &note);
  free(pj);
  /* Say it on stderr too, for the same reason hpengine does: the registry
   * sweep corroborates a FETCH_FAILED verdict against the run's own output
   * rather than inferring it from a row count, and it has no `[hp:…]` line to
   * match for a collector that is not an hpengine row. */
  fprintf(stderr, "[sched] %s fetch-failure: %s (rc=%d, hosts %d/%d answered, "
                  "%llu bytes)\n",
          d->id,
          answered_all ? "every host answered and the collector reported a "
                         "failure — read the body, not the status"
                       : outcome,
          rc, hosts_ok, hosts, bytes);
}

int scheduler_run_source(db_handle *db, const source_def *d,
                         const char *entity) {
  intel_sink inner = intel_sink_make(db, d->id, "legacy");
  count_sink cs = { .inner = &inner, .n = 0, .notices = 0, .failed = 0 };
  intel_sink sink = { .ctx = &cs, .emit = count_emit };
  /* ctx->cancel used to point at a local nobody else could reach, so every
   * `if (*ctx->cancel)` in the tree was dead code and the watchdog could only
   * log. A run the pool or a manual /run claimed gets that source's slot
   * (sched_cancel_slot), which the watchdog sets at the deadline and shutdown
   * sets for everything; an unclaimed run (--run, a test) keeps a local. */
  volatile int local_cancel = 0;
  volatile int *cancel = entity ? NULL : sched_cancel_slot(d->id);
  if (!cancel) cancel = &local_cancel;
  http_client *http = http_client_new();   /* sources expect ctx->http set */
  llm_client llm; llm_init(&llm, http);
  source_ctx ctx = {0};
  ctx.source_id = d->id;
  ctx.entity = entity;
  ctx.db = db;
  ctx.http = http;
  ctx.llm = &llm;
  ctx.cancel = cancel;
  /* The reading position is for a SCHEDULED run only. An entity pivot must
   * answer from the start of the result set for the entity it was just asked
   * about — resuming there would hand back page 37 of whatever the previous
   * caller asked. source.h says this; here is where it is true. */
  /* A worker thread carries the last transport/TLS verdict of whatever it ran
   * before; clear it so this run's disclosure can only name its own. */
  http_reset_last_error();
  char resume_tok[SCHED_RESUME_MAX];
  if (!entity && sched_resume_load(db, d->id, resume_tok, sizeof resume_tok))
    ctx.resume_in = resume_tok;
  if (!entity) ctx.resume_out = sched_resume_out;
  /* The operator's per-source body ceiling, resolved once per run. 0 when the
   * setting is absent or switched off, which every caller reads as "use the
   * process default" — so a deployment that sets nothing is byte-identical to
   * before this field existed. Resolved HERE rather than inside the collector
   * because lib/ and the collectors hold no database code (source.h). */
  ctx.body_ceiling = (size_t)source_settings_int(db, d->id,
                                                 SRCSET_BODY_CEILING, 0);

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  /* Bind the evidence scope (roadmap 17) around the collector run. httpclient.c
   * has no db_handle or source_id of its own, so its capture hook reads them
   * from this thread-local scope — and fails closed when none is bound. */
  evidence_scope_begin(db, d->id, NULL);
  /* Same thread-local pattern for content-change detection (roadmap 26): the
   * httpclient hook needs a db_handle and source_id it cannot otherwise see. */
  content_change_scope_begin(db, d->id);
  http_credential_refusal_reset();      /* sticky per thread: this run's only */
  hostgate_set_cancel(cancel);
  /* The operator's per-source ceiling applies to EVERY engine's requests, not
   * only to the collectors that read ctx.body_ceiling: hpengine and VJSON rows
   * never do, so the setting was a no-op for them. Restored on return — a
   * worker thread outlives the run, and a leaked ceiling would widen the next
   * source's fetch. An explicit $JO_HTTP_MAX_BODY still clamps it. */
  size_t prev_ceiling = http_body_ceiling_set(ctx.body_ceiling);
  int rc = d->run(&ctx, &sink);
  http_body_ceiling_set(prev_ceiling);
  hostgate_set_cancel(NULL);
  content_change_scope_end();
  evidence_scope_end();
  clock_gettime(CLOCK_MONOTONIC, &t1);
  long duration_ms = (t1.tv_sec - t0.tv_sec) * 1000L +
                     (t1.tv_nsec - t0.tv_nsec) / 1000000L;
  /* Read the transport evidence BEFORE the client is freed. The names of the
   * hosts that never answered are copied out here for the same reason: the
   * client owns those strings and is about to be freed. */
  int hosts = http_client_host_count(http), hosts_ok = 0;
  char hosts_failed[256]; hosts_failed[0] = '\0';
  for (int i = 0; i < hosts; i++) {
    int ok = 0;
    const char *h = http_client_host_at(http, i, NULL, &ok);
    if (h && ok) { hosts_ok++; continue; }
    if (!h) continue;
    size_t have = strlen(hosts_failed);
    size_t need = strlen(h) + (have ? 2 : 0);
    if (have + need < sizeof hosts_failed - 1) {
      if (have) strcat(hosts_failed, ", ");
      strcat(hosts_failed, h);
    }
  }
  unsigned long long bytes_in = http_client_bytes_in(http);
  int requests = http_client_request_count(http);
  /* The host this source really talks to, learned for host-aware dispatch
   * (sched_host_learn): the registry cannot know it for a bespoke collector. */
  if (!entity && hosts > 0 && d->collector && d->collector[0] != '_') {
    const char *h0 = http_client_host_at(http, 0, NULL, NULL);
    if (h0) sched_host_learn(d, h0);
  }
  http_client_free(http);

  /* The run's verdict, computed ONCE and used twice: here, to decide whether
   * the run owes a disclosure, and below for fetch_log and the sources row.
   * It was computed only below until 2026-10-22, which is why the disclosure
   * could not live anywhere but inside each engine — see
   * sched_disclose_fetch_failure for what that cost. */
  /* A host that needs a configured credential (SEC_CONTACT) refused the run's
   * requests before any network. A run that left NOTHING behind because of it
   * is GATED, the way a key-gated collector is: one collector-status-notice
   * naming the credential, rc 0 (a missing credential is a state, not a run
   * failure — see _credential_notice.inc). hpengine and the bespoke SEC
   * collectors file the same notice themselves and then never reach this. */
  {
    const char *need = http_credential_refusal();
    if (need && cs.n == 0 && cs.notices == 0) {
      jo_needs_credential(&sink, d->id, d->name ? d->name : d->id,
                          (const char *[]){ need, NULL }, d->url,
                          "an e-mail address SEC can reach, set from the Keys "
                          "screen or the environment — SEC answers 403 to a "
                          "User-Agent without one");
      rc = 0;
    }
  }
  const char *run_why = NULL;
  const char *run_st = run_status(rc, cs.n, hosts, hosts_ok, &run_why);
  int real_collector = d->collector && d->collector[0] != '_';
  /* `cs.n == 0 && cs.notices == 0` is "the run left NOTHING behind", which is
   * also what excludes every hpengine row without naming one: the engine's own
   * collector-fetch-failure is emitted through this same sink and counts into
   * cs.n (it is not a `collector-*-notice`, so intel_item_is_notice is false
   * for it, which is deliberate — CLAUDE.md records that a dead hp source
   * reports records=1 stored=1). A row that disclosed anything at all is left
   * alone rather than given a second, thinner notice on top. */
  /* Two triggers, one record — see the comment on sched_disclose_fetch_failure.
   * `run_status() != "ok"` is the health verdict; `rc < 0` is the collector's
   * own, and it is the one that reaches the 32 rows in 10,252 whose host
   * answered with a maintenance page. Both require that the run left NOTHING
   * behind, which is what makes this a disclosure rather than a second status. */
  /* What the COLLECTOR emitted, read before our own disclosure goes through
   * the same counting sink. The disclosure is a record on the run line (the
   * convention hp rows set), but it is not evidence the feed had rows: counted
   * into the empties streak it reset consecutive_empties on every run of a
   * source that was being refused with a success status, so the one counter
   * meant to show "empty for 200 runs" read 0 for exactly those sources. */
  long collector_records = cs.n;
  int disclosed = 0;
  if (real_collector && cs.n == 0 && cs.notices == 0 &&
      (strcmp(run_st, "ok") != 0 || rc < 0)) {
    sched_disclose_fetch_failure(&sink, d, entity, rc, hosts, hosts_ok,
                                 hosts_failed, bytes_in, requests, run_why);
    disclosed = 1;
  }
  /* ENG-07: a refusal the scheduler had to disclose is not a record OF the
   * source. The disclosure goes through the counting sink, so cs.n and the
   * stored distinct-uid count both include it (the run line says records=1
   * stored=1, deliberately, as hpengine's does) - but the health surfaces must
   * not read that one row as the source delivering something. Without this a
   * run whose every host answered (run_status() "ok") stored the disclosure,
   * read records=1, and so read 'online', reset consecutive_empties and left
   * nothing for the EMPTY_RESULTSET counter to count. */
  char disc_why[256]; disc_why[0] = '\0';
  if (disclosed) {
    if (run_why) snprintf(disc_why, sizeof disc_why, "%s", run_why);
    else snprintf(disc_why, sizeof disc_why,
                  "collector returned non-zero (rc=%d) with every host answering "
                  "(%llu bytes): a refusal served as a success, or a failure code "
                  "on a quiet run - see its collector-fetch-failure record",
                  rc, bytes_in);
  }
  /* Read the distinct-row count BEFORE the sink is freed — it lives in the
   * sink_state that free() is about to release. */
  int stored_exact = 1;
  long stored = intel_sink_stored(&inner, &stored_exact);
  long stored_notices = intel_sink_stored_notices(&inner);
  intel_sink_free(&inner);        /* make() heap-allocates; nothing freed it */

  /* ── the run line, and why `stored=` sits at the END of it ───────────────
   *
   * Rule 4b: `records=` counts emit() calls, `stored=` counts the distinct
   * rows those calls actually left behind. When they differ, the difference
   * IS the finding — 12,648 emitted onto 31 rows is 12,617 records discarded
   * inside our own sink, with rc=0 and a run that looked healthy. The number
   * exists in fetch_log too, but a number that only appears in a database is
   * a number nobody reads, so it is stated here on every run.
   *
   * It is appended after the duration rather than next to `records=` for a
   * boring compatibility reason: eight parsers in tests/audit/ and tools/
   * match `records=(-?\d+) (\d+)ms` as ONE unit, and inserting a field
   * between those two groups would silently stop every one of them matching —
   * which is precisely the class of quiet failure this line exists to expose.
   * Appending is invisible to all of them.
   *
   * `stored=?` means the sink could not tell us (not an intel sink);
   * `stored>=N` means N is a floor because the counter hit its ceiling.
   *
   * `stored=` counts EVERY distinct uid, notices included, because that is
   * what `SELECT COUNT(*) … WHERE source_id=…` returns and the registry sweep
   * checks the two against each other. `records=` excludes notices, so the
   * collision test must too: comparing records against stored-with-notices let
   * each notice row hide one collapsed record (2,000 records + 1 notice, 2 of
   * them colliding, read stored=2000=records and raised nothing). */
  char sbuf[64], note[192];
  if (stored < 0) snprintf(sbuf, sizeof sbuf, "stored=?");
  else snprintf(sbuf, sizeof sbuf, "stored=%s%ld", stored_exact ? "" : ">=", stored);
  long stored_data = stored - (stored_notices > 0 ? stored_notices : 0);
  note[0] = '\0';
  if (stored >= 0 && stored_exact && stored_data < cs.n)
    snprintf(note, sizeof note,
             " UID-COLLISION: %ld of %ld emitted records collapsed onto a uid"
             " already written this run", cs.n - stored_data, cs.n);
  /* `notices=` and `failed=` trail `stored=` for the same parser-compatibility
   * reason. */
  char nbuf[32] = "", fbuf[160] = "";
  if (cs.notices > 0) snprintf(nbuf, sizeof nbuf, " notices=%ld", cs.notices);
  if (cs.failed > 0)
    snprintf(fbuf, sizeof fbuf, " failed=%ld EMIT-FAILED: the sink refused %ld"
             " record(s); they are not stored", cs.failed, cs.failed);
  /* bytes= trails the other fields like stored= does: the eight run-line
   * parsers match `records=N Nms` as one unit and ignore what follows. */
  char bbuf[64];
  snprintf(bbuf, sizeof bbuf, " bytes=%llu requests=%d", bytes_in, requests);
  fprintf(stderr, "[sched] %s run rc=%d records=%ld %ldms %s%s%s%s%s\n",
          d->id, rc, cs.n, duration_ms, sbuf, nbuf, fbuf, bbuf, note);

  /* Stage 0+1: log the run and detect anomalies — but only for real data
   * collectors. The internal pods (_maint, _enrich) emit nothing and would
   * otherwise trip duration_outlier on their own LLM calls. */
  if (real_collector) {
    const char *why = run_why;
    const char *status = run_st;
    /* A run whose records partly failed to store is still the run it was
     * (status unchanged), but its fetch_log row must not read as whole. */
    char whybuf[160];
    if (!why && cs.failed > 0) {
      snprintf(whybuf, sizeof whybuf, "%ld emitted record(s) refused by the "
               "sink (not stored)", cs.failed);
      why = whybuf;
    }
    long flid = fetch_log_write(db, d->id, status, (int)cs.n, duration_ms, why);
    anomaly_detect(db, d->id, flid, status, (int)cs.n, duration_ms);
    fetch_log_set_stored(db, flid, stored, stored_exact);
    fetch_log_set_bytes(db, flid, bytes_in);
    /* The run's DATA records: the disclosure above is subtracted, so a
     * disclosed refusal reads 'degraded' (ok, stored nothing) with its reason
     * in the status column - never 'online' - and counts toward
     * consecutive_empties. run_status() above is untouched: fetch_log and the
     * red/green verdict stay exactly what they were. */
    long data_records = (stored >= 0 ? stored_data : cs.n) - (disclosed ? 1 : 0);
    if (data_records < 0) data_records = 0;
    sources_row_record(db, d->id, status, data_records, duration_ms,
                       why ? why : (disclosed ? disc_why : NULL));
    /* the empties streak is fed what the COLLECTOR emitted (collector_records), not the
     * scheduler's own disclosure */
    if (!entity) sched_state_record(db, d, status, collector_records);
  }
  return rc;
}

/* sources.quarantined_until in the future → the circuit breaker has this source
 * benched; the scheduler must not probe it (mirrors Node's quarantine skip).
 *
 * Non-static because the ENTITY-PIVOT path needs the same answer. Enforcing
 * this only in scheduler_loop() meant a source the breaker had benched stayed
 * fully reachable through POST /api/search — and since the pivot path also
 * skipped anomaly_detect, pivot traffic could never bench it in the first
 * place. Two halves of one loop; sched_is_quarantined() closes both.
 *
 * The search_only test (sched_row_benched) stays scheduler-only: a search-only
 * source is one that EXISTS to be dispatched, so applying it here would refuse
 * exactly the sources the pivot path is for. */
int sched_is_quarantined(db_handle *db, const char *id) {
  sqlite3_stmt *s; int q = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sources WHERE id=?1 AND quarantined_until IS NOT NULL"
        " AND quarantined_until>datetime('now') LIMIT 1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    q = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }
  return q;
}

/* 1 when the dispatcher must not run `id` now: the user set it search_only
 * (the search tab triggers it on demand; default 'map_cron' if the row or
 * column is absent), or the breaker has it benched. One query per due row. */
static int sched_row_benched(db_handle *db, const char *id) {
  sqlite3_stmt *s; int b = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sources WHERE id=?1 AND (schedule_mode='search_only' OR "
        "(quarantined_until IS NOT NULL AND quarantined_until>datetime('now')))"
        " LIMIT 1", -1, &s, NULL) != SQLITE_OK)
    return sched_is_quarantined(db, id);        /* a database without schedule_mode */
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  b = (sqlite3_step(s) == SQLITE_ROW);
  sqlite3_finalize(s);
  return b;
}

/* ── worker pool ──────────────────────────────────────────────────────────
 *
 * WHY. This loop used to call scheduler_run_source() inline, so all 361
 * scheduled sources shared one thread. The audit measured single Overpass
 * collectors at 100-162 s and 73 portal probes at ~8 s each; one of those
 * blocked the ENTIRE fleet's cadence behind it. The 12 sources declaring
 * update_interval_sec=60 (plane-adsb, unified-flights, the EEW feeds) could
 * never actually refresh at 60 s no matter what they asked for, because the
 * dispatcher had to walk past everything else first. Freshness was
 * structurally unreachable, not merely slow.
 *
 * SHAPE. One dispatcher thread decides WHAT is due and hands ids to a bounded
 * queue; N workers run them. A slow source now occupies one of N slots
 * instead of the whole fleet.
 *
 * TWO LANES. The internal pods (collector id starting with '_': _maint,
 * _enrich, _breach, …) used to share those N workers and the one priority set
 * with the collectors. So the WAL checkpoint — the pod that exists BECAUSE
 * writes are heavy — waited behind collectors exactly when writes were
 * heaviest, and a fleet backlog could starve anomaly triage and repair
 * indefinitely. Pods now have their own queue and their own
 * JO_SCHED_MAINT_WORKERS threads (default 2): a collector never occupies a
 * maintenance worker and a pod never occupies a collector worker. What the
 * lanes still SHARE is the per-source running[] set, so skip-if-running holds
 * across both — and, through scheduler_claim(), against a manual /run.
 *
 * WHY EACH WORKER OWNS ITS DB HANDLE. Unchanged from the reason the loop got
 * its own connection in the first place: a sqlite transaction belongs to a
 * connection, not a thread, and core/intel.c wraps every emit in BEGIN/COMMIT.
 * Sharing one handle across workers would interleave their transactions and
 * let one worker's ROLLBACK discard another's committed-looking rows. WAL +
 * busy_timeout (db_attach) is what makes the concurrent writes safe; the
 * timeout is raised here because N writers contend more than one did.
 *
 * SKIP-IF-RUNNING. Serial execution gave this for free — next[i] was only
 * bumped after the run returned, so a source could not overlap itself. With a
 * pool that has to be explicit, or a 3600 s source taking 4000 s would be
 * queued again while still running, and a wedged source would flood the queue.
 *
 * THREAD SAFETY OF THE PER-RUN HOOKS. scheduler_run_source binds an evidence
 * scope and a content-change scope around run(); both are `__thread` in
 * evidence.c and content_change.c, so each worker gets its own. That was
 * already true — it is the reason this port is a pool and not a rewrite.  */

#define SCHED_QCAP 2048

enum { LANE_COLLECT = 0, LANE_MAINT = 1, LANE_N = 2 };

/* PRIORITY, NOT FIFO. The ring used to hand sources to workers in the order
 * the dispatcher walked the registry, so a healthy 60 s feed queued behind
 * 200 dead hosts waited for every one of their connect timeouts. Each lane is
 * an unordered set with a key per entry (sched_priority): pop takes the
 * lowest key. A linear scan of at most SCHED_QCAP entries per pop is ~2 µs
 * against runs that take seconds; a heap would buy nothing legible. */
typedef struct {
  int          idx[SCHED_QCAP];      /* registry indices awaiting a worker    */
  double       key[SCHED_QCAP];      /* sched_priority() at push time         */
  unsigned long seq[SCHED_QCAP];     /* push order: equal keys run first-in-first-out */
  time_t       force_at[SCHED_QCAP]; /* host-aware pop: taken regardless of its host after this */
  unsigned     skips[SCHED_QCAP];    /* pops that passed this entry over for a busy host */
  int          count;
  int          workers;              /* threads serving this lane             */
  long         dropped;              /* enqueues refused because it was full  */
  pthread_cond_t cv;
} sched_lane;

typedef struct {
  sched_lane   lane[LANE_N];
  char        *running;              /* per-source: queued, in flight, or claimed */
  char        *claimed;              /* per-source: held by scheduler_claim()  */
  time_t      *started;              /* pool run start, 0 = not running in it  */
  time_t      *warned;               /* last deadline warning for this run     */
  /* The plan. next[i] is when source i is next due; due[i] is when its
   * current/last pooled run was dispatched, which the following run is
   * measured from. Both under `mu`: the dispatcher plans, and the thread
   * that finishes a run corrects the plan from the run's outcome
   * (sched_publish). */
  time_t      *next;
  time_t      *due;
  unsigned long pushes;
  /* The due-time heap: (due time, source) pairs, min first. A pair is VALID
   * only while next[i] still equals its time; every change of next[i] pushes a
   * fresh pair (plan_set) and the stale one is dropped when it surfaces, so
   * the dispatcher's per-tick cost is the number of rows that came due, not
   * the size of the registry. */
  struct sched_due { time_t t; int i; } *heap;
  size_t       hn, hcap;
  int          heap_lost;            /* a push could not allocate: rebuild from next[] */
  /* Rows that are due and had no queue room, per lane, oldest first. They
   * STAY due (next[i] unchanged); dflag[i] marks membership so a row is
   * deferred once, not once per heap pair. */
  struct { int *a; size_t head, n, cap; } def[LANE_N];
  char        *dflag;
  /* Runs between q_pop and q_done: the watchdog's whole working set, so it no
   * longer walks the registry once a second. */
  int         *act;
  int         *actpos;               /* source -> position in act[] + 1, 0 = none */
  int          nact;
  int         *held;                 /* source -> host id counted in flight, 0 = none */
  /* ctx->cancel for a claimed or pooled run. Written only under `mu`, with
   * __atomic stores; read by collectors through a volatile pointer. */
  volatile int *cancel;
  pthread_mutex_t mu;
} sched_queue;

static sched_queue g_q = {
  .lane = { [LANE_COLLECT] = { .cv = PTHREAD_COND_INITIALIZER },
            [LANE_MAINT]   = { .cv = PTHREAD_COND_INITIALIZER } },
  .mu = PTHREAD_MUTEX_INITIALIZER,
};
static const source_def **g_all;
static int            g_n;
static int            g_deadline_sec = 0;
static long           g_cancel_cap_sec = 900;   /* JO_SCHED_CANCEL_MAX_SEC */
/* The file the dispatcher's connection is on; every worker attaches to the
 * same one (NULL = db_attach's default, i.e. JO_DB). */
static char           g_db_path[1024];
/* Per-source health state, index-aligned with g_all. Written by the worker
 * that finished the run (under g_q.mu), read by the dispatcher. */
static sched_state   *g_state;
static sched_policy   g_pol;
/* Shutdown coordination — see scheduler_stop_background(). g_inflight counts
 * runs currently executing, not queued ones. g_ready says the per-source
 * arrays above exist (release-stored after they are allocated). */
static atomic_int     g_shutdown = 0;
static atomic_int     g_inflight = 0;
static atomic_int     g_ready = 0;
static atomic_long    g_attach_failures = 0;

#define SCHED_HOSTS_MAX 65000
#define SCHED_HTAB      (1 << 17)
static int    *g_hid;           /* per source: its host id                     */
static int    *g_htab;          /* open-addressing table of host ids           */
static char  **g_hname;         /* host id -> hostname                         */
static int    *g_hinfl;         /* host id -> workers running a source on it   */
static int    *g_hcap;          /* host id -> in-flight cap                    */
static int     g_nh;
static int     g_host_cap_default = 2;
static unsigned g_skip_max = 1000;
static atomic_long g_skip_pops, g_forced_pops;


static int lane_of(const source_def *d) {
  return sched_is_pod(d) ? LANE_MAINT : LANE_COLLECT;
}

/* Allocate the per-source arrays for `all[0..n)`. Separate from
 * scheduler_loop so tests/unit/test_sched_pool.c can drive the pool without
 * the dispatcher. */
static int sched_pool_init(const source_def **all, int n) {
  g_all = all;
  g_n   = n;
  g_q.running = calloc((size_t)n, 1);
  g_q.claimed = calloc((size_t)n, 1);
  g_q.started = calloc((size_t)n, sizeof(time_t));
  g_q.warned  = calloc((size_t)n, sizeof(time_t));
  g_q.next    = calloc((size_t)(n > 0 ? n : 1), sizeof(time_t));
  g_q.due     = calloc((size_t)(n > 0 ? n : 1), sizeof(time_t));
  g_q.cancel  = calloc((size_t)n, sizeof(int));
  g_state     = calloc((size_t)n, sizeof(sched_state));
  g_q.dflag   = calloc((size_t)(n > 0 ? n : 1), 1);
  g_q.act     = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
  g_q.actpos  = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
  g_q.held    = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
  g_hid       = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
  g_htab      = calloc((size_t)SCHED_HTAB, sizeof(int));
  g_hname     = calloc((size_t)SCHED_HOSTS_MAX + 1, sizeof(char *));
  g_hinfl     = calloc((size_t)SCHED_HOSTS_MAX + 1, sizeof(int));
  g_hcap      = calloc((size_t)SCHED_HOSTS_MAX + 1, sizeof(int));
  g_nh        = 0;
  g_host_cap_default = (int)env_long("JO_SCHED_HOST_INFLIGHT", 2, 1, 64);
  g_skip_max  = (unsigned)env_long("JO_SCHED_HOST_SKIP_MAX", 1000, 1, 1000000);
  if (!g_q.running || !g_q.claimed || !g_q.started || !g_q.warned ||
      !g_q.next || !g_q.due || !g_q.cancel || !g_state || !g_q.dflag ||
      !g_q.act || !g_q.actpos || !g_q.held || !g_hid || !g_htab || !g_hname ||
      !g_hinfl || !g_hcap)
    return -1;
  atomic_store(&g_ready, 1);
  return 0;
}

/* ── host-aware dispatch ────────────────────────────────────────────────
 * A worker that pops a source whose host is not ready used to park inside
 * hostgate_acquire - up to 300 s for an override host - and 615 news.google.com
 * rows at a 1.5 s gap could hold all 8 workers for a quarter of an hour while
 * the rest of the queue waited. The dispatcher now knows a source's host and
 * leaves a queued entry whose host is busy or not ready (hostgate's gap or a
 * server-requested penalty) where it is, taking the next eligible one.
 *
 * The registry does not know the host of a bespoke collector, so it is LEARNED
 * at run time (the first host the run contacted, sched_host_learn) and seeded
 * before the first run from the row's portal url - a hint only: a wrong hint
 * can delay an entry, never lose or reorder it permanently, because an entry
 * skipped JO_SCHED_HOST_SKIP_MAX times, or queued longer than its interval, is
 * taken regardless (and then waits in hostgate_acquire exactly as before).
 *
 * All of it under g_q.mu. Host ids are 1-based; 0 = unknown host. */
static unsigned host_hash(const char *h) {
  unsigned x = 2166136261u;
  for (; *h; h++) { x ^= (unsigned char)*h; x *= 16777619u; }
  return x;
}

/* The id for `host`, creating it; 0 when the table is full. Caller holds g_q.mu. */
static int host_intern(const char *host) {
  if (!g_htab || !host || !*host) return 0;
  size_t k = host_hash(host) & (SCHED_HTAB - 1);
  for (;;) {
    int id = g_htab[k];
    if (!id) break;
    if (!strcmp(g_hname[id], host)) return id;
    k = (k + 1) & (SCHED_HTAB - 1);
  }
  if (g_nh >= SCHED_HOSTS_MAX) return 0;
  char *nm = strdup(host);
  if (!nm) return 0;
  int id = ++g_nh;
  g_hname[id] = nm;
  /* One worker at a time on a host that paces itself (a gap of a second or
   * more: reddit 30 s, news.google.com 1.5 s) - a second would only wait out
   * the gap inside hostgate_acquire, which is the parking this avoids. */
  g_hcap[id] = hostgate_pace_ms(host) >= 1000 ? 1 : g_host_cap_default;
  g_htab[k] = id;
  return id;
}

static int host_is_loopback(const char *h) {
  return !strcmp(h, "localhost") || !strcmp(h, "127.0.0.1") || !strcmp(h, "::1");
}

static void sched_host_learn(const source_def *d, const char *host) {
  if (!atomic_load(&g_ready) || !host || !*host || host_is_loopback(host)) return;
  int i = registry_index(d->id);
  if (i < 0 || i >= g_n) return;
  pthread_mutex_lock(&g_q.mu);
  int id = host_intern(host);
  if (id) g_hid[i] = id;
  pthread_mutex_unlock(&g_q.mu);
}

/* Before any run has taught us, take a scheduled row's portal url as its host
 * (a hint: source_def.url is the human-facing page, usually on the API's host). */
static void sched_hosts_seed(void) {
  pthread_mutex_lock(&g_q.mu);
  for (int i = 0; i < g_n; i++) {
    const source_def *d = g_all[i];
    if (d->update_interval_sec <= 0 || sched_is_pod(d) || !d->url ||
        strncmp(d->url, "http", 4) != 0) continue;
    char h[128];
    if (!hostgate_url_host(d->url, h, sizeof h) || host_is_loopback(h)) continue;
    int id = host_intern(h);
    if (id) g_hid[i] = id;
  }
  pthread_mutex_unlock(&g_q.mu);
}

/* 1 when source `i` should not be started now: its host already has its cap of
 * workers, or hostgate says the host's next slot is not yet. Caller holds g_q.mu. */
static int host_blocked(int i) {
  int id = g_hid ? g_hid[i] : 0;
  if (!id) return 0;
  if (g_hinfl[id] >= g_hcap[id]) return 1;
  return hostgate_peek_ready_ms(g_hname[id]) > 0;
}

/* ── the due-time heap (see sched_queue) ─────────────────────────────────── */
typedef struct sched_due sched_due;

static void heap_sift_down(size_t k) {
  size_t n = g_q.hn;
  sched_due v = g_q.heap[k];
  for (;;) {
    size_t c = 2 * k + 1;
    if (c >= n) break;
    if (c + 1 < n && g_q.heap[c + 1].t < g_q.heap[c].t) c++;
    if (g_q.heap[c].t >= v.t) break;
    g_q.heap[k] = g_q.heap[c];
    k = c;
  }
  g_q.heap[k] = v;
}

static int heap_reserve(size_t want) {
  if (want <= g_q.hcap) return 1;
  size_t nc = g_q.hcap ? g_q.hcap : 1024;
  while (nc < want) nc *= 2;
  sched_due *nh = realloc(g_q.heap, nc * sizeof *nh);
  if (!nh) return 0;
  g_q.heap = nh; g_q.hcap = nc;
  return 1;
}

static void heap_push(time_t t, int i) {
  if (!heap_reserve(g_q.hn + 1)) { g_q.heap_lost = 1; return; }
  size_t k = g_q.hn++;
  while (k > 0) {
    size_t p = (k - 1) / 2;
    if (g_q.heap[p].t <= t) break;
    g_q.heap[k] = g_q.heap[p];
    k = p;
  }
  g_q.heap[k].t = t; g_q.heap[k].i = i;
}

static sched_due heap_pop(void) {
  sched_due top = g_q.heap[0];
  g_q.hn--;
  if (g_q.hn > 0) { g_q.heap[0] = g_q.heap[g_q.hn]; heap_sift_down(0); }
  return top;
}

/* Rebuild from next[]: every scheduled row, one pair each. O(n), run at boot,
 * when stale pairs outnumber live rows 4:1, and after an allocation failure. */
static void heap_rebuild(void) {
  g_q.hn = 0; g_q.heap_lost = 0;
  if (!heap_reserve((size_t)g_n + 16)) { g_q.heap_lost = 1; return; }
  for (int i = 0; i < g_n; i++)
    if (g_all[i]->update_interval_sec > 0) {
      g_q.heap[g_q.hn].t = g_q.next[i]; g_q.heap[g_q.hn].i = i; g_q.hn++;
    }
  for (size_t k = g_q.hn / 2; k-- > 0;) heap_sift_down(k);
}

/* THE way next[i] changes. Caller holds g_q.mu. */
static void plan_set(int i, time_t t) {
  g_q.next[i] = t;
  if (g_all[i]->update_interval_sec <= 0) return;      /* on demand: never due */
  heap_push(t, i);
  if (g_q.hn > 4 * (size_t)g_n + 4096) heap_rebuild();
}

static void defer_add(int ln, int i) {                  /* caller holds g_q.mu */
  if (g_q.dflag[i]) return;
  if (g_q.def[ln].head > 4096 && g_q.def[ln].head * 2 > g_q.def[ln].n) {
    memmove(g_q.def[ln].a, g_q.def[ln].a + g_q.def[ln].head,
            (g_q.def[ln].n - g_q.def[ln].head) * sizeof(int));
    g_q.def[ln].n -= g_q.def[ln].head;
    g_q.def[ln].head = 0;
  }
  if (g_q.def[ln].n == g_q.def[ln].cap) {
    size_t nc = g_q.def[ln].cap ? g_q.def[ln].cap * 2 : 1024;
    int *na = realloc(g_q.def[ln].a, nc * sizeof(int));
    if (!na) return;                  /* stays in next[]; the heap pair is gone, so rebuild */
    g_q.def[ln].a = na; g_q.def[ln].cap = nc;
  }
  g_q.def[ln].a[g_q.def[ln].n++] = i;
  g_q.dflag[i] = 1;
}

static void set_cancel(int i, int v) {           /* caller holds g_q.mu */
  __atomic_store_n((int *)&g_q.cancel[i], v, __ATOMIC_RELEASE);
}

/* The cancel slot of a run this module knows about, or NULL. */
static volatile int *sched_cancel_slot(const char *id) {
  if (!atomic_load(&g_ready) || !id) return NULL;
  int i = registry_index(id);
  if (i < 0 || i >= g_n) return NULL;
  volatile int *slot = NULL;
  pthread_mutex_lock(&g_q.mu);
  if (g_q.running[i]) slot = &g_q.cancel[i];
  pthread_mutex_unlock(&g_q.mu);
  return slot;
}

int scheduler_claim(const char *id) {
  if (!atomic_load(&g_ready) || !id) return 1;   /* no pool: nothing to share */
  int i = registry_index(id);
  if (i < 0 || i >= g_n) return 1;
  int ok;
  pthread_mutex_lock(&g_q.mu);
  if (g_q.running[i]) ok = 0;
  else { g_q.running[i] = 1; g_q.claimed[i] = 1; set_cancel(i, 0); ok = 1; }
  pthread_mutex_unlock(&g_q.mu);
  return ok;
}

void scheduler_release(const char *id) {
  if (!atomic_load(&g_ready) || !id) return;
  int i = registry_index(id);
  if (i < 0 || i >= g_n) return;
  pthread_mutex_lock(&g_q.mu);
  if (g_q.claimed[i]) { g_q.claimed[i] = 0; g_q.running[i] = 0; }
  pthread_mutex_unlock(&g_q.mu);
}

/* A finished run's outcome reaches the plan HERE, from whichever thread ran
 * it (a pool worker or a manual/claimed run). The dispatcher plans a row's
 * next attempt when it dispatches it, from the state BEFORE the run — so
 * until this existed every change landed one cycle late: a source released
 * from quarantine by a successful probe still waited out the 7-day probe
 * cadence, a recovered source its last backoff, a first failure was retried
 * at 1x not 2x, and a manual run's verdict never reached the dispatcher. */
static void sched_publish(const source_def *d, const sched_state *st, time_t now) {
  if (!atomic_load(&g_ready)) return;
  int i = registry_index(d->id);
  if (i < 0 || i >= g_n) return;
  pthread_mutex_lock(&g_q.mu);
  g_state[i] = *st;
  g_state[i].last_run = now;
  /* A pooled run is measured from its dispatch (a 90 s run on a 60 s interval
   * is due again at once, not at 150 s); a manual run from now. */
  time_t last = (g_q.started[i] && g_q.due[i]) ? g_q.due[i] : now;
  time_t nd = sched_next_due(st, &g_pol, d, last);
  if (nd > 0) plan_set(i, nd);
  pthread_mutex_unlock(&g_q.mu);
}

/* 1 = queued; 0 = already queued, in flight or claimed by a manual run (the
 * run in flight IS this cycle's refresh); -1 = the lane is full. The
 * dispatcher reserves room before it pushes, so -1 is not expected there —
 * it is counted, and the caller leaves the row due. */
static int q_push(int i, double key) {
  sched_lane *L = &g_q.lane[lane_of(g_all[i])];
  int r;
  pthread_mutex_lock(&g_q.mu);
  if (g_q.running[i]) r = 0;
  else if (L->count >= SCHED_QCAP) { L->dropped++; r = -1; }
  else {
    g_q.running[i] = 1;
    L->idx[L->count] = i;
    L->key[L->count] = key;
    L->seq[L->count] = g_q.pushes++;
    L->skips[L->count] = 0;
    long iv = sched_row_interval(&g_state[i], g_all[i]);
    L->force_at[L->count] = time(NULL) + (time_t)(iv > 0 ? iv : 1);
    L->count++;
    pthread_cond_signal(&L->cv);
    r = 1;
  }
  pthread_mutex_unlock(&g_q.mu);
  return r;
}

/* Next index for lane `ln`, or -1 once shutdown has begun.
 *
 * Lowest key first among the entries whose host can take a run NOW (see the
 * host-aware dispatch note). An entry whose host is busy stays queued and the
 * next eligible one is taken; one skipped JO_SCHED_HOST_SKIP_MAX times, or
 * queued past its own interval, is taken anyway so nothing starves. When every
 * queued entry waits on a busy host the worker sleeps - woken by any run
 * finishing, else every 200 ms for a gap to elapse - instead of parking inside
 * hostgate_acquire. The maintenance lane is not host-aware (pods reach no
 * upstream). */
static int q_pop(int ln) {
  sched_lane *L = &g_q.lane[ln];
  unsigned char blk[SCHED_QCAP];       /* 1 = skippable, 2 = blocked but forced */
  int best;
  pthread_mutex_lock(&g_q.mu);
  for (;;) {
    while (L->count == 0 && !atomic_load(&g_shutdown))
      pthread_cond_wait(&L->cv, &g_q.mu);
    if (atomic_load(&g_shutdown)) { pthread_mutex_unlock(&g_q.mu); return -1; }
    time_t now = time(NULL);
    best = -1;
    for (int k = 0; k < L->count; k++) {
      blk[k] = 0;
      if (ln == LANE_COLLECT && host_blocked(L->idx[k]))
        blk[k] = (L->skips[k] < g_skip_max && now < L->force_at[k]) ? 1 : 2;
      if (blk[k] == 1) continue;
      if (best < 0 || L->key[k] < L->key[best] ||
          (L->key[k] == L->key[best] && L->seq[k] < L->seq[best])) best = k;
    }
    if (best >= 0) break;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 200L * 1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    pthread_cond_timedwait(&L->cv, &g_q.mu, &ts);
  }
  int passed = 0;
  for (int k = 0; k < L->count; k++)
    if (blk[k] == 1) { L->skips[k]++; passed = 1; }
  if (passed) atomic_fetch_add(&g_skip_pops, 1);
  if (blk[best] == 2) atomic_fetch_add(&g_forced_pops, 1);
  int i = L->idx[best];
  /* swap-remove: order inside the array carries no meaning, the key does */
  L->count--;
  L->idx[best] = L->idx[L->count];
  L->key[best] = L->key[L->count];
  L->seq[best] = L->seq[L->count];
  L->force_at[best] = L->force_at[L->count];
  L->skips[best] = L->skips[L->count];
  g_q.started[i] = time(NULL);
  g_q.warned[i]  = 0;
  set_cancel(i, 0);
  int hid = ln == LANE_COLLECT ? g_hid[i] : 0;
  g_q.held[i] = hid;
  if (hid) g_hinfl[hid]++;
  g_q.actpos[i] = ++g_q.nact;                 /* position + 1 */
  g_q.act[g_q.nact - 1] = i;
  pthread_mutex_unlock(&g_q.mu);
  return i;
}

static void q_done(int i) {
  pthread_mutex_lock(&g_q.mu);
  g_q.running[i] = 0;
  g_q.started[i] = 0;
  if (g_q.actpos[i]) {
    int pos = g_q.actpos[i] - 1, last = g_q.act[g_q.nact - 1];
    g_q.act[pos] = last;
    g_q.actpos[last] = pos + 1;
    g_q.actpos[i] = 0;
    g_q.nact--;
  }
  int hid = g_q.held[i];
  g_q.held[i] = 0;
  if (hid && g_hinfl[hid] > 0) {
    g_hinfl[hid]--;
    /* a worker may be asleep because this host was at its cap */
    pthread_cond_broadcast(&g_q.lane[LANE_COLLECT].cv);
  }
  pthread_mutex_unlock(&g_q.mu);
}

/* A worker whose connection cannot be opened used to log once and EXIT, so
 * one transient failure at boot (a locked file, a full fd table, the volume
 * not yet mounted) shrank the pool for the life of the process — silently,
 * since the heartbeat reported the configured worker count, not the live
 * one. It now retries with backoff (1 s doubling to 60 s) until it attaches
 * or shutdown begins, and every failure is counted on the heartbeat line. */
static int worker_attach(db_handle *own, int ln) {
  long wait = 1;
  for (;;) {
    if (atomic_load(&g_shutdown)) return -1;
    if (db_attach(own, g_db_path[0] ? g_db_path : NULL) == 0) {
      /* N concurrent writers contend for the WAL write lock far more than one
       * did; 5 s (db_attach's default) starts returning SQLITE_BUSY on a busy
       * fleet, which surfaces as a spurious emit failure. */
      sqlite3_exec(own->h, "PRAGMA busy_timeout=30000;", NULL, NULL, NULL);
      return 0;
    }
    long n = atomic_fetch_add(&g_attach_failures, 1) + 1;
    fprintf(stderr, "[sched] %s worker cannot open its DB connection "
                    "(attach failure #%ld); retrying in %lds\n",
            ln == LANE_MAINT ? "maint" : "collector", n, wait);
    for (long s = 0; s < wait && !atomic_load(&g_shutdown); s++) sleep(1);
    if (wait < 60) wait = wait * 2 > 60 ? 60 : wait * 2;
  }
}

static void *worker_thread(void *arg) {
  int ln = (int)(intptr_t)arg;
  db_handle own = {0};
  if (worker_attach(&own, ln) != 0) return NULL;
  for (;;) {
    int i = q_pop(ln);
    if (i < 0) break;                      /* shutdown */
    atomic_fetch_add(&g_inflight, 1);
    scheduler_run_source(&own, g_all[i], NULL);
    atomic_fetch_sub(&g_inflight, 1);
    /* scheduler_run_source persisted the new health state and published it
     * to the plan (sched_publish) while this slot was still held. */
    q_done(i);
  }
  db_close(&own);
  return NULL;
}

/* Shutdown drain.
 *
 * Workers are detached and scheduler_loop never returns, so there is nothing
 * to join. That was survivable until process exit started racing them:
 * atexit's OPENSSL_cleanup frees the CRYPTO lock objects while ~8-16 workers
 * are still inside curl_easy_perform, and TSan caught it as four
 * heap-use-after-frees ending in a SEGV in CRYPTO_THREAD_write_lock. A clean
 * shutdown could therefore exit by signal instead of 0, and a worker could be
 * torn down mid-emit().
 *
 * This does not pretend to join. It stops handing out new work, asks every
 * run in flight to stop (ctx->cancel — cooperative, so a collector that never
 * polls it still runs to its own end), wakes the idle workers so they leave
 * q_pop and close their connections, and waits a bounded time for in-flight
 * fetches to land, which is the part that matters: an emit() that has begun
 * gets to finish its transaction. A worker parked in a multi-second network
 * call may still be running when the wait expires — that is why main() must
 * also skip the atexit teardown rather than rely on this alone. */
void scheduler_stop_background(int wait_ms) {
  atomic_store(&g_shutdown, 1);
  pthread_mutex_lock(&g_q.mu);
  if (atomic_load(&g_ready))
    for (int i = 0; i < g_n; i++)
      if (g_q.started[i] || g_q.claimed[i]) set_cancel(i, 1);
  for (int ln = 0; ln < LANE_N; ln++) pthread_cond_broadcast(&g_q.lane[ln].cv);
  pthread_mutex_unlock(&g_q.mu);
  const int step = 25;
  for (int waited = 0; waited < wait_ms; waited += step) {
    if (atomic_load(&g_inflight) == 0) break;
    struct timespec ts = { step / 1000, (long)(step % 1000) * 1000000L };
    nanosleep(&ts, NULL);
  }
  int left = atomic_load(&g_inflight);
  if (left > 0)
    fprintf(stderr, "[sched] shutdown: %d collector run(s) still in flight "
                    "after %d ms; exiting without waiting further\n",
            left, wait_ms);
}

/* Sources that have outrun the deadline are reported, and once a run has
 * outrun its cancel window its cancel flag is set. Cancellation is
 * COOPERATIVE: only the sources and pods that poll ctx->cancel stop early (the
 * WAL pod between TRUNCATE attempts, the retention pod between batches, the
 * hpengine walkers between pages - which disclose the cut as a truncation
 * notice). There is no safe hard kill - cancelling a thread mid-transaction
 * would corrupt exactly what the per-worker connection is there to protect -
 * so a source that never polls keeps its slot until its own HTTP timeouts end
 * it, and keeps being reported once a minute. The real containment is still
 * the pool (an overrun costs one slot, not the fleet). Before this, the flag
 * the collectors poll was a local nobody else could reach, so the deadline
 * only ever logged.
 *
 * THE CANCEL WINDOW IS BOUNDED, NOT THE CADENCE. The deadline is one number
 * (300 s) for a fleet whose legitimate run times reach many minutes - a daily
 * bulk page walk can take longer than 300 s every single time, and cancelling
 * it AT the deadline would cut the same walk at the same page on every run,
 * forever (house rule 2). So the window was the row's own interval:
 * max(deadline, interval). Under the compiled global 6 h cadence that made it
 * SIX HOURS for every row, which is no cancellation at all: a handful of long
 * walks (data.qld.gov.au is ~72 minutes) could hold a meaningful share of an
 * 8-worker pool for the whole of it. The window is now
 * max(deadline, min(interval, JO_SCHED_CANCEL_MAX_SEC)) - a fast row is still
 * not cancelled inside its own interval, and no row's window exceeds the cap
 * (default 900 s) whatever the cadence policy says. An operator who sets
 * JO_SCHED_DEADLINE_SEC above the cap has chosen a longer window and gets it. */
static long cancel_after(int i) {
  long iv = sched_row_interval(&g_state[i], g_all[i]);   /* the row's cadence as it runs, not the declared one */
  if (iv > g_cancel_cap_sec) iv = g_cancel_cap_sec;
  return iv > g_deadline_sec ? iv : g_deadline_sec;
}

static void watchdog_scan(void) {
  if (g_deadline_sec <= 0) return;
  time_t now = time(NULL);
  pthread_mutex_lock(&g_q.mu);
  /* Only the runs in flight - a handful - not the whole registry. */
  for (int a = 0; a < g_q.nact; a++) {
    int i = g_q.act[a];
    if (!g_q.started[i]) continue;
    long over = (long)(now - g_q.started[i]);
    if (over <= g_deadline_sec) continue;
    int first = 0;
    if (over > cancel_after(i) &&
        !__atomic_load_n((int *)&g_q.cancel[i], __ATOMIC_ACQUIRE)) {
      set_cancel(i, 1);
      first = 1;
    }
    if (first || now - g_q.warned[i] >= 60) {
      int asked = __atomic_load_n((int *)&g_q.cancel[i], __ATOMIC_ACQUIRE);
      g_q.warned[i] = now;
      fprintf(stderr, "[sched] WARN %s has held a worker slot for %lds "
                      "(deadline %ds, interval %ds, cancel window %lds)%s\n", g_all[i]->id, over,
              g_deadline_sec, (int)sched_row_interval(&g_state[i], g_all[i]),
              cancel_after(i),
              first ? " - cancel requested"
                    : asked ? " - still running after cancel" : "");
    }
  }
  pthread_mutex_unlock(&g_q.mu);
}

/* A short, human-readable list of sources currently occupying a worker slot
 * and how long each has held it — for a diagnostic message elsewhere, never
 * for control flow. Added for the wal-checkpoint pod's "a reader is holding
 * a snapshot open" line (collectors/pod/wal_checkpoint_pod.c): that pod only
 * knows ITS OWN connection, so a BUSY/incomplete-PASSIVE result used to name
 * no suspect at all. This reads the exact state watchdog_scan already
 * tracks, so the two never disagree. Truncates silently at `cap`; always
 * NUL-terminates.
 *
 * `exclude_id` (may be NULL) is skipped — `g_q.started[]` spans BOTH lanes
 * (scheduler.c's own busy-count read does the same, unconditionally), so a
 * maintenance pod calling this while it is itself the running() entry would
 * otherwise name ITSELF, with the longest hold of anything in the list
 * (every retry it has just spent counts against its own elapsed time). A
 * caller passes its own ctx->source_id; every other maint-lane pod
 * (embedding-backfill included) is a legitimate suspect and stays. */
void scheduler_running_summary(char *buf, size_t cap, const char *exclude_id) {
  if (!buf || cap == 0) return;
  buf[0] = '\0';
  if (!atomic_load(&g_ready)) return;
  time_t now = time(NULL);
  size_t used = 0;
  pthread_mutex_lock(&g_q.mu);
  for (int i = 0; i < g_n && used + 1 < cap; i++) {
    if (!g_q.started[i]) continue;
    if (exclude_id && strcmp(g_all[i]->id, exclude_id) == 0) continue;
    long over = (long)(now - g_q.started[i]);
    int n = snprintf(buf + used, cap - used, "%s%s(%lds)",
                      used ? ", " : "", g_all[i]->id, over);
    if (n < 0) break;
    used += (size_t)n < cap - used ? (size_t)n : cap - used - 1;
  }
  pthread_mutex_unlock(&g_q.mu);
  if (used == 0) snprintf(buf, cap, "(nothing currently running)");
}

/* ── boot ramp ────────────────────────────────────────────────────────────
 *
 * The first-run offset of the k-th of m sources in one lane. The old ramp was
 * `(i / workers) * stag` over EVERY registered source — interval-0 entity
 * pivots included, which the scheduler never runs — so with 18,643 sources,
 * 8 workers and 3 s, the last scheduled source first ran ~1.9 h after boot,
 * and a 60 s feed registered late in the array was stale for most of that.
 *
 * Now k counts only the sources this lane will actually run at boot, the
 * gentle `stag` per worker spacing is kept when it fits, the whole ramp is
 * compressed into `ramp` seconds when it does not, and no source waits longer
 * than its own interval for its first run (the offset is folded modulo the
 * interval, which keeps short-interval sources spread out rather than piled at
 * one instant). ramp <= 0 disables the compression; stag <= 0 disables the
 * ramp altogether. Politeness per upstream is hostgate's job, which is what
 * makes a tighter ramp safe. */
long sched_boot_offset(int k, int m, int workers, long stag, long ramp,
                       long interval) {
  if (stag <= 0 || k <= 0 || m <= 1) return 0;
  if (workers < 1) workers = 1;
  long off  = (long)(k / workers) * stag;
  long span = (long)((m - 1) / workers) * stag;
  if (ramp > 0 && span > ramp)
    off = (long)((double)off * (double)ramp / (double)span);
  if (interval > 0 && off >= interval) off %= interval;
  return off;
}

/* Sources the dispatcher will skip at boot anyway (search_only, or benched by
 * the repair pod's breaker), marked in `skip`. One query, not two per source:
 * this runs before the first dispatch and the per-source probes are what the
 * dispatcher does later, once each source is actually due. */
static void boot_skip_set(db_handle *db, char *skip, int n) {
  static const char *Q[] = {
    "SELECT id FROM sources WHERE schedule_mode='search_only' OR "
    "(quarantined_until IS NOT NULL AND quarantined_until>datetime('now'))",
    "SELECT id FROM sources WHERE "
    "quarantined_until IS NOT NULL AND quarantined_until>datetime('now')",
  };
  for (size_t q = 0; q < sizeof Q / sizeof Q[0]; q++) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, Q[q], -1, &s, NULL) != SQLITE_OK) continue;
    while (sqlite3_step(s) == SQLITE_ROW) {
      int i = registry_index((const char *)sqlite3_column_text(s, 0));
      if (i >= 0 && i < n) skip[i] = 1;
    }
    sqlite3_finalize(s);
    return;
  }
}

/* The cadence policy or a per-source interval changed: a row whose next due
 * time is further away than its NEW spacing is pulled in to now + spacing, so
 * a faster cadence takes effect within one interval and a slower one lets the
 * already-planned run happen and then stretches. A row under a health hold
 * keeps it - an edit to one source's interval must not re-probe every
 * quarantined source. The one place the whole registry is still walked, and
 * only on a change. Returns how many rows moved. */
static long sched_replan(time_t now) {
  long pulled = 0;
  pthread_mutex_lock(&g_q.mu);
  for (int i = 0; i < g_n; i++) {
    long e = sched_next_delay(&g_state[i], &g_pol, g_all[i]);
    if (e <= 0 || sched_hold_until(&g_state[i], &g_pol) > now) continue;
    if (g_q.next[i] > now + (time_t)e) { plan_set(i, now + (time_t)e); pulled++; }
  }
  pthread_mutex_unlock(&g_q.mu);
  return pulled;
}

/* One dispatcher tick: collect the rows that are due, queue as many as the
 * lanes have room for, and re-plan each. Returns how many due rows are still
 * waiting for room (they STAY due).
 *
 * The cost is the number of rows that came due, not the size of the registry:
 * the heap's top says at once whether anything is, and a tick with nothing due
 * is a lock and a comparison. It was a walk over every registered row - 93,000
 * of them - once a second. Deferred rows are served first (they are the
 * oldest), then the heap in due order, so a standing backlog drains most
 * overdue first rather than in registry order. `cand` has room for
 * SCHED_QCAP * LANE_N rows. */
static int sched_tick(db_handle *db, time_t now, int *cand) {
  int nc = 0, deferred = 0;
  pthread_mutex_lock(&g_q.mu);
  if (g_q.heap_lost) heap_rebuild();
  int room[LANE_N];
  for (int ln = 0; ln < LANE_N; ln++) room[ln] = SCHED_QCAP - g_q.lane[ln].count;
  /* 1. rows deferred by an earlier tick */
  for (int ln = 0; ln < LANE_N; ln++) {
    while (g_q.def[ln].head < g_q.def[ln].n && room[ln] > 0) {
      int i = g_q.def[ln].a[g_q.def[ln].head++];
      g_q.dflag[i] = 0;
      const source_def *d = g_all[i];
      if (d->update_interval_sec <= 0 || g_q.next[i] > now) continue;   /* re-planned meanwhile */
      if (g_q.running[i]) {
        plan_set(i, now + (time_t)sched_next_delay(&g_state[i], &g_pol, d));
        continue;
      }
      room[ln]--;
      cand[nc++] = i;
    }
    if (g_q.def[ln].head == g_q.def[ln].n) g_q.def[ln].head = g_q.def[ln].n = 0;
  }
  /* 2. rows that came due */
  while (g_q.hn > 0 && g_q.heap[0].t <= now) {
    sched_due e = heap_pop();
    int i = e.i;
    if (g_q.next[i] != e.t || g_q.dflag[i]) continue;                  /* stale pair */
    const source_def *d = g_all[i];
    if (d->update_interval_sec <= 0) continue;
    if (g_q.running[i]) {
      /* Still queued, in flight or claimed: that run is this cycle's
       * refresh. Look again one spacing on; its outcome re-plans it. */
      plan_set(i, now + (time_t)sched_next_delay(&g_state[i], &g_pol, d));
      continue;
    }
    int ln = lane_of(d);
    if (room[ln] <= 0) { defer_add(ln, i); continue; }
    room[ln]--;
    cand[nc++] = i;
  }
  for (int ln = 0; ln < LANE_N; ln++)
    deferred += (int)(g_q.def[ln].n - g_q.def[ln].head);
  pthread_mutex_unlock(&g_q.mu);
  for (int c = 0; c < nc; c++) {
    int i = cand[c];
    const source_def *d = g_all[i];
    pthread_mutex_lock(&g_q.mu);
    sched_state st = g_state[i];
    time_t was_due = g_q.next[i];
    pthread_mutex_unlock(&g_q.mu);
    /* The row's cadence under every mechanism (sched_row_interval): backoff
     * doubles from it and the overdue ratio is measured against it. */
    long base = sched_row_interval(&st, d);
    time_t then;
    if (sched_row_benched(db, d->id)) {
      then = now + (time_t)base;            /* search_only or benched: look again */
    } else {
      double overdue = (double)(now - was_due) / (double)(base > 0 ? base : 1);
      if (q_push(i, sched_priority(&st, source_trust_score(db, d->id), overdue)) < 0) {
        pthread_mutex_lock(&g_q.mu);        /* stays due */
        defer_add(lane_of(d), i);
        deferred++;
        pthread_mutex_unlock(&g_q.mu);
        continue;
      }
      /* Provisional - planned from the state before the run, and corrected
       * from its outcome by sched_publish. */
      then = now + (time_t)sched_state_interval(&st, &g_pol, base);
    }
    pthread_mutex_lock(&g_q.mu);
    g_q.due[i] = now;
    plan_set(i, then);
    pthread_mutex_unlock(&g_q.mu);
  }
  return deferred;
}

void scheduler_loop(db_handle *db) {
  int n = registry_count();
  sched_policy_load(&g_pol);
  int *cand = malloc(sizeof(int) * SCHED_QCAP * LANE_N);
  if (!cand || sched_pool_init(registry_all(), n) != 0) {
    fprintf(stderr, "[sched] out of memory; scheduler off\n");
    return;
  }
  time_t *next = g_q.next;
  const char *dbf = (db && db->h) ? sqlite3_db_filename(db->h, "main") : NULL;
  if (dbf && *dbf) snprintf(g_db_path, sizeof g_db_path, "%s", dbf);
  /* Health state survives restarts: a source that was on its 11th failure
   * yesterday is on its 11th failure now, not its 0th. */
  int n_restored = 0, n_q0 = 0;
  for (int i = 0; i < n; i++)
    if (sched_state_load_one(db, g_all[i]->id, &g_state[i]) > 0) {
      n_restored++;
      if (g_state[i].quarantined) n_q0++;
    }
  fprintf(stderr, "[sched] health: restored state for %d source(s), %d in "
                  "health quarantine; backoff cap %lds, quarantine after %d "
                  "failures, probe every %lds\n",
          n_restored, n_q0, g_pol.backoff_cap_sec, g_pol.quarantine_after,
          g_pol.quarantine_probe_sec);

  /* Measured on this fleet, 60 s window, boot stagger disabled so worker count
   * was the only variable: 1 worker → 5 runs (a single 55.8 s `5g-coverage`
   * consumed the whole window and the fleet never got past "a"); 8 → 63 runs;
   * 16 → 97 runs. The work is network-bound, not CPU-bound, so the ceiling is
   * upstream politeness (hostgate) and sqlite write contention, not cores. 8
   * is the conservative default for a first deploy; raise it once the
   * heartbeat below shows a standing backlog. 1 = the old serial behaviour. */
  /* DEFAULT 24 (2026-11-16, audit ENG-18). The scheduling audit measured a lap
   * of the fleet at ~18 h with 8 workers and left 24 unmeasured for write
   * contention. Measured on the UNBATCHED sink, 300 s boots from fresh copies
   * of one database, other builds competing for the machine: 8 workers 217 runs
   * (0.72/s), 24 workers 363 runs (1.21/s), and at 24 zero failed= emits and
   * zero SQLITE_BUSY lines. The 1.7x (not 3x) gain is the boot ramp, hostgate
   * pacing and shared egress, not the database. The clamp stays 1-32. */
  int workers = (int)env_long("JO_SCHED_WORKERS", 24, 1, 32);
  /* The pods are few, mostly short, and must never wait on the fleet. Two, so
   * one long LLM-bound pod (repair, enrichment) cannot hold the WAL
   * checkpoint or retention behind it. */
  int mworkers = (int)env_long("JO_SCHED_MAINT_WORKERS", 2, 1, 8);
  g_q.lane[LANE_COLLECT].workers = workers;
  g_q.lane[LANE_MAINT].workers   = mworkers;
  g_deadline_sec = (int)env_long("JO_SCHED_DEADLINE_SEC", 300, 0, INT_MAX);   /* 0 = watchdog off */
  /* The ceiling on the interval-based part of a run's cancel window, whatever
   * the cadence policy makes a row's interval - see cancel_after(). */
  g_cancel_cap_sec = env_long("JO_SCHED_CANCEL_MAX_SEC", 900, 1, INT_MAX);

  time_t now0 = time(NULL);
  /* Stagger first runs so the fleet doesn't storm upstreams at boot (many do
   * nationwide tiled Overpass): one source per worker per JO_SCHED_STAGGER_SEC
   * (3 s), compressed into JO_SCHED_RAMP_SEC (900 s) and into each source's
   * own interval — see sched_boot_offset(). JO_SCHED_STAGGER_SEC=0 disables
   * the ramp. */
  long stag = env_long("JO_SCHED_STAGGER_SEC", 3, 0, 86400);
  long ramp = env_long("JO_SCHED_RAMP_SEC", 900, 0, 7L * 86400L);
  char *skip = calloc((size_t)(n > 0 ? n : 1), 1);
  if (skip) boot_skip_set(db, skip, n);
  /* Pass 1: which sources start on the ramp, per lane. A source that is not
   * due yet starts when it IS due and takes no ramp slot: a restored backoff
   * or quarantine probe (the outage did not end because the process did),
   * and — the same rule — a healthy source whose last run was less than one
   * interval ago. The cadence is the source's, not the process's: a restart
   * used to re-fetch the whole fleet inside the ramp whatever ran an hour
   * before. Nor is anything the dispatcher will not run given a slot. */
  time_t *hold = calloc((size_t)(n > 0 ? n : 1), sizeof(time_t));
  int m[LANE_N] = {0}, n_wait = 0;
  for (int i = 0; i < n; i++) {
    time_t h = sched_next_due(&g_state[i], &g_pol, g_all[i], g_state[i].last_run);
    if (hold) hold[i] = h;
    next[i] = h > now0 ? h : now0;
    if (g_all[i]->update_interval_sec <= 0 || (skip && skip[i])) continue;
    if (h > now0) n_wait++;
    else m[lane_of(g_all[i])]++;
  }
  /* Pass 2: place them. */
  int k[LANE_N] = {0};
  long last[LANE_N] = {0};
  for (int i = 0; i < n; i++) {
    const source_def *d = g_all[i];
    if (d->update_interval_sec <= 0 || (skip && skip[i])) continue;
    if (hold && hold[i] > now0) continue;
    int ln = lane_of(d);
    long off = sched_boot_offset(k[ln]++, m[ln], g_q.lane[ln].workers, stag,
                                 ramp, sched_row_interval(&g_state[i], d));
    if (off > last[ln]) last[ln] = off;
    next[i] = now0 + (time_t)off;
  }
  free(skip);
  free(hold);
  /* The plan is in next[]; the heap is built from it once, here, and kept
   * current by plan_set() from then on. */
  pthread_mutex_lock(&g_q.mu);
  heap_rebuild();
  pthread_mutex_unlock(&g_q.mu);
  sched_hosts_seed();
  fprintf(stderr, "[sched] boot ramp: %d scheduled source(s) start within %lds, "
                  "%d pod(s) within %lds (stagger %lds, ramp cap %lds); %d not "
                  "due yet keep their own time\n",
          m[LANE_COLLECT], last[LANE_COLLECT], m[LANE_MAINT], last[LANE_MAINT],
          stag, ramp, n_wait);

  for (int ln = 0; ln < LANE_N; ln++)
    for (int i = 0; i < g_q.lane[ln].workers; i++) {
      pthread_t t;
      if (pthread_create(&t, NULL, worker_thread, (void *)(intptr_t)ln) == 0)
        pthread_detach(t);
      else fprintf(stderr, "[sched] %s worker %d failed to start\n",
                   ln == LANE_MAINT ? "maint" : "collector", i);
    }
  fprintf(stderr, "[sched] %d sources registered, %d workers + %d maintenance "
                  "worker(s)\n", n, workers, mworkers);

  unsigned long tick = 0;
  int deferred = 0;
  unsigned cad_gen = cadence_generation();
  unsigned ovr_gen = source_interval_generation();
  for (;;) {
    if (atomic_load(&g_shutdown)) { sleep(1); continue; }
    time_t now = time(NULL);
    /* The cadence policy changed (PUT /api/sources/cadence), or a per-source
     * interval was set or cleared (PUT/DELETE /api/sources/:id/interval): a
     * row whose next due time is further away than its NEW spacing is pulled
     * in to now + spacing, so a faster cadence takes effect within one
     * interval and a slower one lets the already-planned run happen and then
     * stretches. A row under a health hold keeps it - an edit to one
     * source's interval must not re-probe every quarantined source. This is
     * the one place the whole registry is still walked, and only on a change. */
    if (cadence_generation() != cad_gen || source_interval_generation() != ovr_gen) {
      cad_gen = cadence_generation();
      ovr_gen = source_interval_generation();
      long pulled = sched_replan(now);
      fprintf(stderr, "[sched] cadence changed (policy or a per-source interval): %ld row(s) re-planned\n", pulled);
    }
    deferred = sched_tick(db, now, cand);
    watchdog_scan();
    /* Once a minute: how much of the pool is busy, how deep the backlog is,
     * and how often the host gate had to make a worker wait. Without this the
     * only symptom of an undersized pool or a throttling upstream is "the map
     * feels stale", which is what made the serial loop survive this long. */
    if (++tick % 60 == 0) {
      long waits = 0, timeouts = 0, inflight = 0;
      hostgate_counters(&waits, &timeouts, &inflight);
      pthread_mutex_lock(&g_q.mu);
      int queued = g_q.lane[LANE_COLLECT].count, mqueued = g_q.lane[LANE_MAINT].count;
      int busy = 0, mbusy = 0, backed = 0, quar = 0;
      for (int i = 0; i < n; i++) {
        if (g_q.started[i]) { if (lane_of(g_all[i]) == LANE_MAINT) mbusy++; else busy++; }
        if (g_state[i].quarantined) quar++;
        else if (g_state[i].backoff_until > now) backed++;
      }
      /* Per-host in flight: how many workers are running a source whose host
       * is known, how many hosts that is, and the three busiest. */
      long h_infl = 0; int h_busy = 0;
      int top_id[3] = {0, 0, 0};
      for (int h = 1; h <= g_nh; h++) {
        if (g_hinfl[h] <= 0) continue;
        h_infl += g_hinfl[h]; h_busy++;
        for (int t = 0; t < 3; t++)
          if (!top_id[t] || g_hinfl[h] > g_hinfl[top_id[t]]) {
            for (int u = 2; u > t; u--) top_id[u] = top_id[u - 1];
            top_id[t] = h; break;
          }
      }
      char top[256]; top[0] = '\0';
      for (int t = 0; t < 3 && top_id[t]; t++) {
        size_t have = strlen(top);
        snprintf(top + have, sizeof top - have, "%s%s:%d", have ? "," : "",
                 g_hname[top_id[t]], g_hinfl[top_id[t]]);
      }
      pthread_mutex_unlock(&g_q.mu);
      /* `backed_off`/`quarantined`, then the maintenance lane, are APPENDED so
       * the existing parsers of this line keep matching (same reason `stored=`
       * trails the run line). */
      fprintf(stderr, "[sched] busy=%d/%d queued=%d | hostgate inflight=%ld "
                      "waited=%ld over_budget=%ld | backed_off=%d quarantined=%d"
                      " | maint busy=%d/%d queued=%d | attach_failures=%ld"
                      " | deferred=%d | hosts busy=%d inflight=%ld top=%s"
                      " skip_pops=%ld forced=%ld\n",
              busy, workers, queued, inflight, waits, timeouts, backed, quar,
              mbusy, mworkers, mqueued, atomic_load(&g_attach_failures), deferred,
              h_busy, h_infl, top[0] ? top : "-",
              atomic_load(&g_skip_pops), atomic_load(&g_forced_pops));
    }
    sleep(1);
  }
}

/* Runs on its OWN connection. A sqlite transaction belongs to a connection,
 * not a thread, so sharing the server's handle meant this loop's BEGIN/COMMIT
 * around every emit() interleaved with the HTTP thread's transactions — one
 * side's ROLLBACK could discard the other's committed-looking work. */
static void *sched_thread(void *arg) {
  (void)arg;
  db_handle own = {0};
  if (db_attach(&own, NULL) != 0) {
    fprintf(stderr, "[sched] cannot open its own DB connection; scheduler off\n");
    return NULL;
  }
  scheduler_loop(&own);               /* infinite; lives for process lifetime */
  db_close(&own);
  return NULL;
}

void scheduler_start_background(db_handle *db) {
  if (getenv("JO_NO_SCHED")) {
    fprintf(stderr, "[sched] background scheduler disabled (JO_NO_SCHED)\n");
    return;
  }
  pthread_t t;
  if (pthread_create(&t, NULL, sched_thread, db) == 0) {
    pthread_detach(t);
    fprintf(stderr, "[sched] background scheduler started (serve + refresh)\n");
  } else {
    fprintf(stderr, "[sched] pthread_create failed; serving without scheduler\n");
  }
}
