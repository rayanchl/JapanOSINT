/* core/sourceapi.h — the source pages' API: one source in full, the
 * registry searched and paged on the server, and the registry grouped by
 * collector.
 *
 * Why this exists beside statusapi.c and intelapi.c. Both of those serve the
 * WHOLE registry in one body (20 MB / 8 s for /api/status) and neither says
 * which engine runs a source, how it fetches and parses, what shape of record
 * it stores, or which collector it belongs to — the facts the source pages
 * are for. Every builder here is a DB read plus the in-memory registry; the
 * HTTP layer runs them on a worker thread and caches the body
 * (core/httpd.c), the same way the fleet routes are served.
 *
 * Every number is measured or null. A bounded list says how much of how many
 * it shows. Nothing here invents a value for a source that never ran. */
#ifndef JO_SOURCEAPI_H
#define JO_SOURCEAPI_H
#include "db.h"

/* GET /api/sources/:id/detail — malloc'd JSON, or NULL when no source has
 * that id (the caller answers 404). */
char *sourceapi_detail(db_handle *db, const char *id);

/* The search / list parameters, parsed by the HTTP layer. Strings may be NULL
 * or "" (= no filter); `limit <= 0` means the default page (50, max 500). */
typedef struct {
  const char *q;          /* substring over id, name, name_ja, description,
                           * collector, fetch host (case-insensitive)       */
  const char *collector;  /* exact collector key                            */
  const char *category;   /* exact category                                 */
  const char *engine;     /* hpengine | vjson | vrss | vgeo | vcsv | rss |
                           * socrata | native                                */
  const char *kind;       /* pivot | scheduled | scheduled-pivot |
                           * unreachable | internal                          */
  const char *status;     /* online | degraded | offline | pending           */
  const char *grade;      /* A..F, or "unrated"                              */
  int         has_items;  /* 1 = only sources with stored items; 0 = all     */
  const char *sort;       /* name (default) | id | items | fetched | grade |
                           * collector | interval                            */
  int         desc;       /* 1 = descending                                  */
  int         limit, offset;
} sourceapi_query;

/* GET /api/sources/search — {data, page, facets, meta}. Never NULL. */
char *sourceapi_search(db_handle *db, const sourceapi_query *q);

/* GET /api/collectors — every collector key with its tallies. Never NULL. */
char *sourceapi_collectors(db_handle *db);

/* GET /api/collectors/:key — the collector's header, its id-prefix groups and
 * its sources (a search page restricted to the key, honouring q/limit/offset).
 * NULL when no registered source has that collector. */
char *sourceapi_collector(db_handle *db, const char *key, const sourceapi_query *q);

/* GET /api/sources/:id/summary — the small-model summary of a source, built
 * ONLY from the measured facts sourceapi_detail() reports, cached in the
 * source_summary table by (id, data fingerprint, model). `refresh` ignores
 * the cache. Returns {summary, result_type, caveats[], model, facts_used,
 * generated_at, cached} or, when the model cannot be reached or answers
 * nothing usable, {error: <llm status code>, facts: {...}} — never a summary
 * that was not generated. NULL only when the id is unknown. */
char *sourceapi_summary(db_handle *db, const char *id, int refresh);

#endif
