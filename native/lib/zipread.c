/* lib/zipread.c — see header. */
#include <stdio.h>
#include "zipread.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static unsigned rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned long rd32(const unsigned char *p) {
  return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
         ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static unsigned long long rd64(const unsigned char *p) {
  return (unsigned long long)rd32(p) | ((unsigned long long)rd32(p + 4) << 32);
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
  /* Nor does it get to choose the up-front allocation: deflate cannot expand
   * past ~1032:1, so a 300-byte member claiming 256 MB used to cost a 256 MB
   * malloc per call (x 19,165 members in a zip_all_entries read). The buffer
   * still grows, so an honest member larger than this bound loses nothing. */
  if (comp < ((size_t)-1) / 1100) {
    size_t plausible = comp * 1100 + 1024;
    if (uncomp > plausible) uncomp = plausible;
  }
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

/* Locate the End Of Central Directory record: scan back from the tail for
 * its signature. The trailing comment is <=64K, so bound the scan. A candidate
 * whose declared comment length ends exactly at the end of the file is the
 * record; failing that, the last one whose comment at least fits. (A hostile
 * comment can itself contain "PK\5\6"; its own length field then rarely lands
 * on the end of the file.) Returns 1 and sets *eocd, or 0. */
static int zip_find_eocd(const unsigned char *b, size_t len, size_t *eocd) {
  if (len < 22) return 0;
  size_t maxback = len < 66000 ? len : 66000;
  int have_loose = 0; size_t loose = 0;
  for (size_t back = 22; back <= maxback; back++) {
    size_t p = len - back;
    if (rd32(b + p) != 0x06054b50UL) continue;
    size_t end = p + 22 + rd16(b + p + 20);
    if (end == len) { *eocd = p; return 1; }
    if (end < len && !have_loose) { have_loose = 1; loose = p; }
  }
  if (have_loose) { *eocd = loose; return 1; }
  return 0;
}

/* Where the central directory is and how many entries it declares, read from
 * the EOCD and — when the archive is ZIP64 — from the ZIP64 EOCD record the
 * locator points at. The 16-bit count (and the 32-bit offset and size) of a
 * plain EOCD saturate at 0xFFFF / 0xFFFFFFFF, and entries past 65,535 used to
 * be silently unreachable. */
typedef struct {
  size_t cd_off, cd_end;            /* central directory [cd_off, cd_end) */
  unsigned long long declared;      /* entries the archive says it holds */
  int zip64;
} zip_cd;

static int zip_open_cd(const unsigned char *b, size_t len, zip_cd *cd) {
  size_t eocd = 0;
  memset(cd, 0, sizeof *cd);
  if (!zip_find_eocd(b, len, &eocd)) return 0;
  unsigned long long nent = rd16(b + eocd + 10);
  unsigned long long cdsize = rd32(b + eocd + 12);
  unsigned long long cdoff = rd32(b + eocd + 16);
  int z64 = nent == 0xFFFF || cdsize == 0xFFFFFFFFULL || cdoff == 0xFFFFFFFFULL;
  if (eocd >= 20 && rd32(b + eocd - 20) == 0x07064b50UL) {
    unsigned long long zoff = rd64(b + eocd - 20 + 8);
    if (zoff < len && len - zoff >= 56 && rd32(b + zoff) == 0x06064b50UL) {
      nent = rd64(b + zoff + 32);
      cdsize = rd64(b + zoff + 40);
      cdoff = rd64(b + zoff + 48);
      z64 = 1;
    }
  }
  if (cdoff >= len) return 0;
  cd->zip64 = z64;
  cd->declared = nent;
  cd->cd_off = (size_t)cdoff;
  cd->cd_end = (cdsize <= len - cdoff) ? (size_t)(cdoff + cdsize) : len;
  return 1;
}

int zip_entry_count(const char *buf, size_t len) {
  zip_cd cd;
  if (!buf || !zip_open_cd((const unsigned char *)buf, len, &cd)) return -1;
  return cd.declared > 0x7fffffffULL ? 0x7fffffff : (int)cd.declared;
}

/* One central-directory entry at `p`: its name, and its sizes and local-header
 * offset with ZIP64's 0xFFFFFFFF placeholders resolved from the extra field.
 * Returns the entry's total length in the directory (the advance), or 0 when
 * the record is malformed and the walk cannot continue. *usable is cleared
 * when the entry is well framed but its numbers cannot be known (a ZIP64
 * placeholder with no ZIP64 extra) — the caller reports it unreadable. */
typedef struct {
  unsigned method;
  unsigned long long comp, uncomp, lho;
  char name[512];
  int usable;
} zip_ent;

static size_t zip_read_ent(const unsigned char *b, size_t len, size_t p,
                           size_t cd_end, zip_ent *e) {
  if (p + 46 > len || p + 46 > cd_end || rd32(b + p) != 0x02014b50UL) return 0;
  unsigned nlen = rd16(b + p + 28), elen = rd16(b + p + 30), clen = rd16(b + p + 32);
  if (p + 46 + nlen > len) return 0;
  e->method = rd16(b + p + 10);
  e->comp = rd32(b + p + 20);
  e->uncomp = rd32(b + p + 24);
  e->lho = rd32(b + p + 42);
  e->usable = 1;
  size_t ncopy = nlen < sizeof e->name - 1 ? nlen : sizeof e->name - 1;
  memcpy(e->name, b + p + 46, ncopy);
  e->name[ncopy] = 0;
  if (e->uncomp == 0xFFFFFFFFULL || e->comp == 0xFFFFFFFFULL || e->lho == 0xFFFFFFFFULL) {
    int need_u = e->uncomp == 0xFFFFFFFFULL, need_c = e->comp == 0xFFFFFFFFULL,
        need_l = e->lho == 0xFFFFFFFFULL, ok = 0;
    size_t x = p + 46 + nlen, xend = x + elen;
    if (xend > len) xend = len;
    while (x + 4 <= xend) {
      unsigned id = rd16(b + x), sz = rd16(b + x + 2);
      size_t d = x + 4;
      if (d + sz > xend) break;
      if (id == 0x0001) {
        size_t o = d;
        ok = 1;
        if (need_u) { if (o + 8 <= d + sz) { e->uncomp = rd64(b + o); o += 8; } else ok = 0; }
        if (need_c) { if (o + 8 <= d + sz) { e->comp = rd64(b + o); o += 8; } else ok = 0; }
        if (need_l) { if (o + 8 <= d + sz) { e->lho = rd64(b + o); o += 8; } else ok = 0; }
        break;
      }
      x = d + sz;
    }
    if (!ok) e->usable = 0;
  }
  return 46 + (size_t)nlen + elen + clen;
}

/* The member's bytes, given its central-directory numbers. NULL when it is
 * truncated, unsupported or will not inflate. */
static char *zip_read_member(const unsigned char *b, size_t len, const zip_ent *e,
                             size_t *out_len) {
  if (out_len) *out_len = 0;
  if (!e->usable) return NULL;
  unsigned long long lho = e->lho;
  if (lho > len || len - lho < 30 || rd32(b + lho) != 0x04034b50UL) return NULL;
  unsigned lnlen = rd16(b + lho + 26), lelen = rd16(b + lho + 28);
  size_t doff = (size_t)lho + 30 + lnlen + lelen;
  if (doff > len) return NULL;
  size_t avail = len - doff;
  unsigned long long comp = e->comp, uncomp = e->uncomp;
  if (comp > avail) comp = avail;
  const unsigned char *data = b + doff;

  if (e->method == 0) {                                   /* stored */
    /* The size the directory declares, never "the rest of the file": a stored
     * empty member used to come back as everything after it, and a member cut
     * off by a truncated download came back short with rc success. Both are
     * unreadable, not partial. */
    unsigned long long want = uncomp ? uncomp : comp;
    if (want > avail) return NULL;
    size_t n = (size_t)want;
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, data, n); o[n] = 0;
    if (out_len) *out_len = n;
    return o;
  }
  if (e->method != 8) return NULL;                        /* deflate only */
  return inflate_raw(data, (size_t)comp, uncomp > (unsigned long long)((size_t)-1) ? 0 : (size_t)uncomp, out_len);
}

