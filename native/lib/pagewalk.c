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
#include <limits.h>   /* LONG_MAX: pw_odata_v2_count() */

/* The ONLY cap in this file that stops a walk. Every stop it causes emits a
 * collector-truncation-notice naming JO_PAGE_MAX as the remedy, so the bound is
 * stated in-band rather than hidden.
 *
 * ONE CEILING FOR THE WHOLE TREE, because there were three and an operator
 * could not have known. This file's header calls pw_walk "the only page walk in
 * the tree", and of the continuation DECISION that is true — but the BOUND was
 * copied three times with two different numbers and two different knobs:
 *
 *   lib/pagewalk.c   20   $JO_PAGE_MAX            the generated V* fleet
 *   lib/jsonlist.c   20   $JO_JSONLIST_PAGE_MAX   hpengine rows with page_walk=1
 *   lib/hpengine.c   10   (no knob at all)        rows declaring their own paging
 *   lib/geojson.c    20   $JO_GEOJSON_PAGE_MAX    the VGEO fleet — missed by the
 *                                                 unification, found 2026-11-07
 *
 * So `JO_PAGE_MAX=50` raised one population, silently did nothing for the
 * second, and could not reach the third at any value — and the third is the
 * largest: 1,919 scheduled rows declare page_param/next_path/{page} without a
 * per-row page_max, and stopped at 10 pages because of a number none of them
 * mentions. A ceiling nobody can see is the same shape as the discard this
 * module exists to disclose.
 *
 * `legacy_env` keeps an older spelling working rather than ignoring it: an
 * operator who set $JO_JSONLIST_PAGE_MAX gets what they asked for, and
 * $JO_PAGE_MAX wins when both are set. */
#define JO_PAGE_MAX_DEFAULT 20   /* exhaustive-ok: runaway ceiling, disclosed as a truncation notice */

int jo_page_max(const char *legacy_env) {
  const char *e = getenv("JO_PAGE_MAX");
  if ((!e || !*e) && legacy_env) e = getenv(legacy_env);
  int v = (e && *e) ? atoi(e) : 0;
  if (v > 0) return v;
  return JO_PAGE_MAX_DEFAULT;
}

/* `declared` is a source's own MEASURED ceiling (pw_walk_max, or an hp row's
 * page_max), 0 for "use the tree's one default above". JO_PAGE_MAX always
 * wins, because it is the operator's brake and a source is not allowed to talk
 * past it — including downwards: an operator who sets JO_PAGE_MAX=1 to make a
 * run cheap must get one page from every source, not 160 from the ones that
 * asked for more.
 *
 * This lives here, exported, because hpengine resolved the same question with
 * its own expression — `s->page_max > 0 ? s->page_max : jo_page_max(NULL)` —
 * which honoured the knob only when a row declared NO ceiling. So the brake
 * did not reach the rows most in need of it, while hpengine.h's own comment
 * said page_max was "raised or lowered for the whole tree by $JO_PAGE_MAX":
 * a capability a comment claims and nothing implements. It cost a whole run
 * (an attempt to bound SCEDC's declared 200 pages to 5 read 80 pages before
 * the clock stopped it, 2026-10-21) and it silently defeated
 * tools/audit_page_ceiling.py, whose entire shallow-probe design rests on
 * JO_PAGE_MAX bounding EVERY row: a cycling row declaring page_max = 200 was
 * walked 200 pages deep by a tool whose flag says 3. One function now, called
 * from both engines. */
int jo_page_max_for(int declared) {
  const char *e = getenv("JO_PAGE_MAX");
  int v = (e && *e) ? atoi(e) : 0;
  if (v > 0) return v;
  if (declared > 0) return declared;
  return jo_page_max(NULL);
}
static int pw_page_max(int declared) { return jo_page_max_for(declared); }
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

/* Dotted lookup that tolerates a missing level, with the literal-key fallback
 * for a key that itself contains a dot (`@odata.nextLink`, `odata.count`):
 * tried only AFTER the split fails, so a path that resolves by splitting
 * resolves to the same node. lib/jsonlist.c's dotted() is the original; this
 * is the copy the shared readers below use, CASE-INSENSITIVE as that one is
 * (cJSON_GetObjectItem), because the VJSON fleet — the larger population —
 * has always been read that way. */
static const cJSON *pw_dotted(const cJSON *doc, const char *path) {
  if (!doc || !path || !*path) return NULL;
  char buf[128];
  snprintf(buf, sizeof buf, "%s", path);
  const cJSON *cur = doc;
  char *save = NULL;
  for (char *tok = strtok_r(buf, ".", &save); tok && cur;
       tok = strtok_r(NULL, ".", &save))
    cur = cJSON_GetObjectItem(cur, tok);
  if (!cur && strchr(path, '.')) cur = cJSON_GetObjectItem(doc, path);
  return cur;
}

/* See pagewalk.h. Moved here from lib/jsonlist.c's resolve_link (2026-11-07)
 * so the three walkers resolve a link the same way. */
