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
#endif
