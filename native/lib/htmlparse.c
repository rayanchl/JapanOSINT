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

int html_attr(const char *s, const char *attr, char *out, size_t n) {
  out[0] = 0;
  if (!s) return 0;
  size_t al = strlen(attr);
  for (const char *p = s; (p = strchr(p, *attr ? attr[0] : '=')) != NULL; p++) {
    if (strncasecmp(p, attr, al) != 0) continue;
    const char *e = p + al;
    while (*e == ' ' || *e == '\t') e++;
    if (*e != '=') continue;
    e++;
    while (*e == ' ' || *e == '\t') e++;
    char q = *e;
    if (q != '"' && q != '\'') continue;
    const char *v = e + 1;
    const char *ve = strchr(v, q);
    if (!ve) return 0;
    size_t len = (size_t)(ve - v);
    if (len >= n) len = n - 1;
    memcpy(out, v, len);
    out[len] = 0;
    return 1;
  }
  return 0;
}
