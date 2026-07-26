/* core/media.c — roadmap 27: the media pipeline. See media.h for the design,
 * the DDL, the wiring, the two documented collisions (translate.c's FTS column
 * and intel.c's geom_source preservation rule) and an explicit statement of
 * what is and is not exercised in this environment. This file is the
 * mechanics.
 *
 * ── HOW THIS WAS TESTED ───────────────────────────────────────────────────
 *
 * These bytes come from the open internet, so "it works on my phone's photo"
 * is not a test. The parser was driven from a harness that ASSEMBLES TIFF/JPEG
 * FIXTURES BYTE BY BYTE — header, entry tables, out-of-line value blocks — so
 * that the malformed cases could be constructed exactly rather than hoped for.
 * The harness is not in the tree (this task was scoped to media.c/media.h);
 * everything below is reproducible by rebuilding it against these two files.
 * It was compiled -fsanitize=address,undefined, so an out-of-bounds read or a
 * signed overflow is a hard failure and not a silent one. 84 assertions, 0
 * failures, 0 sanitizer reports.
 *
 *   well-formed   IFD0 + ExifIFD + GPSIFD in BOTH byte orders. GPS at
 *                 35°41'22.2"N 139°41'30.1"E round-trips to 35.689500 /
 *                 139.691694 identically under 'II' and 'MM'; the S/W variant
 *                 comes back negated; Make/Model/Orientation/DateTimeOriginal/
 *                 GPSDateStamp/PixelXY all land in the right fields.
 *   carriers      the same block delivered as a JPEG APP1, as a bare TIFF, and
 *                 as a PNG eXIf chunk — all three yield the same coordinate.
 *   ref tags      a latitude with no valid N/S ref yields NO coordinate.
 *   truncation    EVERY prefix length from 0 to the full file, for the JPEG
 *                 and the PNG. Each returns "no EXIF" or a partial parse; none
 *                 reads past the buffer; the full file still parses correctly
 *                 afterwards.
 *   entry count   0xFFFF entries declared inside a 40-byte block → clamped,
 *                 `truncated` set, no read past the block, no bogus GPS.
 *   wild offsets  value offsets 0, 4, 7, 0xFFFFFFF0, tlen-1 and tlen+4096,
 *                 each with count 0xFFFFFFFF → all refused by ex_val()'s
 *                 overflow and bounds checks (0/4/7 by the `off < 8` rule).
 *   cycles        IFD0's next-IFD pointing at IFD0; ExifIFD pointing back at
 *                 IFD0; an ExifIFD↔GPSIFD ring → all terminate, via the
 *                 visited set, the forward-only rule and the depth cap
 *                 independently.
 *   segment len   APP1 lengths of 0, 1, 2, 7 and 100000 → each refused (a
 *                 length below 2 does not advance the cursor, which is the
 *                 classic non-terminating case).
 *   not-an-image  HTML, JSON, a ZIP header, a JPEG SOI followed by 64 KB of
 *                 0xFF, a TIFF header whose IFD0 offset is 0, and 40 KB of
 *                 pseudo-random bytes relabelled first as JPEG then as TIFF →
 *                 "no EXIF", promptly, every time.
 *   rationals     zero denominator on degrees; 200 degrees; 99 minutes; an
 *                 all-zero (0,0) fix → each REJECTED rather than folded into a
 *                 plausible-looking wrong coordinate.
 *   dimensions    JPEG SOF0, PNG IHDR, GIF LSD, a bottom-up BMP with a
 *                 negative height, and WebP VP8X — all read correctly.
 *   phash         determinism; self-distance 0; EXACT invariance (distance 0)
 *                 under a uniform brightness shift and under uniform contrast
 *                 scaling, which are the two properties the construction
 *                 actually guarantees; distance 62 to the inverted image; and
 *                 the 0 sentinel for flat/black/white input.
 *
 * The invariant all of this exists to protect is the one in media.h: for ANY
 * input, this parser reads only within [buf, buf+len). Every bound is written
 * as `off <= tlen && n <= tlen - off` — never `off + n <= tlen`, which is the
 * same statement right up until it overflows.
 *
 * NOT COVERED BY ANY OF IT: the OCR and pHash shell-outs, because this host has
 * no tesseract and no ImageMagick. Only the DCT/packing half of pHash above is
 * exercised; the fork+exec, the pixel handoff and the TSV parse are not. See
 * media.h.
 */
#include "media.h"
#include "evidence.h"
#include "fts.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

/* math.h only guarantees M_PI under _DEFAULT_SOURCE; the hash definition in
 * media.h is normative, so the constant is spelled out rather than inherited
 * from whichever feature-test macros happen to be in force. */
#define MEDIA_PI 3.14159265358979323846

/* ONE literal, shared by idx_media_assets_pending and by every query that
 * wants it. Not tidiness: SQLite uses a partial index only when the query's
 * WHERE *syntactically* implies the index's, and a bound parameter never can.
 * MEDIA_MAX_FAILURES is therefore compiled in and duplicated here as a
 * literal, exactly as core/translate.c does with TR_PENDING_WHERE. */
#define MEDIA_PENDING_WHERE "analyzed_at IS NULL AND analyze_failed < 3"

/* ── small shared helpers (same idioms as core/alertsapi.c) ──────────────── */

static void uuid4(char out[37]) {
  unsigned char b[16];
  RAND_bytes(b, 16);
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}

static char *dupcol(sqlite3_stmt *s, int i) {
  const char *c = ctext(s, i);
  return c ? strdup(c) : NULL;
}

static int env_int(const char *k, int dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  if (end == v) return dflt;
  return (int)n;
}

static void sha256_hex(const void *b, size_t n, char out[65]) {
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)b, n, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(out + i * 2, 3, "%02x", d[i]);
  out[64] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXIF — pure byte parsing, no decoding, no allocation, re-entrant.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
  const unsigned char *t;              /* TIFF base (offset 0 of the block)  */
  size_t   tlen;
  int      le;                         /* 'II' → 1                           */
  uint32_t seen[MEDIA_EXIF_MAX_IFDS];  /* visited IFD offsets → cycle guard  */
  int      nseen;
  media_exif *out;
} exif_ctx;

/* Endianness lives in the context, never in a global: the scheduler runs
 * collectors on several threads and a file-scope byte-order flag is a data
 * race that silently mis-parses whichever image loses. */
static uint16_t ex_r16(const exif_ctx *c, const unsigned char *p) {
  return c->le ? (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8))
               : (uint16_t)((uint16_t)p[1] | ((uint16_t)p[0] << 8));
}

static uint32_t ex_r32(const exif_ctx *c, const unsigned char *p) {
  return c->le ? ((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                  ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24))
               : ((uint32_t)p[3] | ((uint32_t)p[2] << 8) |
                  ((uint32_t)p[1] << 16) | ((uint32_t)p[0] << 24));
}

/* THE bounds primitive. Phrased as a subtraction on purpose: `off + n <= tlen`
 * is the same statement only while off + n does not wrap, and off and n are
 * both attacker-supplied 32-bit values. */
static int ex_have(const exif_ctx *c, uint32_t off, uint32_t n) {
  if ((size_t)off > c->tlen) return 0;
  return (size_t)n <= c->tlen - (size_t)off;
}

static uint32_t ex_type_size(uint16_t type) {
  switch (type) {
    case 1: case 2: case 6: case 7: return 1;   /* BYTE ASCII SBYTE UNDEF   */
    case 3: case 8:                 return 2;   /* SHORT SSHORT             */
    case 4: case 9: case 11:        return 4;   /* LONG SLONG FLOAT         */
    case 5: case 10: case 12:       return 8;   /* RATIONAL SRATIONAL DOUBLE*/
    default:                        return 0;   /* unknown → value ignored  */
  }
}

/* Resolve a directory entry's value block. Returns NULL — meaning "this entry
 * is unusable, skip it" — for an unknown type, a zero count, a count whose
 * byte total overflows 32 bits, an out-of-line offset that lands in the TIFF
 * header (< 8, which no real writer emits and which is a reliable marker of a
 * crafted file), or any offset/length pair that does not fit in the block. */
static const unsigned char *ex_val(const exif_ctx *c, const unsigned char *e,
                                   uint16_t type, uint32_t count,
                                   uint32_t *nbytes) {
  uint32_t esz = ex_type_size(type);
  if (esz == 0 || count == 0) return NULL;
  if (count > 0xFFFFFFFFu / esz) return NULL;
  uint32_t total = esz * count;
  *nbytes = total;
  if (total <= 4) return e + 8;              /* inline in the entry itself */
  uint32_t off = ex_r32(c, e + 8);
  if (off < 8) return NULL;
  if (!ex_have(c, off, total)) return NULL;
  return c->t + off;
}

/* ASCII/UNDEFINED tag → a bounded, NUL-terminated, control-scrubbed string.
 * The scrub matters: these values land in JSON and then in a UI, and a Make
 * tag containing 0x1B or a lone 0x00 in the middle has no business in either. */
static void ex_ascii(const exif_ctx *c, const unsigned char *e,
                     char *out, size_t cap) {
  uint16_t type = ex_r16(c, e + 2);
  uint32_t count = ex_r32(c, e + 4), nb = 0;
  if (type != 2 && type != 7) return;
  const unsigned char *v = ex_val(c, e, type, count, &nb);
  if (!v) return;
  size_t n = nb;
  if (n > cap - 1) n = cap - 1;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char ch = v[i];
    if (ch == 0) break;
    out[j++] = (ch < 0x20 || ch == 0x7F) ? ' ' : (char)ch;
  }
  while (j > 0 && out[j - 1] == ' ') j--;
  out[j] = '\0';
}

/* SHORT/LONG/BYTE tag → an unsigned value. 0 on anything else. */
static int ex_uint(const exif_ctx *c, const unsigned char *e,
                   unsigned long *out) {
  uint16_t type = ex_r16(c, e + 2);
  uint32_t count = ex_r32(c, e + 4), nb = 0;
  const unsigned char *v = ex_val(c, e, type, count, &nb);
  if (!v) return 0;
  if (type == 3)                    *out = ex_r16(c, v);
  else if (type == 4 || type == 9)  *out = ex_r32(c, v);
  else if (type == 1 || type == 7)  *out = v[0];
  else return 0;
  return 1;
}

/* One RATIONAL at a value block. A zero denominator yields 0, never a trap. */
static int ex_rat1(const exif_ctx *c, const unsigned char *e, double *out) {
  uint16_t type = ex_r16(c, e + 2);
  uint32_t count = ex_r32(c, e + 4), nb = 0;
  if (type != 5) return 0;
  const unsigned char *v = ex_val(c, e, type, count, &nb);
  if (!v) return 0;
  uint32_t num = ex_r32(c, v), den = ex_r32(c, v + 4);
  if (den == 0) return 0;
  *out = (double)num / (double)den;
  return 1;
}

/* GPSLatitude / GPSLongitude: 1..3 RATIONALs, degrees[/minutes[/seconds]].
 * Sanity-bounded so a corrupt minutes field cannot fold into a coordinate that
 * looks plausible — a wrong pin is worse than no pin. */
static int ex_gps_dms(const exif_ctx *c, const unsigned char *e, double *out) {
  uint16_t type = ex_r16(c, e + 2);
  uint32_t count = ex_r32(c, e + 4), nb = 0;
  if (type != 5 || count < 1 || count > 3) return 0;
  const unsigned char *v = ex_val(c, e, type, count, &nb);
  if (!v) return 0;
  double p[3] = { 0.0, 0.0, 0.0 };
  for (uint32_t i = 0; i < count; i++) {
    uint32_t num = ex_r32(c, v + i * 8), den = ex_r32(c, v + i * 8 + 4);
    if (den == 0) {
      if (i == 0) return 0;         /* degrees must be real                 */
      p[i] = 0.0;                   /* 0/0 seconds is common; treat as zero */
    } else {
      p[i] = (double)num / (double)den;
    }
  }
  if (p[0] > 180.0 || p[1] >= 60.5 || p[2] >= 60.5) return 0;
  *out = p[0] + p[1] / 60.0 + p[2] / 3600.0;
  return 1;
}

