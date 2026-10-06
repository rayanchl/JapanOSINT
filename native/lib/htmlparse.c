/* lib/htmlparse.c — see header. Simple tolerant scanners (no regex engine);
 * mirror the JS regex *intent* for the patterns the 18 collectors use. */
#include "htmlparse.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

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
    const char *aclose = atext ? strcasestr(atext, "</a>") : NULL;
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

typedef struct { const char *s, *e; int colspan, rowspan; } ht_raw;

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

  /* Pass 2: the grid, with rowspans carried down. */
  int width = 0;
  for (int r = 0; r < nrow; r++) {
    int a = row_start[r], b = r + 1 < nrow ? row_start[r + 1] : ncell, w = 0;
    for (int i = a; i < b; i++) w += cells[i].colspan;
    if (w > width) width = w;
  }
  typedef struct { int left; char *text; char *href; } carry;
  carry *pend = width ? calloc((size_t)width + 1000, sizeof *pend) : NULL;
  out->rows = nrow ? calloc((size_t)nrow, sizeof *out->rows) : NULL;
  for (int r = 0; r < nrow && out->rows; r++) {
    int a = row_start[r], b = r + 1 < nrow ? row_start[r + 1] : ncell;
    html_row *row = &out->rows[out->nrows];
    int cap = width + 1000;
    row->cells = calloc((size_t)cap, sizeof *row->cells);
    if (!row->cells) break;
    int col = 0;
    for (int i = a; i <= b; i++) {
      while (col < cap && pend && pend[col].left > 0) {       /* rowspan from above */
        row->cells[col].text = strdup(pend[col].text ? pend[col].text : "");
        row->cells[col].href = pend[col].href ? strdup(pend[col].href) : NULL;
        if (--pend[col].left == 0) { free(pend[col].text); free(pend[col].href); pend[col].text = pend[col].href = NULL; }
        col++;
      }
      if (i == b) break;
      char *txt = ht_text(cells[i].s, (size_t)(cells[i].e - cells[i].s));
      char *href = ht_href(cells[i].s, (size_t)(cells[i].e - cells[i].s));
      for (int k = 0; k < cells[i].colspan && col < cap; k++, col++) {
        row->cells[col].text = k == 0 ? txt : strdup("");
        row->cells[col].href = k == 0 ? href : NULL;
        if (k == 0 && cells[i].rowspan > 1 && pend) {
          free(pend[col].text); free(pend[col].href);
          pend[col].left = cells[i].rowspan - 1;
          pend[col].text = strdup(txt ? txt : "");
          pend[col].href = href ? strdup(href) : NULL;
        }
      }
    }
    if (col > out->width) out->width = col;
    row->ncells = col;
    out->nrows++;
  }
  if (pend) { for (int i = 0; i < width + 1000; i++) { free(pend[i].text); free(pend[i].href); } free(pend); }
  /* Rectangular: pad every row to the table's width. */
  for (int r = 0; r < out->nrows; r++) {
    html_row *row = &out->rows[r];
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
