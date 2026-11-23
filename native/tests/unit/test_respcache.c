/* tests/unit/test_respcache.c — core/respcache.c, the TTL cache for whole JSON
 * bodies behind /api/status and /api/intel/sources.
 *
 * What matters: a hit is a byte-for-byte COPY the caller owns (editing it must
 * not corrupt the cache), a stale entry is a miss and says so in the stats, a
 * failure/empty body is never cached ("never cache a failure"), the 8-slot
 * table evicts the OLDEST entry rather than a fresh one, and the pointer/age
 * contract holds for NULL arguments. Includes respcache.c for its statics. */
#include "../../core/respcache.c"
#include <assert.h>
#include <unistd.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

int main(void) {
  long age = -1; char *b;
  long long h0, m0, h1, m1; size_t bytes;

  printf("-- hit / miss\n");
  respcache_drop(NULL);
  ok(respcache_get("status", 5, &age) == NULL && age == 0, "empty cache: miss, age zeroed");
  respcache_put("status", "{\"a\":1}");
  b = respcache_get("status", 5, &age);
  ok(b && !strcmp(b, "{\"a\":1}") && age >= 0 && age < 500, "put then get returns the same bytes with a small age");
  b[1] = 'X';                                   /* caller scribbles on its copy */
  char *b2 = respcache_get("status", 5, NULL);
  ok(b2 && !strcmp(b2, "{\"a\":1}"), "the cache hands out a COPY: editing it does not change the stored body");
  free(b); free(b2);
  ok(respcache_get("other", 5, &age) == NULL, "a different key misses");

  printf("-- what is never cached\n");
  respcache_drop(NULL);
  respcache_put("k", "");
  ok(respcache_get("k", 5, NULL) == NULL, "empty body is not cached");
  respcache_put("k", NULL);
  ok(respcache_get("k", 5, NULL) == NULL, "NULL body is not cached");
  respcache_put("", "body");
  ok(respcache_get("", 5, NULL) == NULL, "empty key is not cached");
  respcache_put(NULL, "body");
  respcache_put("k", "body");
  ok(respcache_get("k", 0, NULL) == NULL, "ttl 0 is always a miss");
  ok(respcache_get("k", -3, NULL) == NULL, "negative ttl is always a miss");
  ok(respcache_get(NULL, 5, NULL) == NULL && respcache_get("", 5, NULL) == NULL, "NULL / empty key lookups miss");
  /* a good entry must survive a later failed refresh */
  respcache_put("k", "good");
  respcache_put("k", "");
  b = respcache_get("k", 5, NULL);
  ok(b && !strcmp(b, "good"), "a failed (empty) refresh does not overwrite a good cached body");
  free(b);

  printf("-- staleness\n");
  respcache_drop(NULL);
  respcache_put("s", "{}");
  usleep(1300 * 1000);
  ok(respcache_get("s", 1, &age) == NULL && age == 0, "entry older than ttl (1 s) is a miss");
  b = respcache_get("s", 60, &age);
  ok(b && age >= 1200, "the same entry under a longer ttl hits and reports its true age (>=1.2 s)");
  free(b);

  printf("-- replacement, eviction and drop\n");
  respcache_drop(NULL);
  respcache_put("a", "one"); respcache_put("a", "two");
  b = respcache_get("a", 5, NULL);
  ok(b && !strcmp(b, "two"), "put on an existing key replaces it");
  free(b);
  respcache_stats(NULL, NULL, &bytes);
  ok(bytes == 3, "replacement frees the old body (bytes held = 3, not 6)");
  respcache_drop(NULL);
  char k[16];
  for (int i = 0; i < 8; i++) { snprintf(k, sizeof k, "key%d", i); respcache_put(k, "x"); usleep(3000); }
  respcache_put("key8", "y");                                    /* ninth distinct key: evicts the oldest */
  ok(respcache_get("key0", 5, NULL) == NULL, "ninth key evicts the OLDEST entry (key0)");
  b = respcache_get("key1", 5, NULL); ok(b != NULL, "the next-oldest survives"); free(b);
  b = respcache_get("key8", 5, NULL); ok(b && !strcmp(b, "y"), "the new key is present"); free(b);
  respcache_drop("key1");
  ok(respcache_get("key1", 5, NULL) == NULL, "drop(key) removes one entry");
  b = respcache_get("key2", 5, NULL); ok(b != NULL, "drop(key) leaves the others"); free(b);
  respcache_drop(NULL);
  respcache_stats(NULL, NULL, &bytes);
  ok(bytes == 0 && respcache_get("key2", 5, NULL) == NULL, "drop(NULL) clears everything");
  respcache_drop("never-existed");

  printf("-- stats\n");
  respcache_stats(&h0, &m0, NULL);
  respcache_put("st", "z");
  b = respcache_get("st", 5, NULL); free(b);
  respcache_get("nope", 5, NULL);
  respcache_stats(&h1, &m1, &bytes);
  ok(h1 == h0 + 1 && m1 == m0 + 1 && bytes == 1, "hits, misses and bytes tracked");
  respcache_stats(NULL, NULL, NULL);

  printf("-- key length\n");
  {
    char longk[200]; memset(longk, 'k', sizeof longk - 1); longk[sizeof longk - 1] = 0;
    respcache_drop(NULL);
    respcache_put(longk, "long");
    b = respcache_get(longk, 5, NULL);
    ok(b && !strcmp(b, "long"), "an over-long key IS retrievable");
    free(b);
    char k2[200]; memcpy(k2, longk, sizeof k2); k2[100] = 'z';
    respcache_put(k2, "different");
    b = respcache_get(longk, 5, NULL);
    ok(b && !strcmp(b, "long"), "two keys sharing a 63-char prefix are not conflated");
    free(b);
    b = respcache_get(k2, 5, NULL);
    ok(b && !strcmp(b, "different"), "the second long key (differs at position 100) has its own body");
    respcache_put(longk, "long2");
    b = respcache_get(longk, 5, NULL);
    ok(b && !strcmp(b, "long2"), "re-putting a long key replaces in place (no slot burned)");
    free(b);
    respcache_drop(longk);
    b = respcache_get(longk, 5, NULL);
    ok(b == NULL, "drop of a long key removes it");
    free(b);
    b = respcache_get(k2, 5, NULL);
    ok(b && !strcmp(b, "different"), "...and only it");
    free(b);
  }

  respcache_drop(NULL);
  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
