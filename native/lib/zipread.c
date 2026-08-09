/* lib/zipread.c — see header. */
#include "zipread.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static unsigned rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned long rd32(const unsigned char *p) {
  return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
         ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* Hard ceiling on inflated output.
 *
 * `uncomp` comes from the ZIP central directory — a 32-bit field the FILE
 * chooses, not something we measured. Trusting it meant a 300-byte archive
 * claiming 4 GiB caused a 4 GiB malloc, and the growth path below doubled
 * without any limit, so a deflate bomb could grow until the allocator gave
 * up. That was tolerable while the only callers were known feeds (GDELT,
 * GTFS-JP); it is not tolerable now that uploaded bytes can reach a ZIP
 * parser. Override with JO_ZIP_MAX_OUT if a legitimate feed needs more. */
#define ZIP_DEFAULT_MAX_OUT (256UL * 1024UL * 1024UL)
static size_t zip_max_out(void) {
  const char *e = getenv("JO_ZIP_MAX_OUT");
  if (e && *e) {
    long long v = atoll(e);
    if (v >= 1024 && (unsigned long long)v <= 4096ULL * 1024 * 1024)
      return (size_t)v;
  }
  return ZIP_DEFAULT_MAX_OUT;
}

/* Raw-inflate `comp` bytes at `data` into a fresh buffer. `uncomp` is the
 * expected output size when known (0 = unknown → grow geometrically), and is
 * CLAMPED — it is attacker-controlled. Returns malloc'd NUL-terminated bytes,
 * *out_len = produced length, NULL on error or on exceeding the ceiling. */
static char *inflate_raw(const unsigned char *data, size_t comp,
                         size_t uncomp, size_t *out_len) {
  z_stream s; memset(&s, 0, sizeof s);
  if (inflateInit2(&s, -15) != Z_OK) return NULL;
  const size_t maxout = zip_max_out();
  if (uncomp > maxout) uncomp = maxout;   /* declared size is not evidence */
  size_t capacity = uncomp ? uncomp + 1 : (comp ? comp * 4 + 64 : 1024);
  if (capacity > maxout + 1) capacity = maxout + 1;
  char *o = malloc(capacity);
  if (!o) { inflateEnd(&s); return NULL; }
  s.next_in = (Bytef *)data;
  s.avail_in = (uInt)comp;
  s.next_out = (Bytef *)o;
  s.avail_out = (uInt)(capacity - 1);
  for (;;) {
    int rc = inflate(&s, Z_NO_FLUSH);
    if (rc == Z_STREAM_END) break;
    if (rc != Z_OK && rc != Z_BUF_ERROR) { free(o); inflateEnd(&s); return NULL; }
    if (s.avail_out == 0) {                              /* grow */
      size_t used = capacity - 1 - s.avail_out;
      if (capacity - 1 >= maxout) {   /* deflate bomb — refuse, don't OOM */
        free(o); inflateEnd(&s); return NULL;
      }
      size_t ncap = capacity * 2;
      if (ncap > maxout + 1) ncap = maxout + 1;
      char *no = realloc(o, ncap);
      if (!no) { free(o); inflateEnd(&s); return NULL; }
      o = no; capacity = ncap;
      s.next_out = (Bytef *)(o + used);
      s.avail_out = (uInt)(capacity - 1 - used);
    } else if (rc == Z_BUF_ERROR) {   /* no progress and not at stream end */
      free(o); inflateEnd(&s); return NULL;
    }
  }
  size_t total = capacity - 1 - s.avail_out;
  inflateEnd(&s);
  o[total] = 0;
  if (out_len) *out_len = total;
  return o;
}

/* basename after the last '/' (ZIP paths always use '/'). */
static const char *zbase(const char *p) {
  const char *s = strrchr(p, '/');
  return s ? s + 1 : p;
}

