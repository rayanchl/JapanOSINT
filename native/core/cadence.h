/* core/cadence.h — ONE system-wide collection cadence policy.
 *
 * Every source declares its own `update_interval_sec` (measured when it was
 * authored: 60 s for an earthquake feed, a week for a statistical bulk file).
 * The policy sits ABOVE those declarations and is the operator's: a single
 * global interval that every scheduled row runs at, or a floor and a ceiling
 * the declared values are clamped into, or nothing (each row as declared).
 * Entity pivots (interval 0) are never touched — they run when asked.
 *
 * Stored in app_settings (one row per key) so it survives restarts and is
 * editable from the Sources page; JO_SCHED_INTERVAL_SEC /
 * JO_SCHED_MIN_INTERVAL_SEC / JO_SCHED_MAX_INTERVAL_SEC bootstrap it when the
 * database holds no policy; and when neither says anything the COMPILED
 * default is a global cadence of six hours (CAD_DEFAULT_GLOBAL_SEC in
 * cadence.c, the owner's decision of 2026-10-22). "Declared" is therefore a
 * choice an operator makes by saving zeros, not the silence of an unset knob. The scheduler reads cadence_apply() for every
 * row, so a change takes effect without a restart: a row whose next due time
 * is further away than the new interval is pulled in to now + interval.
 * The declared value stays as data — the source pages show both. */
#ifndef JO_CADENCE_H
#define JO_CADENCE_H
#include "db.h"

typedef struct {
  long global_sec;   /* >0: every scheduled row runs at this interval */
  long min_sec;      /* >0: no scheduled row runs more often than this */
  long max_sec;      /* >0: no scheduled row runs less often than this */
} cadence_policy;

/* Load the policy from the database (creating app_settings if it is missing),
 * else from the environment, else the compiled default (global 6 h). Safe to
 * call more than once. */
void cadence_init(db_handle *db);

/* The interval the scheduler uses for a row declaring `declared`. <= 0 (an
 * entity pivot or a pod that is not scheduled) is returned unchanged. */
long cadence_apply(long declared);

/* "declared" | "global" | "clamped" for a row declaring `declared`. */
const char *cadence_mode_for(long declared);

/* WHICH rows the policy does not move (2026-11-14, the owner's decision "let
 * all transport or live-needing sources be real time"). Two kinds run at their
 * OWN cadence — the declared interval, or the measured one for an adaptive
 * row — whatever the system-wide policy says:
 *   "pod"      a `_` maintenance pod: it asks no upstream (2026-11-07);
 *   "realtime" a transport source (category "transport"), or any source
 *              declaring an interval under CAD_REALTIME_BELOW_SEC — its author
 *              measured minute-scale data (vehicle positions, webcams, grid
 *              telemetry, quake and warning feeds), which a six-hour policy
 *              turns into a stale snapshot.
 * NULL for every other row. An operator's per-source interval still outranks
 * both (core/source_interval.h). Measured over the registry when it shipped:
 * 2,239 transport rows (2,006 of them declare LONGER than six hours — static
 * timetables, which now run less often, as declared) and 1,024 rows declaring
 * under an hour; together about +122,000 runs a day on 345,000. */
#define CAD_REALTIME_BELOW_SEC 3600
struct source_def;
const char *cadence_exempt_reason(const struct source_def *d);

/* The policy applied to THIS row's base interval (declared, or measured for
 * an adaptive row): `base` unchanged for an exempt row, cadence_apply(base)
 * otherwise. Every reader that shows or uses "the interval under policy"
 * calls this, so a page cannot show a policy figure the scheduler ignores. */
long cadence_apply_for(const struct source_def *d, long base);

cadence_policy cadence_get(void);
/* Where the current policy came from: "db" | "env" | "default". */
const char *cadence_source(void);
/* Increments on every successful cadence_set; the scheduler re-plans on change. */
unsigned cadence_generation(void);

/* Validate and persist a policy. Returns 0, or -1 with *err naming the
 * rejected field (a static string). who may be NULL. */
int cadence_set(db_handle *db, const cadence_policy *p, const char *who, const char **err);

/* Parse {"global_interval_sec":N,"min_interval_sec":N,"max_interval_sec":N}
 * onto `base` (fields absent from the body keep base's value). Returns 0 or
 * -1 with *err set. */
int cadence_parse(const char *json, const cadence_policy *base, cadence_policy *out, const char **err);

/* The policy, its provenance and what it costs: the registry's scheduled
 * rows, runs/day as declared and runs/day under the policy — per-source
 * intervals honoured, realtime rows and maintenance pods exempt, as the
 * scheduler runs them (exempt_rows / exempt_runs_per_day count them);
 * the adaptive measurement and health backoff are per-row state and are not
 * counted. Malloc'd JSON. */
char *cadence_json(db_handle *db);

#endif