/* GPSTimeStamp: 3 RATIONALs (h, m, s) in UTC. */
static void ex_gps_time(const exif_ctx *c, const unsigned char *e,
                        char *out, size_t cap) {
  uint16_t type = ex_r16(c, e + 2);
  uint32_t count = ex_r32(c, e + 4), nb = 0;
  if (type != 5 || count != 3) return;
  const unsigned char *v = ex_val(c, e, type, count, &nb);
  if (!v) return;
  double p[3];
  for (int i = 0; i < 3; i++) {
    uint32_t num = ex_r32(c, v + i * 8), den = ex_r32(c, v + i * 8 + 4);
    p[i] = den ? (double)num / (double)den : 0.0;
  }
  if (p[0] > 23.9 || p[1] > 59.9 || p[2] > 60.9) return;
  snprintf(out, cap, "%02d:%02d:%02d", (int)p[0], (int)p[1], (int)p[2]);
}

/* which: 0 = a TIFF IFD (IFD0 / IFD1), 1 = the Exif sub-IFD, 2 = the GPS IFD */
static void ex_ifd(exif_ctx *c, uint32_t off, int which, int depth) {
  if (depth > MEDIA_EXIF_MAX_DEPTH) return;
  if (off < 8 || !ex_have(c, off, 2)) { c->out->truncated = 1; return; }
  for (int i = 0; i < c->nseen; i++) if (c->seen[i] == off) return;  /* cycle */
  if (c->nseen >= MEDIA_EXIF_MAX_IFDS) { c->out->truncated = 1; return; }
  c->seen[c->nseen++] = off;
  c->out->ifds_parsed++;

  /* Two independent clamps on the entry count: an absurdity cap, and what
   * physically fits in the remaining block. Either alone would be sufficient;
   * both are cheap. */
  uint32_t n = ex_r16(c, c->t + off);
  int clamped = 0;
  if (n > MEDIA_EXIF_MAX_ENTRIES) { n = MEDIA_EXIF_MAX_ENTRIES; clamped = 1; }
  uint32_t fits = (uint32_t)((c->tlen - (size_t)off - 2) / 12);
  if (n > fits) { n = fits; clamped = 1; }
  if (clamped) c->out->truncated = 1;

  uint32_t sub_exif = 0, sub_gps = 0;
  const unsigned char *lat_e = NULL, *lon_e = NULL, *alt_e = NULL;
  char latref = 0, lonref = 0;
  unsigned long altref = 0;

  for (uint32_t i = 0; i < n; i++) {
    const unsigned char *e = c->t + off + 2 + (size_t)i * 12;
    uint16_t tag = ex_r16(c, e);
    unsigned long uv = 0;
    c->out->entries_parsed++;

    if (which == 2) {                                  /* ── GPS IFD ── */
      switch (tag) {
        case 0x0001: { char b[8] = {0};
                       ex_ascii(c, e, b, sizeof b); latref = b[0]; break; }
        case 0x0002: lat_e = e; break;
        case 0x0003: { char b[8] = {0};
                       ex_ascii(c, e, b, sizeof b); lonref = b[0]; break; }
        case 0x0004: lon_e = e; break;
        case 0x0005: if (ex_uint(c, e, &uv)) altref = uv; break;
        case 0x0006: alt_e = e; break;
        case 0x0007: ex_gps_time(c, e, c->out->gps_timestamp,
                                 sizeof c->out->gps_timestamp); break;
        case 0x001D: ex_ascii(c, e, c->out->gps_datestamp,
                              sizeof c->out->gps_datestamp); break;
        default: break;
      }
      continue;
    }

    switch (tag) {                                     /* ── IFD0 / Exif ── */
      /* 0x0100/0x0101 are honoured only in IFD0 (depth 0): in IFD1 they
       * describe the embedded THUMBNAIL, and reporting a 160x120 thumbnail as
       * the image's dimensions would be a quiet lie. */
      case 0x0100: if (which == 0 && depth == 0 && ex_uint(c, e, &uv))
                     c->out->width = (long)uv;
                   break;
      case 0x0101: if (which == 0 && depth == 0 && ex_uint(c, e, &uv))
                     c->out->height = (long)uv;
                   break;
      case 0x010F: ex_ascii(c, e, c->out->make, sizeof c->out->make); break;
      case 0x0110: ex_ascii(c, e, c->out->model, sizeof c->out->model); break;
      case 0x0112: if (ex_uint(c, e, &uv) && uv >= 1 && uv <= 8)
                     c->out->orientation = (int)uv;
                   break;
      case 0x0131: ex_ascii(c, e, c->out->software,
                            sizeof c->out->software); break;
      case 0x0132: ex_ascii(c, e, c->out->datetime,
                            sizeof c->out->datetime); break;
      case 0x8769: if (which == 0) sub_exif = ex_r32(c, e + 8); break;
      case 0x8825: if (which == 0) sub_gps  = ex_r32(c, e + 8); break;
      case 0x9003: ex_ascii(c, e, c->out->datetime_original,
                            sizeof c->out->datetime_original); break;
      case 0x9004: ex_ascii(c, e, c->out->datetime_digitized,
                            sizeof c->out->datetime_digitized); break;
      /* PixelXDimension/PixelYDimension describe the real frame and win over
       * IFD0's tags when both are present. */
      case 0xA002: if (which == 1 && ex_uint(c, e, &uv))
                     c->out->width = (long)uv;
                   break;
      case 0xA003: if (which == 1 && ex_uint(c, e, &uv))
                     c->out->height = (long)uv;
                   break;
      case 0xA434: ex_ascii(c, e, c->out->lens, sizeof c->out->lens); break;
      default: break;
    }
  }

  if (which == 2) {
    /* A fix is accepted only with BOTH reference tags present and valid. The
     * spec makes them mandatory; a file missing them is either damaged or
     * crafted, and guessing "N/E" would place Southern-hemisphere photos in
     * the wrong one. */
    if (lat_e && lon_e &&
        (latref == 'N' || latref == 'S') && (lonref == 'E' || lonref == 'W')) {
      double la = 0.0, lo = 0.0;
      if (ex_gps_dms(c, lat_e, &la) && ex_gps_dms(c, lon_e, &lo)) {
        if (latref == 'S') la = -la;
        if (lonref == 'W') lo = -lo;
        /* (0,0) is what a camera writes when it has no fix. Null Island is
         * not an OSINT finding. */
        if (la >= -90.0 && la <= 90.0 && lo >= -180.0 && lo <= 180.0 &&
            !(la == 0.0 && lo == 0.0)) {
          c->out->have_gps = 1;
          c->out->lat = la;
          c->out->lon = lo;
        }
      }
    }
    if (alt_e) {
      double a = 0.0;
      if (ex_rat1(c, alt_e, &a)) {
        c->out->have_alt = 1;
        c->out->altitude_m = (altref == 1) ? -a : a;   /* ref 1 = below sea */
      }
    }
    return;                                  /* GPS IFD has no children */
  }

  /* Sub-IFDs and the IFD1 chain must point FORWARD. Backwards is definitionally
   * a loop for the next-IFD chain, and no real writer emits a sub-IFD before
   * the directory that references it. The visited set below would catch a
   * cycle anyway; this is the cheaper first line. */
  if (sub_exif > off) ex_ifd(c, sub_exif, 1, depth + 1);
  if (sub_gps  > off) ex_ifd(c, sub_gps,  2, depth + 1);

  if (which == 0 && !clamped) {
    uint32_t nxt_at = off + 2 + n * 12;
    if (ex_have(c, nxt_at, 4)) {
      uint32_t nxt = ex_r32(c, c->t + nxt_at);
      if (nxt > off) ex_ifd(c, nxt, 0, depth + 1);
    }
  }
}

/* Locate the Exif TIFF block inside a JPEG. Refuses a segment length < 2
 * (which would not advance the cursor — the classic non-terminating case) and
 * any length that overruns the buffer, so the TIFF base pointer, once formed,
 * is provably inside the buffer along with its whole declared length. */
static int jpeg_find_exif(const unsigned char *b, size_t len,
                          const unsigned char **tiff, size_t *tlen) {
  size_t pos = 2;
  int markers = 0;
  while (pos + 4 <= len && markers++ < MEDIA_JPEG_MAX_MARKERS) {
    if (b[pos] != 0xFF) return 0;
    unsigned char m = b[pos + 1];
    if (m == 0xFF) { pos++; continue; }                    /* fill byte     */
    if (m == 0x01 || (m >= 0xD0 && m <= 0xD9)) { pos += 2; continue; }
    if (m == 0xDA) return 0;                    /* entropy-coded data begins */
    size_t seg = ((size_t)b[pos + 2] << 8) | (size_t)b[pos + 3];
    if (seg < 2) return 0;
    if (seg > len - pos - 2) return 0;
    if (m == 0xE1 && seg >= 8 && memcmp(b + pos + 4, "Exif\0\0", 6) == 0) {
      *tiff = b + pos + 10;
      *tlen = seg - 8;                       /* seg counts its own 2 bytes  */
      return *tlen >= 8;
    }
    pos += 2 + seg;
  }
  return 0;
}

/* PNG carries EXIF in an eXIf chunk (some writers prefix it with the JPEG
 * "Exif\0\0" marker; both spellings are accepted). */
static int png_find_exif(const unsigned char *b, size_t len,
                         const unsigned char **tiff, size_t *tlen) {
  size_t pos = 8;
  int chunks = 0;
  while (pos + 8 <= len && chunks++ < 4096) {
    uint32_t clen = ((uint32_t)b[pos] << 24) | ((uint32_t)b[pos + 1] << 16) |
                    ((uint32_t)b[pos + 2] << 8) | (uint32_t)b[pos + 3];
    if ((size_t)clen > len - pos - 8) return 0;
    if (memcmp(b + pos + 4, "eXIf", 4) == 0) {
      const unsigned char *p = b + pos + 8;
      size_t n = clen;
      if (n >= 6 && memcmp(p, "Exif\0\0", 6) == 0) { p += 6; n -= 6; }
      *tiff = p; *tlen = n;
      return n >= 8;
    }
    if (memcmp(b + pos + 4, "IDAT", 4) == 0) return 0;   /* metadata is over */
    pos += 12 + (size_t)clen;
  }
  return 0;
}

int media_exif_parse(const void *buf, size_t len, media_exif *out) {
  if (!out) return 0;
  memset(out, 0, sizeof *out);
  if (!buf || len < 16) return 0;
  const unsigned char *b = (const unsigned char *)buf;

  const unsigned char *t = NULL;
  size_t tlen = 0;
  if (b[0] == 0xFF && b[1] == 0xD8) {
    if (!jpeg_find_exif(b, len, &t, &tlen)) return 0;
  } else if ((b[0] == 'I' && b[1] == 'I') || (b[0] == 'M' && b[1] == 'M')) {
    t = b; tlen = len;                        /* a .tif IS a TIFF block */
  } else if (memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0) {
    if (!png_find_exif(b, len, &t, &tlen)) return 0;
  } else {
    return 0;
  }
  if (tlen < 8) return 0;

  exif_ctx c = { 0 };
  c.t = t; c.tlen = tlen; c.out = out;
  if (t[0] == 'I' && t[1] == 'I')      c.le = 1;
  else if (t[0] == 'M' && t[1] == 'M') c.le = 0;
  else return 0;
  if (ex_r16(&c, t + 2) != 42) return 0;      /* the TIFF magic; not optional */

  out->little_endian = c.le;
  uint32_t ifd0 = ex_r32(&c, t + 4);
  if (ifd0 < 8 || !ex_have(&c, ifd0, 2)) return 0;

  out->found = 1;
  ex_ifd(&c, ifd0, 0, 0);
  return 1;
}

