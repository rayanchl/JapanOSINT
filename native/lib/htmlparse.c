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
  while ((p = strstr(p, "<a ")) != NULL) {
    const char *h = strstr(p, "href=\"");
    const char *tagend = strchr(p, '>');
    p += 3;
    if (!h || !tagend || h > tagend) continue;      /* not this tag's href */
    h += 6;
    const char *he = strchr(h, '"');
    if (!he) continue;
    size_t hlen = (size_t)(he - h);
    if (!hlen || hlen > 800) continue;
    const char *atext = strchr(he, '>');
    const char *aclose = atext ? strstr(atext, "</a>") : NULL;
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
