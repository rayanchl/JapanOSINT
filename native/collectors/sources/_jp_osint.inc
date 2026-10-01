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
 * 0 (never -1) when their credential env var is unset — but "return 0 and emit
 * nothing" is no longer the whole answer, because it reads exactly like a
 * source that ran and found nothing. See R6 in SOURCE_AUTHORING_CONTRACT.md and
 * `_credential_notice.inc`: the gate emits one upserting
 * `collector-status-notice` row so the state is data, not a log line. */
#ifndef JO_JP_OSINT_INC
#define JO_JP_OSINT_INC

#include "lib/htmlparse.h"   /* the one anchor scanner + dedupe set */
/* Shared small helpers (jo_sv / jo_env / jo_urlencode / jo_get / jo_num / …)
 * live in jocore.h — the same four this file used to define inline were
 * extracted there, so include it rather than duplicate them (guarded, so a
 * collector that also includes jocore.h directly is fine). The anchor-scanner
 * helpers below (jo_tag_inner / jo_next_entry / jo_emit_anchors) are unique to
 * this include and stay. */
#include "lib/jocore.h"

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
 * its tail.
 *
 * NO FABRICATION / HONEST FAILURE (the twin house rule): `if (!html) return 0;`
 * used to be the whole of the failure handling here, and every one of the ~36
 * callers discards the return value. A 403 scrape-block, a DNS failure and a
 * genuinely empty result page were therefore the same observable event —
 * "ok, 0 records" — for a third of the JP-OSINT fleet. A failure has to degrade
 * to an EXPLICIT error, so it now does both:
 *
 *   - it emits a `collector-fetch-error` row (keyed per service+url, so it
 *     upserts rather than piling up) saying which endpoint did not answer.
 *     That is the disclosure that survives; a log line nobody reads is not one.
 *   - jo_emit_anchors_ex() reports it through `fetch_failed` so a caller can
 *     return a non-zero status and let scheduler.c's run_status() classify it.
 *
 * The SIGNATURE and return contract of jo_emit_anchors() are deliberately
 * untouched — it is included by 204 translation units and callers accumulate
 * its result with `total += …`, so a negative return would silently subtract
 * from a running count. Callers that want the distinction opt in by calling
 * the _ex form; everyone else gets the error record for free. */
static inline int jo_emit_anchors_ex(const source_ctx *ctx, intel_sink *sink,
                           const char *url, const char *href_must,
                           const char *service, const char *record_type,
                           const char *base, const char *query,
                           int max, const char *tag, int *fetch_failed) {
  if (fetch_failed) *fetch_failed = 0;
  char *html = jo_get(ctx, url, NULL, tag);
  if (!html) {
    if (fetch_failed) *fetch_failed = 1;
    fprintf(stderr, "[%s] FETCH FAILED %s — reporting as data, not as 0 rows\n",
            tag, url);
    cJSON *ep = cJSON_CreateObject();
    cJSON_AddStringToObject(ep, "service", service ? service : (tag ? tag : "?"));
    if (query) cJSON_AddStringToObject(ep, "query", query);
    cJSON_AddStringToObject(ep, "endpoint", url);
    cJSON_AddStringToObject(ep, "error", "fetch_failed");
    /* jo_get already logged the concrete status; it does not hand it back, so
     * say exactly what is known rather than guessing a code. */
    cJSON_AddStringToObject(ep, "detail",
      "no HTTP 200 body (transport error, non-200, or empty response)");
    cJSON_AddBoolToObject(ep, "success", 0);
    char *epj = cJSON_PrintUnformatted(ep);
    cJSON_Delete(ep);

    char ekey[420], etitle[420];
    snprintf(ekey, sizeof ekey, "%.150s|fetch-error:%.250s",
             service ? service : (tag ? tag : "?"), url);
    snprintf(etitle, sizeof etitle, "%s — fetch failed (%.200s)",
             service ? service : (tag ? tag : "?"), url);
    intel_item err = {0};
    err.remote_key      = ekey;
    err.title           = etitle;
    err.link            = url;
    err.lang            = "en";
    err.record_type     = "collector-fetch-error";
    err.properties_json = epj ? epj : "{}";
    err.tags_json       = "[\"osint-search\",\"fetch-error\"]";
    sink->emit(sink, &err);
    free(epj);
    return 0;
  }
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
     * reads is not a disclosure.
     *
     * Built by jo_truncation_notice_ex() like every other notice in the tree:
     * `service` IS this record's source_id (same value, the canonical spelling),
     * and the endpoint travels as `url` — the name lib/pagewalk.c already
     * publishes it under, so one key means one thing across all callers. */
    fprintf(stderr, "[%s] emitted %d of %d matching anchors — %d DISCARDED by the "
                    "caller's cap (docs/SOURCE_EXHAUSTIVENESS.md)\n",
            tag, emitted, matched, matched - emitted);
    cJSON *extra = cJSON_CreateObject();
    if (extra) cJSON_AddStringToObject(extra, "url", url);
    jo_truncation_notice_ex(sink, service, query, emitted, matched,
                            "caller-supplied per-page cap",
                            "raise or drop the `max` this caller passes to "
                            "jo_emit_anchors() so every matching anchor on the "
                            "page is emitted — see docs/SOURCE_EXHAUSTIVENESS.md",
                            extra);
  } else {
    fprintf(stderr, "[%s] emitted %d\n", tag, emitted);
  }
  return emitted;
}