char *jo_resolve_link(const char *base, const char *href) {
  if (!href || !href[0]) return NULL;
  size_t hl = strlen(href);
  if (hl > 4000) return NULL;            /* not a URL anyone meant */
  if (!strncasecmp(href, "http://", 7) || !strncasecmp(href, "https://", 8))
    return strdup(href);
  if (!base || (strncasecmp(base, "http://", 7) && strncasecmp(base, "https://", 8)))
    return NULL;
  size_t cap = strlen(base) + hl + 2;
  char *out = malloc(cap);
  if (!out) return NULL;
  if (href[0] == '?') {                       /* same path, new query */
    const char *q = strchr(base, '?');
    const char *f = strchr(base, '#');
    if (!q || (f && f < q)) q = f;
    size_t head = q ? (size_t)(q - base) : strlen(base);
    memcpy(out, base, head);
    snprintf(out + head, cap - head, "%s", href);
    return out;
  }
  if (href[0] == '/' && href[1] != '/') {     /* same origin, new path */
    const char *p = strstr(base, "://");
    const char *e = p ? p + 3 : base;
    while (*e && *e != '/' && *e != '?' && *e != '#') e++;
    size_t head = (size_t)(e - base);
    memcpy(out, base, head);
    snprintf(out + head, cap - head, "%s", href);
    return out;
  }
  free(out);
  return NULL;
}

/* THE ONE NEXT-LINK READER (2026-11-07). There were three, and an audit that
 * read every copy found each blind where another could see:
 *
 *   lib/pagewalk.c   pw_next_link   absolute links only — so `nextRecordsUrl`
 *                                   (Salesforce, ALWAYS a path: "/services/…")
 *                                   sat in its list matching nothing, and a
 *                                   relative `next` stopped the walk; lacked
 *                                   `links.next_url`, `nextUrl`,
 *                                   `pagination.next_page` and friends
 *   lib/jsonlist.c   next_link      lacked `nextLink`, `next_page_url`,
 *                                   `nextRecordsUrl`, `{links,paging,
 *                                   pagination}.nextLink`
 *   lib/geojson.c    gj_next_link   only `links[rel=next]`, and handed a
 *                                   relative href to the fetch UNRESOLVED
 *
 * So the union, resolved one way. Relative resolution is limited to the two
 * spellings RFC 3986 makes unambiguous and that actually occur — "?query"
 * (bio.tools answers `"next": "?page=2&format=json"`) and "/absolute-path" —
 * which take nothing from the API but the origin and path we already asked.
 * Anything else (a bare cursor token, another scheme, "//host") is not a URL
 * and is refused rather than glued onto the base and hoped for. A link that is
 * present but null/false/empty means "last page" in every dialect here.
 * Ordered most- to least-specific; the ARRAY form is selected by rel, never by
 * position, because a server is free to put "self" first. */
char *jo_next_link(const cJSON *doc, const char *base) {
  if (!cJSON_IsObject(doc)) return NULL;
  static const char *const KEYS[] = {
    "links.next", "next", "next_url", "nextUrl", "nextPageUrl",
    "meta.next", "paging.next", "@odata.nextLink", "next_page",
    "meta.pagination.next", "pagination.next", "pagination.next_page",
    "meta.pagination.next_page", "links.next_url",
    /* OCHA FTS (api.hpc.tools) puts it at meta.nextLink: ocha-fts-flow-2026
     * stopped after its first 200 of 20,161 flows (measured 2026-09-15). */
    "meta.nextLink",
    /* pagewalk's spellings jsonlist lacked. */
    "nextLink", "next_page_url", "nextRecordsUrl",
    "links.nextLink", "paging.nextLink", "pagination.nextLink", NULL };
  for (int i = 0; KEYS[i]; i++) {
    const cJSON *v = pw_dotted(doc, KEYS[i]);
    if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
      char *u = jo_resolve_link(base, v->valuestring);
      if (u) return u;
    }
    /* CKAN and some JSON:API servers nest it as {next: {href: "..."}}. */
    if (cJSON_IsObject(v)) {
      const cJSON *h = cJSON_GetObjectItem(v, "href");
      if (cJSON_IsString(h) && h->valuestring) {
        char *u = jo_resolve_link(base, h->valuestring);
        if (u) return u;
      }
    }
  }
  /* The ARRAY form: {"links":[{"rel":"self",…},{"rel":"next","href":…}]}, which
   * Oracle ORDS, HAL and OGC API servers publish. br-tcu-inabilitados (ORDS)
   * stopped at its first 25 records of 766 without it (measured 2026-09-14). */
  const cJSON *arr = cJSON_GetObjectItem(doc, "links");
  if (cJSON_IsArray(arr)) {
    const cJSON *l;
    cJSON_ArrayForEach(l, arr) {
      const cJSON *rel  = cJSON_GetObjectItem(l, "rel");
      const cJSON *href = cJSON_GetObjectItem(l, "href");
      if (cJSON_IsString(rel) && rel->valuestring &&
          strcasecmp(rel->valuestring, "next") == 0 &&
          cJSON_IsString(href) && href->valuestring) {
        char *u = jo_resolve_link(base, href->valuestring);
        if (u) return u;
      }
    }
  }
  return NULL;
}

