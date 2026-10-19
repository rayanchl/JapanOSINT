/* lib/htmlparse.c — see header. Simple tolerant scanners (no regex engine);
 * mirror the JS regex *intent* for the patterns the 18 collectors use. */
#include "htmlparse.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* Case-insensitive (ASCII) strstr, local so this file stays free-standing
 * (tests/htmlparse_test.c links it alone). strcasestr() is a GNU extension:
 * without _GNU_SOURCE glibc does not declare it, so on Linux it was implicitly
 * declared — a truncated pointer, or a hard error under GCC 14. */
static const char *html_ci_strstr(const char *hay, const char *needle) {
  size_t nl = strlen(needle);
  if (!nl) return hay;
  int c0 = tolower((unsigned char)needle[0]);
  for (const char *p = hay; *p; p++)
    if (tolower((unsigned char)*p) == c0 && strncasecmp(p, needle, nl) == 0)
      return p;
  return NULL;
}

char *html_strip(const char *in) {
  if (!in) { char *e = malloc(1); if (e) e[0] = 0; return e; }
  size_t L = strlen(in);
  char *o = malloc(L + 1);
  if (!o) return NULL;
  size_t w = 0;
  int in_tag = 0, pending_ws = 0, started = 0;
  for (size_t i = 0; i < L; i++) {
    char c = in[i];
    if (in_tag) { if (c == '>') in_tag = 0; continue; }
    if (c == '<') { in_tag = 1; pending_ws = 1; continue; }   /* tag → ws */
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
      pending_ws = 1; continue;
    }
    if (pending_ws && started) o[w++] = ' ';
    pending_ws = 0; started = 1;
    o[w++] = c;
  }
  o[w] = 0;                                  /* trailing ws already dropped */
  return o;
}

/* match <tag, allowing optional namespace prefix and attributes; returns
 * pointer just after the opening '>' or NULL. *self_closed set if `/>`. */
static const char *open_tag(const char *p, const char *tag, int *self_closed) {
  size_t tl = strlen(tag);
  for (; (p = strchr(p, '<')) != NULL; p++) {
    const char *q = p + 1;
    if (*q == '/' || *q == '!' || *q == '?') continue;
    const char *ne = q;
    while (*ne && *ne != ' ' && *ne != '>' && *ne != '/' &&
           *ne != '\t' && *ne != '\n' && *ne != '\r') ne++;
    const char *base = q;
    for (const char *c = q; c < ne; c++) if (*c == ':') base = c + 1;
    if ((size_t)(ne - base) == tl && strncasecmp(base, tag, tl) == 0) {
      const char *gt = strchr(q, '>');
      if (!gt) return NULL;
      if (self_closed) *self_closed = (gt > q && gt[-1] == '/');
      return gt + 1;
    }
  }
  return NULL;
}

/* find matching </tag> (case-insensitive, ns tolerant) from p; returns
 * pointer to the '<' of the close tag, or NULL. */
static const char *close_tag(const char *p, const char *tag) {
  size_t tl = strlen(tag);
  for (; (p = strchr(p, '<')) != NULL; p++) {
    if (p[1] != '/') continue;
    const char *cn = p + 2, *ce = cn;
    while (*ce && *ce != '>') ce++;
    const char *cb = cn;
    for (const char *c = cn; c < ce; c++) if (*c == ':') cb = c + 1;
    while (cb < ce && (ce[-1] == ' ')) ce--;
    if ((size_t)(ce - cb) == tl && strncasecmp(cb, tag, tl) == 0) return p;
  }
  return NULL;
}

int html_tag(const char *s, const char *tag, char *out, size_t n) {
  out[0] = 0;
  if (!s) return 0;
  int sc = 0;
  const char *inner = open_tag(s, tag, &sc);
  if (!inner) return 0;
  if (sc) return 0;                          /* <tag/> → empty, treat absent */
  const char *end = close_tag(inner, tag);
  if (!end) return 0;
  size_t len = (size_t)(end - inner);
  if (len >= n) len = n - 1;
  memcpy(out, inner, len);
  out[len] = 0;
  return 1;
}

const char *html_block(const char *from, const char *tag,
                        const char **inner, int *inner_len) {
  if (!from) return NULL;
  int sc = 0;
  const char *istart = open_tag(from, tag, &sc);
  if (!istart) return NULL;
  if (sc) { *inner = istart; *inner_len = 0; return istart; }
  const char *end = close_tag(istart, tag);
  if (!end) return NULL;
  *inner = istart;
  *inner_len = (int)(end - istart);
  const char *gt = strchr(end, '>');
  return gt ? gt + 1 : end;
}

