/* core/docmeta.c — see docmeta.h for the rationale, the threat model, the work
 * caps and the honest list of what is parsed versus only labelled.
 *
 * Layout, top to bottom: bounded primitives → text/encoding hygiene → PDF →
 * ZIP/OOXML → plain text/HTML/CSV/JSON → sniff → the one public entry point.
 *
 * House rule for this file, enforced by review rather than by the compiler:
 * NOTHING below treats the input as a C string. The input is `const unsigned
 * char *b` plus `size_t len`, always carried as a pair, and the only search
 * primitive is mem_find(). The needles are compile-time literals, so strlen()
 * on a NEEDLE is fine and is used freely; strlen() on the HAYSTACK never
 * appears. */
#include "docmeta.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Bounded primitives.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Substring search over a length-delimited, possibly NUL-containing buffer.
 * memchr-driven so the common case is a libc scan rather than a byte loop.
 * Returns NULL when the needle does not fit or does not occur. */
static const unsigned char *mem_find(const unsigned char *h, size_t hlen,
                                     const char *needle, size_t nlen) {
  if (!h || nlen == 0 || hlen < nlen) return NULL;
  size_t last = hlen - nlen;             /* highest start offset that can fit */
  size_t off = 0;
  while (off <= last) {
    const unsigned char *q = memchr(h + off, (unsigned char)needle[0],
                                    last - off + 1);
    if (!q) return NULL;
    if (memcmp(q, needle, nlen) == 0) return q;
    off = (size_t)(q - h) + 1;
  }
  return NULL;
}

/* ASCII-case-insensitive variant, for HTML where <TITLE> is as legal as
 * <title>. Deliberately ASCII-only: tolower() on a byte >= 0x80 is
 * locale-dependent and would make tag matching depend on the server's LANG. */
