/* core/source_interval.h — a per-source collection interval set by the
 * operator, from the source's own page.
 *
 * Three things can say how often a scheduled row runs, and they compose in
 * ONE order (core/scheduler.c sched_row_interval):
 *
 *   1. the row's authored update_interval_sec, re-measured from its own items
 *      when it is adaptive (sched_state_base);
 *   2. the operator's SYSTEM-WIDE policy on top of that (core/cadence.h): a
 *      global interval, or a floor and a ceiling;
 *   3. THIS: an interval the operator set for ONE source, which outranks both.
 *
 * Specific beats general. A floor of six hours is a statement about the fleet;
 * "this one source every hour" is a statement about that source, made by the
 * same operator, later and more precisely, and it would be useless if the
 * floor it was set under silently held it. So an enabled override IS the
 * row's cadence, and the pages say so ("set for this source") beside the
 * declared, measured and policy figures, none of which is hidden by it.
 *
 * Stored in source_interval_override (one row per source) so it survives
 * restarts; cached in memory because the scheduler asks for every row every
 * tick. A change takes effect in the running scheduler without a restart,
 * exactly as a cadence-policy change does: a row whose next due time is
 * further away than its new interval is pulled in to now + interval.
 *
 * Disabling keeps the number (a re-enable does not start from a guess);
 * clearing deletes the row. An entity pivot (interval 0) is refused: it runs
 * when asked, and an override cannot create a schedule, exactly as the
 * adaptive measurement cannot. */
#ifndef JO_SOURCE_INTERVAL_H
#define JO_SOURCE_INTERVAL_H
#include "db.h"

/* The bounds an interval may take; shared with core/source_settings.c, which
 * offers the same knob as the `interval_sec` key of the Settings section. */
#define SI_MIN_SEC 10L
#define SI_MAX_SEC (366L * 86400L)

typedef struct {
  int  present;          /* a row exists for this source                     */
  long interval_sec;     /* the stored number, kept while disabled           */
  int  enabled;
  char updated_at[32];
  char updated_by[128];
} source_interval;

/* Create the table if missing and load every row into the cache. Safe to
 * call more than once (a re-init re-reads the database). */
void source_interval_init(db_handle *db);

/* The ENABLED override for `id`, else 0. Cache only, no database: this is
 * what the scheduler reads for every row on every tick. */
long source_interval_for(const char *id);

/* The stored row for `id` (present=0 when none). Reads the database. */
int source_interval_get(db_handle *db, const char *id, source_interval *out);

/* Validate and persist. Refused (-1, *err a static string) for an id that
 * is not registered, a row with no schedule (interval 0), a value under 10 s
 * or over a year. `who` may be NULL. 0 on success, and the cache and
 * generation are updated so the scheduler re-plans. */
int source_interval_set(db_handle *db, const char *id, long interval_sec,
                        int enabled, const char *who, const char **err);

/* Delete the row (0 when there was none to delete too). */
int source_interval_clear(db_handle *db, const char *id, const char **err);

/* Increments on every successful set or clear; the scheduler re-plans on
 * change, as it does for cadence_generation(). */
unsigned source_interval_generation(void);

/* How many enabled overrides the cache holds (for the pages' cost line). */
int source_interval_count(void);

/* The composition for one source as JSON (malloc'd): declared, measured,
 * policy, the override row, and the effective figure with what decided it.
 * `can_write` is echoed so a page does not offer a form it cannot submit.
 * NULL when `id` is not registered. */
char *source_interval_json(db_handle *db, const char *id, int can_write);

#endif
