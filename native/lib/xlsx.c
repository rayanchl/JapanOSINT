/* lib/xlsx.c — see header.
 *
 * An .xlsx is a ZIP of XML parts. The four parts this reader touches:
 *   xl/workbook.xml              <sheet name=… r:id=…/> in tab order
 *   xl/_rels/workbook.xml.rels   r:id → part path of each sheet
 *   xl/sharedStrings.xml         <si> … </si> table that t="s" cells index
 *   xl/worksheets/sheetN.xml     <sheetData><row r><c r t><v>…
 *
 * The XML walk is a hand parser for exactly those elements, in the same style
 * as hpengine.c's hp_xml_* helpers (which are static there and shaped around
 * record flattening, not around a cell grid). Excel's own output is regular
 * enough that this is sufficient, and it is what the ministries' files are
 * written with; an optional namespace prefix (`<x:c>`) is tolerated because
 * some non-Excel writers emit one.
 *
 * The ZIP side is lib/zipread.c, which already inflates named members with a
 * decompression ceiling (JO_ZIP_MAX_OUT), so a deflate bomb dressed as a
 * workbook stops there. */
#include "xlsx.h"
#include "zipread.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── growable byte buffer ─────────────────────────────────────────────────── */
typedef struct { char *p; size_t n, cap; } xb;

static int xb_put(xb *b, const char *s, size_t n) {
  if (b->n + n + 1 > b->cap) {
    size_t nc = b->cap ? b->cap : 256;
    while (nc < b->n + n + 1) nc *= 2;
    char *np = realloc(b->p, nc);
    if (!np) return -1;
    b->p = np; b->cap = nc;
  }
  memcpy(b->p + b->n, s, n);
  b->n += n;
  b->p[b->n] = 0;
  return 0;
}
static int xb_putc(xb *b, char c) { return xb_put(b, &c, 1); }

static void seterr(char *err, size_t errn, const char *fmt, const char *a,
                   const char *b) {
  if (!err || !errn) return;
  snprintf(err, errn, fmt, a ? a : "", b ? b : "");
}

/* ── minimal XML ──────────────────────────────────────────────────────────── */

/* Is the tag starting at `p` (just past '<') the element `name`, allowing an
 * optional `prefix:`? Returns the length consumed (prefix + name) or 0. A
 * match requires the name to END there — `<c` must not match `<col` or
 * `<cols>`, and `<t` must not match `<tr>`. */
static size_t tag_is(const char *p, const char *end, const char *name) {
  const char *q = p;
  while (q < end && (isalnum((unsigned char)*q) || *q == '_' || *q == '.')) q++;
  if (q < end && *q == ':') q++; else q = p;     /* prefix present, or not */
  size_t nl = strlen(name);
  if ((size_t)(end - q) < nl || strncmp(q, name, nl) != 0) return 0;
  const char *e = q + nl;
  if (e < end && (isalnum((unsigned char)*e) || *e == '_' || *e == '.' || *e == ':' || *e == '-'))
    return 0;
  return (size_t)(e - p);
}

/* Find the next start tag `name` in [p,end). Sets *body to just past its '>'
 * and *selfclose. Returns a pointer to the '<' or NULL. Skips comments,
 * processing instructions and CDATA so a `<c` inside a comment cannot be
 * mistaken for a cell. */
static const char *find_tag(const char *p, const char *end, const char *name,
                            const char **body, int *selfclose) {
  while (p < end) {
    const char *lt = memchr(p, '<', (size_t)(end - p));
    if (!lt) return NULL;
    if (lt + 1 >= end) return NULL;
    if (lt[1] == '!' || lt[1] == '?') {
      const char *close;
      if (end - lt >= 4 && !strncmp(lt, "<!--", 4)) {
        close = strstr(lt + 4, "-->"); if (!close || close >= end) return NULL;
        p = close + 3;
      } else if (end - lt >= 9 && !strncmp(lt, "<![CDATA[", 9)) {
        close = strstr(lt + 9, "]]>"); if (!close || close >= end) return NULL;
        p = close + 3;
      } else {
        close = memchr(lt, '>', (size_t)(end - lt)); if (!close) return NULL;
        p = close + 1;
      }
      continue;
    }
    if (lt[1] == '/') { p = lt + 2; continue; }
    size_t nl = tag_is(lt + 1, end, name);
    const char *gt = memchr(lt, '>', (size_t)(end - lt));
    if (!gt) return NULL;
    if (nl) {
      *selfclose = gt > lt && gt[-1] == '/';
      *body = gt + 1;
      return lt;
    }
    p = gt + 1;
  }
  return NULL;
}