/* OData v2's `d.__count` (see pagewalk.h). The rest of the declared-total
 * rule is jo_declared_total() below. */
long pw_odata_v2_count(const cJSON *doc) {
  if (!cJSON_IsObject(doc)) return -1;
  const cJSON *d = cJSON_GetObjectItemCaseSensitive(doc, "d");
  const cJSON *c = cJSON_IsObject(d) ? cJSON_GetObjectItemCaseSensitive(d, "__count") : NULL;
  if (cJSON_IsNumber(c)) return c->valuedouble >= 0 ? (long)c->valuedouble : -1;
  if (!cJSON_IsString(c) || !c->valuestring || !c->valuestring[0]) return -1;
  long v = 0;
  for (const char *p = c->valuestring; *p; p++) {
    if (*p < '0' || *p > '9') return -1;   /* the whole string, or nothing */
    if (v > (LONG_MAX - 9) / 10) return -1;
    v = v * 10 + (*p - '0');
  }
  return v;
}

/* A number that can be a record count: a non-negative INTEGER. A fractional
 * value under a key spelled `total` is a sum of money or a measurement, never
 * how many records exist, so it is not read as one (strictly narrower than
 * what any of the three old lists accepted). */
static int pw_is_count(const cJSON *v) {
  return cJSON_IsNumber(v) && v->valuedouble >= 0 &&
         v->valuedouble < 9.0e15 && v->valuedouble == (double)(long long)v->valuedouble;
}

/* THE ONE DECLARED-TOTAL READER (2026-11-07). See pagewalk.h for the contract.
 *
 * There were three lists — lib/jsonlist.c `declared_total`, this file's
 * `pw_total_available` and lib/geojson.c `gj_declared_total` — and an audit
 * that read all three found them disagreeing in both directions: this file
 * lacked `odata.count`, `totalItems`, `nhits`, `response.numFound`,
 * `totalHits`, `payload.totalHits`, `hits.total`, `meta.pagination.total`,
 * `pagination.total`, `total_results`, `totalResultCount` and `meta.count`;
 * jsonlist lacked `totalRecords`, `total_rows` and `totalFeatures`; geojson
 * knew four spellings. A row's notice could therefore say "the upstream
 * declares no total" in one engine about a body another engine read the
 * number out of — the shape this tree has repaired three times already.
 *
 * MATCHING IS CASE-INSENSITIVE, as jsonlist's and geojson's always were
 * (cJSON_GetObjectItem); this file used to match case-sensitively, so for the
 * pw_walk population the change is strictly additive (`resultcount` and
 * `resultCount`, both of which this tree has met, are now one entry).
 *
 * BARE ROOT `count` IS THE ONE DELIBERATE DECISION. It is the commonest
 * spelling of "records in THIS page" as well as of "the whole match set", and
 * the lists disagreed: this file excluded it, jsonlist read it fourth, geojson
 * read it last. Reading a page-size `count` as the total is the DANGEROUS
 * direction — a cut-short walk then looks complete, and jsonlist's last-resort
 * continuation (`available > total`) never fires. Reading a real total as
 * "unknown" only loses a number from a notice. The tree's own evidence that
 * `count` is sometimes the total: Django REST Framework's envelope
 * (Spaceflight News `count` 36,457 beside 100, IFRC GO, InterPro — the
 * comments in collectors/sources/soc_news_media.c and geoeo_ifrc_go.c claim
 * lib/pagewalk.c "discloses against count", which was FALSE until today,
 * because this file never read it) and the Austrian Parliament's filter API
 * (hp3b55_atparl.c). So the rule is the one that separates the two meanings
 * without guessing: a bare `count` is a total only when it EXCEEDS the records
 * this page holds — a count of the page's own records can never be larger
 * than the page. Equal is ambiguous and reads as unknown, which changes no
 * walk: every trigger that reads the total compares `available > fetched`, and
 * an equal total cannot satisfy it. It is checked LAST, after every
 * unambiguous spelling. `result.count` and `meta.count` are nested, name the
 * whole set in every family that publishes them, and are unaffected. */