/* The central-directory walk both named and indexed lookups need. `name`
 * non-NULL selects by path or basename; NULL selects the `index`-th entry in
 * central-directory order. `name_out` (when given) receives the entry's own
 * path, which is the only way an indexed caller learns what it just read.
 *
 * One copy on purpose: an indexed walk written beside the named one is two
 * readings of the same bytes that drift apart, which is the mistake this tree
 * has already paid for in its manifest parsers. */
static char *zip_cd_entry(const char *buf, size_t len, const char *name,
                          int index, char *name_out, size_t name_n,
                          size_t *out_len) {
  if (out_len) *out_len = 0;
  if (name_out && name_n) name_out[0] = 0;
  if (!buf || len < 22) return NULL;
  if (!name && index < 0) return NULL;
  const unsigned char *b = (const unsigned char *)buf;
  zip_cd cd;
  if (!zip_open_cd(b, len, &cd)) return NULL;

  size_t p = cd.cd_off;
  zip_ent e;
  for (long long i = 0; ; i++) {
    size_t adv = zip_read_ent(b, len, p, cd.cd_end, &e);
    if (!adv) return NULL;
    int hit = name ? (strcmp(e.name, name) == 0 || strcmp(zbase(e.name), name) == 0)
                   : (i == (long long)index);
    if (hit) {
      if (name_out && name_n) snprintf(name_out, name_n, "%s", e.name);
      return zip_read_member(b, len, &e, out_len);
    }
    p += adv;
  }
}