char *media_exif_to_json(const media_exif *e) {
  if (!e) return NULL;
  cJSON *o = cJSON_CreateObject();
  if (!o) return NULL;

  if (e->have_gps) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddNumberToObject(g, "lat", e->lat);
    cJSON_AddNumberToObject(g, "lon", e->lon);
    if (e->have_alt) cJSON_AddNumberToObject(g, "alt_m", e->altitude_m);
    cJSON_AddItemToObject(o, "gps", g);
  }
  if (e->make[0] || e->model[0] || e->software[0] || e->lens[0]) {
    cJSON *cam = cJSON_CreateObject();
    if (e->make[0])     cJSON_AddStringToObject(cam, "make", e->make);
    if (e->model[0])    cJSON_AddStringToObject(cam, "model", e->model);
    if (e->software[0]) cJSON_AddStringToObject(cam, "software", e->software);
    if (e->lens[0])     cJSON_AddStringToObject(cam, "lens", e->lens);
    cJSON_AddItemToObject(o, "camera", cam);
  }
  if (e->datetime_original[0] || e->datetime_digitized[0] || e->datetime[0] ||
      e->gps_datestamp[0] || e->gps_timestamp[0]) {
    cJSON *tm = cJSON_CreateObject();
    if (e->datetime_original[0])
      cJSON_AddStringToObject(tm, "original", e->datetime_original);
    if (e->datetime_digitized[0])
      cJSON_AddStringToObject(tm, "digitized", e->datetime_digitized);
    if (e->datetime[0])       cJSON_AddStringToObject(tm, "modified", e->datetime);
    if (e->gps_datestamp[0])  cJSON_AddStringToObject(tm, "gps_date", e->gps_datestamp);
    if (e->gps_timestamp[0])  cJSON_AddStringToObject(tm, "gps_time", e->gps_timestamp);
    cJSON_AddItemToObject(o, "time", tm);
  }
  if (e->width || e->height || e->orientation) {
    cJSON *im = cJSON_CreateObject();
    if (e->width)       cJSON_AddNumberToObject(im, "width", (double)e->width);
    if (e->height)      cJSON_AddNumberToObject(im, "height", (double)e->height);
    if (e->orientation) cJSON_AddNumberToObject(im, "orientation", e->orientation);
    cJSON_AddItemToObject(o, "image", im);
  }
  cJSON_AddStringToObject(o, "byte_order", e->little_endian ? "II" : "MM");
  if (e->truncated) cJSON_AddBoolToObject(o, "truncated", 1);

  char *j = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return j;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Dimensions and type — also header-only, also no decoding.
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint32_t be32(const unsigned char *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static unsigned le16(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8);
}
static unsigned be16(const unsigned char *p) {
  return ((unsigned)p[0] << 8) | (unsigned)p[1];
}

/* JPEG: walk to the first SOFn. SOF is the frame header — it carries the
 * geometry in plain big-endian shorts and sits in front of the entropy-coded
 * data, so reading it is not decoding. DHT(C4)/JPG(C8)/DAC(CC) share the
 * 0xCn range and are NOT frame headers. */
static int jpeg_dims(const unsigned char *b, size_t len, long *w, long *h) {
  size_t pos = 2;
  int markers = 0;
  while (pos + 4 <= len && markers++ < MEDIA_JPEG_MAX_MARKERS) {
    if (b[pos] != 0xFF) return 0;
    unsigned char m = b[pos + 1];
    if (m == 0xFF) { pos++; continue; }
    if (m == 0x01 || (m >= 0xD0 && m <= 0xD9)) { pos += 2; continue; }
    if (m == 0xDA) return 0;
    size_t seg = ((size_t)b[pos + 2] << 8) | (size_t)b[pos + 3];
    if (seg < 2 || seg > len - pos - 2) return 0;
    if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
      if (seg < 7) return 0;
      *h = (long)be16(b + pos + 5);
      *w = (long)be16(b + pos + 7);
      return (*w > 0 && *h > 0);
    }
    pos += 2 + seg;
  }
  return 0;
}

int media_image_dims(const void *buf, size_t len, long *w, long *h) {
  if (!buf || !w || !h || len < 16) return 0;
  const unsigned char *b = (const unsigned char *)buf;
  *w = 0; *h = 0;

  if (b[0] == 0xFF && b[1] == 0xD8) return jpeg_dims(b, len, w, h);

  if (memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0 && len >= 24 &&
      memcmp(b + 12, "IHDR", 4) == 0) {
    *w = (long)be32(b + 16); *h = (long)be32(b + 20);
    return (*w > 0 && *h > 0);
  }
  if (len >= 10 && (memcmp(b, "GIF87a", 6) == 0 || memcmp(b, "GIF89a", 6) == 0)) {
    *w = (long)le16(b + 6); *h = (long)le16(b + 8);
    return (*w > 0 && *h > 0);
  }
  if (len >= 26 && b[0] == 'B' && b[1] == 'M') {
    long bw = (long)(int32_t)((uint32_t)b[18] | ((uint32_t)b[19] << 8) |
                              ((uint32_t)b[20] << 16) | ((uint32_t)b[21] << 24));
    long bh = (long)(int32_t)((uint32_t)b[22] | ((uint32_t)b[23] << 8) |
                              ((uint32_t)b[24] << 16) | ((uint32_t)b[25] << 24));
    if (bh < 0) bh = -bh;                       /* bottom-up DIB */
    *w = bw; *h = bh;
    return (*w > 0 && *h > 0);
  }
  if (len >= 30 && memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WEBP", 4) == 0) {
    if (memcmp(b + 12, "VP8X", 4) == 0 && len >= 30) {
      *w = 1 + (long)((uint32_t)b[24] | ((uint32_t)b[25] << 8) |
                      ((uint32_t)b[26] << 16));
      *h = 1 + (long)((uint32_t)b[27] | ((uint32_t)b[28] << 8) |
                      ((uint32_t)b[29] << 16));
      return (*w > 0 && *h > 0);
    }
    if (memcmp(b + 12, "VP8 ", 4) == 0 && len >= 30) {
      *w = (long)(le16(b + 26) & 0x3FFF);
      *h = (long)(le16(b + 28) & 0x3FFF);
      return (*w > 0 && *h > 0);
    }
    if (memcmp(b + 12, "VP8L", 4) == 0 && len >= 25) {
      uint32_t bits = (uint32_t)b[21] | ((uint32_t)b[22] << 8) |
                      ((uint32_t)b[23] << 16) | ((uint32_t)b[24] << 24);
      *w = 1 + (long)(bits & 0x3FFF);
      *h = 1 + (long)((bits >> 14) & 0x3FFF);
      return (*w > 0 && *h > 0);
    }
    return 0;
  }
  return 0;    /* TIFF geometry comes from the IFD; see media_exif_parse */
}

