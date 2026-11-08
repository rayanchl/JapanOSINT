/* test_jo_codec.c — core/jo_codec.c reproduces the copies it replaced.
 *
 * 2026-11-07: base64url, hex-digit, URL-decode, query-string and SHA-256 hex
 * helpers were copied across ~25 core modules (counts in jo_codec.h). Each
 * REFERENCE below is the removed code verbatim (only renamed), and every
 * shared function is run against it on fixed edge cases plus a few thousand
 * pseudo-random inputs, so "byte-identical" is a measurement here, not a
 * claim. Where the copies differed in behaviour the flag that keeps each
 * caller's behaviour is pinned too:
 *   - cursor decoders are STRICT ('=' padding or any stray byte -> NULL);
 *     auth.c's JWT decoder SKIPS such bytes (JO_B64_SKIP_INVALID);
 *   - qs_get()/qs_param() never match a bare "?key"; qget() did
 *     (JO_QS_BARE_KEY); qs_param() copied raw, the others decode;
 *   - nearapi.c's qget reported truncation (jo_qs_get_tl). */
#include "../../core/jo_codec.c"
#include "core/jo_codec.h"
#include <openssl/sha.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- references: the removed copies, verbatim ------------------------- */
static char *ref_b64url(const char *in) {
  static const char T[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t len = strlen(in);
  char *out = malloc(((len + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    unsigned a = (unsigned char)in[i];
    unsigned b = i + 1 < len ? (unsigned char)in[i + 1] : 0;
    unsigned c = i + 2 < len ? (unsigned char)in[i + 2] : 0;
    unsigned v = (a << 16) | (b << 8) | c;
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    if (i + 1 < len) out[o++] = T[(v >> 6) & 63];
    if (i + 2 < len) out[o++] = T[v & 63];
  }
  out[o] = 0;
  return out;
}
static char *ref_b64url_decode(const char *in) {
  static signed char R[256];
  static int init = 0;
  if (!init) {
    for (int i = 0; i < 256; i++) R[i] = -1;
    const char *T =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 64; i++) R[(unsigned char)T[i]] = (signed char)i;
    init = 1;
  }
  size_t len = strlen(in);
  char *out = malloc(len / 4 * 3 + 4);
  if (!out) return NULL;
  size_t o = 0; unsigned v = 0; int bits = 0;
  for (size_t i = 0; i < len; i++) {
    signed char d = R[(unsigned char)in[i]];
    if (d < 0) { free(out); return NULL; }
    v = (v << 6) | (unsigned)d; bits += 6;
    if (bits >= 8) { bits -= 8; out[o++] = (char)((v >> bits) & 0xFF); }
  }
  out[o] = 0;
  return out;
}
static unsigned char *ref_auth_decode(const char *in, size_t in_len, size_t *out_len) {
  static const char *T =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  int rev[256]; memset(rev, -1, sizeof rev);
  for (int i = 0; i < 64; i++) rev[(unsigned char)T[i]] = i;
  size_t cap = (in_len / 4 + 1) * 3 + 3;
  unsigned char *out = malloc(cap);
  if (!out) return NULL;
  size_t o = 0; unsigned val = 0; int bits = -8;
  for (size_t i = 0; i < in_len; i++) {
    int c = rev[(unsigned char)in[i]];
    if (c < 0) continue;
    val = ((val << 6) | (unsigned)c) & 0xFFFFu; bits += 6;
    if (bits >= 0) { out[o++] = (unsigned char)((val >> bits) & 0xFF); bits -= 8; }
  }
  *out_len = o; return out;
}
static int ref_hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static int ref_url_decode(const char *s, size_t n, char *out, size_t cap) {   /* nearapi */
  size_t o = 0, i = 0;
  for (; i < n && o + 1 < cap; i++) {
    int c = (unsigned char) s[i];
    if (c == '+') { out[o++] = ' '; continue; }
    if (c == '%' && i + 2 < n) {
      int h = ref_hexv((unsigned char) s[i + 1]), l = ref_hexv((unsigned char) s[i + 2]);
      if (h >= 0 && l >= 0) { out[o++] = (char) ((h << 4) | l); i += 2; continue; }
    }
    out[o++] = (char) c;
  }
  out[o] = 0;
  return i < n;
}
static int ref_qs_param(const char *qs, const char *key, char *out, size_t cap) {  /* alertsapi */
  if (!out || cap == 0) return 0;
  out[0] = 0;
  if (!qs || !*qs || !key) return 0;
  size_t kl = strlen(key);
  for (const char *p = qs; *p; ) {
    const char *amp = strchr(p, '&');
    size_t seglen = amp ? (size_t)(amp - p) : strlen(p);
    if (seglen > kl && p[kl] == '=' && strncmp(p, key, kl) == 0) {
      size_t vl = seglen - kl - 1;
      if (vl >= cap) vl = cap - 1;
      memcpy(out, p + kl + 1, vl);
      out[vl] = 0;
      return 1;
    }
    if (!amp) break;
    p = amp + 1;
  }
  return 0;
}
static int ref_qs_get(const char *qs, const char *key, char *out, size_t n) {  /* timeline/upload */
  if (!out || n == 0) return 0;
  out[0] = 0;
  if (!qs || !*qs) return 0;
  size_t kl = strlen(key);
  const char *p = qs;
  for (;;) {
    const char *amp = strchr(p, '&');
    const char *end = amp ? amp : p + strlen(p);
    const char *eqp = memchr(p, '=', (size_t)(end - p));
    if (eqp && (size_t)(eqp - p) == kl && strncmp(p, key, kl) == 0) {
      size_t o = 0;
      for (const char *v = eqp + 1; v < end && o + 1 < n; v++) {
        if (*v == '+') { out[o++] = ' '; continue; }
        if (*v == '%' && v + 2 < end) {
          int h = ref_hexv((unsigned char)v[1]), l = ref_hexv((unsigned char)v[2]);
          if (h >= 0 && l >= 0) { out[o++] = (char)(h * 16 + l); v += 2; continue; }
        }
        out[o++] = *v;
      }
      out[o] = 0;
      return 1;
    }
    if (!amp) return 0;
    p = amp + 1;
  }
}
static int ref_qget(const char *query, const char *key, char *out, size_t cap,
                    int *too_long) {                                         /* nearapi */
  out[0] = 0;
  if (!query || !*query) return 0;
  size_t klen = strlen(key);
  const char *p = query;
  while (*p) {
    const char *amp = strchr(p, '&');
    const char *seg_end = amp ? amp : p + strlen(p);
    const char *eq = memchr(p, '=', (size_t) (seg_end - p));
    size_t kn = eq ? (size_t) (eq - p) : (size_t) (seg_end - p);
    if (kn == klen && memcmp(p, key, klen) == 0) {
      const char *vstart = eq ? eq + 1 : seg_end;
      if (ref_url_decode(vstart, (size_t) (seg_end - vstart), out, cap) && too_long)
        *too_long = 1;
      return 1;
    }
    if (!amp) break;
    p = amp + 1;
  }
  return 0;
}
static void ref_sha256_hex(const void *b, size_t n, char out[65]) {
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)b, n, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(out + i * 2, 3, "%02x", d[i]);
  out[64] = '\0';
}

/* ---- inputs ----------------------------------------------------------- */
static unsigned rng = 12345;
static unsigned rnd(void) { rng = rng * 1103515245u + 12345u; return (rng >> 16) & 0x7fff; }
static void rand_str(char *b, size_t n, const char *alpha) {
  size_t al = strlen(alpha);
  for (size_t i = 0; i < n; i++) b[i] = alpha[rnd() % al];
  b[n] = 0;
}

static void same_str(const char *a, const char *b) {
  assert((a == NULL) == (b == NULL));
  if (a) assert(strcmp(a, b) == 0);
}

int main(void) {
  const char *B64A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  const char *QSA  = "ab=&%+2F0x9Gz";
  int checked = 0;

  /* hexv: every byte value */
  for (int c = -1; c < 256; c++) assert(jo_hexv(c) == ref_hexv(c));

  /* b64url: fixed cases incl. the 0/1/2-remainder lengths and high bytes */
  const char *fixed[] = { "", "f", "fo", "foo", "foob", "fooba", "foobar",
                          "{\"ts\":\"2026-11-07T00:00:00.000Z\",\"uid\":\"a|b\"}",
                          "\xe6\x97\xa5\xe6\x9c\xac\xff\x80" };
  for (size_t i = 0; i < sizeof fixed / sizeof *fixed; i++) {
    char *a = jo_b64url_encode_str(fixed[i]), *b = ref_b64url(fixed[i]);
    same_str(a, b);
    char *da = jo_b64url_decode_str(a), *db = ref_b64url_decode(b);
    same_str(da, db); assert(strcmp(da, fixed[i]) == 0);     /* round trip */
    free(a); free(b); free(da); free(db); checked++;
  }
  { char *e = jo_b64url_encode_str("foobar"); assert(!strcmp(e, "Zm9vYmFy")); free(e);
    e = jo_b64url_encode_str("fo"); assert(!strcmp(e, "Zm8")); free(e);   /* no padding */ }

  /* strict vs skip: padding and stray bytes */
  const char *dirty[] = { "Zm8=", "Zm8", "Zm 8", "Zm9v\nYmFy", "Zm+8", "Zm/8", "*",
                          "", "A", "AA", "AAA", "____", "----" };
  for (size_t i = 0; i < sizeof dirty / sizeof *dirty; i++) {
    char *a = jo_b64url_decode_str(dirty[i]), *b = ref_b64url_decode(dirty[i]);
    same_str(a, b); free(a); free(b);
    size_t la = 0, lb = 0;
    unsigned char *x = jo_b64url_decode(dirty[i], strlen(dirty[i]), &la, JO_B64_SKIP_INVALID);
    unsigned char *y = ref_auth_decode(dirty[i], strlen(dirty[i]), &lb);
    assert(x && y && la == lb && memcmp(x, y, la) == 0);
    free(x); free(y); checked++;
  }
  { char *s = jo_b64url_decode_str("Zm8="); assert(s == NULL);            /* strict */
    size_t l = 0; unsigned char *u = jo_b64url_decode("Zm8=", 4, &l, JO_B64_SKIP_INVALID);
    assert(u && l == 2 && !memcmp(u, "fo", 2)); free(u); }

  /* fuzz: random strings over the alphabet (and with stray bytes) */
  for (int r = 0; r < 3000; r++) {
    char in[80]; size_t n = rnd() % 70;
    rand_str(in, n, r % 3 ? B64A : "ABCxyz09-_=+/ .");
    char *a = jo_b64url_decode_str(in), *b = ref_b64url_decode(in);
    if (a || b) { assert(a && b && strcmp(a, b) == 0); }
    free(a); free(b);
    size_t la = 0, lb = 0;
    unsigned char *x = jo_b64url_decode(in, n, &la, JO_B64_SKIP_INVALID);
    unsigned char *y = ref_auth_decode(in, n, &lb);
    assert(la == lb && memcmp(x, y, la) == 0);
    free(x); free(y);
    char raw[80]; size_t rn = rnd() % 60;
    for (size_t i = 0; i < rn; i++) raw[i] = (char)(1 + rnd() % 255);
    raw[rn] = 0;
    char *e1 = jo_b64url_encode_str(raw), *e2 = ref_b64url(raw);
    same_str(e1, e2); free(e1); free(e2);
    checked++;
  }

  /* url_decode + query strings, fixed and fuzzed, at several caps */
  const char *qss[] = { "a=1&b=2", "b=2&a=1", "xa=1&a=2", "a=&a=3", "a", "a&b=1",
                        "&a=1", "a=1&", "a=%41%4", "a=%zz+x", "a=%2Fq%2f", "", "==&a=b=c",
                        "a=+++&a=x" };
  const char *keys[] = { "a", "b", "xa", "", "c" };
  size_t caps[] = { 1, 2, 3, 8, 64 };
  for (size_t qi = 0; qi < sizeof qss / sizeof *qss + 3000; qi++) {
    char qbuf[64];
    const char *qs;
    if (qi < sizeof qss / sizeof *qss) qs = qss[qi];
    else { rand_str(qbuf, rnd() % 40, QSA); qs = qbuf; }
    for (size_t ki = 0; ki < sizeof keys / sizeof *keys; ki++)
      for (size_t ci = 0; ci < sizeof caps / sizeof *caps; ci++) {
        size_t cap = caps[ci];
        char o1[64], o2[64]; int t1 = 0, t2 = 0;
        int r1 = jo_qs_get(qs, keys[ki], o1, cap, 0);
        int r2 = ref_qs_param(qs, keys[ki], o2, cap);
        assert(r1 == r2 && !strcmp(o1, o2));
        r1 = jo_qs_get(qs, keys[ki], o1, cap, JO_QS_DECODE);
        r2 = ref_qs_get(qs, keys[ki], o2, cap);
        assert(r1 == r2 && !strcmp(o1, o2));
        r1 = jo_qs_get_tl(qs, keys[ki], o1, cap, JO_QS_DECODE | JO_QS_BARE_KEY, &t1);
        r2 = ref_qget(qs, keys[ki], o2, cap, &t2);
        assert(r1 == r2 && !strcmp(o1, o2) && t1 == t2);
        size_t n = strlen(qs);
        r1 = jo_url_decode(qs, n, o1, cap);
        r2 = ref_url_decode(qs, n, o2, cap);
        assert(r1 == r2 && !strcmp(o1, o2));
      }
    checked++;
  }
  /* the flags, spelled out */
  { char o[16];
    assert(jo_qs_get("flag&a=1", "flag", o, sizeof o, JO_QS_DECODE) == 0);
    assert(jo_qs_get("flag&a=1", "flag", o, sizeof o, JO_QS_DECODE | JO_QS_BARE_KEY) == 1 && !o[0]);
    assert(jo_qs_get("a=x%41", "a", o, sizeof o, 0) == 1 && !strcmp(o, "x%41"));
    assert(jo_qs_get("a=x%41", "a", o, sizeof o, JO_QS_DECODE) == 1 && !strcmp(o, "xA"));
    int t = 0;
    assert(jo_qs_get_tl("a=abcdef", "a", o, 4, JO_QS_DECODE, &t) == 1 && t == 1 && !strcmp(o, "abc")); }

  /* mongoose-backed lookup keeps each caller's tri-state */
  { char o[8]; int tl = 0;
    assert(jo_qs_get_mg("q=abc", "q", o, sizeof o, &tl) == 1 && !strcmp(o, "abc") && !tl);
    assert(jo_qs_get_mg("q=", "q", o, sizeof o, &tl) == 0 && !o[0] && !tl);
    assert(jo_qs_get_mg("x=1", "q", o, sizeof o, &tl) == 0 && !o[0] && !tl);
    assert(jo_qs_get_mg("q=abcdefghij", "q", o, sizeof o, &tl) == -1 && !o[0] && tl == 1);
    assert(jo_qs_get_mg(NULL, "q", o, sizeof o, NULL) == 0); }

  /* sha256 hex */
  for (int r = 0; r < 200; r++) {
    char in[100]; size_t n = rnd() % 99;
    for (size_t i = 0; i < n; i++) in[i] = (char)rnd();
    char a[65], b[65];
    jo_sha256_hex(in, n, a); ref_sha256_hex(in, n, b);
    assert(!strcmp(a, b) && strlen(a) == 64);
  }
  { char a[65]; jo_sha256_hex("", 0, a);
    assert(!strcmp(a, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")); }

  printf("test_jo_codec: %d input groups identical to the removed copies; all passed\n", checked);
  return 0;
}