/* An attribute name only ever STARTS at the beginning of the buffer, after
 * whitespace, or immediately after a tag's '<'. Without this left boundary the
 * scan matched the name as a bare substring, so `src` was satisfied by the
 * "src" inside `data-src=` and `id` by the one inside `data-id=`. On the
 * lazy-loading markup the camera scrapers meet — `<img data-src="spinner.gif"
 * src="snapshot.jpg">` — that stored the placeholder and the real snapshot URL
 * was never seen. (The RIGHT boundary is already implied: after the name we
 * require optional spaces then '=', so `srcset=` cannot satisfy `src`.) */
static int attr_start(const char *s, const char *p) {
  if (p == s) return 1;
  char c = p[-1];
  return c == '<' || c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
         c == '\f' || c == '\v';
}

/* One scan of the buffer. `allow_unquoted` = 0 considers only quoted values.
 *
 * html_attr runs this twice, quoted-first, because an unquoted match can be a
 * fragment of ANOTHER attribute's quoted text and this scanner is deliberately
 * not quote-aware (it is handed text-bearing XML blocks, where an apostrophe in
 * prose would desync any quote tracking). Given
 *   <Weakness Description="compare Name=Other here" Name="Real Name">
 * a single pass returned `Other` — the bad-character filter below does not
 * catch it, because the filter only fires when the closing quote is GLUED to
 * the value (`href=/x"`); put a space before it and the fragment reads as a
 * clean unquoted value. Preferring any quoted match resolves it without
 * needing to know where the quotes are. */
static int html_attr_scan(const char *s, const char *attr, char *out, size_t n,
                          int allow_unquoted) {
  size_t al = strlen(attr);
  /* The compare below is case-insensitive; the SCAN has to be too. It used
   * strchr(p, attr[0]) — a lowercase 'h' — so `<BASE HREF="…">` (upper-case
   * markup is common on older Japanese government pages) was never found. */
  for (const char *p = s; *p; p++) {
    if (!attr_start(s, p)) continue;
    if (strncasecmp(p, attr, al) != 0) continue;
    const char *e = p + al;
    while (*e == ' ' || *e == '\t') e++;
    if (*e != '=') continue;
    e++;
    while (*e == ' ' || *e == '\t') e++;
    char q = *e;
    const char *v, *ve;
    if (q == '"' || q == '\'') {
      v = e + 1;
      ve = strchr(v, q);
      if (!ve) return 0;                     /* unterminated quote → give up */
    } else if (!allow_unquoted) {
      continue;
    } else {
      /* Unquoted value (`src=snapshot.jpg`), which HTML allows and municipal
       * pages do emit: runs to whitespace or the tag's '>'. A spec-legal
       * unquoted value contains none of ` " ' = < > ` — enforcing that is what
       * keeps a stray `href=/x` sitting INSIDE another attribute's quoted
       * value (`title="see href=/x"`) from beating the tag's real href, since
       * this scanner is deliberately not quote-aware (it is also handed
       * text-bearing XML blocks, where an apostrophe in prose would desync
       * any quote tracking). */
      v = e;
      ve = v;
      while (*ve && *ve != '>' && *ve != ' ' && *ve != '\t' && *ve != '\n' &&
             *ve != '\r' && *ve != '\f' && *ve != '\v') ve++;
      if (ve == v) continue;                        /* `attr=` with no value */
      int bad = 0;
      for (const char *c = v; c < ve; c++)
        if (*c == '"' || *c == '\'' || *c == '=' || *c == '<' || *c == '`')
          bad = 1;
      if (bad) continue;
    }
    size_t len = (size_t)(ve - v);
    if (len >= n) len = n - 1;
    memcpy(out, v, len);
    out[len] = 0;
    return 1;
  }
  return 0;
}

int html_attr(const char *s, const char *attr, char *out, size_t n) {
  if (!out || n == 0) return 0;
  out[0] = 0;
  if (!s || !attr || !*attr) return 0;
  if (html_attr_scan(s, attr, out, n, 0)) return 1;   /* quoted wins outright */
  out[0] = 0;
  return html_attr_scan(s, attr, out, n, 1);
}

/* ── anchors: the single implementation both anchor consumers share ─────── */