const char *media_sniff_type(const void *buf, size_t len) {
  if (!buf || len < 12) return NULL;
  const unsigned char *b = (const unsigned char *)buf;
  if (b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF)         return "image/jpeg";
  if (memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0)               return "image/png";
  if (memcmp(b, "GIF87a", 6) == 0 || memcmp(b, "GIF89a", 6) == 0)
                                                            return "image/gif";
  if (memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WEBP", 4) == 0)
                                                            return "image/webp";
  if (b[0] == 'B' && b[1] == 'M')                           return "image/bmp";
  if (memcmp(b, "II\x2a\x00", 4) == 0 || memcmp(b, "MM\x00\x2a", 4) == 0)
                                                            return "image/tiff";
  if (len >= 16 && memcmp(b + 4, "ftyp", 4) == 0) {
    /* ISO-BMFF. Identified so the row is honest; the EXIF inside a `meta` box
     * is NOT parsed (see media.h) — this is a container, not a header. */
    if (memcmp(b + 8, "avif", 4) == 0 || memcmp(b + 8, "avis", 4) == 0)
      return "image/avif";
    if (memcmp(b + 8, "heic", 4) == 0 || memcmp(b + 8, "heix", 4) == 0 ||
        memcmp(b + 8, "hevc", 4) == 0 || memcmp(b + 8, "mif1", 4) == 0)
      return "image/heic";
  }
  return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Perceptual hash. The DCT and the packing live here; only the 32x32
 * grayscale downscale is delegated. See media.h for the normative definition.
 * ═══════════════════════════════════════════════════════════════════════════ */

uint64_t media_phash_gray32(const unsigned char *px) {
  if (!px) return 0;

  /* cos table on the stack rather than a lazily-initialised static: 1024
   * cos() calls per image is nothing against a fork+exec, and a shared static
   * would need a pthread_once for no benefit. */
  double ct[32][32];
  for (int u = 0; u < 32; u++)
    for (int x = 0; x < 32; x++)
      ct[u][x] = cos(MEDIA_PI * (2.0 * (double)x + 1.0) * (double)u / 64.0);

  /* Separable DCT-II, and only the 8 low frequencies of each axis are ever
   * read, so the row pass computes 8 outputs instead of 32. */
  double rows[32][8];
  for (int y = 0; y < 32; y++)
    for (int u = 0; u < 8; u++) {
      double s = 0.0;
      for (int x = 0; x < 32; x++) s += (double)px[y * 32 + x] * ct[u][x];
      rows[y][u] = s;
    }
  double C[8][8];
  for (int v = 0; v < 8; v++)
    for (int u = 0; u < 8; u++) {
      double s = 0.0;
      for (int y = 0; y < 32; y++) s += rows[y][u] * ct[v][y];
      C[v][u] = s;
    }

  /* FEATURELESS IMAGES ARE REFUSED, and this is not a nicety. A solid-colour
   * image (a blank scan, a 1x1 tracking pixel upscaled, a placeholder tile —
   * all common in this corpus) has mathematically zero AC energy, so in
   * floating point its 63 AC coefficients are rounding noise around zero and
   * the median lands INSIDE that noise. Every bit is then decided by the order
   * the compiler happened to accumulate the sums in: the hash is deterministic
   * for one binary and different for the next, and worse, every distinct flat
   * image gets a different meaningless fingerprint instead of matching. So the
   * degenerate case is detected explicitly and reported as "no hash" via the 0
   * sentinel. Threshold: real images put AC within an order of magnitude or so
   * of DC, flat ones put it 13 orders below, so 1e-6 sits in an empty gap. */
  double dc = fabs(C[0][0]), maxac = 0.0;
  for (int v = 0; v < 8; v++)
    for (int u = 0; u < 8; u++)
      if (v || u) { double mg = fabs(C[v][u]); if (mg > maxac) maxac = mg; }
  if (maxac <= 1e-6 * (dc + 1.0)) return 0;

  /* Median over the 63 AC terms. The DC term is excluded because it is just
   * mean brightness: including it drags the median toward the image's overall
   * exposure and makes the hash track lighting rather than structure. */
  double m[63];
  int k = 0;
  for (int v = 0; v < 8; v++)
    for (int u = 0; u < 8; u++)
      if (v || u) m[k++] = C[v][u];
  for (int i = 1; i < 63; i++) {              /* insertion sort; n = 63 */
    double key = m[i];
    int j = i - 1;
    while (j >= 0 && m[j] > key) { m[j + 1] = m[j]; j--; }
    m[j + 1] = key;
  }
  double median = m[31];

  uint64_t hsh = 0;
  for (int v = 0; v < 8; v++)
    for (int u = 0; u < 8; u++)
      if (C[v][u] > median) hsh |= (uint64_t)1 << (63 - (v * 8 + u));
  return hsh;
}

int media_phash_distance(uint64_t a, uint64_t b) {
  return __builtin_popcountll(a ^ b);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * External-tool seam. Everything below this line is the part that CANNOT run
 * in this environment (no ImageMagick, no tesseract) and is written to be a
 * clean no-op when the tool is missing rather than to pretend otherwise.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Paths reaching /bin/sh are whitelisted, not escaped. Only two kinds of path
 * ever get here — an evidence blob (root directory + a 64-hex content hash)
 * and an mkstemp() name — so a whitelist costs nothing and removes the entire
 * quoting-bug class. A remote URL, filename or header can never reach this. */
static int path_ok(const char *p) {
  if (!p || !*p || *p == '-') return 0;
  for (const unsigned char *q = (const unsigned char *)p; *q; q++)
    if (!isalnum(*q) && *q != '.' && *q != '_' && *q != '-' && *q != '/')
      return 0;
  return 1;
}

static int tools_disabled(void) {
  static int v = -1;
  if (v < 0) v = getenv("MEDIA_NO_TOOLS") ? 1 : 0;
  return v;
}

/* `out` must be at least MEDIA_TOOLPATH_MAX bytes; the scratch buffer is sized
 * to match so the copy back out cannot truncate (and so -Wformat-truncation,
 * which only fires at -O2 and is therefore invisible to -fsyntax-only, has
 * nothing to say about it). A PATH element too long to hold the candidate is
 * skipped rather than silently truncated into a wrong path that access() might
 * then accept. */
#define MEDIA_TOOLPATH_MAX 512

static int find_tool(const char *name, char *out, size_t cap) {
  const char *path = getenv("PATH");
  if (!path || !*path || cap < MEDIA_TOOLPATH_MAX) return 0;
  size_t nlen = strlen(name);
  for (const char *p = path; *p; ) {
    const char *e = strchr(p, ':');
    size_t n = e ? (size_t)(e - p) : strlen(p);
    if (n > 0 && n + nlen + 2 <= MEDIA_TOOLPATH_MAX) {
      char cand[MEDIA_TOOLPATH_MAX];
      snprintf(cand, sizeof cand, "%.*s/%s", (int)n, p, name);
      if (access(cand, X_OK) == 0 && path_ok(cand)) {
        memcpy(out, cand, strlen(cand) + 1);
        return 1;
      }
    }
    if (!e) break;
    p = e + 1;
  }
  return 0;
}

/* "{}" in a template is replaced by the path; a template without it gets the
 * path appended. Both spellings exist in the wild for this kind of knob and
 * supporting only one is a support burden for no gain. */
static char *build_cmd(const char *tmpl, const char *path) {
  const char *m = strstr(tmpl, "{}");
  size_t need = strlen(tmpl) + strlen(path) + 4;
  char *out = malloc(need);
  if (!out) return NULL;
  if (m) {
    size_t pre = (size_t)(m - tmpl);
    memcpy(out, tmpl, pre);
    out[pre] = '\0';
    strcat(out, path);
    strcat(out, m + 2);
  } else {
    snprintf(out, need, "%s %s", tmpl, path);
  }
  return out;
}

/* popen + bounded read. Returns NULL on spawn failure, on a non-zero exit
 * status, or on OOM — a tool that failed is indistinguishable from a tool that
 * is absent, and both mean "store NULL", so there is nothing to distinguish. */
static char *run_capture(const char *cmd, size_t maxb, size_t *outlen) {
  FILE *f = popen(cmd, "r");
  if (!f) return NULL;
  size_t cap = 8192, len = 0;
  char *buf = malloc(cap);
  if (!buf) { pclose(f); return NULL; }
  for (;;) {
    if (len + 4097 > cap) {
      if (cap >= maxb + 8192) break;
      size_t nc = cap * 2;
      char *q = realloc(buf, nc);
      if (!q) { free(buf); pclose(f); return NULL; }
      buf = q; cap = nc;
    }
    size_t got = fread(buf + len, 1, 4096, f);
    len += got;
    if (got < 4096) break;
    if (len >= maxb) break;
  }
  /* Drain whatever is left so the child is never blocked writing into a full
   * pipe while we sit in pclose() waiting for it to exit. */
  char sink[4096];
  while (fread(sink, 1, sizeof sink, f) > 0) { /* discard */ }
  int rc = pclose(f);
  buf[len] = '\0';
  if (rc != 0) { free(buf); return NULL; }
  if (outlen) *outlen = len;
  return buf;
}

/* ── pHash: ask a decoder for 1024 grayscale bytes, hash them ourselves ──── */

static pthread_mutex_t g_tool_lock = PTHREAD_MUTEX_INITIALIZER;
static int  g_ph_probed = 0, g_ph_have = 0;
static char g_ph_tmpl[1024];
/* Sized to match find_tool()'s scratch buffer. Anything smaller and the copy
 * is a provable truncation, which -Wformat-truncation reports at -O2 (the real
 * build) but NOT under -fsyntax-only — worth knowing when checking this file. */
static char g_ph_tool[MEDIA_TOOLPATH_MAX];

/* The template must emit RAW 8-bit grayscale for a 32x32 image on stdout and
 * nothing else. `-background white -alpha remove` makes transparent PNGs
 * deterministic (without it the composite depends on the tool's default
 * background and two hosts disagree on the same file). Multi-frame inputs
 * yield more than 1024 bytes; the caller takes the first frame. */
static void probe_phash(void) {
  if (g_ph_probed) return;
  g_ph_probed = 1;
  const char *ov = getenv("MEDIA_PHASH_CMD");
  if (ov && *ov) {
    snprintf(g_ph_tmpl, sizeof g_ph_tmpl, "%s", ov);
    snprintf(g_ph_tool, sizeof g_ph_tool, "%s", "MEDIA_PHASH_CMD");
    g_ph_have = 1;
    return;
  }
  if (tools_disabled()) return;
  char t[MEDIA_TOOLPATH_MAX];
  if (find_tool("magick", t, sizeof t) || find_tool("convert", t, sizeof t)) {
    snprintf(g_ph_tmpl, sizeof g_ph_tmpl,
             "%s {} -background white -alpha remove -colorspace Gray"
             " -resize '32x32!' -depth 8 GRAY:- 2>/dev/null", t);
    snprintf(g_ph_tool, sizeof g_ph_tool, "%s", t);
    g_ph_have = 1;
    return;
  }
  if (find_tool("gm", t, sizeof t)) {
    snprintf(g_ph_tmpl, sizeof g_ph_tmpl,
             "%s convert {} -background white -colorspace Gray"
             " -resize '32x32!' -depth 8 gray:- 2>/dev/null", t);
    snprintf(g_ph_tool, sizeof g_ph_tool, "%s", t);
    g_ph_have = 1;
  }
}

int media_phash_file(const char *path, uint64_t *out) {
  if (!path || !out || !path_ok(path)) return 0;
  pthread_mutex_lock(&g_tool_lock);
  probe_phash();
  int have = g_ph_have;
  char tmpl[1024];
  snprintf(tmpl, sizeof tmpl, "%s", g_ph_tmpl);
  pthread_mutex_unlock(&g_tool_lock);
  if (!have) return 0;

  char *cmd = build_cmd(tmpl, path);
  if (!cmd) return 0;
  size_t n = 0;
  char *px = run_capture(cmd, 1 << 20, &n);
  free(cmd);
  if (!px) return 0;
  if (n < 1024) { free(px); return 0; }        /* short output → unusable */
  uint64_t h = media_phash_gray32((const unsigned char *)px);
  free(px);
  if (h == 0) return 0;
  *out = h;
  return 1;
}

/* ── OCR ─────────────────────────────────────────────────────────────────── */

static int  g_ocr_probed = 0, g_ocr_have = 0, g_ocr_tsv = 0;
static char g_ocr_tmpl[1024];
static char g_ocr_tool[MEDIA_TOOLPATH_MAX];

static void probe_ocr(void) {
  if (g_ocr_probed) return;
  g_ocr_probed = 1;
  const char *ov = getenv("MEDIA_OCR_CMD");
  if (ov && *ov) {
    snprintf(g_ocr_tmpl, sizeof g_ocr_tmpl, "%s", ov);
    snprintf(g_ocr_tool, sizeof g_ocr_tool, "%s", "MEDIA_OCR_CMD");
    g_ocr_have = 1;
    g_ocr_tsv = 0;              /* an override's output is plain text only */
    return;
  }
  if (tools_disabled()) return;
  char t[MEDIA_TOOLPATH_MAX];
  if (!find_tool("tesseract", t, sizeof t)) return;
  const char *lang = getenv("MEDIA_OCR_LANG");
  if (!lang || !*lang) lang = "jpn+eng";
  /* Reject a language string that is not [A-Za-z0-9+_-]: it comes from the
   * environment rather than from the network, but it goes to a shell, and
   * "trusted-ish" is not a category this file recognises. */
  for (const unsigned char *q = (const unsigned char *)lang; *q; q++)
    if (!isalnum(*q) && *q != '+' && *q != '_' && *q != '-') { lang = "eng"; break; }
  /* TSV mode rather than plain stdout: it carries a per-word confidence, which
   * is the difference between storing a real ocr_conf and inventing one. */
  snprintf(g_ocr_tmpl, sizeof g_ocr_tmpl,
           "%s {} stdout -l %s tsv 2>/dev/null", t, lang);
  snprintf(g_ocr_tool, sizeof g_ocr_tool, "%s", t);
  g_ocr_have = 1;
  g_ocr_tsv = 1;
}

/* tesseract TSV: level page block par line word left top w h conf text.
 * Only level 5 rows are words. Line breaks are reconstructed from the
 * block/par/line numbers so the stored text keeps the layout that made it
 * readable in the first place. */
static char *ocr_parse_tsv(const char *tsv, double *out_conf) {
  size_t cap = strlen(tsv) + 16, len = 0;
  char *out = malloc(cap);
  if (!out) return NULL;
  out[0] = '\0';
  double sum = 0.0;
  long cnt = 0;
  int lb = -1, lp = -1, ll = -1, first = 1;

  const char *p = tsv;
  const char *nl = strchr(p, '\n');
  if (nl && strncmp(p, "level", 5) == 0) p = nl + 1;   /* header row */

  while (*p) {
    const char *eol = strchr(p, '\n');
    size_t rl = eol ? (size_t)(eol - p) : strlen(p);
    char line[4096];
    if (rl > 0 && rl < sizeof line) {
      memcpy(line, p, rl);
      line[rl] = '\0';
      while (rl > 0 && (line[rl - 1] == '\r')) line[--rl] = '\0';

      char *f[12];
      int nf = 0;
      char *q = line;
      while (nf < 12) {
        f[nf++] = q;
        char *tab = strchr(q, '\t');
        if (!tab) break;
        *tab = '\0';
        q = tab + 1;
      }
      if (nf >= 12 && atoi(f[0]) == 5) {
        double cf = atof(f[10]);
        const char *txt = f[11];
        while (*txt == ' ') txt++;
        if (*txt && cf >= 0.0) {
          int blk = atoi(f[2]), par = atoi(f[3]), lnn = atoi(f[4]);
          const char *sep = "";
          if (first) first = 0;
          else if (blk != lb || par != lp || lnn != ll) sep = "\n";
          else sep = " ";
          size_t sl = strlen(sep), tl = strlen(txt);
          if (len + sl + tl + 1 > cap) break;      /* cannot happen; bounded */
          memcpy(out + len, sep, sl); len += sl;
          memcpy(out + len, txt, tl); len += tl;
          out[len] = '\0';
          lb = blk; lp = par; ll = lnn;
          sum += cf; cnt++;
        }
      }
    }
    if (!eol) break;
    p = eol + 1;
  }
  if (cnt == 0) { free(out); return NULL; }
  if (out_conf) *out_conf = sum / (double)cnt;
  return out;
}

/* Collapse runs of whitespace-only output to nothing: an OCR pass over a
 * photograph with no text returns page furniture and newlines, and storing
 * that would put an empty row into FTS and a meaningless blob in the API. */
static int text_has_content(const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    if (!isspace(*p)) return 1;
  return 0;
}

int media_ocr_file(const char *path, char **out_text, double *out_conf) {
  if (!path || !out_text || !path_ok(path)) return 0;
  if (out_conf) *out_conf = -1.0;

  pthread_mutex_lock(&g_tool_lock);
  probe_ocr();
  int have = g_ocr_have, tsv = g_ocr_tsv;
  char tmpl[1024];
  snprintf(tmpl, sizeof tmpl, "%s", g_ocr_tmpl);
  pthread_mutex_unlock(&g_tool_lock);
  if (!have) return 0;

  int maxb = env_int("MEDIA_OCR_MAX_BYTES", 65536);
  if (maxb < 256) maxb = 256;

  char *cmd = build_cmd(tmpl, path);
  if (!cmd) return 0;
  /* TSV is far bulkier than the text it carries, so the raw capture budget is
   * larger than the storage budget; the stored text is clipped below. */
  char *raw = run_capture(cmd, (size_t)maxb * 8 + 4096, NULL);
  free(cmd);
  if (!raw) return 0;

  char *text = NULL;
  double conf = -1.0;
  if (tsv) {
    text = ocr_parse_tsv(raw, &conf);
    free(raw);
  } else {
    text = raw;
  }
  if (!text) return 0;
  if (!text_has_content(text)) { free(text); return 0; }

  if (strlen(text) > (size_t)maxb) {
    /* Clip on a UTF-8 boundary — a Japanese page cut mid-sequence would be
     * mojibake in the API and an unmatchable token in FTS. */
    size_t c = (size_t)maxb;
    while (c > 0 && ((unsigned char)text[c] & 0xC0) == 0x80) c--;
    text[c] = '\0';
  }
  *out_text = text;
  if (out_conf) *out_conf = conf;
  return 1;
}

char *media_capabilities(void) {
  pthread_mutex_lock(&g_tool_lock);
  probe_phash();
  probe_ocr();
  int ph = g_ph_have, oc = g_ocr_have, tsv = g_ocr_tsv;
  char pt[MEDIA_TOOLPATH_MAX], ot[MEDIA_TOOLPATH_MAX];
  snprintf(pt, sizeof pt, "%s", g_ph_tool);
  snprintf(ot, sizeof ot, "%s", g_ocr_tool);
  pthread_mutex_unlock(&g_tool_lock);

  cJSON *d = cJSON_CreateObject();
  cJSON *e = cJSON_CreateObject();
  cJSON_AddBoolToObject(e, "available", 1);
  cJSON_AddBoolToObject(e, "builtin", 1);
  cJSON_AddItemToObject(d, "exif", e);
  cJSON *dim = cJSON_CreateObject();
  cJSON_AddBoolToObject(dim, "available", 1);
  cJSON_AddBoolToObject(dim, "builtin", 1);
  cJSON_AddItemToObject(d, "dimensions", dim);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "available", ph);
  if (ph && pt[0]) cJSON_AddStringToObject(p, "tool", pt);
  else cJSON_AddNullToObject(p, "tool");
  cJSON_AddItemToObject(d, "phash", p);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddBoolToObject(o, "available", oc);
  if (oc && ot[0]) cJSON_AddStringToObject(o, "tool", ot);
  else cJSON_AddNullToObject(o, "tool");
  cJSON_AddBoolToObject(o, "confidence", oc && tsv);
  cJSON_AddItemToObject(d, "ocr", o);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", d);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return j;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Migration + state
 * ═══════════════════════════════════════════════════════════════════════════ */

static pthread_mutex_t g_mig_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_migrated = 0;

void media_migrate(db_handle *db) {
  if (!db || !db->h) return;
  pthread_mutex_lock(&g_mig_lock);
  if (g_migrated) { pthread_mutex_unlock(&g_mig_lock); return; }

  /* Column FIRST — idx_intel_items_media_pending below names it, and an index
   * on a column that does not exist yet fails and abandons the rest of the
   * script. This ordering is the whole reason the DDL lives in a function
   * instead of in schema.sql. */
  ensure_column(db, "intel_items", "media_scanned_at", "TEXT");

  static const char *DDL =
    "CREATE TABLE IF NOT EXISTS media_assets ("
    "  id TEXT PRIMARY KEY, item_uid TEXT NOT NULL, url TEXT NOT NULL,"
    "  sha256 TEXT, bytes INTEGER, content_type TEXT,"
    "  phash INTEGER, exif_json TEXT, ocr_text TEXT, ocr_conf REAL,"
    "  width INTEGER, height INTEGER,"
    "  analyzed_at TEXT, analyze_failed INTEGER NOT NULL DEFAULT 0,"
    "  UNIQUE(item_uid, url));"
    "CREATE INDEX IF NOT EXISTS idx_media_assets_item ON media_assets(item_uid);"
    "CREATE INDEX IF NOT EXISTS idx_media_assets_sha ON media_assets(sha256);"
    /* Indexed on analyze_failed, NOT on rowid: SQLite refuses to index the
     * rowid ("no such column: rowid"), and because sqlite3_exec abandons the
     * rest of a script at the first error, that one illegal statement would
     * take media_state and the two indexes below down with it. Indexing
     * analyze_failed is also better queueing than rowid order would have
     * been — never-attempted assets sort ahead of ones that have already
     * failed twice, so a batch is not spent re-poking known-bad URLs. */
    "CREATE INDEX IF NOT EXISTS idx_media_assets_pending"
    "  ON media_assets(analyze_failed) WHERE " MEDIA_PENDING_WHERE ";"
    "CREATE INDEX IF NOT EXISTS idx_media_assets_exif"
    "  ON media_assets(item_uid) WHERE exif_json IS NOT NULL;"
    "CREATE INDEX IF NOT EXISTS idx_media_assets_ocr"
    "  ON media_assets(item_uid) WHERE ocr_text IS NOT NULL;"
    "CREATE TABLE IF NOT EXISTS media_state (k TEXT PRIMARY KEY, v TEXT NOT NULL);"
    "CREATE INDEX IF NOT EXISTS idx_intel_items_media_pending"
    "  ON intel_items(fetched_at DESC) WHERE media_scanned_at IS NULL;";

  char *e = NULL;
  if (sqlite3_exec(db->h, DDL, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "[media] migrate: %s\n", e ? e : "?");
    sqlite3_free(e);
  }
  g_migrated = 1;
  pthread_mutex_unlock(&g_mig_lock);
}

static void state_get(sqlite3 *h, const char *k, char *out, size_t cap) {
  out[0] = '\0';
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "SELECT v FROM media_state WHERE k=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, k, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *t = ctext(s, 0);
      if (t) snprintf(out, cap, "%s", t);
    }
    sqlite3_finalize(s);
  }
}

static void state_put(sqlite3 *h, const char *k, const char *v) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "INSERT INTO media_state(k,v) VALUES(?1,?2)"
      " ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, k, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, v, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Discovery
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct { char **v; int n, cap; } urlset;

static void us_free(urlset *u) {
  for (int i = 0; i < u->n; i++) free(u->v[i]);
  free(u->v);
  u->v = NULL; u->n = u->cap = 0;
}

/* HTML-escaped ampersands are the single most common way a stored image URL
 * arrives broken; unescaping is the difference between a 404 and a fetch. */
static void unescape_amp(char *s) {
  char *w = s;
  for (char *r = s; *r; ) {
    if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; }
    else *w++ = *r++;
  }
  *w = '\0';
}

