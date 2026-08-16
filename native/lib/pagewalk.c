/* lib/pagewalk.c — see pagewalk.h for why this exists and what it refuses to do. */
#include "pagewalk.h"
#include "feedlib.h"
#include "jocore.h"        /* jo_truncation_notice_ex() — the ONE notice builder */
#include "../core/url_override.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* The ONLY cap in this file that stops a walk. Every stop it causes emits a
 * collector-truncation-notice naming JO_PAGE_MAX as the remedy, so the bound is
 * stated in-band rather than hidden. */
#define PW_PAGE_MAX_DEFAULT 20   /* exhaustive-ok: runaway ceiling, disclosed as a truncation notice */

static int pw_page_max(void) {
  const char *e = getenv("JO_PAGE_MAX");
  int v = e ? atoi(e) : 0;
  if (v > 0) return v;
  return PW_PAGE_MAX_DEFAULT;
}
static int pw_walk_enabled(void) {
  const char *e = getenv("JO_PAGE_WALK");
  return !(e && e[0] == '0' && e[1] == 0);
}

cJSON *pw_fetch_json(const source_ctx *c, const char *url, void *ud) {
  (void)ud;
  return feed_get_json(c->http, url, 25000);
}

/* ── envelope readers ─────────────────────────────────────────────────────
 * Every accessor below is best-effort and returns "unknown" rather than a
 * guess. A wrong `total` would turn a disclosure into a false claim, which is
 * worse than saying nothing. */

/* A next-page URL the SERVER supplied. Only absolute http(s) is accepted: a
 * relative or templated value would have to be resolved against assumptions
 * about the API, which is exactly what this module refuses to do. */
static char *pw_next_link(const cJSON *doc) {
  if (!cJSON_IsObject(doc)) return NULL;
  static const char *const direct[] = {
    "next", "@odata.nextLink", "nextLink", "next_page_url", "nextPageUrl",
    "nextRecordsUrl", NULL
  };
  static const char *const nested[] = { "links", "paging", "meta", "pagination", NULL };

  const cJSON *cand = NULL;
  for (int i = 0; direct[i] && !cand; i++) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(doc, direct[i]);
    if (cJSON_IsString(v)) cand = v;
  }
  for (int i = 0; nested[i] && !cand; i++) {
    const cJSON *o = cJSON_GetObjectItemCaseSensitive(doc, nested[i]);
    if (!cJSON_IsObject(o)) continue;
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, "next");
    if (cJSON_IsObject(v)) v = cJSON_GetObjectItemCaseSensitive(v, "href");
    if (cJSON_IsString(v)) cand = v;
  }
  if (!cand || !cand->valuestring) return NULL;
  const char *u = cand->valuestring;
  if (strncasecmp(u, "http://", 7) && strncasecmp(u, "https://", 8)) return NULL;
  size_t n = strlen(u);
  if (n < 8 || n > 4000) return NULL;
  char *out = malloc(n + 1);
  if (out) memcpy(out, u, n + 1);
  return out;
}

/* How many records the upstream says exist in total, or -1 when it did not
 * say. Only unambiguous, well-known field names — `count` is deliberately
 * absent because half the APIs here use it for "records in THIS page". */
static long pw_total_available(const cJSON *doc) {
  if (!cJSON_IsObject(doc)) return -1;
  static const char *const keys[] = {
    "number_of_results", "totalResults", "total_count", "totalCount",
    "totalRecords", "numFound", "total_rows", "@odata.count", "totalFeatures",
    /* `resultcount` is unambiguous in a way bare `count` is not: both APIs in
     * this tree that publish it (HUDOC, AUR) mean the size of the whole match
     * set, not the size of this page. Checked before adding — that is the only
     * reason `count` above is deliberately absent from this list. */
    "resultcount",
    NULL
  };
  for (int i = 0; keys[i]; i++) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(doc, keys[i]);
    if (cJSON_IsNumber(v) && v->valuedouble >= 0) return (long)v->valuedouble;
  }
  const cJSON *meta = cJSON_GetObjectItemCaseSensitive(doc, "meta");
  if (cJSON_IsObject(meta)) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(meta, "total");
    if (cJSON_IsNumber(v) && v->valuedouble >= 0) return (long)v->valuedouble;
  }
  return -1;
}

/* ── URL parameter surgery ────────────────────────────────────────────────
 * Locate `name=<digits>` in the query string. Returns 1 and reports the value
 * plus the byte range of the digits, so a caller can rewrite just the number. */
