/* lib/joclock.h — the two millisecond clocks, in one place.
 *
 * jo_mono_ms()  CLOCK_MONOTONIC: for intervals, timeouts and pacing. It does
 *               not jump when the wall clock is set.
 * jo_wall_ms()  the wall clock: for a timestamp that is stored or compared
 *               with one that was.
 *
 * Nine files each carried a private copy (mono_ms / now_ms), and the name
 * now_ms meant the monotonic clock in four of them and the wall clock in
 * three. Pick by what the value is FOR, not by the name it used to have. */
#ifndef JO_CLOCK_H
#define JO_CLOCK_H
#include <sys/time.h>
#include <time.h>

static inline long long jo_mono_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + t.tv_nsec / 1000000LL;
}

static inline long long jo_wall_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}
#endif
