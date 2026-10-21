/* core/thresholds.h — the ONE registry of embedding-similarity thresholds.
 *
 * Every cut-off a vector comparison is judged against (entity merge
 * candidates, story joining, semantic alerts, record relevance, suggestions,
 * the id resolver, camera duplicates, the repair gate, the corpus KNN arm)
 * used to be a named constant with its own getenv() in its own module. None
 * of them has been measured on the production model, so an operator needs to
 * move them while the server runs, and see which value is in force and why.
 *
 * Resolution, per key, on every threshold_get():
 *   1. a runtime override in settings_thresholds (written by
 *      PUT /api/admin/thresholds/:key, platform operators only, audited);
 *   2. the key's environment variable — the SAME names, and the same
 *      "ignored when unparsable or out of range" rule, the modules had;
 *   3. the compiled default.
 * The DB layer is cached in memory: loaded by threshold_migrate() (called
 * from db_open) and refreshed by every threshold_set()/threshold_reset(), so
 * a hot path takes a mutex and reads an array, never SQLite. A DB value is
 * clamped to [min,max] when it is loaded; a PUT outside the range is refused.
 */
#ifndef JO_THRESHOLDS_H
#define JO_THRESHOLDS_H
#include "db.h"
#include "../third_party/cJSON.h"

typedef struct {
  const char *key;          /* stable API name, e.g. "story_tau"            */
  const char *env;          /* environment override, e.g. "JO_STORY_TAU"     */
  double      dflt;         /* compiled default (the module's constant)      */
  double      min, max;     /* inclusive range for env and runtime values    */
  const char *metric;       /* "cosine" | "cosine_margin" | "l2_distance"    */
  const char *description;
} threshold_def;

/* Source of the value in force. */
typedef enum { THR_SRC_DEFAULT = 0, THR_SRC_ENV = 1, THR_SRC_DB = 2 } threshold_src;

/* The table and its length (for listing). */
const threshold_def *threshold_defs(int *n);
const threshold_def *threshold_find(const char *key);

/* The value in force for `key`. An unknown key is a programming error: it
 * prints once to stderr and returns NAN, so a comparison against it is false. */
double threshold_get(const char *key);
/* Same, and which layer it came from. Returns -1 for an unknown key. */
int    threshold_resolve(const char *key, double *value, threshold_src *src);
const char *threshold_src_name(threshold_src s);   /* "db" | "env" | "default" */

/* CREATE TABLE IF NOT EXISTS settings_thresholds, then (re)load the cache
 * from it. Called from db_open(). */
void threshold_migrate(db_handle *db);
/* Re-read the cache from the table (another process wrote it, or a test). */
void threshold_reload(db_handle *db);

/* Persist an override and refresh the cache. 0 ok; -1 unknown key; -2 value
 * out of [min,max] or not finite; -3 database error. */
int  threshold_set(db_handle *db, const char *key, double value,
                   const char *updated_by);
/* Remove the override (env/default take over again). 0 ok (also when there
 * was none); -1 unknown key; -3 database error. */
int  threshold_reset(db_handle *db, const char *key);

/* One entry / the whole list as JSON:
 *   {key, value, source, default, min, max, env, env_value, metric,
 *    description, updated_by, updated_at}
 * The caller owns the result. */
cJSON *threshold_json(const char *key);
char  *threshold_list_json(void);

/* The admin routes, so httpd.c only dispatches (and they are testable):
 *   GET /api/admin/thresholds          → 200 {data:[…], meta:{count,order}}
 *   PUT /api/admin/thresholds/:key     body {value:n} | {reset:true}
 *       → 200 {data:{…}}; 400 bad body / out of range; 404 unknown key.
 * Each successful PUT writes an audit_events row ("threshold.set" /
 * "threshold.reset", tenant "platform") carrying key, old and new value.
 * Returns a malloc'd body and sets *status. */
char *thresholds_api_put(db_handle *db, const char *key, const char *body,
                         size_t body_len, const char *user_id, int *status);

#endif