static int pw_find_num_param(const char *url, const char *name,
                             long *val, size_t *dstart, size_t *dend) {
  size_t nl = strlen(name);
  const char *q = strchr(url, '?');
  if (!q) return 0;
  for (const char *p = q; *p; p++) {
    if (*p != '?' && *p != '&') continue;
    const char *k = p + 1;
    if (strncasecmp(k, name, nl) != 0) continue;
    if (k[nl] != '=') continue;
    const char *d = k + nl + 1;
    if (!isdigit((unsigned char)*d)) continue;
    const char *e = d;
    while (isdigit((unsigned char)*e)) e++;
    /* The WHOLE value must be the integer. `start` and `from` are date
     * parameters at least as often as they are record offsets, and a leading
     * digit run is not enough to tell them apart: `start=2026-08-01` would
     * parse as 2026 and "advance" to 2029-08-01 — a fabricated request for a
     * different time window, whose records would then be attributed to this
     * source and counted into records_used. Requiring the value to end at the
     * parameter boundary rejects every date, timestamp and composite value
     * while accepting every genuine `offset=120` / `limit=20`. */
    if (*e != '&' && *e != 0) continue;
    *val = strtol(d, NULL, 10);
    *dstart = (size_t)(d - url);
    *dend = (size_t)(e - url);
    return 1;
  }
  return 0;
}

/* The page-size parameters that actually occur in this tree, most common
 * first (measured over collectors/sources/{vsrc,csrc14}*.c). */
static const char *const PW_SIZE_PARAMS[] = {
  "limit", "rows", "$limit", "resultRecordCount", "page_size", "per_page",
  "pageSize", "$top", "size", "maxRecords", "count", "retmax", "itemsPerPage",
  /* `length` is the DataTables server-side convention, which HUDOC (ECHR) and
   * GA Tech's GRIP both speak. Measured before adding: all seven occurrences in
   * the fleet are page sizes (`start=0&length=50`, `events?length=50`), none is
   * a duration or a geometry. */
  "length",
  /* `num` is the Google/SerpAPI spelling, which CORDIS also speaks. Measured
   * before adding, same as `length`: all seven occurrences in the fleet are
   * results-per-page (`&num=10`, `&num=20`, `&num=100`, CORDIS `&p=1&num=50`),
   * none is a quantity, a count of anything real, or an identifier. Only the
   * three CORDIS URLs reach pw_walk — the other four are hand-written
   * collectors that never call it — so this widens nothing else. */
  "num",
  /* `rpp` is NSF's spelling of results-per-page. All three occurrences in the
   * fleet are `rpp=25` against api.nsf.gov/services/v1/awards.json, which is
   * the one that matters: paired with `offset=1` it is the only offset walk in
   * the tree that was blocked purely on a size spelling.
   *
   * NOT added, and measured the same way: `max` (17 occurrences) and `nrows`
   * (1). Neither appears in any URL that reaches pw_walk — they are all
   * hand-written or hpengine collectors — so adding them would buy nothing
   * today while widening what a future VJSON URL silently opts into, and `max`
   * is a generic enough name to mean a threshold rather than a page size. */
  "rpp",
  /* `perPage` is the camelCase twin of `per_page`, which was already here. All
   * three occurrences are `?perPage=50&page=0` against avoindata.eduskunta.fi.
   * They walk either way — the inferred-size path below would carry them — but
   * a size the author DECLARED is better evidence than one observed from a
   * response, so it should be the one used. */
  "perPage",
  NULL
};
/* Offset/page parameters. `page`/`p` are 1-based page numbers, the rest are
 * record offsets — the distinction decides how far to advance.
 *
 * `from` is deliberately ABSENT. Measured across every literal URL in the
 * fleet, it is never a record offset: it carries an epoch (`from=1754697600`),
 * an ISO date (`from=2026-01-01`), or an FX currency code. Treating it as an
 * offset would advance a time window and attribute the resulting records to
 * this source. `start` IS kept, because `start=0/1/100/200` are genuine
 * offsets here — but it is also spelled as a date by several sources, which is
 * what pw_offset_plausible() below exists to separate. */
static const char *const PW_OFF_PARAMS[]  = { "offset", "$skip", "skip",
                                              "resultOffset", "startIndex",
                                              "start", NULL };

/* A record offset is a small number. Anything above this is a timestamp that
 * happens to be wholly numeric — `start=1754697600` parses as a valid integer
 * and would otherwise be "advanced" by the page size into a different time
 * window. No paged API in this fleet is walked past ten million records (the
 * JO_PAGE_MAX ceiling stops long before), so this rejects only fabrication. */
