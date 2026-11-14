/* tests/unit/test_datetok.c — the {date…} / {ago:N} tokens in lib/datetok.c.
 * No network, no database, a fixed clock.
 *
 * WHAT IS BEING PROTECTED.
 *   1. The spellings rows use render as they always did ({date}, {date-1d},
 *      {date-1m:%Y%m}, {date:%Y%m%d:-2}, {date:@s}).
 *   2. A format ending in ":<digits>" keeps it: only a SIGNED tail is a day
 *      offset, so {date:%Y-%m-%dT00:00:00} is not cut to "…T00:00".
 *   3. A shift too long to be a real one is left verbatim, never overflowed.
 *   4. "{date" at the end of a string is not a token.
 *   5. An UNTERMINATED token does not swallow the next token's brace.
 *   6. {ago:N} never renders a date before 1970. */

#include "../../lib/datetok.c"

#include <assert.h>

#define NOW ((time_t)1763121600)   /* 2025-11-14T12:00:00Z */

static void expect(const char *tmpl, const char *want) {
  char *got = datetok_expand(tmpl, NOW, 0, 0, 'd');
  if (!got || strcmp(got, want)) {
    fprintf(stderr, "FAIL  \"%s\" -> \"%s\", want \"%s\"\n", tmpl,
            got ? got : "(null)", want);
    exit(1);
  }
  free(got);
}

int main(void) {
  /* 1. unchanged spellings */
  expect("d={date}", "d=2025-11-14");
  expect("d={date-1d}&w={date+2w}", "d=2025-11-13&w=2025-11-28");
  expect("d={date-1m:%Y%m}", "d=202510");
  expect("d={date-1y:%Y}", "d=2024");
  expect("d={date:%Y%m%d:-2}", "d=20251112");
  expect("d={date:%d-%m-%Y:+1}", "d=15-11-2025");
  expect("d={date:%Y-%m-%d:-30}", "d=2025-10-15");
  expect("d={date:@s}&m={date:@ms}", "d=1763121600&m=1763121600000");
  expect("d={date:%Y-%m-%dT%H:%M:%SZ}", "d=2025-11-14T12:00:00Z");
  expect("d={date-365d}&e={date-0000365d}", "d=2024-11-14&e=2024-11-14");

  /* 2. an unsigned ":digits" tail belongs to the format */
  expect("d={date:%Y-%m-%dT00:00:00}", "d=2025-11-14T00:00:00");
  expect("d={date-2d:%Y-%m-%dT23:59}", "d=2025-11-12T23:59");
  expect("d={date:%H:%M:-1}", "d=12:00");          /* signed: still an offset */

  /* 3. an absurd shift is not arithmetic */
  expect("d={date-99999999d}", "d={date-99999999d}");
  expect("d={date+123456789012345678901234567890y}",
         "d={date+123456789012345678901234567890y}");

  /* 4. "{date" at the very end */
  assert(!datetok_present("x{date"));
  assert(datetok_present("x{date}") && datetok_present("x{date-1d}"));
  assert(!datetok_present("x{dated}") && !datetok_present(NULL));
  assert(datetok_expand("x{date", NOW, 0, 0, 'd') == NULL);

  /* 5. an unterminated token is left alone; the next one still renders */
  expect("a={date:%Y&b={q}&c={date}", "a={date:%Y&b={q}&c=2025-11-14");
  expect("a={ago:60&b={q}&c={date}", "a={ago:60&b={q}&c=2025-11-14");

  /* 6. {ago:N} clamps at the epoch */
  expect("s={ago:3600}", "s=2025-11-14T11:00:00Z");
  expect("s={ago:99999999999}", "s=1970-01-01T00:00:00Z");
  expect("s={ago:-5}", "s=2025-11-14T12:00:00Z");

  printf("test_datetok: OK\n");
  return 0;
}