static unsigned char lc(unsigned char c) {
  return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

static const unsigned char *mem_find_ci(const unsigned char *h, size_t hlen,
                                        const char *needle, size_t nlen) {
  if (!h || nlen == 0 || hlen < nlen) return NULL;
  size_t last = hlen - nlen;
  for (size_t off = 0; off <= last; off++) {
    size_t k = 0;
    while (k < nlen && lc(h[off + k]) == lc((unsigned char)needle[k])) k++;
    if (k == nlen) return h + off;
  }
  return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Text hygiene. Everything that becomes a JSON string goes through str_finish.
 *
 * Three jobs, and all three are security-relevant rather than cosmetic:
 *   ENCODING  PDF text strings are UTF-16BE-with-BOM or PDFDocEncoding (close
 *             enough to Latin-1); XML is UTF-8; a crafted file is whatever it
 *             wants. cJSON does not validate UTF-8, so raw high bytes would be
 *             emitted verbatim and produce a JSON document that a strict
 *             consumer rejects — an availability bug reachable from a filename.
 *   SCRUB     Control bytes become spaces. A /Producer holding 0x1B or a lone
 *             CR has no business in a JSON string or a terminal.
 *   TRUNCATE  On a CODEPOINT boundary, so truncation cannot manufacture an
 *             invalid sequence out of a valid one.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Decode one UTF-8 sequence. Returns 0 on any malformity — overlong forms,
 * surrogates and > U+10FFFF included, because "decodes without crashing" is
 * not the same as "is valid UTF-8" and only the latter is safe to re-emit. */
static int u8_next(const unsigned char *s, size_t n, size_t *i,
                   unsigned long *cp) {
  size_t p = *i;
  if (p >= n) return 0;
  unsigned char c = s[p];
  size_t need;
  unsigned long v;
  if (c < 0x80)             { *cp = c; *i = p + 1; return 1; }
  else if ((c & 0xE0) == 0xC0) { need = 1; v = c & 0x1Fu; }
  else if ((c & 0xF0) == 0xE0) { need = 2; v = c & 0x0Fu; }
  else if ((c & 0xF8) == 0xF0) { need = 3; v = c & 0x07u; }
  else return 0;
  if (need > n - p - 1) return 0;
  for (size_t k = 1; k <= need; k++) {
    if ((s[p + k] & 0xC0) != 0x80) return 0;
    v = (v << 6) | (unsigned long)(s[p + k] & 0x3F);
  }
  if ((need == 1 && v < 0x80) || (need == 2 && v < 0x800) ||
      (need == 3 && v < 0x10000)) return 0;              /* overlong */
  if (v > 0x10FFFFul || (v >= 0xD800ul && v <= 0xDFFFul)) return 0;
  *cp = v;
  *i = p + need + 1;
  return 1;
}

static int u8_valid(const unsigned char *s, size_t n) {
  size_t i = 0;
  unsigned long cp;
  while (i < n) if (!u8_next(s, n, &i, &cp)) return 0;
  return 1;
}

/* Append one codepoint. Refuses a PARTIAL write, which is what makes the
 * output valid UTF-8 at every truncation point. Returns the new length. */
static size_t u8_put(unsigned char *o, size_t cap, size_t j, unsigned long cp) {
  if (cp < 0x80) {
    if (cap - j < 1) return j;
    o[j++] = (unsigned char)cp;
  } else if (cp < 0x800) {
    if (cap - j < 2) return j;
    o[j++] = (unsigned char)(0xC0 | (cp >> 6));
    o[j++] = (unsigned char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    if (cap - j < 3) return j;
    o[j++] = (unsigned char)(0xE0 | (cp >> 12));
    o[j++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    o[j++] = (unsigned char)(0x80 | (cp & 0x3F));
  } else {
    if (cap - j < 4) return j;
    o[j++] = (unsigned char)(0xF0 | (cp >> 18));
    o[j++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    o[j++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    o[j++] = (unsigned char)(0x80 | (cp & 0x3F));
  }
  return j;
}

/* Raw bytes → scrubbed, truncated, NUL-terminated UTF-8 in `out` (cap includes
 * the terminator). Encoding is decided by BOM first, then by whether the bytes
 * are already valid UTF-8, then by falling back to Latin-1 — which cannot
 * fail, so there is no path that emits an invalid sequence. Leading and
 * trailing whitespace is trimmed. Returns the length written. */
static size_t str_finish(const unsigned char *raw, size_t rn,
                         char *out, size_t cap) {
  unsigned char *o = (unsigned char *)out;
  size_t j = 0, room = cap ? cap - 1 : 0;
  if (!raw || cap == 0) { if (cap) out[0] = 0; return 0; }

  int u16 = 0, big = 0;
  size_t start = 0;
  if (rn >= 2 && raw[0] == 0xFE && raw[1] == 0xFF)      { u16 = 1; big = 1; start = 2; }
  else if (rn >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) { u16 = 1; big = 0; start = 2; }
  else if (rn >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) start = 3;

  if (u16) {
    size_t i = start;
    while (i + 1 < rn && j < room) {
      unsigned long u = big ? ((unsigned long)raw[i] << 8) | raw[i + 1]
                            : ((unsigned long)raw[i + 1] << 8) | raw[i];
      i += 2;
      if (u >= 0xD800ul && u <= 0xDBFFul && i + 1 < rn) {  /* surrogate pair */
        unsigned long lo = big ? ((unsigned long)raw[i] << 8) | raw[i + 1]
                               : ((unsigned long)raw[i + 1] << 8) | raw[i];
        if (lo >= 0xDC00ul && lo <= 0xDFFFul) {
          u = 0x10000ul + ((u - 0xD800ul) << 10) + (lo - 0xDC00ul);
          i += 2;
        }
      }
      /* An unpaired surrogate is not a codepoint; drop it rather than emit
       * CESU-8, which is not valid UTF-8. */
      if (u >= 0xD800ul && u <= 0xDFFFul) continue;
      if (u < 0x20 || u == 0x7F) u = ' ';
      j = u8_put(o, room, j, u);
    }
  } else {
    const unsigned char *s = raw + start;
    size_t n = rn - start;
    if (u8_valid(s, n)) {
      size_t i = 0;
      unsigned long cp;
      while (i < n && j < room && u8_next(s, n, &i, &cp)) {
        if (cp < 0x20 || cp == 0x7F) cp = ' ';
        j = u8_put(o, room, j, cp);
      }
    } else {
      for (size_t i = 0; i < n && j < room; i++) {
        unsigned long cp = s[i];
        if (cp < 0x20 || cp == 0x7F) cp = ' ';
        j = u8_put(o, room, j, cp);
      }
    }
  }
  size_t a = 0;
  while (a < j && o[a] == ' ') a++;
  while (j > a && o[j - 1] == ' ') j--;
  if (a) memmove(o, o + a, j - a);
  j -= a;
  out[j] = 0;
  return j;
}

/* Add a string only when it survived to non-empty. Absent beats empty: see the
 * honest-empty rule in docmeta.h. */
static void add_str(cJSON *o, const char *key, const char *val) {
  if (val && val[0]) cJSON_AddStringToObject(o, key, val);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PDF.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* The head and tail windows a keyed lookup is allowed to touch. Returns the
 * number of windows (1 when they would overlap, i.e. the whole buffer). */
typedef struct { const unsigned char *p; size_t n; } win;

static int pdf_windows(const unsigned char *b, size_t len, win w[2]) {
  if (len <= 2 * DOCMETA_PDF_WINDOW) {
    w[0].p = b; w[0].n = len;
    return 1;
  }
  w[0].p = b;                          w[0].n = DOCMETA_PDF_WINDOW;
  w[1].p = b + (len - DOCMETA_PDF_WINDOW); w[1].n = DOCMETA_PDF_WINDOW;
  return 2;
}

static int pdf_ws(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == 0;
}

static int pdf_delim(unsigned char c) {
  return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' ||
         c == '{' || c == '}' || c == '/' || c == '%';
}

static int hexval(unsigned char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* Read the object that follows a key at `p` (which points just past the key)
 * into `raw`, returning its length. Handles the three forms an Info value
 * actually takes: a (literal string), a <hex string>, and a /Name. Anything
 * else — including "<<", which is a dictionary, not a value — yields 0.
 * `end` is the hard limit; every read is `< end`. */
static size_t pdf_object(const unsigned char *p, const unsigned char *end,
                         unsigned char *raw, size_t cap) {
  while (p < end && pdf_ws(*p)) p++;
  if (p >= end) return 0;
  size_t o = 0;

  if (*p == '(') {
    p++;
    int depth = 1;
    while (p < end && o < cap) {
      unsigned char c = *p;
      if (c == '\\') {
        p++;
        if (p >= end) break;
        unsigned char e = *p++;
        unsigned char ch;
        switch (e) {
          case 'n': ch = '\n'; break;
          case 'r': ch = '\r'; break;
          case 't': ch = '\t'; break;
          case 'b': ch = '\b'; break;
          case 'f': ch = '\f'; break;
          case '\r':                      /* line continuation: \CR, \CRLF */
            if (p < end && *p == '\n') p++;
            continue;
          case '\n': continue;
          default:
            if (e >= '0' && e <= '7') {   /* \ooo, one to three octal digits */
              unsigned v = (unsigned)(e - '0');
              for (int k = 0; k < 2 && p < end && *p >= '0' && *p <= '7'; k++)
                v = v * 8 + (unsigned)(*p++ - '0');
              ch = (unsigned char)(v & 0xFF);
            } else ch = e;                /* \( \) \\ and the no-op escapes */
        }
        raw[o++] = ch;
        continue;
      }
      if (c == '(') depth++;
      else if (c == ')' && --depth == 0) break;
      raw[o++] = c;
      p++;
    }
    return o;
  }

  if (*p == '<') {
    if (end - p >= 2 && p[1] == '<') return 0;          /* dictionary, not a value */
    p++;
    int hi = -1;
    while (p < end && *p != '>' && o < cap) {
      int v = hexval(*p++);
      if (v < 0) continue;                              /* whitespace is legal here */
      if (hi < 0) hi = v;
      else { raw[o++] = (unsigned char)((hi << 4) | v); hi = -1; }
    }
    if (hi >= 0 && o < cap) raw[o++] = (unsigned char)(hi << 4);  /* odd digit → pad */
    return o;
  }

  if (*p == '/') {
    p++;
    while (p < end && !pdf_ws(*p) && !pdf_delim(*p) && o < cap) {
      if (*p == '#' && end - p >= 3) {                  /* #XX name escape */
        int a = hexval(p[1]), c2 = hexval(p[2]);
        if (a >= 0 && c2 >= 0) { raw[o++] = (unsigned char)((a << 4) | c2); p += 3; continue; }
      }
      raw[o++] = *p++;
    }
    return o;
  }
  return 0;
}

/* Look up one Info key across the head and tail windows. First plausible value
 * wins (see the incremental-update caveat in docmeta.h). At most
 * DOCMETA_MAX_KEY_HITS candidate occurrences are parsed per window, so a
 * buffer packed with bare "/Author" tokens costs the window scan and nothing
 * more. Returns 1 and fills `out` (UTF-8, scrubbed, truncated) on success. */
static int pdf_value(const unsigned char *b, size_t len, const char *key,
                     char *out, size_t cap) {
  win w[2];
  int nw = pdf_windows(b, len, w);
  size_t klen = strlen(key);
  unsigned char raw[DOCMETA_RAW_MAX];
  for (int i = 0; i < nw; i++) {
    const unsigned char *p = w[i].p, *end = w[i].p + w[i].n;
    size_t rem = w[i].n;
    for (int hit = 0; hit < DOCMETA_MAX_KEY_HITS; hit++) {
      const unsigned char *q = mem_find(p, rem, key, klen);
      if (!q) break;
      /* The next byte must terminate the key, or "/Type" also matches
       * "/TypeSomething" and we would parse the wrong object. */
      const unsigned char *v = q + klen;
      if (v >= end || pdf_ws(*v) || pdf_delim(*v)) {
        size_t rn = pdf_object(v, end, raw, sizeof raw);
        if (rn > 0) {
          if (str_finish(raw, rn, out, cap) > 0) return 1;
        }
      }
      size_t used = (size_t)(q - p) + klen;
      p += used;
      rem -= used;
    }
  }
  return 0;
}

/* Presence-only probe, same windows, no value parse. */
static int pdf_has(const unsigned char *b, size_t len, const char *tok) {
  win w[2];
  int nw = pdf_windows(b, len, w);
  size_t tl = strlen(tok);
  for (int i = 0; i < nw; i++)
    if (mem_find(w[i].p, w[i].n, tok, tl)) return 1;
  return 0;
}

/* "/Type /Page" and "/Type/Page" occurrences, minus "/Pages". Approximate by
 * construction and named so in the output: a PDF whose page tree lives in a
 * compressed object stream reports 0 here, which is why 0 is omitted rather
 * than emitted as a page count of zero. */
static int pdf_pages(const unsigned char *b, size_t len) {
  size_t n = len < DOCMETA_PAGE_SCAN ? len : DOCMETA_PAGE_SCAN;
  int total = 0;
  static const char *pats[2] = { "/Type/Page", "/Type /Page" };
  for (int k = 0; k < 2; k++) {
    size_t pl = strlen(pats[k]);
    const unsigned char *p = b;
    size_t rem = n;
    const unsigned char *q;
    while ((q = mem_find(p, rem, pats[k], pl)) != NULL) {
      const unsigned char *after = q + pl;
      /* Exclude "/Pages", the page-tree node, from the leaf count. */
      if (after >= b + n || *after != 's') total++;
      size_t used = (size_t)(q - p) + pl;
      p += used;
      rem -= used;
    }
  }
  return total;
}

/* D:YYYYMMDDHHmmSSOHH'mm' → ISO-8601, at the precision actually supplied.
 * Every field is range-checked; an out-of-range component means the whole
 * string is refused (0 returned) and only the raw form is reported. Inventing
 * "2019-13-45" from garbage would be worse than saying nothing. */
static int pdf_date_iso(const char *in, char *out, size_t cap) {
  const unsigned char *s = (const unsigned char *)in;
  size_t n = strlen(in), i = 0;
  if (n >= 2 && s[0] == 'D' && s[1] == ':') i = 2;
  int f[6] = { 0, 0, 0, 0, 0, 0 };      /* Y M D h m s */
  int have = 0;
  for (; have < 6; have++) {
    size_t need = have == 0 ? 4 : 2;
    if (n - i < need) break;
    int v = 0;
    size_t k = 0;
    for (; k < need; k++) {
      if (s[i + k] < '0' || s[i + k] > '9') break;
      v = v * 10 + (s[i + k] - '0');
    }
    if (k < need) break;
    f[have] = v;
    i += need;
  }
  if (have < 1) return 0;
  if (have >= 2 && (f[1] < 1 || f[1] > 12)) return 0;
  if (have >= 3 && (f[2] < 1 || f[2] > 31)) return 0;
  if (have >= 4 && f[3] > 23) return 0;
  if (have >= 5 && f[4] > 59) return 0;
  if (have >= 6 && f[5] > 60) return 0;                 /* leap second */
  if (f[0] < 1 || f[0] > 9999) return 0;

  char tz[8] = { 0 };
  if (i < n) {
    unsigned char o = s[i];
    if (o == 'Z' || o == 'z') tz[0] = 'Z';
    else if ((o == '+' || o == '-') && n - i >= 3 &&
             s[i + 1] >= '0' && s[i + 1] <= '9' && s[i + 2] >= '0' && s[i + 2] <= '9') {
      int oh = (s[i + 1] - '0') * 10 + (s[i + 2] - '0');
      int om = 0;
      size_t j = i + 3;
      if (j < n && s[j] == '\'') j++;
      if (n - j >= 2 && s[j] >= '0' && s[j] <= '9' && s[j + 1] >= '0' && s[j + 1] <= '9')
        om = (s[j] - '0') * 10 + (s[j + 1] - '0');
      if (oh <= 23 && om <= 59)
        snprintf(tz, sizeof tz, "%c%02d:%02d", (char)o, oh, om);
    }
  }
  /* Built at the precision available. A PDF that gives only a year gets
   * "2019", not "2019-01-01T00:00:00" — the zeros would be fabricated. */
  int w;
  if (have >= 6)      w = snprintf(out, cap, "%04d-%02d-%02dT%02d:%02d:%02d%s",
                                   f[0], f[1], f[2], f[3], f[4], f[5], tz);
  else if (have == 5) w = snprintf(out, cap, "%04d-%02d-%02dT%02d:%02d%s",
                                   f[0], f[1], f[2], f[3], f[4], tz);
  else if (have == 4) w = snprintf(out, cap, "%04d-%02d-%02dT%02d%s",
                                   f[0], f[1], f[2], f[3], tz);
  else if (have == 3) w = snprintf(out, cap, "%04d-%02d-%02d", f[0], f[1], f[2]);
  else if (have == 2) w = snprintf(out, cap, "%04d-%02d", f[0], f[1]);
  else                w = snprintf(out, cap, "%04d", f[0]);
  return (w > 0 && (size_t)w < cap);
}

/* Emit a PDF date as both the normalised and the raw form. Keeping the raw
 * string matters forensically: the offset notation and any producer-specific
 * malformity is evidence about the tool that wrote the file. */
static void pdf_add_date(cJSON *o, const char *key, const char *raw_key,
                         const char *raw) {
  if (!raw || !raw[0]) return;
  char iso[48];
  if (pdf_date_iso(raw, iso, sizeof iso)) cJSON_AddStringToObject(o, key, iso);
  cJSON_AddStringToObject(o, raw_key, raw);
}

static void pdf_extract(const unsigned char *b, size_t len, cJSON *root) {
  cJSON *o = cJSON_CreateObject();
  if (!o) return;
  cJSON_AddItemToObject(root, "pdf", o);

  /* Version from the header line: "%PDF-1.7". Bounded to 8 bytes and stopped
   * at any non-version byte, so a header of "%PDF-" followed by 64 MiB of
   * 'A' yields nothing rather than a long string. */
  if (len > 5) {
    char ver[9];
    size_t v = 0;
    for (size_t i = 5; i < len && v < sizeof ver - 1; i++) {
      unsigned char c = b[i];
      if ((c >= '0' && c <= '9') || c == '.') ver[v++] = (char)c;
      else break;
    }
    ver[v] = 0;
    add_str(o, "version", ver);
  }

  static const struct { const char *key, *out; } KEYS[] = {
    { "/Title",    "title"    }, { "/Author",   "author"   },
    { "/Subject",  "subject"  }, { "/Keywords", "keywords" },
    { "/Creator",  "creator"  }, { "/Producer", "producer" },
  };
  char val[DOCMETA_MAX_VALUE];
  for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++)
    if (pdf_value(b, len, KEYS[i].key, val, sizeof val))
      add_str(o, KEYS[i].out, val);

  if (pdf_value(b, len, "/CreationDate", val, sizeof val))
    pdf_add_date(o, "created", "created_raw", val);
  if (pdf_value(b, len, "/ModDate", val, sizeof val))
    pdf_add_date(o, "modified", "modified_raw", val);
  cJSON_AddStringToObject(o, "dates_normalized", "iso-8601");

  int pages = pdf_pages(b, len);
  if (pages > 0) {
    cJSON_AddNumberToObject(o, "approx_pages", pages);
    if (len > DOCMETA_PAGE_SCAN) cJSON_AddBoolToObject(o, "page_count_partial", 1);
  }

  int enc = pdf_has(b, len, "/Encrypt");
  cJSON_AddBoolToObject(o, "encrypted", enc);
  cJSON_AddBoolToObject(o, "linearized", pdf_has(b, len, "/Linearized"));
  if (pdf_has(b, len, "/JavaScript") || pdf_has(b, len, "/JS"))
    cJSON_AddBoolToObject(o, "has_javascript", 1);
  if (pdf_has(b, len, "/EmbeddedFiles")) cJSON_AddBoolToObject(o, "has_embedded_files", 1);
  if (pdf_has(b, len, "/AcroForm"))      cJSON_AddBoolToObject(o, "has_acroform", 1);
  if (enc)
    cJSON_AddStringToObject(o, "note",
      "/Encrypt present: Info strings may be ciphertext and are not decrypted");

  /* Say so when the middle of the file was never looked at, so a caller can
   * tell "no author" from "no author in the 2 MiB we read". */
  if (len > 2 * DOCMETA_PDF_WINDOW) cJSON_AddBoolToObject(o, "scan_windowed", 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ZIP / OOXML. Bounded central-directory walk plus a capped raw inflate.
 * See docmeta.h for why lib/zipread.c is not called from here.
 * ═══════════════════════════════════════════════════════════════════════════ */

static unsigned zrd16(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8);
}
static unsigned long zrd32(const unsigned char *p) {
  return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
         ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

#define ZK_ZIP  0
#define ZK_DOCX 1
#define ZK_XLSX 2
#define ZK_PPTX 3

typedef struct {
  int    entries;        /* central-directory entries seen (capped)          */
  int    kind;           /* ZK_*                                             */
  int    found;          /* the wanted entry was located                     */
  size_t lho;            /* its local header offset                          */
  unsigned method;
  unsigned long comp, uncomp;
} zipidx;

/* Which OOXML flavour a member path implies. Checked against the payload
 * directories rather than [Content_Types].xml because the directory prefix is
 * present in every real document and needs no inflate to see. */
static int zip_kind_of(const char *name) {
  if (strncmp(name, "word/", 5) == 0) return ZK_DOCX;
  if (strncmp(name, "xl/",   3) == 0) return ZK_XLSX;
  if (strncmp(name, "ppt/",  4) == 0) return ZK_PPTX;
  return ZK_ZIP;
}

/* Walk the central directory once. `want` may be NULL (kind/entry count only).
 * Every field is re-bounded against the buffer; the walk stops on the first
 * structure that does not fit rather than trusting the declared entry count. */
static int zip_index(const unsigned char *b, size_t len, const char *want,
                     zipidx *zi) {
  memset(zi, 0, sizeof *zi);
  if (!b || len < 22) return 0;

  /* End Of Central Directory: scan back over the maximum comment size. */
  size_t maxback = len < 66000 ? len : 66000;
  size_t eocd = 0;
  int got = 0;
  for (size_t back = 22; back <= maxback; back++) {
    size_t p = len - back;
    if (zrd32(b + p) == 0x06054b50UL) { eocd = p; got = 1; break; }
  }
  if (!got) return 0;

  unsigned nent = zrd16(b + eocd + 10);
  unsigned long cdoff = zrd32(b + eocd + 16);
  if (cdoff >= len) return 0;

  size_t p = (size_t)cdoff;
  unsigned cap = nent < DOCMETA_ZIP_MAX_ENTRIES ? nent : DOCMETA_ZIP_MAX_ENTRIES;
  for (unsigned i = 0; i < cap; i++) {
    if (p > len || len - p < 46) break;
    if (zrd32(b + p) != 0x02014b50UL) break;
    unsigned nlen = zrd16(b + p + 28);
    unsigned elen = zrd16(b + p + 30);
    unsigned clen = zrd16(b + p + 32);
    if (len - p - 46 < nlen) break;

    char nbuf[512];
    size_t ncopy = nlen < sizeof nbuf - 1 ? nlen : sizeof nbuf - 1;
    memcpy(nbuf, b + p + 46, ncopy);
    nbuf[ncopy] = 0;
    zi->entries++;
    if (zi->kind == ZK_ZIP) zi->kind = zip_kind_of(nbuf);

    if (want && !zi->found && strcmp(nbuf, want) == 0) {
      zi->found  = 1;
      zi->method = zrd16(b + p + 10);
      zi->comp   = zrd32(b + p + 20);
      zi->uncomp = zrd32(b + p + 24);
      zi->lho    = (size_t)zrd32(b + p + 42);
    }

    size_t adv = 46 + (size_t)nlen + (size_t)elen + (size_t)clen;
    if (len - p < adv) break;
    p += adv;
  }
  return 1;
}

/* Read the indexed entry's bytes, NUL-terminated, capped at
 * DOCMETA_ZIP_MAX_UNCOMP in ONE allocation that never grows. *truncated is set
 * when the member was larger than the cap; the prefix is still returned
 * because docProps XML puts the interesting elements early. Returns NULL for
 * anything unsupported or malformed. Caller frees. */
static unsigned char *zip_read(const unsigned char *b, size_t len,
                               const zipidx *zi, size_t *out_len,
                               int *truncated) {
  *out_len = 0;
  *truncated = 0;
  if (!zi->found) return NULL;
  size_t lho = zi->lho;
  if (lho > len || len - lho < 30) return NULL;
  if (zrd32(b + lho) != 0x04034b50UL) return NULL;
  unsigned lnlen = zrd16(b + lho + 26), lelen = zrd16(b + lho + 28);
  size_t hdr = 30 + (size_t)lnlen + (size_t)lelen;
  if (len - lho < hdr) return NULL;
  size_t doff = lho + hdr;
  size_t avail = len - doff;
  size_t comp = zi->comp > avail ? avail : (size_t)zi->comp;
  const unsigned char *data = b + doff;

  if (zi->method == 0) {                                     /* stored */
    size_t n = zi->uncomp && (size_t)zi->uncomp <= avail ? (size_t)zi->uncomp : avail;
    if (n > DOCMETA_ZIP_MAX_UNCOMP) { n = DOCMETA_ZIP_MAX_UNCOMP; *truncated = 1; }
    unsigned char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, data, n);
    o[n] = 0;
    *out_len = n;
    return o;
  }
  if (zi->method != 8) return NULL;                          /* deflate only */
  if (comp == 0) return NULL;

  /* THE BOMB CONTAINMENT. Fixed ceiling, no realloc, no trust in `uncomp`:
   * whatever the file declares, at most DOCMETA_ZIP_MAX_UNCOMP bytes are ever
   * produced and the stream is abandoned the moment the buffer is full. */
  size_t cap = DOCMETA_ZIP_MAX_UNCOMP;
  unsigned char *o = malloc(cap + 1);
  if (!o) return NULL;
  z_stream s;
  memset(&s, 0, sizeof s);
  if (inflateInit2(&s, -15) != Z_OK) { free(o); return NULL; }
  s.next_in = (Bytef *)data;
  s.avail_in = (uInt)comp;
  s.next_out = (Bytef *)o;
  s.avail_out = (uInt)cap;
  int rc = inflate(&s, Z_FINISH);
  size_t total = cap - s.avail_out;
  inflateEnd(&s);
  /* Z_OK / Z_BUF_ERROR with a full output buffer is the cap biting, not a
   * failure. Z_STREAM_END is a clean finish. Anything else with no output is
   * a corrupt member. */
  if (rc != Z_STREAM_END) {
    if (total == 0) { free(o); return NULL; }
    if (s.avail_out == 0) *truncated = 1;
  }
  o[total] = 0;
  *out_len = total;
  return o;
}

/* Fallback for a ZIP whose EOCD is missing (truncated download, or the archive
 * is an appended blob): read the FIRST local header's name only. Enough to
 * separate a .docx from a bare .zip, and bounded to one header. */
static int zip_kind_from_local(const unsigned char *b, size_t len) {
  if (len < 30 || zrd32(b) != 0x04034b50UL) return ZK_ZIP;
  unsigned nlen = zrd16(b + 26);
  if (len - 30 < nlen) return ZK_ZIP;
  char nbuf[512];
  size_t ncopy = nlen < sizeof nbuf - 1 ? nlen : sizeof nbuf - 1;
  memcpy(nbuf, b + 30, ncopy);
  nbuf[ncopy] = 0;
  return zip_kind_of(nbuf);
}

/* ── OOXML docProps XML ──────────────────────────────────────────────────── */

/* &amp; &#12345; and friends → bytes, into a bounded raw buffer that
 * str_finish() then validates. Numeric refs are emitted as UTF-8, so an XML
 * file that is itself valid cannot produce an invalid output string. */
static size_t xml_unescape(const unsigned char *s, size_t n,
                           unsigned char *raw, size_t cap) {
  size_t j = 0;
  for (size_t i = 0; i < n && j < cap;) {
    if (s[i] != '&') { raw[j++] = s[i++]; continue; }
    const unsigned char *semi = memchr(s + i, ';', n - i > 12 ? 12 : n - i);
    if (!semi) { raw[j++] = s[i++]; continue; }
    size_t elen = (size_t)(semi - (s + i)) - 1;           /* between & and ; */
    const unsigned char *e = s + i + 1;
    unsigned long cp = 0;
    int ok = 0;
    if (elen == 3 && memcmp(e, "amp", 3) == 0)       { cp = '&';  ok = 1; }
    else if (elen == 2 && memcmp(e, "lt", 2) == 0)   { cp = '<';  ok = 1; }
    else if (elen == 2 && memcmp(e, "gt", 2) == 0)   { cp = '>';  ok = 1; }
    else if (elen == 4 && memcmp(e, "quot", 4) == 0) { cp = '"';  ok = 1; }
    else if (elen == 4 && memcmp(e, "apos", 4) == 0) { cp = '\''; ok = 1; }
    else if (elen >= 2 && e[0] == '#') {
      size_t k = 1;
      int base = 10;
      if (e[k] == 'x' || e[k] == 'X') { base = 16; k++; }
      unsigned long v = 0;
      int digits = 0;
      for (; k < elen; k++) {
        int d = hexval(e[k]);
        if (d < 0 || d >= base) { digits = 0; break; }
        v = v * (unsigned long)base + (unsigned long)d;
        if (v > 0x10FFFFul) { digits = 0; break; }
        digits++;
      }
      if (digits > 0) { cp = v; ok = 1; }
    }
    if (!ok) { raw[j++] = s[i++]; continue; }
    j = u8_put(raw, cap, j, cp);
    i += elen + 2;
  }
  return j;
}

/* Text content of the first <tag>…</tag>. Attributes are tolerated, a
 * self-closing tag yields nothing, and every read is bounded by `n`. */
static int xml_tag(const unsigned char *b, size_t n, const char *tag,
                   char *out, size_t cap) {
  char open[64];
  int w = snprintf(open, sizeof open, "<%s", tag);
  if (w <= 0 || (size_t)w >= sizeof open) return 0;
  const unsigned char *p = b;
  size_t rem = n;
  const unsigned char *q;
  for (int hit = 0; hit < DOCMETA_MAX_KEY_HITS; hit++) {
    q = mem_find(p, rem, open, (size_t)w);
    if (!q) return 0;
    const unsigned char *after = q + w;
    size_t arem = n - (size_t)(after - b);
    if (arem > 0 && (*after == '>' || *after == ' ' || *after == '\t' ||
                     *after == '\r' || *after == '\n' || *after == '/')) {
      const unsigned char *gt = memchr(after, '>', arem);
      if (gt && gt[-1] != '/') {          /* gt[-1] is inside the tag name */
        const unsigned char *val = gt + 1;
        size_t vrem = n - (size_t)(val - b);
        const unsigned char *lt = memchr(val, '<', vrem);
        size_t vlen = lt ? (size_t)(lt - val) : vrem;
        if (vlen > DOCMETA_RAW_MAX) vlen = DOCMETA_RAW_MAX;
        unsigned char raw[DOCMETA_RAW_MAX];
        size_t rn = xml_unescape(val, vlen, raw, sizeof raw);
        if (str_finish(raw, rn, out, cap) > 0) return 1;
      }
    }
    size_t used = (size_t)(q - p) + (size_t)w;
    p += used;
    rem -= used;
  }
  return 0;
}

static void xml_add(cJSON *o, const unsigned char *b, size_t n,
                    const char *tag, const char *key) {
  char v[DOCMETA_MAX_VALUE];
  if (xml_tag(b, n, tag, v, sizeof v)) add_str(o, key, v);
}

/* An <Words>3120</Words>-style integer. Emitted as a number only when the whole
 * value is digits — "many" stays out of the JSON rather than becoming 0. */
static void xml_add_num(cJSON *o, const unsigned char *b, size_t n,
                        const char *tag, const char *key) {
  char v[32];
  if (!xml_tag(b, n, tag, v, sizeof v)) return;
  for (size_t i = 0; v[i]; i++) if (v[i] < '0' || v[i] > '9') return;
  if (!v[0]) return;
  cJSON_AddNumberToObject(o, key, (double)strtod(v, NULL));
}

static void office_extract(const unsigned char *b, size_t len, int kind,
                           const zipidx *base, cJSON *root) {
  cJSON *o = cJSON_CreateObject();
  if (!o) return;
  cJSON_AddItemToObject(root, "office", o);
  cJSON_AddStringToObject(o, "container", "zip");
  cJSON_AddStringToObject(o, "format",
    kind == ZK_DOCX ? "docx" : kind == ZK_XLSX ? "xlsx" :
    kind == ZK_PPTX ? "pptx" : "zip");
  if (base->entries > 0) cJSON_AddNumberToObject(o, "entries", base->entries);

  int any = 0;
  zipidx zi;
  size_t n = 0;
  int trunc = 0;
  unsigned char *x;

  if (zip_index(b, len, "docProps/core.xml", &zi) && zi.found &&
      (x = zip_read(b, len, &zi, &n, &trunc)) != NULL) {
    cJSON *c = cJSON_CreateObject();
    if (c) {
      cJSON_AddItemToObject(o, "core", c);
      /* dc:creator is the author; cp:lastModifiedBy is who saved it LAST and
       * is routinely a different, more interesting person. Both are kept
       * under their own names rather than merged into one "author". */
      xml_add(c, x, n, "dc:creator",        "creator");
      xml_add(c, x, n, "cp:lastModifiedBy", "last_modified_by");
      xml_add(c, x, n, "dc:title",          "title");
      xml_add(c, x, n, "dc:subject",        "subject");
      xml_add(c, x, n, "dc:description",    "description");
      xml_add(c, x, n, "cp:keywords",       "keywords");
      xml_add(c, x, n, "cp:category",       "category");
      xml_add(c, x, n, "cp:revision",       "revision");
      xml_add(c, x, n, "cp:contentStatus",  "content_status");
      /* dcterms:* are already ISO-8601 in OOXML; passed through, not reformatted. */
      xml_add(c, x, n, "dcterms:created",   "created");
      xml_add(c, x, n, "dcterms:modified",  "modified");
      if (trunc) cJSON_AddBoolToObject(c, "truncated", 1);
      any = 1;
    }
    free(x);
  }

  if (zip_index(b, len, "docProps/app.xml", &zi) && zi.found &&
      (x = zip_read(b, len, &zi, &n, &trunc)) != NULL) {
    cJSON *a = cJSON_CreateObject();
    if (a) {
      cJSON_AddItemToObject(o, "app", a);
      xml_add(a, x, n, "Application", "application");
      xml_add(a, x, n, "AppVersion",  "app_version");
      xml_add(a, x, n, "Company",     "company");
      xml_add(a, x, n, "Manager",     "manager");
      xml_add(a, x, n, "Template",    "template");
      xml_add_num(a, x, n, "TotalTime",  "total_edit_minutes");
      xml_add_num(a, x, n, "Pages",      "pages");
      xml_add_num(a, x, n, "Words",      "words");
      xml_add_num(a, x, n, "Characters", "characters");
      if (trunc) cJSON_AddBoolToObject(a, "truncated", 1);
      any = 1;
    }
    free(x);
  }

  /* Honest about WHY nothing came out, because "no docProps" and "the archive
   * was unreadable" are different findings. */
  if (!any)
    cJSON_AddStringToObject(o, "docprops",
      base->entries > 0 ? "absent-or-unsupported-compression" : "unreadable-archive");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Plain text, HTML, CSV, JSON.
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *text_encoding(const unsigned char *b, size_t n, int *bom) {
  *bom = 0;
  if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) { *bom = 1; return "utf-8"; }
  if (n >= 2 && b[0] == 0xFE && b[1] == 0xFF) { *bom = 1; return "utf-16be"; }
  if (n >= 2 && b[0] == 0xFF && b[1] == 0xFE) { *bom = 1; return "utf-16le"; }
  int high = 0;
  for (size_t i = 0; i < n; i++) if (b[i] >= 0x80) { high = 1; break; }
  if (!high) return "ascii";
  return u8_valid(b, n) ? "utf-8" : "unknown-8bit";
}

/* Delimiter-count agreement across the first few lines. A heuristic, reported
 * as one: quoted fields are honoured so an address containing a comma does not
 * break the count, but nothing here validates the whole file. */
static int csv_probe(const unsigned char *b, size_t n, char *delim, int *cols) {
  static const char CAND[] = { ',', '\t', ';', '|' };
  size_t scan = n < 65536 ? n : 65536;
  for (size_t d = 0; d < sizeof CAND; d++) {
    int counts[8];
    int lines = 0;
    size_t i = 0;
    while (i < scan && lines < 8) {
      int q = 0, c = 0;
      size_t start = i;
      while (i < scan && b[i] != '\n') {
        if (b[i] == '"') q = !q;
        else if (!q && b[i] == (unsigned char)CAND[d]) c++;
        i++;
      }
      if (i > start || (i < scan && b[i] == '\n')) counts[lines++] = c;
      if (i < scan) i++;
    }
    if (lines < 2 || counts[0] < 1) continue;
    int ok = 1;
    for (int k = 1; k < lines; k++) if (counts[k] != counts[0]) { ok = 0; break; }
    if (ok) { *delim = CAND[d]; *cols = counts[0] + 1; return 1; }
  }
  return 0;
}

/* <meta name=X content=Y> for the three names worth having. The tag text is
 * copied into a bounded scratch buffer first so attribute order does not
 * matter and no scan escapes the tag. */
static void html_meta(const unsigned char *b, size_t n, cJSON *o) {
  const unsigned char *p = b;
  size_t rem = n;
  const unsigned char *q;
  for (int t = 0; t < DOCMETA_MAX_META_TAGS; t++) {
    q = mem_find_ci(p, rem, "<meta", 5);
    if (!q) return;
    size_t off = (size_t)(q - p);
    size_t after = rem - off - 5;
    const unsigned char *gt = memchr(q + 5, '>', after > 1024 ? 1024 : after);
    if (gt) {
      size_t tl = (size_t)(gt - (q + 5));
      char tag[1025];
      memcpy(tag, q + 5, tl);
      tag[tl] = 0;
      for (size_t i = 0; i < tl; i++) tag[i] = (char)lc((unsigned char)tag[i]);
      const char *key = NULL;
      if (strstr(tag, "generator"))        key = "generator";
      else if (strstr(tag, "\"author\"") || strstr(tag, "'author'") ||
               strstr(tag, "=author"))     key = "author";
      else if (strstr(tag, "description")) key = "description";
      if (key) {
        /* Re-find content= in the ORIGINAL bytes, so the value keeps its case. */
        const unsigned char *cp = mem_find_ci(q + 5, tl, "content", 7);
        if (cp) {
          const unsigned char *v = cp + 7, *tend = q + 5 + tl;
          while (v < tend && (*v == ' ' || *v == '=')) v++;
          unsigned char quote = 0;
          if (v < tend && (*v == '"' || *v == '\'')) { quote = *v; v++; }
          const unsigned char *ve = v;
          while (ve < tend && (quote ? *ve != quote : (*ve != ' ' && *ve != '\t'))) ve++;
          size_t vlen = (size_t)(ve - v);
          if (vlen > DOCMETA_RAW_MAX) vlen = DOCMETA_RAW_MAX;
          unsigned char raw[DOCMETA_RAW_MAX];
          size_t rn = xml_unescape(v, vlen, raw, sizeof raw);
          char out[DOCMETA_MAX_VALUE];
          if (str_finish(raw, rn, out, sizeof out) > 0 &&
              !cJSON_GetObjectItem(o, key))
            cJSON_AddStringToObject(o, key, out);
        }
      }
    }
    size_t used = off + 5;
    p += used;
    rem -= used;
  }
}

static void text_extract(const unsigned char *b, size_t len, const char *type,
                         cJSON *root) {
  cJSON *o = cJSON_CreateObject();
  if (!o) return;
  cJSON_AddItemToObject(root, "text", o);

  size_t n = len < DOCMETA_TEXT_SCAN ? len : DOCMETA_TEXT_SCAN;
  int bom = 0;
  const char *enc = text_encoding(b, n, &bom);
  cJSON_AddStringToObject(o, "encoding", enc);
  cJSON_AddBoolToObject(o, "bom", bom);

  long nl = 0;
  const unsigned char *p = b;
  size_t rem = n;
  const unsigned char *q;
  while ((q = memchr(p, '\n', rem)) != NULL) {
    nl++;
    rem -= (size_t)(q - p) + 1;
    p = q + 1;
  }
  /* A trailing fragment with no newline is still a line. */
  long lines = nl + (n > 0 && b[n - 1] != '\n' ? 1 : 0);
  cJSON_AddNumberToObject(o, "lines", (double)lines);
  if (len > n) cJSON_AddBoolToObject(o, "lines_partial", 1);
  if (mem_find(b, n < 4096 ? n : 4096, "\r\n", 2))
    cJSON_AddStringToObject(o, "line_endings", "crlf");

  if (strcmp(type, "csv") == 0) {
    char d = ',';
    int cols = 0;
    if (csv_probe(b, n, &d, &cols)) {
      char ds[2] = { d == '\t' ? 't' : d, 0 };
      cJSON_AddStringToObject(o, "delimiter", d == '\t' ? "\\t" : ds);
      cJSON_AddNumberToObject(o, "columns", cols);
    }
    cJSON_AddStringToObject(o, "detection", "heuristic");
  } else if (strcmp(type, "json") == 0) {
    size_t i = 0;
    while (i < n && (b[i] == ' ' || b[i] == '\t' || b[i] == '\r' || b[i] == '\n')) i++;
    if (i < n) cJSON_AddStringToObject(o, "structure", b[i] == '[' ? "array" : "object");
    cJSON_AddStringToObject(o, "detection", "heuristic-not-validated");
  } else if (strcmp(type, "html") == 0) {
    size_t h = len < DOCMETA_HTML_SCAN ? len : DOCMETA_HTML_SCAN;
    const unsigned char *t = mem_find_ci(b, h, "<title", 6);
    if (t) {
      size_t trem = h - (size_t)(t - b);
      const unsigned char *gt = memchr(t, '>', trem);
      if (gt) {
        const unsigned char *v = gt + 1;
        size_t vrem = h - (size_t)(v - b);
        const unsigned char *lt = memchr(v, '<', vrem);
        size_t vlen = lt ? (size_t)(lt - v) : vrem;
        if (vlen > DOCMETA_RAW_MAX) vlen = DOCMETA_RAW_MAX;
        unsigned char raw[DOCMETA_RAW_MAX];
        size_t rn = xml_unescape(v, vlen, raw, sizeof raw);
        char out[DOCMETA_MAX_VALUE];
        if (str_finish(raw, rn, out, sizeof out) > 0)
          cJSON_AddStringToObject(o, "title", out);
      }
    }
    html_meta(b, h, o);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Sniffing.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Text-ness over the head window: no NULs, and control bytes outside
 * \t \n \r \f \v under 5%. Deliberately strict — mislabelling a binary as text
 * costs a wrong line count; mislabelling text as binary costs nothing but a
 * generic answer. */
static int looks_texty(const unsigned char *b, size_t len) {
  size_t n = len < DOCMETA_SNIFF_WINDOW ? len : DOCMETA_SNIFF_WINDOW;
  if (n == 0) return 0;
  if (n >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] == 0xFE && b[1] == 0xFF)))
    return 1;                                   /* BOM'd UTF-16 is text */
  size_t ctrl = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = b[i];
    if (c == 0) return 0;
    if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != 0x0B)
      ctrl++;
  }
  return ctrl * 20 <= n;
}

const char *docmeta_sniff(const void *buf, size_t len) {
  const unsigned char *b = (const unsigned char *)buf;
  if (!b || len == 0) return "unknown";

  if (len >= 5 && memcmp(b, "%PDF-", 5) == 0) return "pdf";
  if (len >= 8 && memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0) return "png";
  if (len >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) return "jpeg";
  if (len >= 6 && (memcmp(b, "GIF87a", 6) == 0 || memcmp(b, "GIF89a", 6) == 0))
    return "gif";

  if (len >= 4 && b[0] == 'P' && b[1] == 'K' && b[2] == 0x03 && b[3] == 0x04) {
    zipidx zi;
    int kind;
    if (zip_index(b, len, NULL, &zi) && zi.entries > 0) kind = zi.kind;
    else kind = zip_kind_from_local(b, len);
    return kind == ZK_DOCX ? "docx" : kind == ZK_XLSX ? "xlsx" :
           kind == ZK_PPTX ? "pptx" : "zip";
  }

  if (!looks_texty(b, len)) return "unknown";

  size_t n = len < DOCMETA_SNIFF_WINDOW ? len : DOCMETA_SNIFF_WINDOW;
  if (mem_find_ci(b, n, "<!doctype html", 14) || mem_find_ci(b, n, "<html", 5) ||
      mem_find_ci(b, n, "<head", 5) || mem_find_ci(b, n, "<body", 5))
    return "html";

  size_t i = 0;
  while (i < n && (b[i] == ' ' || b[i] == '\t' || b[i] == '\r' || b[i] == '\n' ||
                   b[i] == 0xEF || b[i] == 0xBB || b[i] == 0xBF)) i++;
  if (i < n && (b[i] == '{' || b[i] == '[')) {
    /* Confirm against the TAIL so a CSV whose first cell is "[x" is not
     * called JSON. Bounded to the last 64 bytes. */
    unsigned char open = b[i], close = open == '{' ? '}' : ']';
    size_t back = len < 64 ? len : 64;
    for (size_t k = 0; k < back; k++) {
      unsigned char c = b[len - 1 - k];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
      if (c == close) return "json";
      break;
    }
  }

  char d;
  int cols;
  if (csv_probe(b, n, &d, &cols)) return "csv";
  return "text";
}

/* Recognised-but-unparsed magics. Reported so an analyst is not left guessing
 * at a hex prefix; nothing here is parsed and nothing is claimed about
 * contents. */
static const char *magic_note(const unsigned char *b, size_t len) {
  if (len >= 8 && memcmp(b, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) == 0)
    return "ole2-cfb — legacy Office (.doc/.xls/.ppt), MSI or encrypted OOXML; not parsed";
  if (len >= 5 && memcmp(b, "{\\rtf", 5) == 0)   return "rtf — not parsed";
  if (len >= 2 && b[0] == 0x1F && b[1] == 0x8B)  return "gzip — decompress and re-submit";
  if (len >= 6 && memcmp(b, "7z\xBC\xAF\x27\x1C", 6) == 0) return "7-zip archive — not parsed";
  if (len >= 4 && memcmp(b, "Rar!", 4) == 0)     return "rar archive — not parsed";
  if (len >= 4 && memcmp(b, "\x7F""ELF", 4) == 0) return "elf executable";
  if (len >= 2 && b[0] == 'M' && b[1] == 'Z')    return "dos/pe executable";
  if (len >= 4 && memcmp(b, "OggS", 4) == 0)     return "ogg media — not parsed";
  if (len >= 4 && (memcmp(b, "II\x2a\x00", 4) == 0 || memcmp(b, "MM\x00\x2a", 4) == 0))
    return "tiff image — use core/media.h media_exif_parse()";
  if (len >= 12 && memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WEBP", 4) == 0)
    return "webp image — use core/media.h media_exif_parse()";
  if (len >= 12 && memcmp(b + 4, "ftyp", 4) == 0)
    return "iso-bmff container (heic/avif/mp4) — not parsed";
  return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Entry point.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* The one type→mime map, used only to decide whether a client's claim
 * disagrees with the bytes. Grouped into FAMILIES rather than exact strings:
 * "text/csv" against a sniffed "text" is not a lie worth flagging, but
 * "application/pdf" against a sniffed "zip" absolutely is. */
static int mime_family(const char *s) {
  if (!s) return -1;
  while (*s == ' ') s++;
  if (strncmp(s, "application/pdf", 15) == 0) return 1;
  if (strncmp(s, "application/zip", 15) == 0 ||
      strncmp(s, "application/vnd.openxmlformats", 30) == 0 ||
      strncmp(s, "application/x-zip", 17) == 0) return 2;
  if (strncmp(s, "image/", 6) == 0) return 3;
  if (strncmp(s, "text/", 5) == 0 || strncmp(s, "application/json", 16) == 0 ||
      strncmp(s, "application/xhtml", 17) == 0 ||
      strncmp(s, "application/csv", 15) == 0) return 4;
  return 0;                                     /* octet-stream & friends */
}

static int type_family(const char *t) {
  if (strcmp(t, "pdf") == 0) return 1;
  if (strcmp(t, "zip") == 0 || strcmp(t, "docx") == 0 || strcmp(t, "xlsx") == 0 ||
      strcmp(t, "pptx") == 0) return 2;
  if (strcmp(t, "png") == 0 || strcmp(t, "jpeg") == 0 || strcmp(t, "gif") == 0) return 3;
  if (strcmp(t, "text") == 0 || strcmp(t, "html") == 0 || strcmp(t, "json") == 0 ||
      strcmp(t, "csv") == 0) return 4;
  return -1;
}

/* Extension → the type it implies, for the mismatch flag only. NULL when the
 * extension says nothing useful. */
static const char *ext_type(const char *ext) {
  static const struct { const char *e, *t; } M[] = {
    { "pdf", "pdf" }, { "docx", "docx" }, { "xlsx", "xlsx" }, { "pptx", "pptx" },
    { "zip", "zip" }, { "png", "png" },   { "jpg", "jpeg" },  { "jpeg", "jpeg" },
    { "gif", "gif" }, { "htm", "html" },  { "html", "html" }, { "json", "json" },
    { "csv", "csv" }, { "txt", "text" },  { "md", "text" },   { "log", "text" },
  };
  for (size_t i = 0; i < sizeof M / sizeof M[0]; i++) {
    const char *e = M[i].e;
    size_t n = strlen(e);
    size_t k = 0;
    while (k < n && ext[k] && lc((unsigned char)ext[k]) == (unsigned char)e[k]) k++;
    if (k == n && ext[n] == 0) return M[i].t;
  }
  return NULL;
}

/* The last resort. Only reached if cJSON itself cannot allocate, in which case
 * the caller still gets a parseable document rather than a NULL to crash on. */
static char *fallback_json(void) {
  static const char lit[] = "{\"type\":\"unknown\"}";
  char *s = malloc(sizeof lit);
  if (s) memcpy(s, lit, sizeof lit);
  return s;
}

char *docmeta_extract(const void *buf, size_t len,
                      const char *filename_hint,
                      const char *content_type_hint) {
  const unsigned char *b = (const unsigned char *)buf;
  if (!b) len = 0;

  const char *type = docmeta_sniff(b, len);
  cJSON *root = cJSON_CreateObject();
  if (!root) return fallback_json();
  cJSON_AddStringToObject(root, "type", type);
  cJSON_AddNumberToObject(root, "bytes", (double)len);

  /* Hex prefix, always. It is the one field that is useful no matter how badly
   * everything else went — an analyst reads the magic by eye. */
  if (len > 0) {
    static const char HEX[] = "0123456789abcdef";
    size_t hn = len < DOCMETA_HEX_PREFIX ? len : DOCMETA_HEX_PREFIX;
    char hex[DOCMETA_HEX_PREFIX * 2 + 1];
    for (size_t i = 0; i < hn; i++) {
      hex[i * 2]     = HEX[b[i] >> 4];
      hex[i * 2 + 1] = HEX[b[i] & 0x0F];
    }
    hex[hn * 2] = 0;
    cJSON_AddStringToObject(root, "sniffed", hex);
  }

  /* Hints are echoed and cross-checked; they never steer the parser. */
  if (filename_hint && filename_hint[0]) {
    char fn[DOCMETA_MAX_VALUE];
    size_t hl = strlen(filename_hint);
    if (hl > DOCMETA_RAW_MAX) hl = DOCMETA_RAW_MAX;
    if (str_finish((const unsigned char *)filename_hint, hl, fn, sizeof fn) > 0) {
      cJSON_AddStringToObject(root, "filename", fn);
      const char *dot = strrchr(fn, '.');
      if (dot && dot[1]) {
        cJSON_AddStringToObject(root, "extension", dot + 1);
        const char *want = ext_type(dot + 1);
        if (want && strcmp(want, type) != 0 && strcmp(type, "unknown") != 0)
          cJSON_AddBoolToObject(root, "extension_mismatch", 1);
      }
    }
  }
  if (content_type_hint && content_type_hint[0]) {
    char ct[DOCMETA_MAX_VALUE];
    size_t hl = strlen(content_type_hint);
    if (hl > DOCMETA_RAW_MAX) hl = DOCMETA_RAW_MAX;
    if (str_finish((const unsigned char *)content_type_hint, hl, ct, sizeof ct) > 0) {
      cJSON_AddStringToObject(root, "content_type_claimed", ct);
      int cf = mime_family(ct), tf = type_family(type);
      if (cf > 0 && tf > 0 && cf != tf)
        cJSON_AddBoolToObject(root, "content_type_mismatch", 1);
    }
  }

  if (strcmp(type, "pdf") == 0) {
    pdf_extract(b, len, root);
  } else if (strcmp(type, "docx") == 0 || strcmp(type, "xlsx") == 0 ||
             strcmp(type, "pptx") == 0 || strcmp(type, "zip") == 0) {
    zipidx zi;
    int kind;
    if (zip_index(b, len, NULL, &zi) && zi.entries > 0) kind = zi.kind;
    else { memset(&zi, 0, sizeof zi); kind = zip_kind_from_local(b, len); }
    office_extract(b, len, kind, &zi, root);
  } else if (strcmp(type, "text") == 0 || strcmp(type, "html") == 0 ||
             strcmp(type, "json") == 0 || strcmp(type, "csv") == 0) {
    text_extract(b, len, type, root);
  } else if (strcmp(type, "png") == 0 || strcmp(type, "jpeg") == 0 ||
             strcmp(type, "gif") == 0) {
    /* Labelled, not parsed. core/media.h owns images and already does EXIF,
     * dimensions and pHash properly; a second parser here would be a worse
     * one. No fabricated fields. */
    cJSON_AddStringToObject(root, "note",
      "image — use core/media.h media_exif_parse() / media_image_dims()");
  } else {
    const char *mn = magic_note(b, len);
    if (mn) cJSON_AddStringToObject(root, "magic_note", mn);
  }

  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out ? out : fallback_json();
}