#define PW_OFFSET_MAX 10000000L
static const char *const PW_PAGE_PARAMS[] = { "page", "pageNumber", "p", NULL };

static int pw_find_any(const char *url, const char *const *names,
                       const char **which, long *val,
                       size_t *dstart, size_t *dend) {
  for (int i = 0; names[i]; i++)
    if (pw_find_num_param(url, names[i], val, dstart, dend)) {
      if (which) *which = names[i];
      return 1;
    }
  return 0;
}

/* Rewrite the digits at [dstart,dend) with `nv`. */
static char *pw_set_num(const char *url, size_t dstart, size_t dend, long nv) {
  char num[32];
  int nn = snprintf(num, sizeof num, "%ld", nv);
  if (nn <= 0) return NULL;
  size_t len = strlen(url) - (dend - dstart) + (size_t)nn;
  char *out = malloc(len + 1);
  if (!out) return NULL;
  memcpy(out, url, dstart);
  memcpy(out + dstart, num, (size_t)nn);
  strcpy(out + dstart + (size_t)nn, url + dend);
  return out;
}

/* FNV-1a over the page's serialised form. Used only to answer "is this byte
 * for byte the page I already have", so collisions cost one extra fetch and a
 * slightly early stop, never a wrong record. Serialising is what the fetch
 * already did in reverse and the buffer is freed immediately, so the cost is
 * one transient copy of a page we are holding anyway. Returns 0 when the doc
 * cannot be printed, which never equals a real fingerprint's initial basis. */
static unsigned long pw_fingerprint(const cJSON *doc) {
  char *t = cJSON_PrintUnformatted((cJSON *)doc);
  if (!t) return 0;
  unsigned long h = 1469598103934665603UL;          /* FNV-1a 64 offset basis */
  for (const unsigned char *p = (const unsigned char *)t; *p; p++) {
    h ^= *p;
    h *= 1099511628211UL;
  }
  free(t);
  return h ? h : 1;                                 /* never collide with "unprintable" */
}

/* The next URL derived from the collector's OWN parameters, or NULL when the
 * URL carries nothing that can be legitimately advanced. */
static char *pw_advance_url(const char *url, long page_size, int pages_done) {
  long v = 0; size_t ds = 0, de = 0;
  if (pw_find_any(url, PW_OFF_PARAMS, NULL, &v, &ds, &de)) {
    if (page_size <= 0) return NULL;
    if (v < 0 || v > PW_OFFSET_MAX) return NULL;   /* a timestamp, not an offset */
    return pw_set_num(url, ds, de, v + page_size);
  }
  if (pw_find_any(url, PW_PAGE_PARAMS, NULL, &v, &ds, &de)) {
    (void)pages_done;
    return pw_set_num(url, ds, de, v + 1);
  }
  return NULL;
}

/* ── the disclosure ─────────────────────────────────────────────────────────
 * The record itself is built by jo_truncation_notice_ex() (lib/jocore.h), which
 * is the single builder for `collector-truncation-notice` across the tree — one
 * record_type, one uid convention, one tag set, one `records_available == -1 ->
 * null` rule. This function only supplies the three facts that are peculiar to
 * a page walk and that the six-property base cannot carry.
 *
 * `query` is deliberately NULL: the uid must stay one row per SOURCE, as it was
 * when this module hand-rolled the record, rather than one row per url — a walk
 * whose offset advances would otherwise leave a new notice behind on every page
 * boundary. The url is published as a property instead, where it belongs. */
static void pw_notice(intel_sink *s, const char *id, const char *url,
                      long used, long available, int pages, long dropped,
                      const char *reason, const char *remedy) {
  cJSON *extra = cJSON_CreateObject();
  if (extra) {
    cJSON_AddStringToObject(extra, "url", url ? url : "");
    cJSON_AddNumberToObject(extra, "pages_read", pages);
    /* Records the pages held that the emitter could not turn into rows. Always
     * present so a consumer never has to infer it from a missing key. */
    cJSON_AddNumberToObject(extra, "records_dropped", (double)dropped);
  }
  jo_truncation_notice_ex(s, id, NULL, used, available, reason, remedy, extra);
}

