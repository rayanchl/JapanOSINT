/* _jp_osint.inc — shared static helpers for the JP-OSINT batch collectors
 * (corporate / government / surveillance / property / research sources added on
 * top of the original 61). Included, never compiled standalone (the Makefile
 * globs *.c, not *.inc — same trick as username_osint_platforms.inc).
 *
 * Every includer must have already pulled in source.h, third_party/cJSON.h and
 * <string.h>/<stdlib.h>/<ctype.h>/<stdio.h>. All symbols are `static`, so each
 * translation unit gets its own copy (no duplicate-symbol link errors).
 *
 * House rules honoured by every helper (see SOURCE_REALITY_REPORT.md / the
 * "no fabricated collector data" guidance): real fetch or honest empty. Nothing
 * here invents records, names, coordinates or values. Key-gated sources return
 * 0 (honest empty) when their credential env var is unset, exactly like
 * gbizinfo.c. */
#ifndef JO_JP_OSINT_INC
#define JO_JP_OSINT_INC

/* jo_sv / jo_env / jo_urlencode / jo_is_utf8 / jo_get used to be defined right
 * here. They were the same four-liners 300+ other collectors had each rewritten
 * under their own names, so they now live in lib/jocore.h — one implementation,
 * one place to fix. Everything below is what is genuinely specific to this
 * batch of JP-OSINT scrapers. */
#include "../../lib/jocore.h"

/* Extract inner text of the FIRST <tag>…</tag> at/after *cur within [buf..end).
 * Advances *cur past the close tag. Returns a malloc'd, CDATA-stripped string
 * (caller frees) or NULL. Lightweight scan — fine for the simple RSS/Atom/HTML
 * the feeds here emit; not a general XML parser. */
static inline char *jo_tag_inner(const char **cur, const char *tag) {
  const char *p = *cur;
  if (!p) return NULL;
  char open[64], close[64];
  snprintf(open, sizeof open, "<%s", tag);
  snprintf(close, sizeof close, "</%s>", tag);
  const char *o = strstr(p, open);
  if (!o) return NULL;
  const char *gt = strchr(o, '>');
  if (!gt) return NULL;
  const char *start = gt + 1;
  const char *c = strstr(start, close);
  if (!c) return NULL;
  size_t len = (size_t)(c - start);
  /* strip a single CDATA wrapper if present */
  const char *s = start;
  if (len >= 12 && strncmp(s, "<![CDATA[", 9) == 0) {
    s += 9; len -= 9;
    if (len >= 3 && strncmp(s + len - 3, "]]>", 3) == 0) len -= 3;
  }
  char *out = (char *)malloc(len + 1);
  if (!out) return NULL;
  memcpy(out, s, len);
  out[len] = 0;
  *cur = c + strlen(close);
  return out;
}

/* Find the next <item> or <entry> block start at/after *cur; returns pointer to
 * it and leaves *cur there, or NULL when no more. */
static inline const char *jo_next_entry(const char **cur) {
  const char *p = *cur;
  if (!p) return NULL;
  const char *it = strstr(p, "<item");
  const char *en = strstr(p, "<entry");
  const char *hit = it;
  if (!hit || (en && en < hit)) hit = en;
  *cur = hit;
  return hit;
}

/* Emit one intel_item per <a> in a fetched, server-rendered listing page whose
 * href contains `href_must` (NULL = any) and — when `query` is non-NULL — whose
 * visible text or href contains the query bytes. Everything emitted (link text,
 * href) is REAL content extracted from the page; nothing is synthesized. Pages
 * that are JS-rendered or shaped differently simply yield 0 (honest empty).
 * Returns the number emitted. `base` is prepended to root-relative hrefs. */
static inline int jo_emit_anchors(const source_ctx *ctx, intel_sink *sink,
                           const char *url, const char *href_must,
                           const char *service, const char *record_type,
                           const char *base, const char *query,
                           int max, const char *tag) {
  char *html = jo_get(ctx, url, NULL, tag);
  if (!html) return 0;
  int emitted = 0;
  const char *p = html;
  /* small dedupe ring of hrefs already emitted */
  char *seen[64]; int nseen = 0;
  while (emitted < max && (p = strstr(p, "<a ")) != NULL) {
    const char *h = strstr(p, "href=\"");
    const char *tagend = strchr(p, '>');
    p += 3;
    if (!h || !tagend || h > tagend) continue;
    h += 6;
    const char *he = strchr(h, '"');
    if (!he) continue;
    size_t hlen = (size_t)(he - h);
    if (hlen == 0 || hlen > 800) continue;
    /* anchor text up to </a> */
    const char *atext = strchr(he, '>');
    const char *aclose = atext ? strstr(atext, "</a>") : NULL;
    if (!atext || !aclose) continue;
    atext += 1;
    /* crude tag-strip of inner text */
    char text[512]; size_t tj = 0; int intag = 0;
    for (const char *q = atext; q < aclose && tj < sizeof text - 1; q++) {
      if (*q == '<') intag = 1;
      else if (*q == '>') intag = 0;
      else if (!intag && (unsigned char)*q != '\n' && (unsigned char)*q != '\t') text[tj++] = *q;
    }
    while (tj && (text[tj-1] == ' ')) tj--;
    text[tj] = 0;
    /* Left-trim too. Only the right side was trimmed, so indented markup
     * (`<a>\n      Some Registry</a>`) produced titles with ~40 leading spaces
     * that sort and display as blank-prefixed junk. */
    char *ttext = text;
    while (*ttext == ' ') ttext++;
    tj = strlen(ttext);
    char href[820];
    snprintf(href, sizeof href, "%.*s", (int)hlen, h);
    p = aclose + 4;
    if (href_must && !strstr(href, href_must)) continue;
    if (tj < 3) continue;
    if (query && *query && !strstr(ttext, query) && !strstr(href, query)) continue;
    /* Skip pure in-page navigation: `href="#..."` is a skip-link or a jump to a
     * section, never a record. With href_must=NULL this helper harvested the
     * whole page, which is how 本文へ移動 ("skip to content") and `English` were
     * filed as financial-penalty records. */
    if (href[0] == '#') continue;
    /* dedupe */
    int dup = 0;
    for (int i = 0; i < nseen; i++) if (strcmp(seen[i], href) == 0) { dup = 1; break; }
    if (dup) continue;
    if (nseen < 64) seen[nseen++] = strdup(href);

    char link[900];
    if (strncmp(href, "http", 4) == 0) snprintf(link, sizeof link, "%s", href);
    else snprintf(link, sizeof link, "%s%s", base ? base : "", href);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", service);
    if (query) cJSON_AddStringToObject(props, "query", query);
    cJSON_AddStringToObject(props, "href", link);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = link;
    it.title           = ttext;
    it.link            = link;
    /* Was hardcoded "ja" for all 134 consumers, which stamped Japanese on UK,
     * Irish and Romanian registry rows. Infer from the host instead; unknown
     * stays NULL rather than asserting a language we don't know. */
    it.lang            = (strstr(link, ".jp/") || strstr(link, ".jp\"") ||
                          strstr(link, "go.jp") || strstr(link, "co.jp"))
                         ? "ja" : NULL;
    it.record_type     = record_type;
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  for (int i = 0; i < nseen; i++) free(seen[i]);
  free(html);
  fprintf(stderr, "[%s] emitted %d\n", tag, emitted);
  return emitted;
}

#endif /* JO_JP_OSINT_INC */
