/* test_seenset_hset.c — lib/seenset's hash-only set (ARC-02): the structure
 * core/intel.c counts DISTINCT stored uids in. Pins the three properties the
 * sink's `stored=` depends on: new/dup answers, a ceiling that stops growth and
 * flips `inexact` (n becomes a floor), and exact counts below the ceiling. */
#include "../../lib/seenset.c"
#include <assert.h>
#include <stdio.h>

int main(void) {
  seen_hset s = {0};
  char k[32];

  assert(!seen_hset_add(&s, NULL));
  assert(seen_hset_add(&s, "a") == 1);
  assert(seen_hset_add(&s, "a") == 0);
  assert(seen_hset_add(&s, "b") == 1);
  assert(s.n == 2 && !s.inexact);

  /* 100,000 distinct keys, each added twice: n is exact, through several
   * doublings, with the default ceiling. */
  for (int i = 0; i < 100000; i++) {
    snprintf(k, sizeof k, "uid-%d", i);
    assert(seen_hset_add(&s, k) == 1);
  }
  for (int i = 0; i < 100000; i++) {
    snprintf(k, sizeof k, "uid-%d", i);
    assert(seen_hset_add(&s, k) == 0);
  }
  assert(s.n == 100002 && !s.inexact);
  seen_hset_free(&s);
  assert(s.n == 0 && s.tab == NULL);

  /* A ceiling of 1024 slots holds about 70% of them, then the table refuses to
   * grow: further keys are "not new" and the count is a floor. */
  seen_hset c = {0}; c.max_slots = 1024;
  int fresh = 0;
  for (int i = 0; i < 2000; i++) {
    snprintf(k, sizeof k, "c-%d", i);
    fresh += seen_hset_add(&c, k);
  }
  assert(c.inexact == 1);
  assert(fresh == (int)c.n && c.n >= 700 && c.n <= 1024 * 7 / 10 + 1);
  /* Keys that made it in stay recognised as duplicates after the ceiling. */
  snprintf(k, sizeof k, "c-%d", 0);
  assert(seen_hset_add(&c, k) == 0);
  seen_hset_free(&c);

  printf("test_seenset_hset: all passed\n");
  return 0;
}