const char *html_anchor_next(const char *from, html_anchor *out) {
  if (!from || !out) return NULL;
  const char *p = from;
  while ((p = strchr(p, '<')) != NULL) {
    /* `<a` plus a delimiter. The old test was the literal "<a ", so an anchor
     * written `<a\nhref=…>` or `<a\thref=…>` — ordinary output from a
     * pretty-printing CMS — was never seen at all, and every record behind it
     * was discarded with no error and no notice. */
    if ((p[1] != 'a' && p[1] != 'A') ||
        (p[2] != ' ' && p[2] != '\t' && p[2] != '\n' && p[2] != '\r')) {
      p++; continue;
    }
    const char *tagend = strchr(p, '>');
    const char *scan = p + 2;
    p += 2;
    if (!tagend) continue;
    /* href=, case-insensitive, either quote style, or unquoted. Matching only
     * the literal lowercase `href="` dropped `href='…'` and `HREF=…` outright;
     * both are legal and both appear in the government listing pages this
     * scanner exists for. Requiring whitespace before the name is what keeps
     * `data-href=` from being mistaken for the real attribute. */
    const char *h = NULL;
    char q = 0;
    for (const char *c = scan; c + 6 <= tagend; c++) {
      if (*c != ' ' && *c != '\t' && *c != '\n' && *c != '\r') continue;
      if (strncasecmp(c + 1, "href", 4) != 0) continue;
      const char *e = c + 5;
      while (*e == ' ' || *e == '\t') e++;
      if (*e != '=') continue;
      e++;
      while (*e == ' ' || *e == '\t') e++;
      h = e; q = *e; break;
    }
    if (!h) continue;                               /* not this tag's href */
    size_t hlen;
    const char *after;                              /* first byte past value */
    if (q == '"' || q == '\'') {
      h++;
      const char *he = strchr(h, q);
      if (!he) continue;
      hlen = (size_t)(he - h);
      after = he + 1;
    } else {
      const char *he = h;
      while (he < tagend && *he != ' ' && *he != '\t' && *he != '\n' &&
             *he != '\r') he++;
      hlen = (size_t)(he - h);
      after = he;
    }
    /* Whitespace inside the quotes is not part of the URL. Hamada city's
     * camera index writes `href=" ./viewer.php?cid=1"`; stored verbatim, the
     * resolved link became `r_view/ ./viewer.php` — a record whose url does
     * not open. Browsers strip it; so does this. */
    while (hlen && (*h == ' ' || *h == '\t' || *h == '\n' || *h == '\r')) { h++; hlen--; }
    while (hlen && (h[hlen - 1] == ' ' || h[hlen - 1] == '\t' ||
                    h[hlen - 1] == '\n' || h[hlen - 1] == '\r')) hlen--;
    if (!hlen || hlen > 800) continue;
    const char *atext = strchr(after, '>');
    /* Case-insensitive: everything else in this scanner (the opening `<a`
     * test above, href's name match) is explicitly case-tolerant because
     * upper-case markup is ordinary on older Japanese municipal/government
     * pages (see the `<BASE HREF=` note on html_attr_scan). A literal
     * strstr() here only ever matched a lowercase `</a>`, so any page whose
     * anchors close `</A>` produced zero anchors — a silent, total discard
     * for that whole class of source, not a partial one, since every anchor
     * on such a page fails the same way. */
    const char *aclose = atext ? html_ci_strstr(atext, "</a>") : NULL;
    if (!atext || !aclose) continue;
    atext++;

    size_t tj = 0;
    int intag = 0;
    for (const char *q = atext; q < aclose && tj < sizeof out->text - 1; q++) {
      if (*q == '<') intag = 1;
      else if (*q == '>') intag = 0;
      else if (!intag && *q != '\n' && *q != '\t' && *q != '\r') out->text[tj++] = *q;
    }
    while (tj && out->text[tj - 1] == ' ') tj--;
    size_t lead = 0;
    while (lead < tj && out->text[lead] == ' ') lead++;
    if (lead) { memmove(out->text, out->text + lead, tj - lead); tj -= lead; }
    out->text[tj] = 0;
    out->text_len = tj;
    out->href     = h;
    out->href_len = hlen;
    return aclose + 4;
  }
  return NULL;
}


/* ── tables ──────────────────────────────────────────────────────────────── */