char *zip_find_entry(const char *buf, size_t len, const char *name,
                     size_t *out_len) {
  if (out_len) *out_len = 0;
  if (!buf || !name || len < 22) return NULL;
  const unsigned char *b = (const unsigned char *)buf;

  /* Locate the End Of Central Directory record: scan back from the tail for
   * its signature. The trailing comment is <=64K, so bound the scan. */
  size_t maxback = len < 66000 ? len : 66000;
  size_t eocd = 0; int found = 0;
  for (size_t back = 22; back <= maxback; back++) {
    size_t p = len - back;
    if (rd32(b + p) == 0x06054b50UL) { eocd = p; found = 1; break; }
  }
  if (!found) return NULL;

  unsigned nent = rd16(b + eocd + 10);
  unsigned long cdoff = rd32(b + eocd + 16);
  if (cdoff >= len) return NULL;

  size_t p = (size_t)cdoff;
  for (unsigned i = 0; i < nent; i++) {
    if (p + 46 > len || rd32(b + p) != 0x02014b50UL) return NULL;
    unsigned method = rd16(b + p + 10);
    unsigned long comp   = rd32(b + p + 20);
    unsigned long uncomp = rd32(b + p + 24);
    unsigned nlen = rd16(b + p + 28);
    unsigned elen = rd16(b + p + 30);
    unsigned clen = rd16(b + p + 32);
    unsigned long lho = rd32(b + p + 42);
    if (p + 46 + nlen > len) return NULL;

    char nbuf[512];
    size_t ncopy = nlen < sizeof nbuf - 1 ? nlen : sizeof nbuf - 1;
    memcpy(nbuf, b + p + 46, ncopy);
    nbuf[ncopy] = 0;

    if (strcmp(nbuf, name) == 0 || strcmp(zbase(nbuf), name) == 0) {
      /* Re-read the local header purely for its name/extra lengths — those
       * are the only fields there we can trust (see header note). */
      if (lho + 30 > len || rd32(b + lho) != 0x04034b50UL) return NULL;
      unsigned lnlen = rd16(b + lho + 26), lelen = rd16(b + lho + 28);
      size_t doff = (size_t)lho + 30 + lnlen + lelen;
      if (doff > len) return NULL;
      size_t avail = len - doff;
      if (comp > avail) comp = avail;
      const unsigned char *data = b + doff;

      if (method == 0) {                                  /* stored */
        size_t n = uncomp && uncomp <= avail ? (size_t)uncomp : avail;
        char *o = malloc(n + 1);
        if (!o) return NULL;
        memcpy(o, data, n); o[n] = 0;
        if (out_len) *out_len = n;
        return o;
      }
      if (method != 8) return NULL;                       /* deflate only */
      return inflate_raw(data, comp, (size_t)uncomp, out_len);
    }
    p += 46 + nlen + elen + clen;
  }
  return NULL;
}

char *zip_first_entry(const char *buf, size_t len, size_t *out_len) {
  if (out_len) *out_len = 0;
  if (!buf || len < 30) return NULL;
  const unsigned char *b = (const unsigned char *)buf;
  if (rd32(b) != 0x04034b50UL) return NULL;          /* "PK\3\4" */

  unsigned method = rd16(b + 8);
  unsigned long comp   = rd32(b + 18);
  unsigned long uncomp = rd32(b + 22);
  unsigned nlen = rd16(b + 26), elen = rd16(b + 28);
  size_t doff = 30 + (size_t)nlen + (size_t)elen;
  if (doff > len) return NULL;
  size_t avail = len - doff;
  if (comp == 0 && avail) comp = avail;              /* tolerate 0 (stored) */
  if (comp > avail) comp = avail;
  const unsigned char *data = b + doff;

  if (method == 0) {                                  /* stored */
    char *o = malloc(comp + 1);
    if (!o) return NULL;
    memcpy(o, data, comp); o[comp] = 0;
    if (out_len) *out_len = comp;
    return o;
  }
  if (method != 8) return NULL;                       /* only deflate else */

  /* Same inflate as zip_find_entry, and now literally the same code.
   *
   * This function used to carry its own copy of the loop WITHOUT the ceiling:
   * it trusted `uncomp` straight from the local header (a 32-bit field the
   * file chooses) for the initial malloc and then doubled without any limit,
   * so a deflate bomb reaching this entry point grew until the allocator gave
   * up — exactly the hole inflate_raw() was written to close, left open on the
   * sibling path. */
  return inflate_raw(data, comp, (size_t)uncomp, out_len);
}
