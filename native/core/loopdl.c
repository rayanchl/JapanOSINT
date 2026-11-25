/* core/loopdl.c — see loopdl.h. */
#include "loopdl.h"
#include "../lib/joenv.h"
#include <stdio.h>
#include <time.h>

#define LOOPDL_STRIDE 1000          /* VM instructions between clock reads */

static __thread long long t_deadline_ms;   /* monotonic; 0 = not armed */
static __thread int       t_tripped;

static long long mono_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int progress_cb(void *unused) {
  (void) unused;
  if (!t_deadline_ms) return 0;                     /* not armed on THIS thread */
  if (t_tripped) return 1;                          /* rest of the request aborts fast */
  if (mono_ms() >= t_deadline_ms) { t_tripped = 1; return 1; }
  return 0;
}

/* Lock waits (API-07): a writer holding the lock is not a VM-step overrun, so
 * the progress handler never sees it. The busy handler gives up at the SAME
 * deadline. Not armed (startup, timers, migrations) it behaves as the busy
 * timeout it replaces. */
static __thread long long t_busy_start_ms;

static int busy_cb(void *unused, int count) {
  (void) unused;
  long long now = mono_ms();
  if (count == 0) t_busy_start_ms = now;
  if (t_deadline_ms) {
    if (t_tripped || now >= t_deadline_ms) { t_tripped = 1; return 0; }
  } else if (now - t_busy_start_ms >= LOOPDL_BUSY_FALLBACK_MS) {
    return 0;
  }
  struct timespec nap = { 0, (count < 5 ? 1 : 5) * 1000000L };
  nanosleep(&nap, NULL);
  return 1;
}

void loopdl_install(sqlite3 *h) {
  if (!h) return;
  sqlite3_progress_handler(h, LOOPDL_STRIDE, progress_cb, NULL);
  sqlite3_busy_handler(h, busy_cb, NULL);
}

long loopdl_ms(void) {
  return jo_env_long("JO_LOOP_QUERY_MS", LOOPDL_DEFAULT_MS, 0, 3600000);
}

void loopdl_arm_ms(long ms) {
  t_tripped = 0;
  t_deadline_ms = ms > 0 ? mono_ms() + ms : 0;
}

void loopdl_arm(void) { loopdl_arm_ms(loopdl_ms()); }

int loopdl_disarm(void) {
  int t = t_tripped;
  t_deadline_ms = 0;
  t_tripped = 0;
  return t;
}

const char *loopdl_body(void) {
  static __thread char b[256];
  snprintf(b, sizeof b,
    "{\"error\":\"query_deadline_exceeded\",\"message\":\"this request ran a "
    "database query longer than the %ld ms the server allows on its event "
    "loop; narrow the request\",\"deadline_ms\":%ld}", loopdl_ms(), loopdl_ms());
  return b;
}