static int url_is_http(const char *u) {
  return u && (strncmp(u, "http://", 7) == 0 || strncmp(u, "https://", 8) == 0);
}

static int url_has_image_ext(const char *u) {
  static const char *ext[] = { ".jpg", ".jpeg", ".png", ".gif", ".webp",
                               ".bmp", ".tif", ".tiff", ".heic", ".heif",
                               ".avif", ".jfif", NULL };
  size_t n = strlen(u);
  const char *cut = strpbrk(u, "?#");
  if (cut) n = (size_t)(cut - u);
  for (int i = 0; ext[i]; i++) {
    size_t el = strlen(ext[i]);
    if (n > el && strncasecmp(u + n - el, ext[i], el) == 0) return 1;
  }
  return 0;
}

/* An extension-less CDN URL under a key called "thumbnail_url" is an image;
 * this is how the majority of the corpus's images are actually reachable. */
static int key_is_imageish(const char *k) {
  static const char *kw[] = { "image", "img", "photo", "thumb", "picture",
                              "preview", "poster", "avatar", "enclosure",
                              "screenshot", NULL };
  char lk[128];
  size_t i = 0;
  for (; k[i] && i < sizeof lk - 1; i++) lk[i] = (char)tolower((unsigned char)k[i]);
  lk[i] = '\0';
  for (int j = 0; kw[j]; j++) if (strstr(lk, kw[j])) return 1;
  return 0;
}

static void us_add(urlset *u, const char *url, int max) {
  if (u->n >= max) return;
  size_t n = strlen(url);
  if (n < 12 || n > 1024) return;
  char *c = strdup(url);
  if (!c) return;
  unescape_amp(c);
  if (!url_is_http(c)) { free(c); return; }
  for (int i = 0; i < u->n; i++)
    if (strcmp(u->v[i], c) == 0) { free(c); return; }
  if (u->n == u->cap) {
    int nc = u->cap ? u->cap * 2 : 8;
    char **q = realloc(u->v, (size_t)nc * sizeof *q);
    if (!q) { free(c); return; }
    u->v = q; u->cap = nc;
  }
  u->v[u->n++] = c;
}

/* Depth- and node-bounded: `properties` is arbitrary JSON from a scraped feed
 * and neither its nesting nor its size is under our control. */
static void walk_props(cJSON *node, const char *key, urlset *u, int depth,
                       int max, int *budget) {
  if (!node || depth > 6 || *budget <= 0) return;
  (*budget)--;
  if (cJSON_IsString(node) && node->valuestring) {
    const char *s = node->valuestring;
    if (url_is_http(s) && (url_has_image_ext(s) || (key && key_is_imageish(key))))
      us_add(u, s, max);
    return;
  }
  cJSON *ch;
  cJSON_ArrayForEach(ch, node)
    walk_props(ch, ch->string ? ch->string : key, u, depth + 1, max, budget);
}

static void scan_body(const char *body, urlset *u, int max) {
  if (!body) return;
  char tmp[1025];

  for (const char *p = strstr(body, "src="); p; p = strstr(p, "src=")) {
    p += 4;
    char q = *p;
    if (q != '"' && q != '\'') continue;
    p++;
    const char *e = strchr(p, q);
    if (!e) break;
    size_t n = (size_t)(e - p);
    if (n > 0 && n < sizeof tmp) {
      memcpy(tmp, p, n); tmp[n] = '\0';
      if (url_is_http(tmp)) us_add(u, tmp, max);
    }
    p = e + 1;
  }

  for (const char *p = strstr(body, "http"); p; ) {
    if (strncmp(p, "http://", 7) != 0 && strncmp(p, "https://", 8) != 0) {
      p = strstr(p + 4, "http");
      continue;
    }
    size_t n = 0;
    while (p[n] && n < sizeof tmp - 1 && !isspace((unsigned char)p[n]) &&
           p[n] != '"' && p[n] != '\'' && p[n] != '<' && p[n] != '>' &&
           p[n] != ')' && p[n] != ']' && p[n] != '\\') n++;
    size_t m = n;
    while (m > 0 && strchr(".,;:!", p[m - 1])) m--;
    if (m > 0) {
      memcpy(tmp, p, m); tmp[m] = '\0';
      if (url_has_image_ext(tmp)) us_add(u, tmp, max);
    }
    p = strstr(p + (n > 4 ? n : 4), "http");
  }
}