int pw_walk(const source_ctx *c, intel_sink *s, const char *id,
            const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page, void *ud) {
  if (!c || !s || !url || !fetch || !emit_page) return -1;

  /* Plan against the URL that will ACTUALLY be fetched. http_request applies
   * url_override_apply() on the way out, so a source whose offset parameter an
   * operator approved (core/pagination_probe.c proposes them, the repair
   * approval route installs them) carries that parameter only in its
   * OVERRIDDEN url. Reading the compile-time one here would mean the walk
   * plans a single page while the fetch goes somewhere paginable — the swap
   * would land and change nothing. Applying it twice is a no-op: the override
   * table is keyed on the original url. */
  url = url_override_apply(url);

  /* The declared page size. It is both the step for an offset walk and the
   * yardstick for "did this page come back full", which is the only evidence
   * we have that more exists when the upstream tells us nothing. */
  long page_size = -1; size_t ds = 0, de = 0;
  pw_find_any(url, PW_SIZE_PARAMS, NULL, &page_size, &ds, &de);

  /* Does the URL carry a page NUMBER the author wrote? That is a weaker
   * requirement than a declared size, and it unlocks the case this module was
   * named for and then did not cover: `?page=1` with no size parameter at all.
   * ROR is the header's own motivating example — measured against the real
   * pw_walk, `https://api.ror.org/organizations?page=1` did exactly ONE fetch,
   * because the continuation test demanded a page_size the URL never states.
   * Nine more fleet URLs are the same shape (bio.tools, luchtmeetnet ×4,
   * data.gov.sg, CORDIS before `num` was recognised).
   *
   * When the URL states no size, the server's OWN first page states it: page 1
   * came back with N records, so a later page holding N again is full by the
   * server's own measure, and a shorter one is the end of the data. That
   * infers nothing about the upstream and invents no parameter — `page` is
   * still a number the author wrote, and it still advances by one.
   *
   * Deliberately page-numbers ONLY. An offset with no declared size cannot be
   * stepped this way: guessing the stride from one observed page would skip or
   * re-read records the moment the server returns fewer than it offered. Those
   * URLs (HUDOC's `start=0`) keep falling through to the disclosure below. */
  long pn = 0; size_t pds = 0, pde = 0;
  const int has_page_param =
    pw_find_any(url, PW_PAGE_PARAMS, NULL, &pn, &pds, &pde);
  /* The walk this module could not do before: page numbers, no declared size,
   * yardstick taken from the server's own first page. */
  const int infer_size_from_server = (page_size <= 0 && has_page_param);

  const int page_max = pw_page_max();
  const int may_walk = pw_walk_enabled();

  char *cur = strdup(url);
  if (!cur) return -1;

  long total = 0, available = -1, dropped = 0;
  int  pages = 0, last_seen = 0;
  int  first_seen = -1;   /* the server's own page size, when the URL states none */
  int  stopped_at_ceiling = 0, stopped_on_repeat = 0, full_last = 0;
  int  stopped_on_same_body = 0;
  unsigned long prev_fp = 0;
  char *next = NULL;

  for (;;) {
    cJSON *doc = fetch(c, cur, ud);
    if (!doc) {
      if (pages == 0) { free(cur); return -1; }   /* dead endpoint = error */
      /* A later page failing is not an error: keep what we have and say so. */
      stopped_at_ceiling = 1;
      break;
    }

    /* An upstream that IGNORES the parameter we advanced answers page 2 with
     * page 1. The next-link guard below cannot see it: we built this URL
     * ourselves, so it differs from the last one by construction even though
     * the response does not. Left unchecked that is F-5 again by another route
     * — every re-emit added to `total`, which the notice then publishes as
     * fact, so the disclosure becomes the fabrication.
     *
     * Checked ONLY on the inferred-size walk, which is the one case where the
     * collector's URL never stated a page size and the yardstick came from the
     * server instead. A URL that declares its own size documents that its
     * author knew the endpoint's paging contract; a bare `?page=1` documents
     * nothing, so it earns the extra evidence before we keep asking. Narrow on
     * purpose: this compares response BODIES, and the rest of the module
     * deliberately reasons about paging without reading content.
     *
     * Compared BEFORE emitting, so a repeat costs one wasted fetch and changes
     * nothing else: no duplicate rows, no inflated count, and `pages` stays
     * the number of DISTINCT pages collected. */
    if (infer_size_from_server) {
      unsigned long fp = pw_fingerprint(doc);
      if (pages > 0 && fp == prev_fp) {
        cJSON_Delete(doc);
        stopped_on_same_body = 1;
        break;
      }
      prev_fp = fp;
    }
    pages++;
    last_seen = 0;
    int emitted = emit_page(c, s, id, doc, ud, &last_seen);
    if (emitted < 0) emitted = 0;
    if (last_seen < emitted) last_seen = emitted;   /* defensive: seen >= emitted */
    if (first_seen < 0) first_seen = last_seen;
    total   += emitted;
    dropped += last_seen - emitted;

    long t = pw_total_available(doc);
    if (t >= 0) available = t;

    next = pw_next_link(doc);
    cJSON_Delete(doc);

    /* A server that hands back a link to the page we just fetched is a real
     * failure mode on cursor APIs at the last page. Following it re-emits the
     * same records until the ceiling — 19 wasted round trips against a live
     * host, and every re-emit added to `total`, which the notice below then
     * publishes as fact. Stop, and say why. */
    if (next && strcmp(next, cur) == 0) {
      free(next); next = NULL;
      stopped_on_repeat = 1;
      break;
    }

    /* Did this page come back full? That is the only evidence that more exists
     * when the upstream says nothing, so it gates both the continuation below
     * and the disclosure after the loop — the two must never disagree, which is
     * why it is computed once, here. The test is on records SEEN, not emitted:
     * see pw_emit_fn. The yardstick is the URL's declared size when it states
     * one, and otherwise the server's own first page (has_page_param only). */
    full_last = (page_size > 0)
      ? (last_seen >= page_size)
      : (has_page_param && first_seen > 0 && last_seen >= first_seen);

    if (!next && may_walk) {
      /* No server-supplied link. Only advance a parameter the author already
       * wrote, and only when this page came back full — a short page is the
       * end of the data, and re-requesting it would be a wasted round trip. */
      if (full_last) next = pw_advance_url(cur, page_size, pages);
    }
    if (!next) break;
    if (!may_walk || pages >= page_max) { stopped_at_ceiling = 1; break; }

    free(cur);
    cur = next; next = NULL;
  }
  free(next);

  /* Did we leave anything? Two independent kinds of evidence:
   *   - we stopped while a continuation was still available (ceiling/failure);
   *   - the upstream counted more than we emitted;
   *   - or the last page came back exactly full and nothing in the response or
   *     the URL let us ask for the rest, which is the 2,592-source case. */
  int more_known    = (available >= 0 && available > total);
  if (stopped_at_ceiling || stopped_on_repeat || stopped_on_same_body ||
      more_known || full_last || dropped > 0) {
    const char *reason =
      stopped_on_same_body ? "the upstream answered the next page with the page "
                           "just fetched, byte for byte, so it is ignoring the "
                           "paging parameter this URL carries; the walk stopped "
                           "rather than re-collecting the same records"
      : stopped_on_repeat  ? "the upstream's next-page link pointed back at the page "
                           "just fetched, so the walk stopped rather than "
                           "re-collecting the same records"
      : stopped_at_ceiling ? "the page ceiling (JO_PAGE_MAX) or a failed page stopped the walk"
      : more_known       ? "the upstream reports more records than were collected"
      : full_last        ? "the last page came back full and the upstream offered "
                           "no next link, nor does this source's URL carry an "
                           "offset parameter that could be advanced"
                         : "records in the pages fetched carried no field this "
                           "collector could use as a title, so they were not "
                           "emitted as rows";
    const char *remedy =
      stopped_on_same_body ? "this endpoint does not page the way its URL implies — "
                           "check whether it needs a different parameter, or drop "
                           "the one it ignores so the bound is stated honestly"
      : stopped_on_repeat  ? "the upstream's pagination is not advancing — check whether "
                           "this endpoint needs a cursor parameter we are not sending"
      : stopped_at_ceiling ? "raise JO_PAGE_MAX, or re-run — the walk resumes from the same URL"
      : (more_known || full_last)
                         ? "give this source an offset/page parameter in its URL so "
                           "lib/pagewalk.c can continue it — see docs/SOURCE_EXHAUSTIVENESS.md"
                         : "give this source an explicit record path or a title field "
                           "mapping — see docs/SOURCE_EXHAUSTIVENESS.md";
    /* records_used is what was EMITTED; records_seen is what the pages held.
     * When those differ the gap is the discard, and it is stated as data. */
    pw_notice(s, id, url, total, available, pages, dropped, reason, remedy);
    fprintf(stderr, "[%s] emitted %ld across %d page(s) — TRUNCATED (%s)\n",
            id, total, pages, reason);
  } else {
    fprintf(stderr, "[%s] emitted %ld across %d page(s)\n", id, total, pages);
  }

  free(cur);
  return (int)total;
}