/* Iterator: ONE pass over the central directory for a caller that reads every
 * member. zip_entry_at(i) restarts the walk from entry 0, so reading N members
 * through it is O(N^2) in directory entries (19,165 members: 183 million
 * record parses). */
int zip_iter_open(zip_iter *it, const char *buf, size_t len) {
  memset(it, 0, sizeof *it);
  if (!buf || len < 22) return -1;
  zip_cd cd;
  if (!zip_open_cd((const unsigned char *)buf, len, &cd)) return -1;
  it->buf = buf; it->len = len;
  it->p = cd.cd_off; it->cd_end = cd.cd_end;
  it->declared = cd.declared;
  it->zip64 = cd.zip64;
  return 0;
}

int zip_iter_next(zip_iter *it, char *name_out, size_t name_n,
                  char **data, size_t *out_len) {
  if (data) *data = NULL;
  if (out_len) *out_len = 0;
  if (name_out && name_n) name_out[0] = 0;
  if (!it || !it->buf || it->done) return 0;
  const unsigned char *b = (const unsigned char *)it->buf;
  zip_ent e;
  size_t adv = zip_read_ent(b, it->len, it->p, it->cd_end, &e);
  if (!adv) { it->done = 1; return 0; }      /* end of the directory, or not walkable */
  it->p += adv;
  it->walked++;
  if (name_out && name_n) snprintf(name_out, name_n, "%s", e.name);
  size_t n = 0;
  char *o = zip_read_member(b, it->len, &e, &n);
  if (!o) return -1;
  if (data) *data = o; else free(o);
  if (out_len) *out_len = n;
  return 1;
}

char *zip_find_entry(const char *buf, size_t len, const char *name,
                     size_t *out_len) {
  if (!name) { if (out_len) *out_len = 0; return NULL; }
  return zip_cd_entry(buf, len, name, -1, NULL, 0, out_len);
}

char *zip_entry_at(const char *buf, size_t len, int index,
                   char *name_out, size_t name_n, size_t *out_len) {
  return zip_cd_entry(buf, len, NULL, index, name_out, name_n, out_len);
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
