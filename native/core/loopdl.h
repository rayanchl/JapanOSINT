/* core/loopdl.h — a deadline for SQLite queries that run on the event loop.
 *
 * Some routes still query inline on the Mongoose thread. A query that overruns
 * there freezes every client (API-07, audit 2026-11-14). One mechanism, one
 * knob: sqlite3_progress_handler on the loop's connection, ARMED per request.
 *
 *   JO_LOOP_QUERY_MS   milliseconds one request may spend inside SQLite on the
 *                      loop thread (default 5000; 0 = no deadline).
 *
 * State is thread-local and set only by loopdl_arm(), which only httpd's
 * request wrapper calls. So the handler does nothing on a worker thread (even
 * one that fell back to the shared handle), at startup, in migrations, or in a
 * timer: nothing there ever arms it. An overrun is interrupted
 * (SQLITE_INTERRUPT) and stays interrupted until loopdl_disarm(), which
 * reports it so the caller can answer 503 instead of whatever the route made
 * of a failed query. */
#ifndef JO_LOOPDL_H
#define JO_LOOPDL_H
#include <sqlite3.h>

#define LOOPDL_DEFAULT_MS 5000

/* Install the progress handler on the loop connection (once). */
void loopdl_install(sqlite3 *h);
/* Arm for this thread from JO_LOOP_QUERY_MS; loopdl_arm_ms for an explicit
 * deadline (<= 0 disables). */
void loopdl_arm(void);
void loopdl_arm_ms(long ms);
/* Disarm; returns 1 when the deadline interrupted a query since arming. */
int  loopdl_disarm(void);
/* The 503 body naming the deadline in force. */
const char *loopdl_body(void);
/* The configured deadline, milliseconds (0 = off). */
long loopdl_ms(void);
#endif
