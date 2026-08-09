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

#include "../../lib/htmlparse.h"   /* the one anchor scanner + dedupe set */
/* Shared small helpers (jo_sv / jo_env / jo_urlencode / jo_get / jo_num / …)
 * live in jocore.h — the same four this file used to define inline were
 * extracted there, so include it rather than duplicate them (guarded, so a
 * collector that also includes jocore.h directly is fine). The anchor-scanner
 * helpers below (jo_tag_inner / jo_next_entry / jo_emit_anchors) are unique to
 * this include and stay. */
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
 * Returns the number emitted. `base` is prepended to root-relative hrefs.
 *
 * EXHAUSTIVE USE (docs/SOURCE_EXHAUSTIVENESS.md): `max <= 0` means EVERY
 * matching anchor on the page — the caller does not get an implicit cap. When a
 * caller does pass a positive max and the page had more, the shortfall is
 * logged with both numbers so the discard is visible. The dedupe table grows
 * instead of wrapping at a fixed 64, so a long listing cannot re-emit or drop
 * its tail. */
static inline int jo_emit_anchors(const source_ctx *ctx, intel_sink *sink,
                           const char *url, const char *href_must,
                           const char *service, const char *record_type,
                           const char *base, const char *query,
                           int max, const char *tag) {
  char *html = jo_get(ctx, url, NULL, tag);
  if (!html) return 0;
  int emitted = 0, matched = 0;
  /* Scanning + dedupe come from lib/htmlparse — the SAME code lib/hpengine.c
   * uses for its HP_HTML rows. There used to be two copies of this scanner and
   * a fix (the fixed 64-slot dedupe ring that dropped a long listing's tail)
   * had to be applied to both. */
  html_seen seen = {0};
  html_anchor a;
  const char *p = html;
  while ((p = html_anchor_next(p, &a)) != NULL) {
    char href[820];
    snprintf(href, sizeof href, "%.*s", (int)a.href_len, a.href);
    if (href_must && !strstr(href, href_must)) continue;
    if (a.text_len < 3) continue;
    if (query && *query && !strstr(a.text, query) && !strstr(href, query)) continue;
    if (!html_seen_add(&seen, href)) continue;
    matched++;
    if (max > 0 && emitted >= max) continue;   /* count what we are skipping */

    char link[900];
    if (strncmp(href, "http", 4) == 0) snprintf(link, sizeof link, "%s", href);
    else snprintf(link, sizeof link, "%s%s", base ? base : "", href);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", service);
    if (query) cJSON_AddStringToObject(props, "query", query);
    cJSON_AddStringToObject(props, "href", link);
    cJSON_AddStringToObject(props, "endpoint", url);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = link;
    it.title           = a.text;
    it.link            = link;
    it.lang            = "ja";
    it.record_type     = record_type;
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  html_seen_free(&seen);
  free(html);
  if (max > 0 && matched > emitted) {
    /* The caller left real hits on the page. Say so with both numbers, and
     * emit it as data too (docs/SOURCE_EXHAUSTIVENESS.md): a log line nobody
     * reads is not a disclosure. */
    fprintf(stderr, "[%s] emitted %d of %d matching anchors — %d DISCARDED by the "
                    "caller's cap (docs/SOURCE_EXHAUSTIVENESS.md)\n",
            tag, emitted, matched, matched - emitted);
    cJSON *p2 = cJSON_CreateObject();
    cJSON_AddStringToObject(p2, "service", service);
    if (query) cJSON_AddStringToObject(p2, "query", query);
    cJSON_AddStringToObject(p2, "endpoint", url);
    cJSON_AddNumberToObject(p2, "records_used", emitted);
    cJSON_AddNumberToObject(p2, "records_available", matched);
    cJSON_AddStringToObject(p2, "reason", "caller-supplied per-page cap");
    char *pj2 = cJSON_PrintUnformatted(p2);
    cJSON_Delete(p2);
    char key[320], title[256];
    snprintf(key, sizeof key, "%.150s|truncation:%.120s", service, query ? query : "");
    snprintf(title, sizeof title, "%s used %d of %d available anchors",
             service, emitted, matched);
    intel_item note = {0};
    note.remote_key      = key;
    note.title           = title;
    note.lang            = "en";
    note.record_type     = "collector-truncation-notice";
    note.properties_json = pj2 ? pj2 : "{}";
    note.tags_json       = "[\"osint-search\",\"truncation-notice\"]";
    sink->emit(sink, &note);
    free(pj2);
  } else {
    fprintf(stderr, "[%s] emitted %d\n", tag, emitted);
  }
  return emitted;
}

#endif /* JO_JP_OSINT_INC */
