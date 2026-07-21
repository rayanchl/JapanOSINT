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

  /* zlib raw inflate (windowBits -15 = no zlib/gzip wrapper, as in ZIP). */
  z_stream s; memset(&s, 0, sizeof s);
  if (inflateInit2(&s, -15) != Z_OK) return NULL;
  size_t capacity = uncomp ? uncomp + 1 : (comp ? comp * 4 + 64 : 1024);
  char *o = malloc(capacity);
  if (!o) { inflateEnd(&s); return NULL; }
  s.next_in = (Bytef *)data;
  s.avail_in = (uInt)comp;
  s.next_out = (Bytef *)o;
  s.avail_out = (uInt)(capacity - 1);
  int rc;
  for (;;) {
    rc = inflate(&s, Z_NO_FLUSH);
    if (rc == Z_STREAM_END) break;
    if (rc != Z_OK && rc != Z_BUF_ERROR) { free(o); inflateEnd(&s); return NULL; }
    if (s.avail_out == 0) {                            /* grow */
      size_t used = capacity - 1 - s.avail_out;        /* == capacity-1 */
      size_t ncap = capacity * 2;
      char *no = realloc(o, ncap);
      if (!no) { free(o); inflateEnd(&s); return NULL; }
      o = no; capacity = ncap;
      s.next_out = (Bytef *)(o + used);
      s.avail_out = (uInt)(capacity - 1 - used);
    } else if (rc == Z_BUF_ERROR) {                    /* no progress & not end */
      free(o); inflateEnd(&s); return NULL;
    }
  }
  size_t total = capacity - 1 - s.avail_out;
  inflateEnd(&s);
  o[total] = 0;
  if (out_len) *out_len = total;
  return o;
}