/* Case-insensitive "<name" at p, followed by whitespace, '>' or '/'. */
static int ht_open_at(const char *p, const char *name) {
  size_t n = strlen(name);
  if (p[0] != '<' || strncasecmp(p + 1, name, n)) return 0;
  char c = p[1 + n];
  return c == '>' || c == '/' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
static int ht_close_at(const char *p, const char *name) {
  size_t n = strlen(name);
  return p[0] == '<' && p[1] == '/' && !strncasecmp(p + 2, name, n) &&
         (p[2 + n] == '>' || p[2 + n] == ' ' || p[2 + n] == '\t' || p[2 + n] == '\n' || p[2 + n] == '\r');
}
/* End of the <table> opened at `p` (just past its </table>), honouring nesting. */
static const char *ht_table_end(const char *p) {
  int depth = 0;
  for (const char *q = p; *q; q++) {
    if (*q != '<') continue;
    if (ht_open_at(q, "table")) depth++;
    else if (ht_close_at(q, "table") && --depth == 0) {
      const char *gt = strchr(q, '>');
      return gt ? gt + 1 : q + strlen(q);
    }
  }
  return p + strlen(p);                 /* unterminated: to the end */
}

static void ht_utf8(char **o, unsigned long cp) {
  char *w = *o;
  if (cp < 0x80) *w++ = (char)cp;
  else if (cp < 0x800) { *w++ = (char)(0xC0 | (cp >> 6)); *w++ = (char)(0x80 | (cp & 63)); }
  else if (cp < 0x10000) { *w++ = (char)(0xE0 | (cp >> 12)); *w++ = (char)(0x80 | ((cp >> 6) & 63)); *w++ = (char)(0x80 | (cp & 63)); }
  else if (cp < 0x110000) { *w++ = (char)(0xF0 | (cp >> 18)); *w++ = (char)(0x80 | ((cp >> 12) & 63));
                            *w++ = (char)(0x80 | ((cp >> 6) & 63)); *w++ = (char)(0x80 | (cp & 63)); }
  *o = w;
}

/* Cell HTML -> text: tags out (a <br> or a block boundary is a space),
 * entities decoded, ASCII whitespace collapsed and trimmed. malloc'd. */
static char *ht_text(const char *s, size_t n) {
  char *out = malloc(n * 2 + 1);       /* an entity never grows past 2x */
  if (!out) return NULL;
  char *w = out;
  int sp = 0;
  for (size_t i = 0; i < n; ) {
    unsigned char c = (unsigned char)s[i];
    if (c == '<') {
      const char *gt = memchr(s + i, '>', n - i);
      i = gt ? (size_t)(gt - s) + 1 : n;
      sp = 1;                           /* every tag boundary may separate words */
      continue;
    }
    if (c == '&') {
      const char *semi = memchr(s + i, ';', n - i > 12 ? 12 : n - i);
      unsigned long cp = 0; int ok = 0;
      if (semi) {
        const char *e = s + i + 1; size_t el = (size_t)(semi - e);
        if (el > 1 && e[0] == '#') {
          char *endp = NULL;
          cp = (e[1] == 'x' || e[1] == 'X') ? strtoul(e + 2, &endp, 16) : strtoul(e + 1, &endp, 10);
          ok = endp == semi && cp > 0;
        } else if (el == 3 && !strncmp(e, "amp", 3)) { cp = '&'; ok = 1; }
        else if (el == 2 && !strncmp(e, "lt", 2))  { cp = '<'; ok = 1; }
        else if (el == 2 && !strncmp(e, "gt", 2))  { cp = '>'; ok = 1; }
        else if (el == 4 && !strncmp(e, "quot", 4)) { cp = '"'; ok = 1; }
        else if (el == 4 && !strncmp(e, "apos", 4)) { cp = '\''; ok = 1; }
        else if (el == 4 && !strncmp(e, "nbsp", 4)) { cp = ' '; ok = 1; }
      }
      if (ok) {
        if (cp == ' ' || cp == 0xA0) sp = 1;
        else { if (sp && w > out) *w++ = ' '; sp = 0; ht_utf8(&w, cp); }
        i = (size_t)(semi - s) + 1;
        continue;
      }
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') { sp = 1; i++; continue; }
    if (sp && w > out) *w++ = ' ';
    sp = 0;
    *w++ = (char)c; i++;
  }
  *w = 0;
  return out;
}

static char *ht_href(const char *s, size_t n) {
  char *tmp = malloc(n + 1);
  if (!tmp) return NULL;
  memcpy(tmp, s, n); tmp[n] = 0;
  char *a = strstr(tmp, "<a "); if (!a) a = strstr(tmp, "<A ");
  char buf[2048];
  char *r = (a && html_attr(a, "href", buf, sizeof buf) && buf[0]) ? strdup(buf) : NULL;
  free(tmp);
  return r;
}

static int ht_span(const char *tag_open, const char *gt, const char *attr) {
  size_t n = (size_t)(gt - tag_open) + 1;
  char *tmp = malloc(n + 1), buf[16];
  if (!tmp) return 1;
  memcpy(tmp, tag_open, n); tmp[n] = 0;
  int v = html_attr(tmp, attr, buf, sizeof buf) ? atoi(buf) : 1;
  free(tmp);
  return v < 1 ? 1 : v > 1000 ? 1000 : v;
}

/* Ceiling on a single table row's width, in cells. Bounds the two calloc()s
 * in pass 2 so their size is provably sane on untrusted input. */
#define HT_MAX_WIDTH 65536
/* Ceiling on a table's row count, for the same reason. */
#define HT_MAX_ROWS  1000000   /* exhaustive-ok: memory guard on untrusted HTML; biting sets html_table.capped_rows, filed by hp_run_table as shape notice (g) */
/* Cells a row may run past the table's widest row before it is cut and
 * flagged. Rowspan carries from above land in columns this row never declared,
 * so a row can legitimately be wider than the colspan sum pass 1 measured. */
#define HT_WIDTH_SLACK 1000

typedef struct { const char *s, *e; int colspan, rowspan; } ht_raw;

/* Grow one row's cell array to at least `need` slots, never past `hardcap`,
 * and zero whatever it adds. Returns the capacity now available: pass 2 writes
 * only at indices below it, so a refused grow truncates the row (and sets
 * capped_width through the k < colspan test) rather than overrunning it. */
static int ht_row_grow(html_row *row, int *cap, int need, int hardcap) {
  if (need <= *cap) return *cap;
  if (need > hardcap) return *cap;
  int nc = *cap ? *cap : 16;
  while (nc < need) {
    if (nc > hardcap / 2) { nc = hardcap; break; }
    nc *= 2;
  }
  if (nc > hardcap) nc = hardcap;
  if (nc <= *cap) return *cap;
  html_cell *t = realloc(row->cells, (size_t)nc * sizeof *t);
  if (!t) return *cap;
  memset(t + *cap, 0, (size_t)(nc - *cap) * sizeof *t);
  row->cells = t;
  *cap = nc;
  return nc;
}

int html_table_parse(const char *html, const char *must, int table_index,
                     html_table *out) {
  memset(out, 0, sizeof *out);
  if (!html) return 0;
  const char *tb = NULL, *te = NULL;
  int seen = 0;
  for (const char *p = html; (p = strchr(p, '<')); p++) {
    if (!ht_open_at(p, "table")) continue;
    const char *end = ht_table_end(p);
    int hit = 1;
    if (must && *must) {
      size_t ml = strlen(must);
      hit = 0;
      for (const char *q = p; q + ml <= end; q++) if (!memcmp(q, must, ml)) { hit = 1; break; }
    }
    if (hit && seen++ == table_index) { tb = p; te = end; break; }
  }
  if (!tb) return 0;

  /* Pass 1: raw rows of raw cells, nested tables folded into their cell. */
  ht_raw *cells = NULL; int ncell = 0, capc = 0;
  int *row_start = NULL; int nrow = 0, capr = 0;
  const char *body = strchr(tb, '>'); body = body ? body + 1 : te;
  ht_raw *cur = NULL;
  for (const char *p = body; p < te; ) {
    if (*p != '<') { p++; continue; }
    if (ht_open_at(p, "table")) { p = ht_table_end(p); continue; }      /* nested */
    int is_tr = ht_open_at(p, "tr");
    int is_cell = ht_open_at(p, "td") || ht_open_at(p, "th");
    int ends = ht_close_at(p, "tr") || ht_close_at(p, "td") || ht_close_at(p, "th") ||
               ht_close_at(p, "table") || ht_close_at(p, "tbody") || ht_close_at(p, "thead") ||
               ht_close_at(p, "tfoot");
    if (is_tr || is_cell || ends) {
      if (cur) { cur->e = p; cur = NULL; }
      if (is_tr || (is_cell && nrow == 0)) {
        /* Bounded, and the bound is why: `nrow` feeds
         * calloc((size_t)nrow, …) below, and an int that grows without a
         * ceiling can overflow to negative on a pathological document — a
         * negative int cast to size_t becomes ~SIZE_MAX, which calloc refuses,
         * but `capr * 2` overflowing first would make the realloc here small
         * and the writes past it real. This parser is fed arbitrary scraped
         * HTML, so neither is theoretical. A table that claims more rows than
         * this is truncated at the parse, which the caller reads as a short
         * table rather than as invented rows. */
        if (nrow >= HT_MAX_ROWS) { out->capped_rows = 1; break; }  /* disclosed: capped_rows */
        if (nrow == capr) { capr = capr ? capr * 2 : 64; int *nr = realloc(row_start, (size_t)capr * sizeof *nr); if (!nr) break; row_start = nr; }
        row_start[nrow++] = ncell;
      }
      if (is_cell) {
        const char *gt = strchr(p, '>');
        if (!gt || gt > te) break;
        if (ncell == capc) { capc = capc ? capc * 2 : 256; ht_raw *nc = realloc(cells, (size_t)capc * sizeof *nc); if (!nc) break; cells = nc; }
        cur = &cells[ncell++];
        cur->s = gt + 1; cur->e = te;
        cur->colspan = ht_span(p, gt, "colspan");
        cur->rowspan = ht_span(p, gt, "rowspan");
        p = gt + 1;
        continue;
      }
    }
    p++;
  }
  if (cur) cur->e = te;

  /* Pass 2: the grid, with rowspans carried down.
   *
   * The row width is summed in a WIDE type and clamped. ht_span() already
   * bounds one colspan to [1, 1000], but a row's width is the SUM over its
   * cells, and this parser is fed arbitrary scraped HTML — a table with enough
   * cells could overflow an int and make `(size_t)width + 1000` wrap to near
   * SIZE_MAX at the calloc below. gcc saw exactly that and reported an
   * allocation range starting at 18446744071562067968, which is
   * (size_t)INT_MIN + 1000. A wrapped size is not a big allocation, it is a
   * small one followed by writes past its end. */
  long long width_ll = 0;
  for (int r = 0; r < nrow; r++) {
    int a = row_start[r], b = r + 1 < nrow ? row_start[r + 1] : ncell;
    long long w = 0;
    for (int i = a; i < b; i++) w += cells[i].colspan;
    if (w > width_ll) width_ll = w;
  }
  /* A real table does not exceed this; a document that claims to has been
   * truncated at the parse, which the caller sees as a short row rather than
   * as invented cells. */
  if (width_ll > HT_MAX_WIDTH) width_ll = HT_MAX_WIDTH;
  if (width_ll < 0) width_ll = 0;
  int width = (int)width_ll;
  typedef struct { int left; char *text; char *href; } carry;
  carry *pend = width ? calloc((size_t)width + HT_WIDTH_SLACK, sizeof *pend) : NULL;
  size_t nrow_sz = (nrow > 0 && nrow <= HT_MAX_ROWS) ? (size_t)nrow : 0;
  out->rows = nrow_sz ? calloc(nrow_sz, sizeof *out->rows) : NULL;
  /* The width a row may NOT pass. `hardcap` is the flagging bound and part of
   * this parser's contract (htmlparse.h); each row is grown TOWARDS it as its
   * cells arrive rather than allocated at it, because `width` is the widest
   * row in the table and most rows are not that row.
   *
   * It used to be allocated outright, per row: `calloc(width + 1000)` for
   * every one of up to HT_MAX_ROWS rows. On the one-cell-per-row table the
   * unit test feeds it — 1,000,000 rows, width 1 — that is 1,001 cells x 16
   * bytes x 1,000,000 = 16 GB, and htmlparsetest died to the OOM killer with
   * `make: *** Killed`, which reads like a flaky runner rather than an
   * allocation bug. The two bounds were added to stop a WRAPPED size from
   * becoming a small allocation plus an overrun; multiplying the slack by the
   * row ceiling traded that for an honest, enormous one. */
  const int hardcap = width + HT_WIDTH_SLACK;
  for (int r = 0; r < nrow && out->rows; r++) {
    int a = row_start[r], b = r + 1 < nrow ? row_start[r + 1] : ncell;
    html_row *row = &out->rows[out->nrows];
    int cap = 0;  /* exhaustive-ok: allocated capacity of this row's growing cell array (ht_row_grow), not a bound on records */
    int col = 0;
    for (int i = a; i <= b; i++) {
      while (col < hardcap && pend && pend[col].left > 0) {   /* rowspan from above */
        if (ht_row_grow(row, &cap, col + 1, hardcap) <= col) break;
        row->cells[col].text = strdup(pend[col].text ? pend[col].text : "");
        row->cells[col].href = pend[col].href ? strdup(pend[col].href) : NULL;
        if (--pend[col].left == 0) { free(pend[col].text); free(pend[col].href); pend[col].text = pend[col].href = NULL; }
        col++;
      }
      if (i == b) break;
      char *txt = ht_text(cells[i].s, (size_t)(cells[i].e - cells[i].s));
      char *href = ht_href(cells[i].s, (size_t)(cells[i].e - cells[i].s));
      int k = 0;
      for (; k < cells[i].colspan && col < hardcap &&
             ht_row_grow(row, &cap, col + 1, hardcap) > col; k++, col++) {
        row->cells[col].text = k == 0 ? txt : strdup("");
        row->cells[col].href = k == 0 ? href : NULL;
        if (k == 0 && cells[i].rowspan > 1 && pend) {
          free(pend[col].text); free(pend[col].href);
          pend[col].left = cells[i].rowspan - 1;
          pend[col].text = strdup(txt ? txt : "");
          pend[col].href = href ? strdup(href) : NULL;
        }
      }
      /* The row ran past the width bound (+ its slack): this cell, or the
       * rest of its colspan, has nowhere to go. Said, not swallowed — and a
       * cell that got no slot at all frees what it parsed. */
      if (k < cells[i].colspan) {
        out->capped_width = 1;
        if (k == 0) { free(txt); free(href); }
      }
    }
    if (col > out->width) out->width = col;
    row->ncells = col;
    out->nrows++;
  }
  if (pend) { for (int i = 0; i < width + HT_WIDTH_SLACK; i++) { free(pend[i].text); free(pend[i].href); } free(pend); }
  /* Rectangular: pad every row to the table's width. Each row now holds only
   * the cells it actually produced, so the padding has to grow it first — and
   * a row that cannot be grown keeps its own width rather than being padded
   * into memory it does not own. */
  for (int r = 0; r < out->nrows; r++) {
    html_row *row = &out->rows[r];
    if (row->ncells >= out->width) continue;
    html_cell *t = realloc(row->cells, (size_t)out->width * sizeof *t);
    if (!t) continue;
    memset(t + row->ncells, 0,
           (size_t)(out->width - row->ncells) * sizeof *t);
    row->cells = t;
    for (int k = row->ncells; k < out->width; k++) row->cells[k].text = strdup("");
    row->ncells = out->width;
  }
  free(cells); free(row_start);
  return 1;
}

void html_table_free(html_table *t) {
  if (!t) return;
  for (int r = 0; r < t->nrows; r++) {
    for (int k = 0; k < t->rows[r].ncells; k++) { free(t->rows[r].cells[k].text); free(t->rows[r].cells[k].href); }
    free(t->rows[r].cells);
  }
  free(t->rows);
  memset(t, 0, sizeof *t);
}

/* ── strongest label per link (see htmlparse.h) ─────────────────────────── */

/* Hashed, like lib/seenset.c, and for the same page: PYPI_SIMPLE_INDEX offers
 * 824,355 links on one listing, and a linear lookup per anchor is quadratic. */
static unsigned long long hl_hash(const char *k) {
  unsigned long long h = 1469598103934665603ULL;
  for (const unsigned char *p = (const unsigned char *)k; *p; p++)
    h = (h ^ *p) * 1099511628211ULL;
  return h;
}

static int hl_find(const html_label_set *s, const char *key) {
  if (s->h && s->hcap) {
    unsigned long long m = (unsigned long long)(s->hcap - 1);
    for (unsigned long long x = hl_hash(key) & m; s->h[x] >= 0; x = (x + 1) & m)
      if (strcmp(s->v[s->h[x]].key, key) == 0) return s->h[x];
    return -1;
  }
  for (int i = 0; i < s->n; i++) if (strcmp(s->v[i].key, key) == 0) return i;
  return -1;
}

/* Index the newest entry, growing the table to stay at most half full. A
 * failed allocation drops the index and lookups fall back to the scan. */
static void hl_index_last(html_label_set *s) {
  if (s->n * 2 > s->hcap) {
    int nc = s->hcap ? s->hcap * 2 : 128;
    while (s->n * 2 > nc) nc *= 2;
    int *h = malloc((size_t)nc * sizeof *h);
    free(s->h);
    s->h = h;
    s->hcap = h ? nc : 0;
    if (!h) return;
    for (int i = 0; i < nc; i++) h[i] = -1;
    for (int i = 0; i < s->n; i++) {
      unsigned long long x = hl_hash(s->v[i].key) & (unsigned long long)(nc - 1);
      while (h[x] >= 0) x = (x + 1) & (unsigned long long)(nc - 1);
      h[x] = i;
    }
    return;
  }
  if (!s->h) return;
  unsigned long long m = (unsigned long long)(s->hcap - 1);
  unsigned long long x = hl_hash(s->v[s->n - 1].key) & m;
  while (s->h[x] >= 0) x = (x + 1) & m;
  s->h[x] = s->n - 1;
}

/* `s` without whitespace (ASCII, U+3000, U+00A0, a literal "&nbsp;") and
 * without a trailing ellipsis ("...", "…", "‥"), malloc'd. */
static char *hl_norm(const char *s) {
  size_t n = strlen(s);
  char *o = malloc(n + 1);
  if (!o) return NULL;
  size_t w = 0;
  for (size_t i = 0; i < n; ) {
    unsigned char c = (unsigned char)s[i];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { i++; continue; }
    if (c == 0xE3 && i + 2 < n && (unsigned char)s[i+1] == 0x80 &&
        (unsigned char)s[i+2] == 0x80) { i += 3; continue; }          /* U+3000 */
    if (c == 0xC2 && i + 1 < n && (unsigned char)s[i+1] == 0xA0) { i += 2; continue; }
    if (c == '&' && !strncasecmp(s + i, "&nbsp;", 6)) { i += 6; continue; }
    o[w++] = s[i++];
  }
  o[w] = 0;
  for (;;) {
    if (w >= 3 && !memcmp(o + w - 3, "...", 3)) { w -= 3; o[w] = 0; continue; }
    if (w >= 3 && (!memcmp(o + w - 3, "\xE2\x80\xA6", 3) ||       /* … */
                   !memcmp(o + w - 3, "\xE2\x80\xA5", 3))) {      /* ‥ */
      w -= 3; o[w] = 0; continue;
    }
    break;
  }
  return o;
}

int html_label_covers(const char *a, const char *b) {
  if (!a || !b) return 0;
  char *na = hl_norm(a), *nb = hl_norm(b);
  int r = na && nb && (!*nb || strstr(na, nb) != NULL);
  free(na); free(nb);
  return r;
}

/* Keep `label` among the entry's other labels unless it is already there.
 * Out of memory loses only this side-label. */
static void hl_keep_other(html_label_ent *e, const char *label) {
  for (int i = 0; i < e->nothers; i++) if (!strcmp(e->others[i], label)) return;
  if (e->nothers == e->cothers) {
    int nc = e->cothers ? e->cothers * 2 : 2;
    char **g = realloc(e->others, (size_t)nc * sizeof *g);
    if (!g) return;
    e->others = g; e->cothers = nc;
  }
  char *d = strdup(label);
  if (d) e->others[e->nothers++] = d;
}

int html_label_offer(html_label_set *s, const char *key, const char *label,
                     int strength) {
  if (!s || !key || !label) return -1;
  int i = hl_find(s, key);
  if (i >= 0) {
    html_label_ent *e = &s->v[i];
    if (strength > e->strength && !html_label_covers(e->label, label)) {
      char *nl = strdup(label);
      if (nl) {
        /* The loser is kept, not dropped — if it was the PAGE's label for
         * this link. A borrowed one (HTML_LABEL_CONTEXT) was the engine's
         * guess from neighbouring text, often another item's (the Gifu date
         * was the previous card's), and attaching it to this record would
         * state an association the page never made. */
        if (e->strength >= HTML_LABEL_IMG) hl_keep_other(e, e->label);
        free(e->label);
        e->label = nl;
        e->strength = strength;
        /* the new label may have been an "other" already */
        for (int j = 0; j < e->nothers; j++)
          if (!strcmp(e->others[j], nl)) {
            free(e->others[j]);
            memmove(e->others + j, e->others + j + 1,
                    (size_t)(e->nothers - j - 1) * sizeof *e->others);
            e->nothers--;
            break;
          }
        return 0;
      }
    }
    if (strength >= HTML_LABEL_IMG && strcmp(e->label, label) != 0)
      hl_keep_other(e, label);
    return 0;
  }
  if (s->n == s->cap) {
    int nc = s->cap ? s->cap * 2 : 64;
    html_label_ent *nv = realloc(s->v, (size_t)nc * sizeof *nv);
    if (!nv) return -1;
    s->v = nv; s->cap = nc;
  }
  char *k = strdup(key), *l = strdup(label);
  if (!k || !l) { free(k); free(l); return -1; }
  s->v[s->n] = (html_label_ent){ .key = k, .label = l, .strength = strength };
  s->n++;
  hl_index_last(s);
  return 1;
}

void html_label_free(html_label_set *s) {
  if (!s) return;
  for (int i = 0; i < s->n; i++) {
    free(s->v[i].key); free(s->v[i].label);
    for (int j = 0; j < s->v[i].nothers; j++) free(s->v[i].others[j]);
    free(s->v[i].others);
  }
  free(s->v);
  free(s->h);
  s->v = NULL; s->n = s->cap = 0;
  s->h = NULL; s->hcap = 0;
}