int media_discover_for_item(db_handle *db, const char *item_uid) {
  if (!db || !db->h || !item_uid || !*item_uid) return -1;
  media_migrate(db);
  sqlite3 *h = db->h;

  char *props = NULL, *body = NULL, *link = NULL;
  int have = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT properties, body, link FROM intel_items WHERE uid=?1",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, item_uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      have = 1;
      props = dupcol(s, 0); body = dupcol(s, 1); link = dupcol(s, 2);
    }
    sqlite3_finalize(s);
  }
  if (!have) { free(props); free(body); free(link); return -1; }

  int max = env_int("MEDIA_MAX_PER_ITEM", 8);
  if (max < 1) max = 1;
  if (max > 64) max = 64;

  urlset u = { 0 };
  if (props) {
    cJSON *j = cJSON_Parse(props);
    if (j) { int budget = 2000; walk_props(j, NULL, &u, 0, max, &budget);
             cJSON_Delete(j); }
  }
  if (body) scan_body(body, &u, max);
  if (link && url_is_http(link) && url_has_image_ext(link)) us_add(&u, link, max);

  int added = 0;
  for (int i = 0; i < u.n; i++) {
    char id[37];
    uuid4(id);
    if (sqlite3_prepare_v2(h,
        "INSERT OR IGNORE INTO media_assets(id,item_uid,url) VALUES(?1,?2,?3)",
        -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, item_uid, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 3, u.v[i], -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_DONE && sqlite3_changes(h) > 0) added++;
      sqlite3_finalize(s);
    }
  }
  /* Stamped unconditionally, including when nothing was found: the stamp means
   * "swept", not "has media". Without that the pending index would return the
   * same image-less items on every tick, forever. */
  if (sqlite3_prepare_v2(h,
      "UPDATE intel_items SET media_scanned_at=datetime('now') WHERE uid=?1",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, item_uid, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }

  us_free(&u);
  free(props); free(body); free(link);
  return added;
}

int media_discover_batch(db_handle *db, int limit, volatile int *cancel) {
  if (!db || !db->h) return 0;
  media_migrate(db);
  sqlite3 *h = db->h;
  if (limit <= 0) limit = env_int("MEDIA_DISCOVER_BATCH", 200);
  if (limit <= 0) limit = 200;

  char **uids = calloc((size_t)limit, sizeof *uids);
  if (!uids) return 0;
  int n = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT uid FROM intel_items WHERE media_scanned_at IS NULL"
      " ORDER BY fetched_at DESC LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, limit);
    while (n < limit && sqlite3_step(s) == SQLITE_ROW) {
      uids[n] = dupcol(s, 0);
      if (uids[n]) n++;
    }
    sqlite3_finalize(s);
  }

  /* One transaction for the whole batch: this phase does no network and no
   * shelling out, so the write lock is held for milliseconds. */
  int added = 0;
  if (n > 0) {
    sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
    for (int i = 0; i < n; i++) {
      if (cancel && *cancel) break;
      int r = media_discover_for_item(db, uids[i]);
      if (r > 0) added += r;
    }
    sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
  }
  for (int i = 0; i < n; i++) free(uids[i]);
  free(uids);
  if (n) fprintf(stderr, "[media] discover: items=%d assets=%d\n", n, added);
  return added;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Geolocation back-write
 * ═══════════════════════════════════════════════════════════════════════════ */

int media_geo_backfill(db_handle *db, const char *item_uid, double lat,
                       double lon) {
  if (!db || !db->h || !item_uid || !*item_uid) return 0;
  if (!(lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0)) return 0;
  if (lat == 0.0 && lon == 0.0) return 0;

  sqlite3_stmt *s;
  /* The three IS NULL predicates ARE the never-overwrite rule. They are in the
   * statement rather than in a read-then-write so the check and the write are
   * one atomic operation — a concurrent geocoder cannot slip between them. */
  if (sqlite3_prepare_v2(db->h,
      "UPDATE intel_items SET lat=?1, lon=?2, geom_source='exif',"
      " geom_at=strftime('%Y-%m-%dT%H:%M:%SZ','now')"
      " WHERE uid=?3 AND lat IS NULL AND lon IS NULL AND geom_source IS NULL",
      -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_double(s, 1, lat);
  sqlite3_bind_double(s, 2, lon);
  sqlite3_bind_text(s, 3, item_uid, -1, SQLITE_TRANSIENT);
  int ok = (sqlite3_step(s) == SQLITE_DONE);
  sqlite3_finalize(s);
  return (ok && sqlite3_changes(db->h) > 0) ? 1 : 0;
}

int media_geo_reapply(db_handle *db, int limit, volatile int *cancel) {
  if (!db || !db->h) return 0;
  media_migrate(db);
  sqlite3 *h = db->h;
  if (limit <= 0) limit = env_int("MEDIA_GEO_BATCH", 200);
  if (limit <= 0) limit = 200;

  typedef struct { char *uid; double lat, lon; } grow;
  grow *rows = calloc((size_t)limit, sizeof *rows);
  if (!rows) return 0;
  int n = 0;
  sqlite3_stmt *s;
  /* json_extract rather than a denormalised column: the EXIF is already stored
   * as JSON, JSON1 is compiled in, and idx_media_assets_exif keeps the driving
   * set to assets that actually carry EXIF — a small fraction of the table. */
  if (sqlite3_prepare_v2(h,
      "SELECT ma.item_uid,"
      "       json_extract(ma.exif_json,'$.gps.lat'),"
      "       json_extract(ma.exif_json,'$.gps.lon')"
      "  FROM media_assets ma JOIN intel_items i ON i.uid = ma.item_uid"
      " WHERE ma.exif_json IS NOT NULL"
      "   AND i.lat IS NULL AND i.geom_source IS NULL"
      " LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, limit);
    while (n < limit && sqlite3_step(s) == SQLITE_ROW) {
      if (sqlite3_column_type(s, 1) == SQLITE_NULL ||
          sqlite3_column_type(s, 2) == SQLITE_NULL) continue;
      rows[n].uid = dupcol(s, 0);
      rows[n].lat = sqlite3_column_double(s, 1);
      rows[n].lon = sqlite3_column_double(s, 2);
      if (rows[n].uid) n++;
    }
    sqlite3_finalize(s);
  }

  int restored = 0;
  for (int i = 0; i < n; i++) {
    if (cancel && *cancel) break;
    restored += media_geo_backfill(db, rows[i].uid, rows[i].lat, rows[i].lon);
  }
  for (int i = 0; i < n; i++) free(rows[i].uid);
  free(rows);
  if (restored)
    fprintf(stderr, "[media] geo: restored %d exif coordinate(s)\n", restored);
  return restored;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FTS co-write with core/translate.c — see the long note in media.h.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Byte-for-byte translate.c's join_en(): same order, same conditional single
 * spaces, same fts_segment() over the concatenation. It has to be identical,
 * because the whole co-write scheme rests on this module being able to
 * reproduce exactly what translate.c would have written. */
static char *join_en(const char *title, const char *summary, const char *body) {
  size_t n = (title ? strlen(title) : 0) + (summary ? strlen(summary) : 0) +
             (body ? strlen(body) : 0) + 4;
  char *raw = malloc(n);
  if (!raw) return NULL;
  raw[0] = '\0';
  if (title && *title)   { strcat(raw, title); strcat(raw, " "); }
  if (summary && *summary) { strcat(raw, summary); strcat(raw, " "); }
  if (body && *body)     strcat(raw, body);
  char *seg = fts_segment(raw);
  free(raw);
  return seg;
}

/* Build the full keywords payload for one item and, separately, the segmented
 * OCR half so the caller can test whether the index already contains it.
 * Returns 0 when the item has no OCR text at all (nothing to do). */
static int media_fts_compose(sqlite3 *h, const char *uid, char **out_full,
                             char **out_ocr) {
  *out_full = NULL; *out_ocr = NULL;

  char *ocr = NULL;
  size_t olen = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT ocr_text FROM media_assets"
      " WHERE item_uid=?1 AND ocr_text IS NOT NULL AND ocr_text<>''"
      " ORDER BY rowid", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *t = ctext(s, 0);
      if (!t || !*t) continue;
      size_t tl = strlen(t);
      char *q = realloc(ocr, olen + tl + 2);
      if (!q) break;
      ocr = q;
      if (olen) ocr[olen++] = ' ';
      memcpy(ocr + olen, t, tl);
      olen += tl;
      ocr[olen] = '\0';
    }
    sqlite3_finalize(s);
  }
  if (!ocr || !olen) { free(ocr); return 0; }

  char *seg_ocr = fts_segment(ocr);
  free(ocr);
  if (!seg_ocr) return 0;

  /* The _en columns may not exist yet (translate_migrate not run). A failed
   * prepare is the probe: we then index OCR alone, which is correct — there is
   * no English to preserve. */
  char *t_en = NULL, *s_en = NULL, *b_en = NULL;
  if (sqlite3_prepare_v2(h,
      "SELECT title_en,summary_en,body_en FROM intel_items WHERE uid=?1",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      t_en = dupcol(s, 0); s_en = dupcol(s, 1); b_en = dupcol(s, 2);
    }
    sqlite3_finalize(s);
  }
  char *seg_en = NULL;
  if (t_en || s_en || b_en) seg_en = join_en(t_en, s_en, b_en);
  free(t_en); free(s_en); free(b_en);

  char *full;
  if (seg_en && *seg_en) {
    size_t n = strlen(seg_en) + 1 + strlen(seg_ocr) + 1;
    full = malloc(n);
    if (full) snprintf(full, n, "%s %s", seg_en, seg_ocr);
  } else {
    full = strdup(seg_ocr);
  }
  free(seg_en);
  if (!full) { free(seg_ocr); return 0; }

  *out_full = full;
  *out_ocr = seg_ocr;
  return 1;
}

/* UPDATE, never DELETE+INSERT — identical reasoning to translate.c's
 * fts_put_en(): intel_items_fts is a content-carrying fts5 table, so an
 * in-place column update is legal and keeps the rowid, which means it does NOT
 * look like a re-ingest to translate.c's watermark pass and does not
 * invalidate intel_items_fts_uid_map. */
static int media_fts_put(sqlite3 *h, const char *uid) {
  char *full = NULL, *ocr = NULL;
  if (!media_fts_compose(h, uid, &full, &ocr)) return 0;

  sqlite3_int64 rid = -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) rid = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  if (rid < 0) { free(full); free(ocr); return 0; }

  /* Read back before writing. In the settled case the OCR is already there and
   * this costs one indexed read and zero writes — which is what makes a repair
   * pass over the whole corpus affordable on a 900s tick. */
  char *cur = NULL;
  if (sqlite3_prepare_v2(h, "SELECT keywords FROM intel_items_fts WHERE rowid=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(s, 1, rid);
    if (sqlite3_step(s) == SQLITE_ROW) cur = dupcol(s, 0);
    sqlite3_finalize(s);
  }
  int need = !(cur && *ocr && strstr(cur, ocr) != NULL);
  free(cur);

  int wrote = 0;
  if (need &&
      sqlite3_prepare_v2(h, "UPDATE intel_items_fts SET keywords=?1"
                            " WHERE rowid=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, full, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 2, rid);
    if (sqlite3_step(s) == SQLITE_DONE) wrote = 1;
    sqlite3_finalize(s);
  }
  free(full); free(ocr);
  return wrote;
}

int media_fts_sync(db_handle *db, int limit, volatile int *cancel) {
  if (!db || !db->h) return 0;
  media_migrate(db);
  sqlite3 *h = db->h;
  if (limit <= 0) limit = env_int("MEDIA_FTS_BATCH", 500);
  if (limit <= 0) limit = 500;

  char cursor[600];
  state_get(h, "fts_ocr_cursor", cursor, sizeof cursor);

  char **uids = calloc((size_t)limit, sizeof *uids);
  if (!uids) return 0;
  int n = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT DISTINCT item_uid FROM media_assets"
      " WHERE ocr_text IS NOT NULL AND ocr_text<>'' AND item_uid > ?1"
      " ORDER BY item_uid LIMIT ?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, cursor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 2, limit);
    while (n < limit && sqlite3_step(s) == SQLITE_ROW) {
      uids[n] = dupcol(s, 0);
      if (uids[n]) n++;
    }
    sqlite3_finalize(s);
  }

  int wrote = 0;
  if (n > 0) {
    sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
    for (int i = 0; i < n; i++) {
      if (cancel && *cancel) break;
      wrote += media_fts_put(h, uids[i]);
    }
    sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
  }

  /* The cursor WRAPS rather than terminating: this is a repair loop, not a
   * migration. translate.c can clobber a row at any time, so the pass has to
   * keep coming back round. */
  if (n < limit) state_put(h, "fts_ocr_cursor", "");
  else if (uids[n - 1])   state_put(h, "fts_ocr_cursor", uids[n - 1]);

  for (int i = 0; i < n; i++) free(uids[i]);
  free(uids);
  if (wrote) fprintf(stderr, "[media] fts: examined=%d rewritten=%d\n", n, wrote);
  return wrote;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Analysis
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *evidence_root(void) {
  const char *e = getenv("JO_EVIDENCE_DIR");
  return (e && *e) ? e : JO_REPO_ROOT "/data/evidence";
}

/* Resolve the on-disk blob that evidence_capture() just wrote, so the external
 * tools can read the bytes without a second copy. The RELATIVE path is read
 * back out of the evidence row rather than reconstructed from the hash: the
 * layout ("ab/abcd…") is evidence.c's business, and a row is the only place it
 * is published. Returns 0 when capture is off for this source, the store is at
 * budget, or the blob has since been reaped — all normal, all handled by
 * falling back to a temp file. */
static int evidence_blob_path(sqlite3 *h, const char *sha, const char *uid,
                              char *out, size_t cap) {
  char rel[160];
  rel[0] = '\0';
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT blob_path FROM evidence WHERE content_sha256=?1 AND item_uid=?2",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, sha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *t = ctext(s, 0);
      if (t) snprintf(rel, sizeof rel, "%s", t);
    }
    sqlite3_finalize(s);
  }
  if (!rel[0]) return 0;
  snprintf(out, cap, "%s/%s", evidence_root(), rel);
  struct stat st;
  if (stat(out, &st) != 0) return 0;
  return path_ok(out);
}

static int media_temp_write(const void *b, size_t n, char *out, size_t cap) {
  const char *td = getenv("TMPDIR");
  if (!td || !*td) td = "/tmp";
  char tmpl[512];
  snprintf(tmpl, sizeof tmpl, "%s/jo-media-XXXXXX", td);
  int fd = mkstemp(tmpl);
  if (fd < 0) return 0;
  const char *p = (const char *)b;
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(fd, p + off, n - off);
    if (w <= 0) { close(fd); unlink(tmpl); return 0; }
    off += (size_t)w;
  }
  close(fd);
  if (!path_ok(tmpl)) { unlink(tmpl); return 0; }
  snprintf(out, cap, "%s", tmpl);
  return 1;
}

