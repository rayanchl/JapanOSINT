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
 * The table never grows. When it is full the least-recently-touched slot is
 * recycled, so a spray of distinct source IPs degrades the limiter's memory
 * (an attacker may evict their own counter) instead of wedging it closed for
 * legitimate callers. That is the correct trade for a DoS guard: failing OPEN
 * under table pressure keeps the server usable, and the paths protected here
 * have their own second line of defence (TOTP secrecy, JWKS negative cache).
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
  RL_CLASS_MAX  = 3
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
