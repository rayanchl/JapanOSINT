/* tests/unit/test_utf8.c — lib/utf8.h, the one bounded UTF-8 decoder.
 *
 * Its contract (stated in the header): *adv is ALWAYS >= 1 and never more than
 * the bytes actually present before the NUL, so no caller can spin or step
 * over the terminator; a malformed/truncated sequence decodes as the raw lead
 * byte with adv=1. The cases that matter are the truncations: the decoder is
 * given an EXACTLY-SIZED heap string so ASan sees any read past the NUL. */
#include "../../lib/utf8.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

/* decode one codepoint from an exactly-sized heap copy of `bytes` */
static unsigned dec(const char *bytes, size_t n, int *adv) {
  unsigned char *h = malloc(n + 1);
  memcpy(h, bytes, n); h[n] = 0;
  unsigned cp = utf8_decode(h, adv);
  free(h);
  return cp;
}

int main(void) {
  int a;
  printf("-- well-formed\n");
  ok(dec("A", 1, &a) == 'A' && a == 1, "ASCII");
  ok(dec("\xc3\xa9", 2, &a) == 0xE9 && a == 2, "2-byte U+00E9");
  ok(dec("\xe6\x9d\xb1", 3, &a) == 0x6771 && a == 3, "3-byte U+6771 (東)");
  ok(dec("\xf0\x9f\x98\x80", 4, &a) == 0x1F600 && a == 4, "4-byte U+1F600");
  ok(dec("\xef\xbf\xbd", 3, &a) == 0xFFFD && a == 3, "U+FFFD itself decodes");
  ok(dec("\x7f", 1, &a) == 0x7f && a == 1, "DEL");
  ok(dec("", 0, &a) == 0 && a == 1, "empty string: NUL decodes as 0 with adv 1");

  printf("-- truncated sequences stay inside the allocation\n");
  ok(dec("\xf0", 1, &a) == 0xF0 && a == 1, "lone 4-byte lead");
  ok(dec("\xf0\x9f", 2, &a) == 0xF0 && a == 1, "4-byte lead + 1 continuation");
  ok(dec("\xf0\x9f\x98", 3, &a) == 0xF0 && a == 1, "4-byte lead + 2 continuations");
  ok(dec("\xe6\x9d", 2, &a) == 0xE6 && a == 1, "3-byte lead + 1 continuation");
  ok(dec("\xe6", 1, &a) == 0xE6 && a == 1, "lone 3-byte lead");
  ok(dec("\xc3", 1, &a) == 0xC3 && a == 1, "lone 2-byte lead");

  printf("-- malformed\n");
  ok(dec("\x80", 1, &a) == 0x80 && a == 1, "stray continuation byte");
  ok(dec("\xbf", 1, &a) == 0xBF && a == 1, "stray continuation byte 0xBF");
  ok(dec("\xf8\x88\x80\x80\x80", 5, &a) == 0xF8 && a == 1, "5-byte (invalid) lead");
  ok(dec("\xff", 1, &a) == 0xFF && a == 1, "0xFF");
  ok(dec("\xc3\x28", 2, &a) == 0xC3 && a == 1, "lead followed by a non-continuation");
  ok(dec("\xe6\x9dX", 3, &a) == 0xE6 && a == 1, "3-byte lead, bad third byte");
  /* documented: overlongs and surrogates are decoded as written */
  ok(dec("\xc0\x80", 2, &a) == 0 && a == 2, "overlong NUL (C0 80) decodes as 0 with adv 2, as documented");
  ok(dec("\xed\xa0\x80", 3, &a) == 0xD800 && a == 3, "surrogate half decoded as written, as documented");

  printf("-- walk every prefix and every byte value without spinning or overrunning\n");
  {
    const char *s = "a\xc3\xa9\xe6\x9d\xb1\xf0\x9f\x98\x80z";
    size_t n = strlen(s);
    for (size_t cut = 0; cut <= n; cut++) {
      unsigned char *h = malloc(cut + 1); memcpy(h, s, cut); h[cut] = 0;
      size_t i = 0; int steps = 0;
      while (h[i]) {
        int adv; utf8_decode(h + i, &adv);
        if (adv < 1 || i + (size_t)adv > cut) { ok(0, "advance stays inside the string"); free(h); goto done; }
        i += (size_t)adv; steps++;
      }
      free(h);
    }
    ok(1, "decoding every truncation of a mixed string always terminates inside the buffer");
    for (int b = 0; b < 256; b++) {
      for (int tail = 0; tail < 3; tail++) {
        unsigned char h[8]; memset(h, 0x80, sizeof h);
        h[0] = (unsigned char)b; h[1 + tail] = 0;
        int adv; utf8_decode(h, &adv);
        if (adv < 1 || (h[0] && (size_t)adv > 1 + (size_t)tail)) { ok(0, "lead byte + short tail"); goto done; }
      }
    }
    ok(1, "all 256 lead bytes with 0-2 continuation bytes then NUL never advance past the NUL");
  }
done:
  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