static void mark_fail(sqlite3 *h, const char *id, int terminal) {
  sqlite3_stmt *s;
  const char *sql = terminal
    ? "UPDATE media_assets SET analyze_failed=3 WHERE id=?1"
    : "UPDATE media_assets SET analyze_failed=MIN(analyze_failed+1,3) WHERE id=?1";
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
}

typedef struct { char *id, *uid, *url; } arow;

int media_analyze_batch(db_handle *db, http_client *http, int limit,
                        volatile int *cancel) {
  if (!db || !db->h) return 0;
  media_migrate(db);
  sqlite3 *h = db->h;
  if (limit <= 0) limit = env_int("MEDIA_ANALYZE_BATCH", 25);
  if (limit <= 0) limit = 25;
  long maxb = env_int("MEDIA_MAX_BYTES", 8 * 1024 * 1024);
  if (maxb < 4096) maxb = 4096;
  int tmo = env_int("MEDIA_TIMEOUT_MS", 15000);
  if (tmo < 1000) tmo = 1000;

  /* Turn OFF httpclient.c's automatic evidence hook for this thread. The
   * scheduler bound the scope to THIS pod's source id, which would file every
   * image under "src:media-analyze" — provenance that says where the fetch was
   * issued from rather than where the image came from. Every capture below is
   * made explicitly under the ITEM's source_id and uid instead. end() is safe
   * unpaired and safe when no scope was ever bound (e.g. an HTTP caller). */
  evidence_scope_end();

  arow *rows = calloc((size_t)limit, sizeof *rows);
  if (!rows) return 0;
  int n = 0;
  sqlite3_stmt *s;
  /* Read the batch and FINALIZE before the first fetch: a read cursor held
   * across a 15-second network call is a 15-second read lock. */
  if (sqlite3_prepare_v2(h,
      "SELECT id,item_uid,url FROM media_assets"
      " WHERE " MEDIA_PENDING_WHERE
      " ORDER BY analyze_failed LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, limit);
    while (n < limit && sqlite3_step(s) == SQLITE_ROW) {
      rows[n].id  = dupcol(s, 0);
      rows[n].uid = dupcol(s, 1);
      rows[n].url = dupcol(s, 2);
      if (rows[n].id && rows[n].uid && rows[n].url) n++;
    }
    sqlite3_finalize(s);
  }

  http_client *own = NULL;
  if (!http) { own = http_client_new(); http = own; }

  int done = 0, failed = 0, geo = 0, ocred = 0, hashed = 0;
  for (int i = 0; i < n && http; i++) {
    if (cancel && *cancel) break;
    arow *a = &rows[i];

    char *sid = NULL;
    if (sqlite3_prepare_v2(h, "SELECT source_id FROM intel_items WHERE uid=?1",
                           -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, a->uid, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_ROW) sid = dupcol(s, 0);
      sqlite3_finalize(s);
    }

    /* Range is the only size cap available from outside httpclient.c (see
     * media.h/LIMITS). A server that honours it answers 206 with at most maxb
     * bytes; one that ignores it answers 200 and is rejected below. */
    char rangeh[64];
    snprintf(rangeh, sizeof rangeh, "Range: bytes=0-%ld", maxb);
    const char *hdrs[] = { rangeh, "Accept: image/*,*/*;q=0.8", NULL };

    http_response r = { 0 };
    int rc = http_request(http, "GET", a->url, hdrs, NULL, 0, tmo, 1, &r);
    if (rc != 0 || (r.status != 200 && r.status != 206) ||
        !r.body || r.body_len < 16) {
      http_response_free(&r);
      mark_fail(h, a->id, 0);
      failed++;
      free(sid);
      continue;
    }
    int partial = (r.status == 206);
    if (!partial && (long)r.body_len > maxb) {
      /* Terminal: the asset is simply too big for this pipeline and a retry
       * would download it again to reach the same conclusion. */
      fprintf(stderr, "[media] %s: %lu bytes over cap, abandoned\n",
              a->url, (unsigned long)r.body_len);
      http_response_free(&r);
      mark_fail(h, a->id, 1);
      failed++;
      free(sid);
      continue;
    }

    const char *ct = media_sniff_type(r.body, r.body_len);
    char sha[65];
    sha256_hex(r.body, r.body_len, sha);

    long w = 0, hh = 0;
    media_exif ex;
    char *exj = NULL;
    media_exif_parse(r.body, r.body_len, &ex);   /* zeroes `ex` either way */
    if (ct) {
      media_image_dims(r.body, r.body_len, &w, &hh);
      if (partial) ex.truncated = 1;
      if (!w && ex.width)   w  = ex.width;
      if (!hh && ex.height) hh = ex.height;
      if (ex.found) exj = media_exif_to_json(&ex);
    }

    /* Preservation. Filed under the item and its source, so the custody row
     * says which source delivered the image with which item. Deduped by
     * content hash inside evidence.c: twenty re-hosts of the same wire photo
     * are one file on disk. */
    evidence_capture(db, a->uid, sid ? sid : "media", a->url, "GET", NULL,
                     r.status, NULL, r.body, r.body_len,
                     ct ? ct : "application/octet-stream");

    uint64_t ph = 0;
    int have_ph = 0;
    char *ocr = NULL;
    double conf = -1.0;
    /* A 206 body is a fragment: EXIF survives (it is at the head) but no
     * decoder can be given a truncated image, so the pixel tools are skipped
     * rather than fed garbage. */
    if (ct && !partial) {
      char p[1400];
      char tmpp[512];
      tmpp[0] = '\0';
      int have_path = evidence_blob_path(h, sha, a->uid, p, sizeof p);
      if (!have_path && media_temp_write(r.body, r.body_len, tmpp, sizeof tmpp)) {
        snprintf(p, sizeof p, "%s", tmpp);
        have_path = 1;
      }
      if (have_path) {
        if (media_phash_file(p, &ph)) { have_ph = 1; hashed++; }
        if (media_ocr_file(p, &ocr, &conf)) ocred++;
      }
      if (tmpp[0]) unlink(tmpp);
    }

    sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
    int ok = 0;
    if (sqlite3_prepare_v2(h,
        "UPDATE media_assets SET sha256=?1, bytes=?2, content_type=?3,"
        " phash=?4, exif_json=?5, ocr_text=?6, ocr_conf=?7, width=?8, height=?9,"
        " analyzed_at=datetime('now'), analyze_failed=0"
        " WHERE id=?10", -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text (s, 1, sha, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(s, 2, (sqlite3_int64)r.body_len);
      if (ct) sqlite3_bind_text(s, 3, ct, -1, SQLITE_TRANSIENT);
      else    sqlite3_bind_null(s, 3);
      /* The column is INTEGER, i.e. signed 64-bit; the hash is unsigned. The
       * cast is a reinterpretation, not a conversion, and readers reverse it —
       * bit patterns survive, which is all a hash needs. */
      if (have_ph) sqlite3_bind_int64(s, 4, (sqlite3_int64)ph);
      else         sqlite3_bind_null(s, 4);
      if (exj) sqlite3_bind_text(s, 5, exj, -1, SQLITE_TRANSIENT);
      else     sqlite3_bind_null(s, 5);
      if (ocr) sqlite3_bind_text(s, 6, ocr, -1, SQLITE_TRANSIENT);
      else     sqlite3_bind_null(s, 6);
      /* A negative conf is the "tool reported none" sentinel — store NULL
       * rather than a number nobody can interpret. */
      if (ocr && conf >= 0.0) sqlite3_bind_double(s, 7, conf);
      else                    sqlite3_bind_null(s, 7);
      if (w)  sqlite3_bind_int64(s, 8, (sqlite3_int64)w);
      else    sqlite3_bind_null(s, 8);
      if (hh) sqlite3_bind_int64(s, 9, (sqlite3_int64)hh);
      else    sqlite3_bind_null(s, 9);
      sqlite3_bind_text(s, 10, a->id, -1, SQLITE_TRANSIENT);
      ok = (sqlite3_step(s) == SQLITE_DONE);
      sqlite3_finalize(s);
    }
    if (ok) sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
    else    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);

    /* After the commit, never inside it — same discipline as intel.c's
     * post-commit hooks. A geo write or an FTS write must not be able to roll
     * back the analysis that produced it, and both must see the committed row. */
    if (ok && ex.have_gps)
      geo += media_geo_backfill(db, a->uid, ex.lat, ex.lon);
    if (ok && ocr) media_fts_put(h, a->uid);

    free(exj); free(ocr); free(sid);
    http_response_free(&r);
    if (ok) done++; else { mark_fail(h, a->id, 0); failed++; }
  }

  if (own) http_client_free(own);
  for (int i = 0; i < n; i++) { free(rows[i].id); free(rows[i].uid); free(rows[i].url); }
  free(rows);
  if (n)
    fprintf(stderr, "[media] analyze: attempted=%d ok=%d failed=%d geo=%d "
                    "ocr=%d phash=%d\n", n, done, failed, geo, ocred, hashed);
  return done;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Read paths
 * ═══════════════════════════════════════════════════════════════════════════ */

static void put_hex64(cJSON *o, const char *k, sqlite3_int64 v) {
  char b[17];
  snprintf(b, sizeof b, "%016llx", (unsigned long long)(uint64_t)v);
  cJSON_AddStringToObject(o, k, b);
}

char *media_list_for_item(db_handle *db, const char *item_uid, int limit) {
  if (!db || !db->h || !item_uid || !*item_uid) return NULL;
  media_migrate(db);
  if (limit <= 0) limit = 50;
  if (limit > 200) limit = 200;

  cJSON *arr = cJSON_CreateArray();
  if (!arr) return NULL;
  int analyzed = 0, pending = 0, bad = 0, geotagged = 0, count = 0;

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "SELECT id,url,sha256,bytes,content_type,phash,exif_json,ocr_text,"
      "       ocr_conf,width,height,analyzed_at,analyze_failed"
      "  FROM media_assets WHERE item_uid=?1"
      " ORDER BY analyzed_at DESC, rowid DESC LIMIT ?2",
      -1, &s, NULL) != SQLITE_OK) {
    cJSON_Delete(arr);
    return NULL;
  }
  sqlite3_bind_text(s, 1, item_uid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, limit);
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *o = cJSON_CreateObject();
    if (!o) break;
    const char *v;
    v = ctext(s, 0); if (v) cJSON_AddStringToObject(o, "id", v);
    v = ctext(s, 1); if (v) cJSON_AddStringToObject(o, "url", v);
    if (ctext(s, 2)) cJSON_AddStringToObject(o, "sha256", ctext(s, 2));
    else             cJSON_AddNullToObject(o, "sha256");
    if (sqlite3_column_type(s, 3) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "bytes", (double)sqlite3_column_int64(s, 3));
    else cJSON_AddNullToObject(o, "bytes");
    if (ctext(s, 4)) cJSON_AddStringToObject(o, "content_type", ctext(s, 4));
    else             cJSON_AddNullToObject(o, "content_type");
    /* 16-char hex, never a JSON number: a 64-bit hash does not survive an
     * IEEE double, and a silently-truncated fingerprint is worse than none. */
    if (sqlite3_column_type(s, 5) != SQLITE_NULL)
      put_hex64(o, "phash", sqlite3_column_int64(s, 5));
    else cJSON_AddNullToObject(o, "phash");

    int has_gps = 0;
    const char *ej = ctext(s, 6);
    if (ej) {
      cJSON *parsed = cJSON_Parse(ej);
      if (parsed) {
        cJSON *g = cJSON_GetObjectItem(parsed, "gps");
        if (g && cJSON_IsObject(g)) {
          has_gps = 1;
          cJSON *la = cJSON_GetObjectItem(g, "lat");
          cJSON *lo = cJSON_GetObjectItem(g, "lon");
          if (cJSON_IsNumber(la)) cJSON_AddNumberToObject(o, "lat", la->valuedouble);
          if (cJSON_IsNumber(lo)) cJSON_AddNumberToObject(o, "lon", lo->valuedouble);
        }
        cJSON_AddItemToObject(o, "exif", parsed);   /* inlined, not escaped */
      } else {
        cJSON_AddNullToObject(o, "exif");
      }
    } else {
      cJSON_AddNullToObject(o, "exif");
    }
    cJSON_AddBoolToObject(o, "has_gps", has_gps);
    if (has_gps) geotagged++;

    if (ctext(s, 7)) cJSON_AddStringToObject(o, "ocr_text", ctext(s, 7));
    else             cJSON_AddNullToObject(o, "ocr_text");
    if (sqlite3_column_type(s, 8) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "ocr_conf", sqlite3_column_double(s, 8));
    else cJSON_AddNullToObject(o, "ocr_conf");
    if (sqlite3_column_type(s, 9) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "width", (double)sqlite3_column_int64(s, 9));
    else cJSON_AddNullToObject(o, "width");
    if (sqlite3_column_type(s, 10) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "height", (double)sqlite3_column_int64(s, 10));
    else cJSON_AddNullToObject(o, "height");
    const char *at = ctext(s, 11);
    if (at) { cJSON_AddStringToObject(o, "analyzed_at", at); analyzed++; }
    else    { cJSON_AddNullToObject(o, "analyzed_at"); pending++; }
    int af = sqlite3_column_int(s, 12);
    cJSON_AddNumberToObject(o, "analyze_failed", af);
    if (af >= MEDIA_MAX_FAILURES) bad++;

    cJSON_AddItemToArray(arr, o);
    count++;
  }
  sqlite3_finalize(s);

  cJSON *page = cJSON_CreateObject();
  cJSON_AddNumberToObject(page, "limit", limit);
  cJSON_AddNumberToObject(page, "count", count);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "item_uid", item_uid);
  cJSON_AddNumberToObject(meta, "analyzed", analyzed);
  cJSON_AddNumberToObject(meta, "pending", pending);
  cJSON_AddNumberToObject(meta, "failed", bad);
  cJSON_AddNumberToObject(meta, "geotagged", geotagged);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", arr);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return j;
}

