/* core/jo_codec.c — see jo_codec.h. Deliberately free of mongoose (only
 * openssl's SHA-256): `make authtest` links core/auth.c with a hand-picked
 * object list, and jo_qs_get_mg() — the one mongoose-backed helper — lives
 * in jo_codec_mg.c so this file can join that list on its own. */
#include "jo_codec.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int jo_hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static const char B64URL[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* The reverse of B64URL, computed rather than looked up: there is no table to
 * initialise, so no first-use race (the copies this replaces filled a static
 * table behind an unsynchronised flag, on routes that run on worker threads). */
static int b64url_val(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '-') return 62;
  if (c == '_') return 63;
  return -1;
}

char *jo_b64url_encode(const void *inv, size_t len) {
  const unsigned char *in = (const unsigned char *)inv;
  char *out = malloc(((len + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    unsigned a = in[i];
    unsigned b = i + 1 < len ? in[i + 1] : 0;
    unsigned c = i + 2 < len ? in[i + 2] : 0;
    unsigned v = (a << 16) | (b << 8) | c;
    out[o++] = B64URL[(v >> 18) & 63];
    out[o++] = B64URL[(v >> 12) & 63];
    if (i + 1 < len) out[o++] = B64URL[(v >> 6) & 63];
    if (i + 2 < len) out[o++] = B64URL[v & 63];
  }
  out[o] = 0;
  return out;
}

char *jo_b64url_encode_str(const char *in) {
  return jo_b64url_encode(in, in ? strlen(in) : 0);
}

unsigned char *jo_b64url_decode(const char *in, size_t len, size_t *out_len,
                                int flags) {
  if (out_len) *out_len = 0;
  if (!in) return NULL;
  /* every 4 input bytes make at most 3; +3 covers a partial quad, +1 the NUL */
  unsigned char *out = malloc(len / 4 * 3 + 4);
  if (!out) return NULL;
  /* `val` is UNSIGNED and masked to the 16 bits the loop can still need (as
   * a signed int it overflowed on the third sextet; auth.c runs this on the
   * Authorization header of every request, before any authentication). */
  size_t o = 0; unsigned val = 0; int bits = 0;
  for (size_t i = 0; i < len; i++) {
    int d = b64url_val((unsigned char)in[i]);
    if (d < 0) {
      if (flags & JO_B64_SKIP_INVALID) continue;
      free(out); return NULL;
    }
    val = ((val << 6) | (unsigned)d) & 0xFFFFu; bits += 6;
    if (bits >= 8) { bits -= 8; out[o++] = (unsigned char)((val >> bits) & 0xFF); }
  }
  out[o] = 0;
  if (out_len) *out_len = o;
  return out;
}

char *jo_b64url_decode_str(const char *in) {
  if (!in) return NULL;
  return (char *)jo_b64url_decode(in, strlen(in), NULL, 0);
}

int jo_url_decode(const char *s, size_t n, char *out, size_t cap) {
  if (!out || cap == 0) return n > 0;
  size_t o = 0, i = 0;
  for (; i < n && o + 1 < cap; i++) {
    int c = (unsigned char)s[i];
    if (c == '+') { out[o++] = ' '; continue; }
    if (c == '%' && i + 2 < n) {
      int h = jo_hexv((unsigned char)s[i + 1]), l = jo_hexv((unsigned char)s[i + 2]);
      if (h >= 0 && l >= 0) { out[o++] = (char)((h << 4) | l); i += 2; continue; }
    }
    out[o++] = (char)c;
  }
  out[o] = 0;
  return i < n;
}

int jo_qs_get_tl(const char *qs, const char *key, char *out, size_t cap,
                 int flags, int *truncated) {
  if (!out || cap == 0) return 0;
  out[0] = 0;
  if (!qs || !*qs || !key) return 0;
  size_t kl = strlen(key);
  for (const char *p = qs; *p;) {
    const char *amp = strchr(p, '&');
    const char *end = amp ? amp : p + strlen(p);
    const char *eqp = memchr(p, '=', (size_t)(end - p));
    size_t kn = eqp ? (size_t)(eqp - p)
                    : ((flags & JO_QS_BARE_KEY) ? (size_t)(end - p) : (size_t)-1);
    if (kn == kl && strncmp(p, key, kl) == 0) {
      const char *v = eqp ? eqp + 1 : end;
      size_t vl = (size_t)(end - v);
      int cut;
      if (flags & JO_QS_DECODE) cut = jo_url_decode(v, vl, out, cap);
      else {
        cut = vl >= cap;
        if (cut) vl = cap - 1;
        memcpy(out, v, vl);
        out[vl] = 0;
      }
      if (cut && truncated) *truncated = 1;
      return 1;
    }
    if (!amp) return 0;
    p = amp + 1;
  }
  return 0;
}

int jo_qs_get(const char *qs, const char *key, char *out, size_t cap,
              int flags) {
  return jo_qs_get_tl(qs, key, out, cap, flags, NULL);
}

void jo_sha256_hex(const void *b, size_t n, char out[65]) {
  static const char H[] = "0123456789abcdef";
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)b, n, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    out[i * 2]     = H[d[i] >> 4];
    out[i * 2 + 1] = H[d[i] & 15];
  }
  out[64] = '\0';
}