/* The historical entry point, byte-identical in signature and return value.
 * Every existing caller keeps compiling and keeps behaving as before; the
 * difference is that a dead endpoint now leaves a collector-fetch-error row
 * behind instead of looking like an empty page. */
static inline int jo_emit_anchors(const source_ctx *ctx, intel_sink *sink,
                           const char *url, const char *href_must,
                           const char *service, const char *record_type,
                           const char *base, const char *query,
                           int max, const char *tag) {
  return jo_emit_anchors_ex(ctx, sink, url, href_must, service, record_type,
                            base, query, max, tag, NULL);
}

/* ---------------------------------------------------- registry-sweep notice */

/* The ~15 registry-sweep collectors (world_reg_*, reg_*, gazette_world,
 * courts_world) all share one shape: a table of national registries walked in
 * order, with a whole-run emit cap in the loop CONDITION. That cap does not
 * trim a page — it terminates the sweep, so once it is hit the registries
 * after it are never fetched at all, and until now nothing said which. Those
 * caps carried an `exhaustive-ok: runaway guard, logged` marker; "logged" is
 * explicitly not an exception (docs/SOURCE_EXHAUSTIVENESS.md §7).
 *
 *   reached / ntotal   how far the sweep got, and how many registries the
 *                      compile-time table holds. Both are always knowable.
 *   emitted            records actually emitted across the registries reached.
 *
 * records_available is -1 and MUST be: the unvisited registries were never
 * asked, so how many records they hold for this entity is genuinely unknown
 * and any number here would be invented. Call it unconditionally — it returns
 * without emitting when the whole table was swept. */
static inline void jo_registry_sweep_notice(intel_sink *sink, const char *sid,
                                            const char *query, int emitted,
                                            int reached, int ntotal,
                                            const char *cap_name, int cap_value,
                                            int cancelled) {
  if (!sink || reached >= ntotal) return;
  char why[520], fix[400];
  snprintf(why, sizeof why,
           "%s stopped the sweep after %d of %d registries in the table; the "
           "remaining %d were never queried, so this run cannot say whether "
           "they hold anything for this entity. records_available is unknown "
           "for exactly that reason — an unvisited registry was never asked.",
           cancelled ? "the run was cancelled and that"
                     : "the whole-run emit cap, which sits in the loop "
                       "condition over registries rather than on a page,",
           reached, ntotal, ntotal - reached);
  snprintf(fix, sizeof fix,
           "raise %s (currently %d) so the sweep reaches every registry in the "
           "table, or rotate the table's start offset across scheduled runs so "
           "the tail is not permanently unread",
           cap_name ? cap_name : "the run cap", cap_value);
  jo_truncation_notice(sink, sid, query, emitted, -1, why, fix);
}

#endif /* JO_JP_OSINT_INC */