long jo_declared_total(const cJSON *doc, long page_records,
                       char *basis, size_t cap) {
  if (basis && cap) basis[0] = 0;
  if (!cJSON_IsObject(doc)) return -1;
  static const char *const KEYS[] = {
    /* jsonlist's order first: the VJSON fleet is the largest population, and
     * a body carrying two of these with different values keeps its winner. */
    "total_count", "totalCount", "total", "meta.count",
    "numberMatched", "totalResults", "result.count", "meta.total",
    "totalElements", "recordsTotal",
    /* ROR `number_of_results`, Solr `numFound`/`response.numFound`, ODS
     * `nhits`, CORDIS `payload.totalHits`. */
    "number_of_results", "numFound", "response.numFound", "nhits",
    "totalHits", "payload.totalHits", "totalResultCount", "total_results",
    "resultCount", "hits.total", "meta.pagination.total", "pagination.total",
    /* Cabo Verde's statistics office: `totalItems: 2425` beside 50 items. */
    "totalItems",
    /* OData v4/v3: single keys containing a dot (pw_dotted's fallback).
     * LegCo Hong Kong answers `odata.count: 551619` beside 200 rows. */
    "@odata.count", "odata.count",
    /* The Linked Data API's envelope (lda.data.parliament.uk), 2026-10-23. */
    "result.totalResults",
    /* Japan Search `jps-cross`: `hit: 5409` beside a 500-record page.
     * Singular, so it does not share bare `count`'s ambiguity. */
    "hit",
    /* This file's spellings jsonlist lacked: `totalRecords`, `total_rows`
     * (CouchDB), `totalFeatures` (GeoServer GeoJSON). */
    "totalRecords", "total_rows", "totalFeatures",
    /* geojson's: `matchedCount`. */
    "matchedCount",
    NULL };
  for (int i = 0; KEYS[i]; i++) {
    const cJSON *v = pw_dotted(doc, KEYS[i]);
    if (pw_is_count(v)) {
      if (basis && cap) snprintf(basis, cap, "%s", KEYS[i]);
      return (long)v->valuedouble;
    }
  }
  /* OData v2 verbose: `d.__count`, a string (measured 2026-10-25 on the NZ
   * Charities Register: "46144" beside 5,000 rows). */
  long od = pw_odata_v2_count(doc);
  if (od >= 0) {
    if (basis && cap) snprintf(basis, cap, "d.__count");
    return od;
  }
  const cJSON *c = cJSON_GetObjectItem(doc, "count");
  if (pw_is_count(c)) {
    long v = (long)c->valuedouble;
    if (page_records == JO_TOTAL_TRUST_BARE_COUNT ||
        (page_records >= 0 && v > page_records)) {
      if (basis && cap) snprintf(basis, cap, "count");
      return v;
    }
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
  /* A `#fragment` ends the query (RFC 3986): nothing after it is a
   * parameter, and a value it follows ends there. Fixed 2026-11-07 with the
   * same defect in lib/jsonlist.c's jsonlist_query_int/_set. */
  for (const char *p = q; *p && *p != '#'; p++) {
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
    if (*e != '&' && *e != '#' && *e != 0) continue;
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
/* `$offset` is Socrata SODA's record offset, and its absence here was the
 * second copy of the same defect fixed in lib/jsonlist.c's PAGERS table: a
 * VJSON_KEYED row reaches pw_walk, pw_walk only advances a parameter it can
 * name, and `$offset` was not a name it knew — so the 13 keyed Socrata rows
 * read one page even after jl_seed_cursor wrote `$offset=0` into their URL for
 * them. The spelling is distinct from `offset` at the parser level (the scan
 * compares from the parameter boundary, so `$offset=0` never matches `offset`),
 * which is exactly why the bare spelling did not cover it. */
static const char *const PW_OFF_PARAMS[]  = { "offset", "$offset", "$skip",
                                              "skip", "resultOffset",
                                              "startIndex", "start", NULL };

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

/* ── what a stop means, in words a reader can act on ───────────────────────
 * Both describers are exported (pagewalk.h) because the same sentence used to
 * be written three times — here, in lib/jsonlist.c and in lib/geojson.c — and
 * every copy said "a host that refuses or rate-limits later pages needs a
 * per-host minimum gap (core/hostgate.c)" WHATEVER the failing page answered.
 * lib/hpengine.c had already learned (hp_cut_reason and its remedy chain)
 * that a 400 after a full page is the signature of a deep-paging cap — GLEIF
 * refuses `page[number]=51` with HTTP 400, 50 pages x 200 = 10,000 records —
 * whose remedy is to NARROW each request, and that a gap cannot move it. A
 * remedy that names a knob which cannot move is worse than none (CLAUDE.md),
 * so the three walkers now take the status-specific sentence from here. Found
 * 2026-11-07 by an audit that read every copy. */
void pw_cut_describe(int failed_page, long status, char *reason, size_t rcap,
                     char *remedy, size_t mcap) {
  char pg[32], fdesc[96];
  if (failed_page > 0) snprintf(pg, sizeof pg, "page %d", failed_page);
  else                 snprintf(pg, sizeof pg, "a later page");
  feed_status_describe(status, fdesc, sizeof fdesc);
  const char *why = "", *fix = "";
  int deep = 0;
  if (status == FEED_ST_TRANSPORT) {
    why = "— the exchange never completed — so the walk stopped before the "
          "upstream ran out";
    fix = "re-run: one transport failure is one reading, and in a proxied "
          "container a reset or an empty reply can be this side's own egress. "
          "A host that fails the same page on every run needs a per-host "
          "minimum gap (core/hostgate.c), measured before it is set";
  } else if (status == FEED_ST_UNPARSED) {
    why = ", so its records were never delivered — a 2xx that does not parse is "
          "usually an error, rate-limit or challenge page served as a success";
    fix = "read that page's BODY, not its status: a rate-limit page wants a "
          "per-host gap (core/hostgate.c) set on a measured budget, a bot wall "
          "wants core/host_agents.def, and a moved endpoint wants re-pointing";
  } else if (status == 204 || status == FEED_ST_DECLARED_EMPTY) {
    why = "— the upstream's own \"nothing matched\" — after a page that gave "
          "evidence of more, so the collection most likely ends exactly on a "
          "page boundary";
    fix = "none needed if records_used equals the upstream's own total; "
          "otherwise check whether it caps how deep it pages";
  } else if (status == 304) {
    why = "Not Modified to a request that carried no conditional header (a "
          "cache or proxy answering for a page it never fetched); a 304 has no "
          "body, so the walk stopped before the upstream ran out";
    fix = "re-run; no conditional header is sent on a later page, so a 304 "
          "that persists is a cache or proxy in front of the upstream "
          "answering for it";
  } else if (status >= 300 && status < 400) {
    why = "— a redirect that could not be followed (a loop, or no Location) — "
          "so the walk stopped before the upstream ran out";
    fix = "fetch that page's URL by hand and re-point the row at where the "
          "redirect really goes";
  } else if (status == 429) {
    why = "(rate limited), so the walk stopped before the upstream ran out";
    fix = "a per-host minimum gap in core/hostgate.c — measure the host's "
          "budget first (how many requests a burst gets, and whether a pause "
          "clears it), because a gap set on one reading is a guess";
  } else if (status == 401 || status == 403 || status == 407) {
    why = "— a refusal — so the walk stopped before the upstream ran out";
    fix = "re-run once: a refusal that appears only deep in a walk can be a "
          "rate limit. If it persists, check whether the row needs a "
          "credential (key_env) or the host refuses this client's agent "
          "(core/host_agents.def)";
  } else if (status == 408 || status >= 500) {
    why = "— a server error — so the walk stopped before the upstream ran out";
    fix = "re-run; a server error on one page is one reading. If the same page "
          "fails on every run the upstream cannot serve it; if it follows a "
          "burst, a per-host minimum gap (core/hostgate.c) may clear it";
  } else if (status >= 400) {
    deep = 1;
    fix = "check whether the upstream caps how deep it pages (GLEIF refuses "
          "page[number]=51 with HTTP 400 — 10,000 records); if it does, "
          "narrow each request (a date window or a filter) so every slice "
          "fits under the cap — no gap or ceiling reaches the rest. See "
          "docs/SOURCE_EXHAUSTIVENESS.md";
  } else {
    why = ", so the walk stopped before the upstream ran out";
    fix = "re-run — the fetch recorded no status, so nothing more specific "
          "can be said about this stop";
  }
  if (reason && rcap) {
    if (deep)
      snprintf(reason, rcap, "%s answered %s although the upstream had given "
               "evidence of more (a full previous page, its own total, or a "
               "next link): either a deep-paging limit refused the rest, or "
               "the collection ends exactly on a page boundary", pg, fdesc);
    else
      snprintf(reason, rcap, "%s answered %s%s%s", pg, fdesc,
               why[0] == ',' ? "" : " ", why);
  }
  if (remedy && mcap) snprintf(remedy, mcap, "%s", fix);
}

/* Is $JO_PAGE_MAX set to a value jo_page_max() honours? (0 and garbage are
 * ignored there, so naming the variable for them would send the reader to a
 * knob that did nothing.) */
static int pw_env_ceiling_set(const char *name) {
  const char *e = name ? getenv(name) : NULL;
  return e && *e && atoi(e) > 0;
}

/* Which ceiling stopped a walk, named — see pagewalk.h. */
void jo_page_ceiling_describe(int page_max, int declared, const char *legacy_env,
                              char *reason, size_t rcap,
                              char *remedy, size_t mcap) {
  const int op = pw_env_ceiling_set("JO_PAGE_MAX");
  const int legacy = !op && declared <= 0 && pw_env_ceiling_set(legacy_env);
  const char *who = op       ? "JO_PAGE_MAX"
                  : declared > 0 ? "this source's own measured ceiling"
                  : legacy   ? legacy_env
                             : "the tree's default";
  if (reason && rcap)
    snprintf(reason, rcap, "the page ceiling (%d page%s, %s) stopped the walk "
             "while the upstream still had pages", page_max,
             page_max == 1 ? "" : "s", who);
  if (!remedy || !mcap) return;
  if (op)
    snprintf(remedy, mcap, "raise $JO_PAGE_MAX: an operator set it, and it "
             "overrides every other ceiling in the tree (jo_page_max_for, "
             "lib/pagewalk.c) — see docs/SOURCE_EXHAUSTIVENESS.md");
  else if (declared > 0)
    snprintf(remedy, mcap, "re-measure the upstream's total and raise the "
             "ceiling this source declares (pw_walk_max / page_max), leaving "
             "headroom; over ~200 pages read the collection over a cycle "
             "instead. $JO_PAGE_MAX overrides it either way — see "
             "docs/SOURCE_EXHAUSTIVENESS.md");
  else if (legacy)
    snprintf(remedy, mcap, "raise $JO_PAGE_MAX (the older $%s set this one, "
             "and $JO_PAGE_MAX wins when both are set), or give this source a "
             "measured ceiling of its own — see docs/SOURCE_EXHAUSTIVENESS.md",
             legacy_env);
  else
    snprintf(remedy, mcap, "raise $JO_PAGE_MAX (default %d pages), or give "
             "this source a measured ceiling of its own — see "
             "docs/SOURCE_EXHAUSTIVENESS.md", JO_PAGE_MAX_DEFAULT);
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
                      const char *reason, const char *remedy,
                      int failed_page, long failed_status, const char *failed_url,
                      const char *resume_at) {
  cJSON *extra = cJSON_CreateObject();
  if (extra) {
    cJSON_AddStringToObject(extra, "url", url ? url : "");
    cJSON_AddNumberToObject(extra, "pages_read", pages);
    /* Records the pages held that the emitter could not turn into rows. Always
     * present so a consumer never has to infer it from a missing key. */
    cJSON_AddNumberToObject(extra, "records_dropped", (double)dropped);
    /* The page that did not deliver, when one did not: which one, what it
     * answered (feed_last_json_status's vocabulary) and where it was asked. */
    if (failed_page > 0) {
      cJSON_AddNumberToObject(extra, "failed_page", failed_page);
      cJSON_AddNumberToObject(extra, "failed_page_status", (double)failed_status);
      cJSON_AddStringToObject(extra, "failed_page_url", failed_url ? failed_url : "");
    }
    /* A cycling walk (pw_walk_max_resume) says where the next run continues,
     * as hpengine's page_resume notice does. */
    if (resume_at)
      cJSON_AddStringToObject(extra, "next_run_resumes_at", resume_at);
  }
  jo_truncation_notice_ex(s, id, NULL, used, available, reason, remedy, extra);
}

int pw_walk(const source_ctx *c, intel_sink *s, const char *id,
            const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page, void *ud) {
  return pw_walk_max(c, s, id, url, fetch, emit_page, ud, 0);
}

/* The host of a URL, lower-cased, without userinfo or port, into `out`.
 * Returns 0 when there is none. For pw_walk_max_resume's rule that a stored
 * position may move a walk THROUGH a collection and never to another server
 * (hpengine's hp_host_of is the same rule). */
static int pw_host(const char *u, char *out, size_t cap) {
  if (!u || !cap) return 0;
  const char *p = strstr(u, "://");
  p = p ? p + 3 : u;
  const char *e = p;
  while (*e && *e != '/' && *e != '?' && *e != '#') e++;
  for (const char *a = p; a < e; a++) if (*a == '@') p = a + 1;
  size_t n = 0;
  while (p + n < e && p[n] != ':' && n + 1 < cap) {
    out[n] = (char)tolower((unsigned char)p[n]);
    n++;
  }
  out[n] = 0;
  return n > 0;
}

static int pw_walk_impl(const source_ctx *c, intel_sink *s, const char *id,
                        const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page,
                        void *ud, int page_max_declared, int resume_on,
                        const char *resume_in, char *resume_out,
                        size_t resume_cap);

int pw_walk_max(const source_ctx *c, intel_sink *s, const char *id,
                const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page,
                void *ud, int page_max_declared) {
  return pw_walk_impl(c, s, id, url, fetch, emit_page, ud, page_max_declared,
                      0, NULL, NULL, 0);
}

/* See pagewalk.h. Added 2026-11-07: pw_walk_max had no way to read a
 * collection over a cycle, so a pw_walk source over its ceiling re-read its
 * first pages for ever — the shape hpengine's page_resume fixed for 669 rows,
 * left unreachable for every bespoke collector that walks through here. */
int pw_walk_max_resume(const source_ctx *c, intel_sink *s, const char *id,
                       const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page,
                       void *ud, int page_max_declared,
                       const char *resume_in, char *resume_out, size_t cap) {
  return pw_walk_impl(c, s, id, url, fetch, emit_page, ud, page_max_declared,
                      1, resume_in, resume_out, cap);
}

static int pw_walk_impl(const source_ctx *c, intel_sink *s, const char *id,
                        const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page,
                        void *ud, int page_max_declared, int resume_on,
                        const char *resume_in, char *resume_out,
                        size_t resume_cap) {
  /* Clear first, so every early return leaves "start over" rather than a
   * stale position: a run that fetched nothing has nowhere better to say. */
  if (resume_out && resume_cap) resume_out[0] = 0;
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
   * yardstick taken from the server's own first page. (void)-ed rather than
   * deleted because it names the case the fullness test below branches on, and
   * the same-body guard that used to be gated on it now runs on every walk —
   * see the London Datastore note inside the loop. */
  const int infer_size_from_server = (page_size <= 0 && has_page_param);
  (void)infer_size_from_server;

  const int page_max = pw_page_max(page_max_declared);
  const int may_walk = pw_walk_enabled();

  /* Start where the previous run stopped, when this is a cycling walk and the
   * stored position is on the row's own host. The position is the URL the
   * walk would have requested next, so pw_advance_url() continues from the
   * numbers IN it — there is no page_start to read back, unlike hpengine,
   * because this walk computes every next page from the page just fetched. */
  int resumed = 0;
  char *cur = NULL;
  if (resume_on && resume_in && resume_in[0]) {
    char hw[256], hg[256];
    if (pw_host(url, hw, sizeof hw) && pw_host(resume_in, hg, sizeof hg) &&
        strcmp(hw, hg) == 0) {
      cur = strdup(resume_in);
      resumed = 1;
      fprintf(stderr, "[%s] resuming the walk at %s\n", id, resume_in);
    } else {
      fprintf(stderr, "[%s] stored resume position is on another host than "
                      "the row's own URL — dropped; this run starts from the "
                      "row's URL\n", id);
    }
  }
  if (!cur) cur = strdup(url);
  if (!cur) return -1;
  char *resume_tok = NULL;   /* the next URL a ceiling stop leaves behind */

  long total = 0, available = -1, dropped = 0;
  int  pages = 0, last_seen = 0;
  int  first_seen = -1;   /* the server's own page size, when the URL states none */
  int  stopped_at_ceiling = 0, stopped_on_repeat = 0, full_last = 0;
  int  stopped_on_same_body = 0;
  /* A later page that failed. It used to share stopped_at_ceiling, so the
   * notice blamed "the page ceiling (JO_PAGE_MAX) or a failed page" and named
   * neither the page, nor what it answered, nor where. The status comes from
   * feed_last_json_status(): pw_fetch_json goes through feed_get_json, and a
   * fetch callback that does not leaves it FEED_ST_UNKNOWN, said as such. */
  int  failed_page = 0;
  long failed_status = FEED_ST_UNKNOWN;
  char *failed_url = NULL;
  unsigned long prev_fp = 0;
  char *next = NULL;

  for (;;) {
    feed_last_json_status_reset();
    cJSON *doc = fetch(c, cur, ud);
    if (!doc) {
      /* Page 1 answered the upstream's own "nothing matched" — a 204, or a
       * 404 whose body says so (feed_status_body_is_declared_empty). That is
       * rule 1's honest empty: rc 0, no records, no notice. It used to be -1,
       * so a quiet FDSN window on a VJSON/VGEO row filed a fetch-failure
       * (2026-11-07). Only page 1: a later page's empty is pw_cut_describe's
       * to name, as hpengine does. A resumed walk whose position answers
       * empty has reached the end of the collection, so the cycle restarts. */
      if (pages == 0 && feed_status_declared_empty(feed_last_json_status())) {
        fprintf(stderr, "[%s] the upstream answered with its own no-data "
                        "status: an EMPTY result, not a refusal\n", id);
        free(cur);
        return 0;
      }
      if (pages == 0) { free(cur); return -1; }   /* dead endpoint = error */
      /* A later page failing is not an error: keep what we have and say so. */
      failed_page = pages + 1;
      failed_status = feed_last_json_status();
      failed_url = strdup(cur);
      break;
    }

    /* An upstream that IGNORES the parameter we advanced answers page 2 with
     * page 1. The next-link guard below cannot see it: we built this URL
     * ourselves, so it differs from the last one by construction even though
     * the response does not. Left unchecked that is F-5 again by another route
     * — every re-emit added to `total`, which the notice then publishes as
     * fact, so the disclosure becomes the fabrication.
     *
     * Checked on EVERY walk since 2026-10-12. It used to be the inferred-size
     * walk only, on the reasoning that a URL declaring its own page size
     * documents an author who knew the endpoint's paging contract. The London
     * Datastore disproves that: `?rows=200&start=0` is a declared size and a
     * declared offset, and that server honours NEITHER — it answers every
     * request with its whole catalogue of 1,306 datasets. 1,306 >= 200 reads
     * as a full page, so the walk would have advanced `start` twenty times,
     * emitted the same 1,306 records on each, and published 26,120 in the
     * notice as fact. The guard costs one FNV pass over a page already held
     * and can only ever stop a walk EARLY: two pages of genuinely different
     * records cannot be byte-identical.
     *
     * Compared BEFORE emitting, so a repeat costs one wasted fetch and changes
     * nothing else: no duplicate rows, no inflated count, and `pages` stays
     * the number of DISTINCT pages collected. */
    {
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

    long t = jo_declared_total(doc, last_seen, NULL, 0);
    if (t >= 0) available = t;

    next = jo_next_link(doc, cur);
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
    if (!may_walk || pages >= page_max) {
      stopped_at_ceiling = 1;
      /* A cycling walk keeps the page it would have read next, exactly as
       * hpengine keeps `page_url` when `st.truncated`. Not when continuation
       * is switched off: JO_PAGE_WALK=0 asks for one page, not a cycle. */
      if (resume_on && may_walk) { resume_tok = next; next = NULL; }
      break;
    }

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
  /* Name the ceiling that actually bit, and the failing page's status-specific
   * reason and remedy (jo_page_ceiling_describe, pw_cut_describe). The
   * ceiling used to read "JO_PAGE_MAX" always, which is wrong for a source
   * carrying its own measured ceiling, and every failed page got the same
   * hostgate remedy whatever it answered. */
  char ceilwhy[200], ceilfix[400], failwhy[400] = "", failfix[400] = "";
  jo_page_ceiling_describe(page_max, page_max_declared, NULL,
                           ceilwhy, sizeof ceilwhy, ceilfix, sizeof ceilfix);
  if (failed_page > 0)
    pw_cut_describe(failed_page, failed_status, failwhy, sizeof failwhy,
                    failfix, sizeof failfix);
  if (failed_page > 0 || stopped_at_ceiling || stopped_on_repeat ||
      stopped_on_same_body || more_known || full_last || dropped > 0) {
    const char *reason =
      failed_page > 0      ? failwhy
      : stopped_on_same_body ? "the upstream answered the next page with the page "
                           "just fetched, byte for byte, so it is ignoring the "
                           "paging parameter this URL carries; the walk stopped "
                           "rather than re-collecting the same records"
      : stopped_on_repeat  ? "the upstream's next-page link pointed back at the page "
                           "just fetched, so the walk stopped rather than "
                           "re-collecting the same records"
      : stopped_at_ceiling ? ceilwhy
      : more_known       ? "the upstream reports more records than were collected"
      : full_last        ? "the last page came back full and the upstream offered "
                           "no next link, nor does this source's URL carry an "
                           "offset parameter that could be advanced"
                         : "records in the pages fetched carried no field this "
                           "collector could use as a title, so they were not "
                           "emitted as rows";
    const char *remedy =
      failed_page > 0      ? failfix
      : stopped_on_same_body ? "this endpoint does not page the way its URL implies — "
                           "check whether it needs a different parameter, or drop "
                           "the one it ignores so the bound is stated honestly"
      : stopped_on_repeat  ? "the upstream's pagination is not advancing — check whether "
                           "this endpoint needs a cursor parameter we are not sending"
      : stopped_at_ceiling && resume_tok
                         ? "none needed: this source reads its collection over a "
                           "cycle, and next_run_resumes_at says where the next "
                           "run continues"
      : stopped_at_ceiling ? ceilfix
      /* A row that DECLARED a one-page ceiling bounded itself on purpose, so
       * "give it a page parameter" names the one knob that must not move —
       * this file's own rule that a remedy pointing at an immovable knob is
       * worse than none. The case it exists for is an index with no honoured
       * ordering (gov-dataeuropa-search), where paging re-reads and skips. */
      : page_max_declared == 1 && (more_known || full_last)
                         ? "none, unless the upstream gains a stable ordering: this row "
                           "declares a one-page ceiling on purpose and its own comment "
                           "says what was measured — see docs/SOURCE_EXHAUSTIVENESS.md"
      : (more_known || full_last)
                         ? "give this source an offset/page parameter in its URL so "
                           "lib/pagewalk.c can continue it — see docs/SOURCE_EXHAUSTIVENESS.md"
                         : "give this source an explicit record path or a title field "
                           "mapping — see docs/SOURCE_EXHAUSTIVENESS.md";
    /* records_used is what was EMITTED; records_seen is what the pages held.
     * When those differ the gap is the discard, and it is stated as data. */
    pw_notice(s, id, url, total, available, pages, dropped, reason, remedy,
              failed_page, failed_status, failed_url,
              resume_on ? (resume_tok ? resume_tok
                           : failed_page > 0 ? "the start — the position was NOT "
                             "advanced because a page failed, so this source "
                             "re-reads the same slice until its paging is fixed"
                           : "the start — the walk ended here (the reason "
                             "above says how), so the next run starts over")
                        : NULL);
    fprintf(stderr, "[%s] emitted %ld across %d page(s) — TRUNCATED (%s)\n",
            id, total, pages, reason);
  } else {
    fprintf(stderr, "[%s] emitted %ld across %d page(s)\n", id, total, pages);
  }

  /* Where the next run starts: the page a ceiling stop left unread, else
   * nothing (cleared at the top) — the end of the collection, a failed page
   * (bookmarking a refused page would retry that one page for ever; the
   * notice says the position was not advanced), or a stop that is not a
   * position at all. hpengine's page_resume rule, line for line. */
  if (resume_on && resume_tok) {
    if (resume_out && resume_cap && strlen(resume_tok) < resume_cap) {
      snprintf(resume_out, resume_cap, "%s", resume_tok);
      fprintf(stderr, "[%s] the next run resumes at %s\n", id, resume_tok);
    } else {
      fprintf(stderr, "[%s] the next position does not fit the caller's buffer "
                      "(%zu bytes) — the next run starts over\n", id, resume_cap);
    }
  } else if (resume_on && resumed) {
    fprintf(stderr, "[%s] the cycle %s; the next run starts over\n", id,
            failed_page > 0 ? "was cut by a failed page" : "completed");
  }
  free(resume_tok);
  free(failed_url);
  free(cur);
  return (int)total;
}
