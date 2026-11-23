/* test_jocore_time.c — ARC-05: the shared clock helpers in lib/jocore.h write
 * exactly the bytes the private copies they replaced wrote. The "old" side
 * below is those copies, verbatim (strftime for the seconds/.000Z/SQL shapes,
 * the hand-assembled snprintf for milliseconds). Each pair is compared in the
 * same wall-clock second (retried across a rollover), and the shapes are also
 * pinned by length and punctuation so a change to either side is caught. */
#include "../../lib/jocore.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static void old_seconds(char *out, size_t n) {     /* cadence.c / source_interval.c / ... */
  time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}
static void old_ms0(char *buf, size_t n) {         /* image_vec.c / entity_vec.c */
  time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
  strftime(buf, n, "%Y-%m-%dT%H:%M:%S.000Z", &tm);
}
static void old_sql(char *out, size_t n) {         /* evidence.c / content_change.c */
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%d %H:%M:%S", &g);
}
static void old_ms(char *o, size_t n) {            /* maintenanceapi.c / certstream_jp.c */
  struct timespec sp; clock_gettime(CLOCK_REALTIME, &sp);
  time_t now = sp.tv_sec; struct tm g; gmtime_r(&now, &g);
  snprintf(o, n, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
    (unsigned)(g.tm_year + 1900) % 10000u, (unsigned)(g.tm_mon + 1) % 100u,
    (unsigned)g.tm_mday % 100u, (unsigned)g.tm_hour % 100u,
    (unsigned)g.tm_min % 100u, (unsigned)g.tm_sec % 100u,
    (unsigned)(sp.tv_nsec / 1000000) % 1000u);
}

typedef void (*fmt_fn)(char *, size_t);

/* Same-second comparison: take the old reading on both sides of the new one
 * and require the new one to equal one of them. */
static void same(fmt_fn old, fmt_fn neu, size_t cap, int skip_ms_tail, const char *what) {
  for (int tries = 0; tries < 5; tries++) {
    char a[64], b[64], c[64];
    old(a, cap); neu(b, cap); old(c, cap);
    if (skip_ms_tail) {              /* milliseconds differ by a few us: cut them */
      size_t n = strlen(a);
      if (n > 4) { a[n - 4] = 0; c[n - 4] = 0; b[strlen(b) - 4] = 0; }
    }
    if (!strcmp(a, b) || !strcmp(c, b)) return;
    usleep(1100000);
  }
  fprintf(stderr, "format mismatch: %s\n", what);
  assert(0);
}

int main(void) {
  char o[64];

  same(old_seconds, jo_iso_now_s, 32, 0, "jo_iso_now_s");
  same(old_ms0, jo_iso_now_ms0, 32, 0, "jo_iso_now_ms0");
  same(old_sql, jo_sql_now, 20, 0, "jo_sql_now");
  same(old_ms, jo_iso_now, 40, 1, "jo_iso_now (ms, ignoring the ms field)");

  /* Shapes. */
  jo_iso_now_s(o, sizeof o);
  assert(strlen(o) == 20 && o[4] == '-' && o[10] == 'T' && o[13] == ':' && o[19] == 'Z');
  jo_iso_now_ms0(o, sizeof o);
  assert(strlen(o) == 24 && !strcmp(o + 19, ".000Z"));
  jo_sql_now(o, sizeof o);
  assert(strlen(o) == 19 && o[10] == ' ' && o[4] == '-' && o[16] == ':');
  jo_iso_now(o, sizeof o);
  assert(strlen(o) == 24 && o[19] == '.' && o[23] == 'Z');

  /* A buffer of exactly the output size (the callers' char[20] / char[32]). */
  char s20[20]; jo_sql_now(s20, sizeof s20);
  assert(strlen(s20) == 19);
  char i32[32]; jo_iso_now_s(i32, sizeof i32);
  assert(strlen(i32) == 20);
  /* Too small: truncated and terminated, never an overrun. */
  char t8[8]; jo_iso_now_s(t8, sizeof t8);
  assert(strlen(t8) == 7);

  /* jo_mono_ms never goes backwards and ticks with real time. */
  long long m0 = jo_mono_ms();
  usleep(30000);
  long long m1 = jo_mono_ms();
  assert(m1 - m0 >= 25 && m1 - m0 < 2000);

  printf("test_jocore_time: all passed\n");
  return 0;
}