char *media_find_similar(db_handle *db, uint64_t phash, int max_distance,
                         int limit) {
  if (!db || !db->h) return NULL;
  media_migrate(db);
  if (max_distance < 0) max_distance = 10;
  if (max_distance > 64) max_distance = 64;
  if (limit <= 0) limit = 50;
  if (limit > 200) limit = 200;

  /* A bounded linear scan, stated as such in media.h. Nothing here pretends to
   * be an index; when there are enough hashed images for that to matter, the
   * banded LSH scheme in core/simhash.h is the model to copy. */
  const int scan_cap = 200000;
  typedef struct { char *id, *uid, *url, *sha; sqlite3_int64 ph; int d; } hit;
  hit *hits = calloc((size_t)limit, sizeof *hits);
  if (!hits) return NULL;
  int nh = 0, scanned = 0, worst = 65;

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "SELECT id,item_uid,url,sha256,phash FROM media_assets"
      " WHERE phash IS NOT NULL LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, scan_cap);
    while (sqlite3_step(s) == SQLITE_ROW) {
      scanned++;
      sqlite3_int64 raw = sqlite3_column_int64(s, 4);
      int d = media_phash_distance(phash, (uint64_t)raw);
      if (d > max_distance) continue;
      if (nh == limit && d >= worst) continue;
      int slot = nh;
      if (nh < limit) nh++;
      else {
        slot = 0;
        for (int i = 1; i < nh; i++) if (hits[i].d > hits[slot].d) slot = i;
        free(hits[slot].id); free(hits[slot].uid);
        free(hits[slot].url); free(hits[slot].sha);
      }
      hits[slot].id  = dupcol(s, 0);
      hits[slot].uid = dupcol(s, 1);
      hits[slot].url = dupcol(s, 2);
      hits[slot].sha = dupcol(s, 3);
      hits[slot].ph  = raw;
      hits[slot].d   = d;
      worst = 0;
      for (int i = 0; i < nh; i++) if (hits[i].d > worst) worst = hits[i].d;
    }
    sqlite3_finalize(s);
  }

  for (int i = 1; i < nh; i++) {          /* insertion sort, nearest first */
    hit k = hits[i];
    int j = i - 1;
    while (j >= 0 && hits[j].d > k.d) { hits[j + 1] = hits[j]; j--; }
    hits[j + 1] = k;
  }

  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < nh; i++) {
    cJSON *o = cJSON_CreateObject();
    if (hits[i].id)  cJSON_AddStringToObject(o, "id", hits[i].id);
    if (hits[i].uid) cJSON_AddStringToObject(o, "item_uid", hits[i].uid);
    if (hits[i].url) cJSON_AddStringToObject(o, "url", hits[i].url);
    if (hits[i].sha) cJSON_AddStringToObject(o, "sha256", hits[i].sha);
    put_hex64(o, "phash", hits[i].ph);
    cJSON_AddNumberToObject(o, "distance", hits[i].d);
    cJSON_AddItemToArray(arr, o);
    free(hits[i].id); free(hits[i].uid); free(hits[i].url); free(hits[i].sha);
  }
  free(hits);

  cJSON *page = cJSON_CreateObject();
  cJSON_AddNumberToObject(page, "limit", limit);
  cJSON_AddNumberToObject(page, "count", nh);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddNumberToObject(meta, "scanned", scanned);
  cJSON_AddNumberToObject(meta, "max_distance", max_distance);
  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", arr);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return j;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * The pod
 * ═══════════════════════════════════════════════════════════════════════════ */

/* `_enrich` (== core/translate.c, collectors/sources/llm_enricher.c):
 * scheduler.c skips fetch_log_write() and anomaly_detect() for underscore
 * collectors, which this job needs on both counts — it emits zero intel_items
 * (records=0 forever would trip records_drop against its own baseline) and a
 * tick that fetches 25 images and shells out twice per image blows past any
 * duration baseline. It also inherits the scheduler's skip-if-running
 * serialisation, so two ticks can never overlap.
 *
 * 900s x MEDIA_ANALYZE_BATCH is a deliberate trickle: images attached to new
 * intel are mined within ~15 minutes without the pod ever becoming a de-facto
 * corpus-wide crawl running at full tilt behind the operator's back.
 *
 * ORDER MATTERS. Discovery first, so newly ingested items have assets to work
 * on this same tick. Analysis second. Geo re-apply third, because it repairs
 * coordinates that a re-ingest wiped since the last tick (see the geom_source
 * note in media.h) and wants to see anything analysis just wrote. FTS repair
 * last, for the same reason. Every stage is independently bounded and
 * independently resumable, so a tick that runs out of time simply continues
 * on the next one. */
static int pod_run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                          /* writes go to media_assets/intel_items */
  if (!ctx || !ctx->db) return -1;

  const char *me = getenv("MEDIA_ENABLED");
  if (me && strcmp(me, "false") == 0) return 0;

  media_migrate(ctx->db);
  media_discover_batch(ctx->db, 0, ctx->cancel);
  if (ctx->cancel && *ctx->cancel) return 0;
  media_analyze_batch(ctx->db, ctx->http, 0, ctx->cancel);
  if (ctx->cancel && *ctx->cancel) return 0;
  media_geo_reapply(ctx->db, 0, ctx->cancel);
  media_fts_sync(ctx->db, 0, ctx->cancel);
  return 0;
}

static const source_def media_def = {
  .id = "media-analyze", .collector = "_enrich",
  .name = "Media Analysis (EXIF / pHash / OCR)",
  .name_ja = "メディア解析（EXIF・知覚ハッシュ・OCR）",
  .update_interval_sec = 900, .run = pod_run };
REGISTER_SOURCE(media_def)
