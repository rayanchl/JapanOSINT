/* core/breach_jobs.h — async admin breach jobs (ingest / fetch).
 *
 * A breach ingest runs for minutes–hours, so it must never run inline on the
 * single mongoose event loop. Each job runs on a detached thread with its OWN
 * WAL database connection (never the server's shared handle — a bulk ingest's
 * long transaction + bulk pragmas would otherwise swallow the event loop's
 * writes). Jobs are single-flighted and tracked in a small in-memory table.
 *
 * Every entry point returns a malloc'd JSON envelope for the HTTP reply and
 * sets *http_status. Caller frees. These are operator-gated at the route. */
#ifndef JO_BREACH_JOBS_H
#define JO_BREACH_JOBS_H
#include <stddef.h>

/* Resolve a CALLER-SUPPLIED breach file path and require it to live inside the
 * breach data directory (JO_BREACH_DIR, default <repo>/data/breach) or to be
 * one of the two committed catalogue files. Writes the resolved absolute path
 * to out[] and returns 0; returns -1 for every rejection — outside the base,
 * nonexistent, unreadable — deliberately without distinguishing them, so the
 * refusal is not an existence oracle.
 *
 * This exists because /api/admin/breach/catalog/preview?path= read any file on
 * the box and echoed its parsed lines back in `sample[]`; the full incident is
 * in the comment above the implementation. Every route that accepts a path
 * from the network must run it through here and then use the RESOLVED string,
 * never the caller's original. `out` should be PATH_MAX bytes. */
int breach_path_confine(const char *req, char *out, size_t cap);

/* The single JSON body every breach_path_confine() rejection answers with.
 * Static storage; do not free. */
const char *breach_path_confine_error(void);

/* Start an async ingest of `path` (a staged dump) for `source_id`. When
 * materialize != 0 each datapoint is indexed into breach_items; dry_run != 0
 * projects counts and writes nothing. 202 on start, 409 if one is already
 * running, 400 on bad args or on a path breach_path_confine() refuses. */
char *breach_job_ingest(const char *source_id, const char *path, const char *type,
                        int materialize, int dry_run, int *http_status);

/* Start an async download of `url` into the breach staging dir for `source_id`.
 * Best-effort, buffered in memory — intended for moderate datasets; stage huge
 * corpora out of band. 202 on start, 409 if busy, 400 on bad args.
 *
 * `url` is caller-supplied, so it is checked with hostgate_url_check_strict()
 * — the same ruleset alertsapi.c applies to a webhook target. Loopback,
 * RFC1918 and the metadata range are refused with 400 rather than fetched. */
char *breach_job_fetch(const char *source_id, const char *url, int *http_status);

/* Snapshot of one job (job_id) or, when job_id is NULL, all jobs, as JSON.
 * Always returns a malloc'd string. */
char *breach_job_status(const char *job_id);

#endif