/* The matching `</name>` for an element whose body starts at p. These parts
 * never nest an element inside itself (no <si> in <si>, no <row> in <row>),
 * so the first close tag with that name is the right one. */
static const char *find_close(const char *p, const char *end, const char *name) {
  while (p < end) {
    const char *lt = memchr(p, '<', (size_t)(end - p));
    if (!lt || lt + 2 >= end) return NULL;
    if (lt[1] == '/' && tag_is(lt + 2, end, name)) return lt;
    p = lt + 1;
  }
  return NULL;
}

/* Attribute `name` of the start tag in [tag, gt). Returns 1 and the RAW
 * (still-escaped) value span, 0 if absent. */
static int attr_span(const char *tag, const char *gt, const char *name,
                     const char **vs, size_t *vn) {
  size_t nl = strlen(name);
  const char *p = tag;
  while (p < gt) {
    if (isspace((unsigned char)*p) && (size_t)(gt - p) > nl + 1 &&
        !strncmp(p + 1, name, nl)) {
      const char *q = p + 1 + nl;
      while (q < gt && isspace((unsigned char)*q)) q++;
      if (q < gt && *q == '=') {
        q++;
        while (q < gt && isspace((unsigned char)*q)) q++;
        if (q < gt && (*q == '"' || *q == '\'')) {
          char quote = *q++;
          const char *e = memchr(q, quote, (size_t)(gt - q));
          if (!e) return 0;
          *vs = q; *vn = (size_t)(e - q);
          return 1;
        }
      }
    }
    p++;
  }
  return 0;
}

static int attr_str(const char *tag, const char *gt, const char *name,
                    char *buf, size_t cap) {
  const char *vs; size_t vn;
  if (!attr_span(tag, gt, name, &vs, &vn)) return 0;
  if (vn >= cap) vn = cap - 1;
  memcpy(buf, vs, vn); buf[vn] = 0;
  return 1;
}

