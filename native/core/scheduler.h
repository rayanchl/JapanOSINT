#ifndef JO_SCHEDULER_H
#define JO_SCHEDULER_H
#include "db.h"
#include "../source.h"
int  scheduler_run_source(db_handle *db, const source_def *d, const char *entity);
void scheduler_loop(db_handle *db);
/* Run scheduler_loop on a detached background thread so httpd_serve both
 * serves and refreshes collectors (parity with Node's in-process scheduler).
 * No-op if JO_NO_SCHED is set (serve-only / multi-instance / migration). */
void scheduler_start_background(db_handle *db);
/* Stop handing collectors to the worker pool and wait up to `wait_ms` for the
 * runs already executing to finish, so an emit() in progress completes its
 * transaction. Workers are detached and scheduler_loop never returns, so this
 * is a drain, not a join: it can return with runs still in flight, and it says
 * so on stderr when it does. Safe to call when the scheduler never started. */
void scheduler_stop_background(int wait_ms);
#endif
