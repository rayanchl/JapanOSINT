/* core/ratelimit.h — fixed-window request throttle keyed on (client, class).
 *
 * WHY THIS EXISTS. The server had no rate limiting anywhere. Two paths make
 * that a real problem rather than a theoretical one:
 *
 *   1. /admin/break-glass/login is mounted OUTSIDE the /api auth gate (it
 *      exists for when Supabase auth is down), and it compares a 6-digit TOTP.
 *      Unthrottled, that is 10^6 guesses with a +/-1 step window — minutes of
 *      traffic. It would be discovered broken DURING an emergency.
 *   2. auth.c refetches the JWKS document when it sees an unknown `kid`. One
 *      unauthenticated request with a made-up kid therefore costs us an
 *      outbound HTTPS round-trip; replayed, it is an amplifier pointed at our
 *      own identity provider.
 *
 * DESIGN. Fixed window, not a token bucket: a window boundary is one integer
 * comparison and needs no per-tick refill maths, and for "is this a flood?"
 * the extra precision of a bucket buys nothing. Open-addressed static table,
 * one mutex, ZERO allocation in the hot path — this runs on the mongoose event
 * loop thread, where a malloc under contention is a latency bug for every
 * other connection.
 *
 * The table never grows. When it is full a slot whose window has already
 * expired is recycled first, then the least-recently-touched slot of a
 * FAIL-OPEN class. Classes differ in what full-table pressure may do to them:
 *
 *   fail-open   (RL_ISOCHRONE, RL_COLLECTOR_RUN) — a spray of distinct keys
 *               may evict a live counter; the server stays usable and the
 *               worst case is one extra window of allowance.
 *   fail-closed (RL_BREAKGLASS, RL_JWKS) — a live counter of these classes is
 *               NEVER evicted, and a new key that finds no evictable slot is
 *               DENIED. These guard pre-auth paths, where "evict my own
 *               counter by spraying 512 other keys" was a reset button: the
 *               JWKS refetch budget is one GLOBAL key (auth.c), so recycling
 *               its slot handed an unauthenticated caller a fresh outbound
 *               fetch, and a fresh TOTP window for break-glass.
 */
#ifndef JO_RATELIMIT_H
#define JO_RATELIMIT_H

/* Route classes. Counters for different classes never share a slot, so a
 * burst of JWKS probes cannot lock a caller out of break-glass. */
typedef enum {
  RL_BREAKGLASS = 0,   /* pre-auth TOTP login                    */
  RL_JWKS       = 1,   /* unauthenticated request → JWKS refetch */
  /* GET /api/isochrone. Seconds of CPU and ~1500 timetable queries per miss
   * (core/isochrone.h), on a worker thread that is not free to spawn without
   * bound. Its own class so a user hammering isochrones cannot spend another
   * caller's break-glass or JWKS allowance. */
  RL_ISOCHRONE  = 2,
  /* A signed-in user making the server run a collector: an explicit
   * POST /api/intel/sources/:id/run by a non-operator, or a cache miss on
   * GET /api/data/<layer> (which runs the layer's collector live). Keyed on
   * the user id, not the IP, so one account cannot fan runs out over the
   * ~18,000 registered sources from many addresses. */
  RL_COLLECTOR_RUN = 3,
  RL_CLASS_MAX  = 4
} rl_class;

/* Charges one request against (key, cls). `key` is the client identity —
 * normally the peer IP as text; NULL/empty is folded into one shared bucket
 * rather than being waved through. Returns 1 if the request may proceed, 0 if
 * the window's allowance is spent. `retry_after_sec` (optional) is filled with
 * the whole seconds left in the current window on a denial, 0 on an allow. */
int ratelimit_allow(rl_class cls, const char *key, int limit, int window_sec,
                    int *retry_after_sec);

/* Observability for /api/status: total denials since boot. */
long ratelimit_denials(void);

#endif