static int put_utf8(xb *b, unsigned long cp) {
  char u[4]; int n;
  if      (cp < 0x80)    { u[0] = (char)cp; n = 1; }
  else if (cp < 0x800)   { u[0] = (char)(0xC0 | (cp >> 6));  u[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
  else if (cp < 0x10000) { u[0] = (char)(0xE0 | (cp >> 12)); u[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
  else                   { u[0] = (char)(0xF0 | (cp >> 18)); u[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); u[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
  return xb_put(b, u, (size_t)n);
}

/* Append [s, s+n) to b with XML entities decoded. Numeric references are
 * decoded to full UTF-8 (a Japanese workbook written by a library that
 * escapes non-ASCII as `&#x6771;` must still read as 東), unlike hpengine's
 * ASCII-only variant. An unknown entity is kept verbatim: it is not ours to
 * guess at. */
static int xml_unescape_put(xb *b, const char *s, size_t n) {
  const char *end = s + n;
  while (s < end) {
    const char *amp = memchr(s, '&', (size_t)(end - s));
    if (!amp) return xb_put(b, s, (size_t)(end - s));
    if (xb_put(b, s, (size_t)(amp - s))) return -1;
    const char *semi = memchr(amp, ';', (size_t)(end - amp));
    if (!semi || semi - amp > 12) { if (xb_putc(b, '&')) return -1; s = amp + 1; continue; }
    size_t el = (size_t)(semi - amp - 1);
    const char *e = amp + 1;
    int rc;
    if      (el == 3 && !strncmp(e, "amp", 3))  rc = xb_putc(b, '&');
    else if (el == 2 && !strncmp(e, "lt", 2))   rc = xb_putc(b, '<');
    else if (el == 2 && !strncmp(e, "gt", 2))   rc = xb_putc(b, '>');
    else if (el == 4 && !strncmp(e, "quot", 4)) rc = xb_putc(b, '"');
    else if (el == 4 && !strncmp(e, "apos", 4)) rc = xb_putc(b, '\'');
    else if (el >= 2 && e[0] == '#') {
      int hex = e[1] == 'x' || e[1] == 'X';
      char *endp = NULL;
      unsigned long cp = strtoul(e + 1 + hex, &endp, hex ? 16 : 10);
      if (endp == semi && cp > 0 && cp <= 0x10FFFF) rc = put_utf8(b, cp);
      else rc = xb_put(b, amp, (size_t)(semi + 1 - amp));
    }
    else rc = xb_put(b, amp, (size_t)(semi + 1 - amp));
    if (rc) return -1;
    s = semi + 1;
  }
  return 0;
}

/* Text of every <t> inside [p,end) concatenated, skipping <rPh> (phonetic
 * runs). Serves <si> (plain or rich text) and <is> (inline string) alike. */
static int collect_t(xb *b, const char *p, const char *end) {
  for (;;) {
    const char *body; int self;
    const char *lt = find_tag(p, end, "rPh", &body, &self);
    const char *tlt = find_tag(p, end, "t", &body, &self);
    if (!tlt) return 0;
    if (lt && lt < tlt) {                          /* skip the phonetic run */
      const char *c = find_close(lt, end, "rPh");
      if (!c) return 0;
      p = c + 1;
      continue;
    }
    if (!self) {
      const char *c = find_close(body, end, "t");
      if (!c) return 0;
      if (xml_unescape_put(b, body, (size_t)(c - body))) return -1;
      p = c + 1;
    } else p = body;
  }
}

/* ── workbook parts ───────────────────────────────────────────────────────── */

typedef struct { char name[256]; char rid[64]; } xsheet;

/* xl/workbook.xml → sheets in tab order. Returns count or -1. */
static int read_sheets(const char *wb, size_t wn, xsheet **out) {
  const char *end = wb + wn, *p = wb;
  xsheet *v = NULL; int n = 0, cap = 0;
  for (;;) {
    const char *body; int self;
    const char *lt = find_tag(p, end, "sheet", &body, &self);
    if (!lt) break;
    const char *gt = body - 1;
    if (n == cap) {
      cap = cap ? cap * 2 : 8;
      xsheet *nv = realloc(v, (size_t)cap * sizeof *nv);
      if (!nv) { free(v); return -1; }
      v = nv;
    }
    xsheet *s = &v[n];
    memset(s, 0, sizeof *s);
    /* The name is an attribute, so it is XML-escaped ("R&amp;D"). */
    const char *vs; size_t vn;
    if (attr_span(lt, gt, "name", &vs, &vn)) {
      xb nb = {0};
      if (xml_unescape_put(&nb, vs, vn) == 0)
        snprintf(s->name, sizeof s->name, "%s", nb.p ? nb.p : "");
      free(nb.p);
    }
    if (!attr_str(lt, gt, "r:id", s->rid, sizeof s->rid))
      attr_str(lt, gt, "id", s->rid, sizeof s->rid);  /* other rel prefix */
    n++;
    p = body;
  }
  *out = v;
  return n;
}

/* xl/_rels/workbook.xml.rels: Id → Target, resolved to a ZIP path. */
static int rel_target(const char *rels, size_t rn, const char *rid,
                      char *path, size_t cap) {
  const char *end = rels + rn, *p = rels;
  for (;;) {
    const char *body; int self;
    const char *lt = find_tag(p, end, "Relationship", &body, &self);
    if (!lt) return 0;
    const char *gt = body - 1;
    char id[64], tgt[400];
    if (attr_str(lt, gt, "Id", id, sizeof id) && !strcmp(id, rid) &&
        attr_str(lt, gt, "Target", tgt, sizeof tgt)) {
      /* Target is relative to xl/ unless it is absolute ("/xl/worksheets/…",
       * which some writers emit). */
      if (tgt[0] == '/') snprintf(path, cap, "%s", tgt + 1);
      else if (!strncmp(tgt, "xl/", 3)) snprintf(path, cap, "%s", tgt);
      else snprintf(path, cap, "xl/%s", tgt);
      return 1;
    }
    p = body;
  }
}

/* xl/sharedStrings.xml → array of malloc'd strings. */
static int read_shared(const char *ss, size_t sn, char ***out) {
  const char *end = ss + sn, *p = ss;
  char **v = NULL; int n = 0, cap = 0;
  for (;;) {
    const char *body; int self;
    const char *lt = find_tag(p, end, "si", &body, &self);
    if (!lt) break;
    xb b = {0};
    if (self) p = body;
    else {
      const char *c = find_close(body, end, "si");
      if (!c) c = end;
      if (collect_t(&b, body, c)) { free(b.p); goto oom; }
      p = c + 1;
    }
    if (n == cap) {
      cap = cap ? cap * 2 : 64;
      char **nv = realloc(v, (size_t)cap * sizeof *nv);
      if (!nv) { free(b.p); goto oom; }
      v = nv;
    }
    v[n++] = b.p ? b.p : strdup("");
    if (!v[n - 1]) goto oom;
  }
  *out = v;
  return n;
oom:
  for (int i = 0; i < n; i++) free(v[i]);
  free(v);
  return -1;
}

static void free_shared(char **v, int n) {
  for (int i = 0; i < n; i++) free(v[i]);
  free(v);
}

/* "C7" → col 3, row 7. Either part may be missing in a sloppy file; 0 = absent. */
static void parse_ref(const char *r, unsigned *col, unsigned *row) {
  unsigned c = 0, rr = 0;
  while (*r >= 'A' && *r <= 'Z') { c = c * 26 + (unsigned)(*r - 'A' + 1); r++; }
  while (*r >= 'a' && *r <= 'z') { c = c * 26 + (unsigned)(*r - 'a' + 1); r++; }
  while (isdigit((unsigned char)*r)) { rr = rr * 10 + (unsigned)(*r - '0'); r++; }
  *col = c; *row = rr;
}

static size_t max_cells(void) {
  const char *e = getenv("JO_XLSX_MAX_CELLS");
  if (e && *e) { long long v = atoll(e); if (v > 0) return (size_t)v; }
  return 2000000;
}

/* RFC 4180: quote when the cell holds the delimiter, a quote, CR or LF. */
static int csv_put_cell(xb *b, const char *s) {
  if (!strpbrk(s, ",\"\r\n")) return xb_put(b, s, strlen(s));
  if (xb_putc(b, '"')) return -1;
  for (; *s; s++) {
    if (*s == '"' && xb_putc(b, '"')) return -1;
    if (xb_putc(b, *s)) return -1;
  }
  return xb_putc(b, '"');
}

/* ── the sheet walk ───────────────────────────────────────────────────────── */

/* One pass over <sheetData>. With `out` NULL it only measures (max row, max
 * column) so the caller can apply the cell ceiling BEFORE any output is
 * built; with `out` set it writes the CSV. Same code both times so the two
 * cannot disagree about where a cell lands. Returns 0, or -1 with err set. */
static int walk_sheet(const char *sx, size_t sn, char **shared, int nshared,
                      xb *out, unsigned *max_row, unsigned *max_col,
                      char *err, size_t errn) {
  const char *end = sx + sn;
  const char *body; int self;
  const char *sd = find_tag(sx, end, "sheetData", &body, &self);
  if (!sd) { seterr(err, errn, "worksheet has no <sheetData>", NULL, NULL); return -1; }
  if (self) { *max_row = *max_col = 0; return 0; }     /* empty sheet */
  const char *sdend = find_close(body, end, "sheetData");
  if (!sdend) sdend = end;

  unsigned cur_row = 0, mrow = 0, mcol = 0;
  const char *p = body;
  xb cell = {0};
  for (;;) {
    const char *rbody; int rself;
    const char *rlt = find_tag(p, sdend, "row", &rbody, &rself);
    if (!rlt) break;
    const char *rgt = rbody - 1;
    char rbuf[32]; unsigned rnum = 0;
    if (attr_str(rlt, rgt, "r", rbuf, sizeof rbuf)) rnum = (unsigned)strtoul(rbuf, NULL, 10);
    /* Rows must ascend; a row numbered at or below the previous one (a
     * writer bug) is placed on the next line rather than dropped or written
     * on top of an earlier row. */
    if (rnum <= cur_row) rnum = cur_row + 1;
    if (out) for (; cur_row + 1 < rnum; cur_row++) if (xb_putc(out, '\n')) goto oom;
    cur_row = rnum;
    if (rnum > mrow) mrow = rnum;

    const char *rend = rself ? rbody : find_close(rbody, sdend, "row");
    if (!rend) rend = sdend;
    unsigned cur_col = 0;
    const char *q = rself ? rend : rbody;
    while (q < rend) {
      const char *cbody; int cself;
      const char *clt = find_tag(q, rend, "c", &cbody, &cself);
      if (!clt) break;
      const char *cgt = cbody - 1;
      char ref[32], type[16] = "";
      unsigned col = 0, rr;
      if (attr_str(clt, cgt, "r", ref, sizeof ref)) parse_ref(ref, &col, &rr);
      if (col <= cur_col) col = cur_col + 1;          /* absent or out of order */
      attr_str(clt, cgt, "t", type, sizeof type);
      const char *cend = cself ? cbody : find_close(cbody, rend, "c");
      if (!cend) cend = rend;

      if (out) {
        /* Pad the gap so the column index survives: a value in C7 with no
         * A7/B7 must still be the third cell of line 7. */
        for (unsigned k = cur_col; k < col; k++)
          if (k > 0 && xb_putc(out, ',')) goto oom;
        cell.n = 0; if (cell.p) cell.p[0] = 0;
        if (!cself) {
          const char *vb; int vself;
          if (!strcmp(type, "inlineStr")) {
            const char *is = find_tag(cbody, cend, "is", &vb, &vself);
            if (is && !vself && collect_t(&cell, vb, cend)) goto oom;
          } else {
            const char *v = find_tag(cbody, cend, "v", &vb, &vself);
            if (v && !vself) {
              const char *vc = find_close(vb, cend, "v");
              if (!vc) vc = cend;
              if (!strcmp(type, "s")) {
                char ib[32]; size_t il = (size_t)(vc - vb);
                if (il >= sizeof ib) il = sizeof ib - 1;
                memcpy(ib, vb, il); ib[il] = 0;
                long idx = strtol(ib, NULL, 10);
                if (idx < 0 || idx >= nshared) {
                  char m[64]; snprintf(m, sizeof m, "%ld of %d", idx, nshared);
                  seterr(err, errn, "cell %s: shared string index %s out of range", ref, m);
                  free(cell.p); return -1;
                }
                if (xb_put(&cell, shared[idx], strlen(shared[idx]))) goto oom;
              } else {
                /* n (number), str (formula result), b, e, d: the stored text
                 * verbatim — see the header on why numbers are not
                 * reformatted and dates stay serials. */
                if (xml_unescape_put(&cell, vb, (size_t)(vc - vb))) goto oom;
              }
            }
          }
        }
        if (csv_put_cell(out, cell.p ? cell.p : "")) goto oom;
      }
      cur_col = col;
      if (col > mcol) mcol = col;
      q = cend + (cself ? 0 : 1);
    }
    if (out) {
      /* Pad to the sheet's width so every non-empty line has the same arity;
       * a row with no cells at all stays an empty line (see header). */
      if (cur_col) for (; cur_col < *max_col; cur_col++) if (xb_putc(out, ',')) goto oom;
      if (xb_putc(out, '\n')) goto oom;
    }
    p = rself ? rbody : rend + 1;
  }
  free(cell.p);
  if (!out) { *max_row = mrow; *max_col = mcol; }
  return 0;
oom:
  free(cell.p);
  seterr(err, errn, "out of memory", NULL, NULL);
  return -1;
}

/* ── entry points ─────────────────────────────────────────────────────────── */

static int check_zip(const char *body, size_t len, char *err, size_t errn) {
  if (!body || len < 4) { seterr(err, errn, "empty body", NULL, NULL); return -1; }
  const unsigned char *b = (const unsigned char *)body;
  if (b[0] == 'P' && b[1] == 'K' && b[2] == 3 && b[3] == 4) return 0;
  if (b[0] == 0xD0 && b[1] == 0xCF && b[2] == 0x11 && b[3] == 0xE0) {
    seterr(err, errn, "OLE2 compound file (.xls), not an .xlsx", NULL, NULL);
    return -1;
  }
  char head[9]; size_t hn = len < 8 ? len : 8;
  for (size_t i = 0; i < hn; i++) head[i] = isprint(b[i]) ? (char)b[i] : '.';
  head[hn] = 0;
  seterr(err, errn, "not a zip (starts with \"%s\")", head, NULL);
  return -1;
}

/* Load workbook.xml and its sheet list; shared by both entry points. */
static int load_workbook(const char *body, size_t len, xsheet **sheets,
                         char *err, size_t errn) {
  if (check_zip(body, len, err, errn)) return -1;
  size_t wn;
  char *wb = zip_find_entry(body, len, "xl/workbook.xml", &wn);
  if (!wb) {
    seterr(err, errn, "zip has no xl/workbook.xml (not a workbook)", NULL, NULL);
    return -1;
  }
  int n = read_sheets(wb, wn, sheets);
  free(wb);
  if (n < 0) { seterr(err, errn, "out of memory", NULL, NULL); return -1; }
  if (n == 0) { seterr(err, errn, "workbook declares no sheets", NULL, NULL); free(*sheets); *sheets = NULL; return -1; }
  return n;
}

char *xlsx_sheet_names(const char *body, size_t len, char *err, size_t errn) {
  xsheet *sheets = NULL;
  int n = load_workbook(body, len, &sheets, err, errn);
  if (n < 0) return NULL;
  xb b = {0};
  for (int i = 0; i < n; i++) {
    if (i && xb_putc(&b, '\n')) break;
    if (xb_put(&b, sheets[i].name, strlen(sheets[i].name))) break;
  }
  free(sheets);
  if (!b.p) { seterr(err, errn, "out of memory", NULL, NULL); return NULL; }
  return b.p;
}

int xlsx_to_csv(const char *body, size_t len, int sheet_index,
                const char *sheet_name, char **csv_out, size_t *csv_len,
                char *err, size_t errn) {
  if (csv_out) *csv_out = NULL;
  if (csv_len) *csv_len = 0;
  if (err && errn) err[0] = 0;
  if (!csv_out) return -1;

  xsheet *sheets = NULL;
  int n = load_workbook(body, len, &sheets, err, errn);
  if (n < 0) return -1;

  int pick = -1;
  if (sheet_name && *sheet_name) {
    for (int i = 0; i < n; i++) if (!strcmp(sheets[i].name, sheet_name)) { pick = i; break; }
    if (pick < 0) {
      char have[512] = ""; size_t w = 0;
      for (int i = 0; i < n && w < sizeof have - 4; i++)
        w += (size_t)snprintf(have + w, sizeof have - w, "%s%s", i ? ", " : "", sheets[i].name);
      seterr(err, errn, "sheet '%s' not found (have: %s)", sheet_name, have);
      free(sheets); return -1;
    }
  } else {
    if (sheet_index < 0 || sheet_index >= n) {
      char a[32], b[32];
      snprintf(a, sizeof a, "%d", sheet_index); snprintf(b, sizeof b, "%d", n);
      seterr(err, errn, "sheet index %s out of range (workbook has %s)", a, b);
      free(sheets); return -1;
    }
    pick = sheet_index;
  }

  char part[512] = "";
  size_t rn;
  char *rels = zip_find_entry(body, len, "xl/_rels/workbook.xml.rels", &rn);
  if (!rels) {
    seterr(err, errn, "zip has no xl/_rels/workbook.xml.rels", NULL, NULL);
    free(sheets); return -1;
  }
  int found = rel_target(rels, rn, sheets[pick].rid, part, sizeof part);
  free(rels);
  if (!found) {
    seterr(err, errn, "sheet '%s' has no relationship for %s", sheets[pick].name, sheets[pick].rid);
    free(sheets); return -1;
  }
  size_t sn;
  char *sx = zip_find_entry(body, len, part, &sn);
  if (!sx) {
    seterr(err, errn, "sheet '%s': zip member %s missing or undecodable", sheets[pick].name, part);
    free(sheets); return -1;
  }
  free(sheets);

  /* Shared strings are optional (a workbook of only numbers has none). */
  char **shared = NULL; int nshared = 0;
  size_t ssn;
  char *ss = zip_find_entry(body, len, "xl/sharedStrings.xml", &ssn);
  if (ss) {
    nshared = read_shared(ss, ssn, &shared);
    free(ss);
    if (nshared < 0) { seterr(err, errn, "out of memory", NULL, NULL); free(sx); return -1; }
  }

  /* Measure first, then bound: the ceiling is applied to the grid the CSV
   * would hold (rows × widest row), before a byte of it is built, so the
   * answer is the whole sheet or a refusal — never a shortened table that
   * looks complete. */
  unsigned mrow = 0, mcol = 0;
  int rc = walk_sheet(sx, sn, shared, nshared, NULL, &mrow, &mcol, err, errn);
  if (rc == 0) {
    size_t ceiling = max_cells();
    if ((size_t)mrow * (size_t)mcol > ceiling) {
      char a[64], b[64];
      snprintf(a, sizeof a, "%u rows x %u cols = %llu cells", mrow, mcol,
               (unsigned long long)mrow * mcol);
      snprintf(b, sizeof b, "%zu", ceiling);
      seterr(err, errn, "sheet exceeds JO_XLSX_MAX_CELLS: %s > %s — refused, not truncated", a, b);
      rc = -1;
    }
  }
  if (rc == 0) {
    xb out = {0};
    rc = walk_sheet(sx, sn, shared, nshared, &out, &mrow, &mcol, err, errn);
    if (rc == 0) {
      if (!out.p) { out.p = strdup(""); if (!out.p) rc = -1; }
      if (rc == 0) { *csv_out = out.p; if (csv_len) *csv_len = out.n; }
    } else free(out.p);
  }
  free_shared(shared, nshared);
  free(sx);
  return rc;
}
