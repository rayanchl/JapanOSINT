/* lib/pagewalk.c — see pagewalk.h for why this exists and what it refuses to do.
 *
 * ONE PAGING CORE (2026-11-16). This file used to be one of FOUR page-walking
 * implementations — lib/jsonlist.c (the VJSON fleet), lib/geojson.c (VGEO),
 * this file's own pw_walk (hand-written collectors) and hpengine's loop — and
 * the repo's history records the same paging fix landing in one copy and
 * missing the others at least four times: the uid-collision guard, the page
 * ceiling, the declared-total lists and the repeat-page guard. It now owns, as
 * the single copy, every DECISION the four make:
 *
 *   a. the upstream's declared total        pw_declared_total / _xml
 *   b. what the next page is                pw_next_link, pw_next_page,
 *                                           pw_next_page_geo, PW_PAGERS
 *   c. whether the upstream moved           pw_page_fp(s), pw_body_fp
 *   d. how many pages a walk may take       pw_page_ceiling, jo_page_max*
 *   e. whether a failed later page is a cut pw_later_page_cut, pw_cut_reason
 *   f. what a stopped walk discloses        pw_notice_emit
 *
 * and the two loops that read a JSON page at a time (pw_walk_list for the
 * jsonlist and GeoJSON drivers, pw_walk_max for the callback one). hpengine
 * keeps its own loop — it carries state the others lack (page_param
 * arithmetic, {page} tokens, page_resume, window_days, detail hops, date_back)
 * — but calls every function above rather than holding a private copy.
 *
 * tests/unit/test_paging_core_single.c fails the build if a second declared-
 * total list or a second pager table reappears in lib/.
 */
#include "pagewalk.h"
#include "feedlib.h"
#include "joenv.h"
#include "jocore.h"        /* jo_memmem(), the notice's cJSON helpers */
#include "../core/url_override.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <limits.h>   /* LONG_MAX: pw_odata_v2_count() */

/* ── (d) the page ceiling ────────────────────────────────────────────────── */

/* The ONLY cap in this file that stops a walk. Every stop it causes emits a
 * collector-truncation-notice naming the knob as the remedy, so the bound is
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

/* THE ONE RESOLUTION, in the order every engine has always used:
 *   $JO_PAGE_MAX            the operator's brake — wins in BOTH directions
 *   `declared`              a source's own MEASURED ceiling (pw_walk_max, an hp
 *                           row's page_max), 0 for none
 *   $<legacy_env>           an older knob (JO_JSONLIST_PAGE_MAX,
 *                           JO_GEOJSON_PAGE_MAX), NULL for none
 *   20                      the tree's default
 * It used to be three expressions. hpengine's read
 * `s->page_max > 0 ? s->page_max : jo_page_max(NULL)`, which honoured the knob
 * only when a row declared NO ceiling, while hpengine.h's own comment said
 * page_max was "raised or lowered for the whole tree by $JO_PAGE_MAX" — a
 * capability a comment claims and nothing implements. It cost a whole run (an
 * attempt to bound SCEDC's declared 200 pages to 5 read 80 pages before the
 * clock stopped it, 2026-10-21) and it silently defeated
 * tools/audit_page_ceiling.py, whose shallow-probe design rests on
 * JO_PAGE_MAX bounding EVERY row. And geojson read its OWN knob
 * ($JO_GEOJSON_PAGE_MAX) and ignored $JO_PAGE_MAX altogether, so the
 * operator's brake did not reach the 300 VGEO rows — see the 2026-11-16 note
 * in docs on what that changed. */
int pw_page_ceiling(const char *legacy_env, int declared) {
  int v = jo_env_int_strict("JO_PAGE_MAX", 0, 1, INT_MAX);
  if (v > 0) return v;
  if (declared > 0) return declared;
  if (legacy_env) {
    v = jo_env_int_strict(legacy_env, 0, 1, INT_MAX);
    if (v > 0) return v;
  }
  return JO_PAGE_MAX_DEFAULT;
}

/* Which knob decided it, for a notice: it used to read "JO_PAGE_MAX" always,
 * which is wrong for a source carrying its own measured ceiling — the reader
 * would go looking for an environment variable nobody set. */
const char *pw_ceiling_name(const char *legacy_env, int declared) {
  if (jo_env_int_strict("JO_PAGE_MAX", 0, 1, INT_MAX) > 0) return "JO_PAGE_MAX";
  if (declared > 0)           return "this source's own measured ceiling";
  if (legacy_env && jo_env_int_strict(legacy_env, 0, 1, INT_MAX) > 0) return legacy_env;
  return "the pagewalk default";
}

/* Historic entry points, kept so no caller changes. */
int jo_page_max(const char *legacy_env) { return pw_page_ceiling(legacy_env, 0); }
int jo_page_max_for(int declared)       { return pw_page_ceiling(NULL, declared); }

static int pw_walk_enabled(void) {
  const char *e = getenv("JO_PAGE_WALK");
  return !(e && e[0] == '0' && e[1] == 0);
}

cJSON *pw_fetch_json(const source_ctx *c, const char *url, void *ud) {
  (void)ud;
  return feed_get_json(c->http, url, 25000);
}

/* ── (c) fingerprints: did the upstream move? ────────────────────────────── */

unsigned long long pw_fnv_str(unsigned long long h, const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; p && *p; p++)
    h = (h ^ *p) * 1099511628211ULL;
  return h;
}

/* FNV-1a 64 over a whole response body, compared for equality between
 * consecutive pages of one walk (hpengine's shape notice (d)); nothing is keyed
 * on it. A NULL body hashes to the basis, as it always did. */
unsigned long long pw_text_hash(const char *s) {
  return pw_fnv_str(PW_FNV_BASIS, s ? s : "");
}

/* The page's serialised form. Used only to answer "is this byte for byte the
 * page I already have", so collisions cost one extra fetch and a slightly early
 * stop, never a wrong record. Returns 0 when the doc cannot be printed, which
 * never equals a real fingerprint. */
unsigned long long pw_body_fp(const cJSON *doc) {
  char *t = cJSON_PrintUnformatted((cJSON *)doc);
  if (!t) return 0;
  unsigned long long h = pw_fnv_str(PW_FNV_BASIS, t);
  free(t);
  return h ? h : 1;                                 /* never collide with "unprintable" */
}

/* Fingerprint of a page's RECORDS (its first eight, serialised), used to notice
 * that the upstream is handing back the SAME page again — which happens
 * whenever a cursor guess is wrong: the server ignores the parameter it does
 * not know and re-serves page 1, and without this the walk would spend the
 * whole ceiling refetching one page and then file a truncation notice claiming
 * pages were pending. Stopping on no-progress makes a wrong guess cost one
 * wasted request and tell no lies. Order-sensitive on purpose: it answers
 * "has the server re-served the page we just read". */
unsigned long long pw_page_fp(const cJSON *arr) {
  unsigned long long h = PW_FNV_BASIS;
  if (!arr) return h;
  int i = 0;
  const cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    char *s = cJSON_PrintUnformatted((cJSON *)rec);
    if (s) { h = pw_fnv_str(h, s); free(s); }
    if (++i >= 8) break;   /* exhaustive-ok: identity probe, not a record read */
  }
  return h;
}

/* ── the page-1 probe's identity set ──────────────────────────────────────
 * The probe asks ONE question: this response came back for `page=1` after a
 * first request that named no page — is it the page we already have (a 1-based
 * API) or the second page (a 0-based one)? It used to be answered by comparing
 * pw_page_fp() above, and that hash answers it only while the collection holds
 * still between the two requests.
 *
 * In hpengine it does not have to. hp_emit_record spends the detail budget per
 * record INSIDE the page loop, so a row with a detail_url puts every hop of
 * page 1 between the two requests of the probe. Measured on
 * us-sf-311-open311-requests (2026-10-24): ~70 s apart, 58 hops at that host's
 * 1200 ms gap, against an Open311 collection with no ORDER BY that reorders on
 * any write. The first eight records were no longer the first eight, the hash
 * missed, the probe read as new data, and the WHOLE first page was emitted a
 * second time — one page_size of collapse per run, every run, on the very
 * metric this tree reads to find rule 4b loss. hptest 28c/28d and 31e are that
 * server; they fail on the hash and pass on identity.
 *
 * So: each record's own content fingerprint, sorted, compared as a set. A
 * reorder cannot move a set, and a record whose CONTENT changed between the
 * two reads is correctly seen as new — re-emitting it updates its row, which
 * is what an update is.
 *
 * And the page is not skipped wholesale. A record that ARRIVED between the two
 * requests is on that response and on nothing we have read, so dropping the
 * page would discard it (house rule 2). The callers emit what the previous
 * page did not carry and skip what it did. */
static unsigned long long pw_rec_fp(const cJSON *rec) {
  unsigned long long h = PW_FNV_BASIS;
  char *s = cJSON_PrintUnformatted((cJSON *)rec);
  if (!s) return h;
  h = pw_fnv_str(h, s);
  free(s);
  return h;
}

static int pw_fp_cmp(const void *a, const void *b) {
  unsigned long long x = *(const unsigned long long *)a;
  unsigned long long y = *(const unsigned long long *)b;
  return x < y ? -1 : x > y ? 1 : 0;
}

unsigned long long *pw_page_fps(const cJSON *arr, int *n_out) {
  if (n_out) *n_out = 0;
  int n = arr ? cJSON_GetArraySize((cJSON *)arr) : 0;
  if (n <= 0) return NULL;
  unsigned long long *fps = malloc((size_t)n * sizeof *fps);
  if (!fps) return NULL;
  int i = 0;
  const cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    if (i >= n) break;
    fps[i++] = pw_rec_fp(rec);
  }
  qsort(fps, (size_t)i, sizeof *fps, pw_fp_cmp);
  if (n_out) *n_out = i;
  return fps;
}

int pw_fps_has(const unsigned long long *fps, int n, const cJSON *rec) {
  if (!fps || n <= 0 || !rec) return 0;
  unsigned long long want = pw_rec_fp(rec);
  int lo = 0, hi = n - 1;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    if (fps[mid] == want) return 1;
    if (fps[mid] < want) lo = mid + 1;
    else                 hi = mid - 1;
  }
  return 0;
}

/* ── URL parameter surgery ────────────────────────────────────────────────
 * Locate `name=<digits>` in the query string. Returns 1 and reports the value
 * plus the byte range of the digits, so a caller can rewrite just the number.
 * `ci` matches the name case-insensitively (the declared-size walk always did;
 * the jsonlist family never did). */
static int pw_url_find(const char *url, const char *name, int ci,
                       long *val, size_t *dstart, size_t *dend) {
  size_t nl = strlen(name);
  const char *q = strchr(url, '?');
  if (!q) return 0;
  for (const char *p = q; *p; p++) {
    if (*p != '?' && *p != '&') continue;
    const char *k = p + 1;
    if ((ci ? strncasecmp(k, name, nl) : strncmp(k, name, nl)) != 0) continue;
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

/* Value of `name=` as a long, or -1 if absent / not wholly an integer. */
long pw_url_int(const char *url, const char *name) {
  long v = 0; size_t a = 0, b = 0;
  return pw_url_find(url, name, 0, &v, &a, &b) ? v : -1;
}

/* Replace `name=<old>` with `name=<new>`, or append it. Caller frees. */
char *pw_url_set(const char *url, const char *name, long value) {
  size_t cap = strlen(url) + strlen(name) + 48;
  char *out = malloc(cap);
  if (!out) return NULL;
  const char *q = strchr(url, '?');
  size_t nlen = strlen(name);
  const char *hit = NULL;
  if (q) {
    for (const char *p = q + 1; p && *p; ) {
      if (!strncmp(p, name, nlen) && p[nlen] == '=') { hit = p; break; }
      p = strchr(p, '&');
      if (p) p++;
    }
  }
  if (!hit) {
    snprintf(out, cap, "%s%c%s=%ld", url, q ? '&' : '?', name, value);
    return out;
  }
  const char *tail = strchr(hit, '&');
  size_t head = (size_t)(hit - url);
  memcpy(out, url, head);
  int w = snprintf(out + head, cap - head, "%s=%ld", name, value);
  if (tail) snprintf(out + head + w, cap - head - w, "%s", tail);
  return out;
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

/* ── (a) the upstream's own count of what exists ──────────────────────────
 * Dotted lookup that tolerates a missing level, for probing candidate keys.
 *
 * A KEY MAY CONTAIN A LITERAL DOT, and splitting every path unconditionally
 * made such a key unreachable. OData spells its envelope fields `odata.count`
 * and `@odata.nextLink` — single keys, dots and all — so the split looked for
 * `doc["odata"]["count"]` and `doc["@odata"]["nextLink"]`, which no OData
 * server has ever published. The literal is tried only AFTER the split fails,
 * and only for a path that has a dot to split, which makes it strictly
 * additive: a path that resolves with the split resolves to the same node. */
static cJSON *pw_dotted(const cJSON *doc, const char *path) {
  char buf[128];
  snprintf(buf, sizeof buf, "%s", path);
  cJSON *cur = (cJSON *)doc;
  char *save = NULL;
  for (char *tok = strtok_r(buf, ".", &save); tok && cur;
       tok = strtok_r(NULL, ".", &save))
    cur = cJSON_GetObjectItem(cur, tok);
  if (!cur && strchr(path, '.')) cur = cJSON_GetObjectItem((cJSON *)doc, path);
  return cur;
}

/* OData v2 "verbose" JSON states its total as `d.__count`, and as a STRING:
 * `{"d":{"results":[…],"__count":"46144"}}` when the request carries
 * `$inlinecount=allpages`. */
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

/* THE declared-total list. It was three: lib/jsonlist.c's `declared_total`
 * (29 keys), this file's `pw_total_available` (16 root keys, meta.total, the
 * two result.* keys), lib/geojson.c's `gj_declared_total` (4 keys) — plus the
 * XML element list in lib/hpengine.c, which is the same idea over text and is
 * pw_declared_total_xml() below. Every key any of them accepted is here.
 *
 * WHERE THE LISTS DISAGREED, and what was kept (also in the 2026-11-16 report):
 *
 *  - bare `count` / `meta.count`: jsonlist and geojson accept them (DRF's
 *    `{count, next, results}` is a whole-set total), this file refused them
 *    ("half the APIs here use it for records in THIS page"). Both are right
 *    for their callers, so they are tagged PW_TOT_COUNT and a caller opts in:
 *    jsonlist, geojson and hpengine do, pw_walk does not. That keeps every
 *    caller's behaviour for these two keys exactly as it was.
 *  - precedence: when one document carries two spellings the FIRST row below
 *    wins. The order is jsonlist's, which serves ~2,000 VJSON rows and every
 *    hp page_walk row; this file's own order differed (e.g. totalResults
 *    before total) and so did geojson's. A document that carries two
 *    spellings with DIFFERENT values is not a case any measured upstream
 *    exhibits.
 *  - case: jsonlist looked keys up case-insensitively, this file did not; the
 *    shared lookup is case-insensitive, so `resultcount` (this file) and
 *    `resultCount` (jsonlist) are one row.
 *  - geojson required the value to be > 0, the others >= 0. >= 0 is kept: a
 *    declared total of zero is a statement, and "unknown" is not.
 *
 * Each row names the API family that publishes it, because a wrong `total`
 * turns a disclosure into a false claim. */
typedef struct { const char *key; unsigned flags; } pw_total_key;
static const pw_total_key PW_TOTAL_KEYS[] = {
  { "total_count", 0 }, { "totalCount", 0 }, { "total", 0 },
  /* `count` is "records in THIS page" as often as it is the whole set. */
  { "count", PW_TOT_COUNT }, { "meta.count", PW_TOT_COUNT },
  { "numberMatched", 0 },                       /* OGC API — Features, WFS    */
  { "totalResults", 0 },
  { "result.count", 0 },                        /* CKAN, data.europa.eu       */
  { "meta.total", 0 },
  { "totalElements", 0 },                       /* Spring Data Page           */
  { "recordsTotal", 0 },                        /* DataTables                 */
  /* The families whose totals the walk was blind to. ROR answers
   * `number_of_results: 108000` next to 20 items, Solr `response.numFound`,
   * OpenDataSoft `nhits`, CORDIS `payload.totalHits`; each of those is the
   * upstream telling us, in its own words, how much it is holding back. */
  { "number_of_results", 0 }, { "numFound", 0 }, { "response.numFound", 0 },
  { "nhits", 0 }, { "totalHits", 0 }, { "payload.totalHits", 0 },
  { "totalResultCount", 0 }, { "total_results", 0 }, { "resultCount", 0 },
  { "hits.total", 0 }, { "meta.pagination.total", 0 }, { "pagination.total", 0 },
  /* Cabo Verde's statistics office answers `totalItems: 2425` beside 50
   * items, with a `currentPage` it echoes back as 0 whatever you send. */
  { "totalItems", 0 },
  /* OData's own spelling, v4 and v3: single keys whose name contains a dot,
   * which is why pw_dotted() tries the literal. LegCo Hong Kong answers
   * `odata.count: 551619` beside 200 rows. */
  { "@odata.count", 0 }, { "odata.count", 0 },
  /* The Linked Data API's envelope, one level down: every
   * lda.data.parliament.uk row read "the upstream declares no total" while
   * the number sat beside the items it was holding back (71,323 Commons oral
   * questions against the 10,000 a run reaches). Measured 2026-10-23. */
  { "result.totalResults", 0 },
  /* Japan Search's `jps-cross` item API: `hit: 5409` beside a 500-record page.
   * Singular, so it does not share bare `count`'s ambiguity. */
  { "hit", 0 },
  /* Absorbed from the declared-size walk's own list, and from geojson's. */
  { "totalRecords", 0 }, { "total_rows", 0 }, { "totalFeatures", 0 },
  { "matchedCount", 0 },
  { NULL, 0 }
};

long pw_declared_total(const cJSON *doc, unsigned flags) {
  if (!cJSON_IsObject(doc) && !cJSON_IsArray(doc)) return -1;
  for (int i = 0; PW_TOTAL_KEYS[i].key; i++) {
    if ((PW_TOTAL_KEYS[i].flags & PW_TOT_COUNT) && !(flags & PW_TOT_COUNT)) continue;
    const cJSON *v = pw_dotted(doc, PW_TOTAL_KEYS[i].key);
    if (v && cJSON_IsNumber(v) && v->valuedouble >= 0) return (long)v->valuedouble;
  }
  /* OData v2 verbose writes its total as a STRING, `d.__count: "46144"`, so no
   * row above can match it. */
  return pw_odata_v2_count(doc);
}

/* The same idea over an XML ENVELOPE. hp_run_json has read a declared total
 * since the truncation notice needed one, and the XML path never did — so NO
 * XML row in the tree could state a real remainder. Japan's KKJ procurement
 * API answers `<SearchHits>50530</SearchHits>` beside 100 results, which is
 * the upstream saying exactly how much it is holding back.
 *
 * Only the text BEFORE the first record is searched. That is the guard that
 * matters: a name like `TotalCount` could occur inside a record as a field of
 * its own, and reading one record's field as the collection's size would make
 * the disclosure itself a fabrication. */
long pw_declared_total_xml(const char *body, const char *rec_open) {
  static const char *const EL[] = {
    "SearchHits", "TotalCount", "TotalResults", "TotalHits", "SearchCount",
    "RecordCount", "ResultCount", "numberMatched", "totalResults",
    "totalCount", "numFound", NULL };
  /* The first RECORD element, found the way the record loop finds it: a tag
   * name is only that name when the next character ends it. KKJ wraps its
   * <SearchResult> records in a <SearchResults> envelope and puts the count
   * inside that envelope, so a plain strstr for "<SearchResult" stops at the
   * wrapper and the whole searchable span is the four lines before it. */
  const char *first = NULL;
  if (rec_open && *rec_open) {
    size_t rl = strlen(rec_open);
    for (const char *p = strstr(body, rec_open); p; p = strstr(p + rl, rec_open)) {
      char c = p[rl];
      if (c == '>' || c == '/' || isspace((unsigned char)c)) { first = p; break; }
    }
  }
  size_t limit = first ? (size_t)(first - body) : strlen(body);
  for (int i = 0; EL[i]; i++) {
    char open[64];
    int n = snprintf(open, sizeof open, "<%s>", EL[i]);
    if (n <= 0 || (size_t)n >= sizeof open) continue;
    const char *p = jo_memmem(body, limit, open, (size_t)n);
    if (!p) continue;
    p += n;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    /* The value must be the WHOLE element text: `<TotalCount>12 of 40<` is not
     * a count, and neither is an empty element. */
    if (end && end != p && *end == '<' && v >= 0) return v;
  }
  return -1;
}

/* ── (b) the next page ────────────────────────────────────────────────────
 * THE pager table. It was two: lib/jsonlist.c's PAGERS (size parameter paired
 * with the cursor that family advances) and this file's PW_SIZE_PARAMS /
 * PW_OFF_PARAMS / PW_PAGE_PARAMS (a size list, an offset list and a page list
 * that were NOT paired), and lib/geojson.c carried a third inline size list
 * and two cursor lists.
 *
 * `uses` says which engine's reading each row belongs to, because the three
 * disciplines genuinely differ and unifying them silently would change what
 * rows fetch:
 *   PWP_JSON  jsonlist_emit_paged / hpengine page_walk: a size is moved only
 *             through ITS OWN cursor, and only when the page came back full.
 *   PWP_DECL  pw_walk: the size is a stride / yardstick, and ANY offset or page
 *             parameter the URL already carries is the one advanced.
 *   PWP_GEO   a size spelling the GeoJSON walk recognises as "a full page".
 *   PWP_HP    a size spelling hpengine reads off a row's own URL as its yardstick.
 * A row without PWP_JSON never reaches the paired loops, which is why a NULL
 * cursor (retmax, rpp) is safe.
 *
 * `alt_cursor` is the same family's other spelling. It is used only when the
 * URL ALREADY carries it, which is how openFDA (`limit` + `skip`) is told
 * apart from Socrata (`limit` + `offset`): both declare `limit`, and moving
 * the wrong one would re-serve page 1 forever. */
const pw_pager PW_PAGERS[] = {
  { "per_page",  "page",   NULL,     1, PWP_JSON | PWP_DECL | PWP_GEO | PWP_HP },  /* CKAN/dane.gov.pl, GitHub, uData */
  { "page_size", "page",   NULL,     1, PWP_JSON | PWP_DECL | PWP_GEO | PWP_HP },  /* DRF                             */
  { "pageSize",  "page",   NULL,     1, PWP_JSON | PWP_DECL | PWP_HP },            /* ArcGIS Hub, many .NET APIs      */
  { "perPage",   "page",   NULL,     1, PWP_JSON | PWP_DECL | PWP_HP },            /* house style                     */
  { "rp",        "page",   NULL,     1, PWP_JSON },                       /* flexigrid — taginfo (OSM)       */
  { "itemsPerPage", "page",NULL,     1, PWP_JSON | PWP_DECL },            /* ERDDAP index.json               */
  { "size",      "page",   NULL,     1, PWP_JSON | PWP_DECL | PWP_HP },            /* Spring Data / Apollo (RESF)     */
  { "num",       "p",      NULL,     1, PWP_JSON | PWP_DECL | PWP_HP },            /* CORDIS search API               */
  { "rows",      "start",  NULL,     0, PWP_JSON | PWP_DECL | PWP_GEO | PWP_HP },  /* Solr / CKAN package_search      */
  { "length",    "start",  NULL,     0, PWP_JSON | PWP_DECL },            /* DataTables — HUDOC (ECHR)       */
  /* GOV.UK Search API (`count` + `start`), J-STAGE and CiNii (the same two
   * names); WFS 2.0 spells the cursor `startIndex`, used when the URL already
   * carries it. Missing until 2026-10-20: 48 `www.gov.uk/api/search.json?
   * ...&count=3` rows declared page_walk and read ONE page of three records
   * a run. */
  { "count",     "start",  "startIndex", 0, PWP_JSON | PWP_DECL | PWP_GEO | PWP_HP },
  { "limit",     "offset", "skip",   0, PWP_JSON | PWP_DECL | PWP_GEO | PWP_HP },  /* Socrata, ODS, most REST; openFDA*/
  /* Socrata SODA's OWN spelling. `$limit` sat on a second list for
   * "recognise a full page, but we cannot ask for the next one", which cost the
   * whole fleet every page after the first: measured 2026-09-19, 571 distinct
   * `$limit=` URLs in this tree, 277 of them on this path. `$offset` is the
   * cursor SODA documents for `$limit`, confirmed live on 14 hosts before it
   * was added. */
  { "$limit",    "$offset", NULL,    0, PWP_JSON | PWP_DECL | PWP_GEO },  /* Socrata SODA                    */
  { "$top",      "$skip",  NULL,     0, PWP_JSON | PWP_DECL | PWP_HP },            /* OData                           */
  { "maxRecords","offset", NULL,     0, PWP_JSON | PWP_DECL | PWP_HP },            /* Airtable-style                  */
  /* ArcGIS REST and WFS 1.x — a PAGERS family as of 2026-10-25, measured one
   * URL per host first (`resultOffset=0` against `=2`, bodies then feature
   * ids: disjoint on 47 hosts, overlapping on 4, ignored on 4, refused on 5 —
   * docs/arcgis-resultoffset-probe-2026-10-24.tsv). An ordering clause belongs
   * in the ROW's URL (`orderByFields` for ArcGIS, `sortBy` for WFS): a row
   * given one of these sizes without a sort is CLAUDE.md's "collision that is
   * really a skip". */
  { "resultRecordCount", "resultOffset", NULL, 0, PWP_JSON | PWP_DECL | PWP_GEO | PWP_HP },
  { "maxFeatures", "startIndex", NULL,   0, PWP_JSON | PWP_GEO },        /* WFS 1.x; 2.0 spelling */
  /* Declared-size walk only. `retmax` is NCBI E-utilities'; `rpp` is NSF's
   * spelling of results-per-page (all three occurrences are `rpp=25` against
   * api.nsf.gov/services/v1/awards.json, paired with `offset=1` — the only
   * offset walk in the tree that was blocked purely on a size spelling).
   * Measured before adding, as `length` and `num` were: none is a quantity or
   * an identifier in any URL that reaches pw_walk. NOT added, and measured the
   * same way: `max` (17 occurrences — a threshold as often as a page size)
   * and `nrows` (1). */
  { "retmax",    NULL,     NULL,     0, PWP_DECL },
  { "rpp",       NULL,     NULL,     0, PWP_DECL },
  /* hpengine's yardstick for "was the previous page short?" when a row puts its
   * size in the URL rather than in `page_size` (WordPress `per_page`, Socrata
   * `$limit`, ...). It used to be a list of its own inside lib/hpengine.c; the
   * spellings only that list knew are here, with no cursor — hpengine advances
   * by its own page_param arithmetic, never through this table. */
  { "Count",     NULL,     NULL,     0, PWP_HP },
  { "take",      NULL,     NULL,     0, PWP_HP },
  { "pagesize",  NULL,     NULL,     0, PWP_HP },
  { NULL, NULL, NULL, 0, 0 }
};

/* Cursor spellings. ONE table, with a bit per use, for the lists that were
 * four: this file's offset list and page list, jsonlist's declared-total page
 * cursors and seed list, and geojson's cursor list.
 *
 * `from` is deliberately ABSENT. Measured across every literal URL in the
 * fleet, it is never a record offset: it carries an epoch (`from=1754697600`),
 * an ISO date (`from=2026-01-01`), or an FX currency code. Treating it as an
 * offset would advance a time window and attribute the resulting records to
 * this source. `start` IS kept, because `start=0/1/100/200` are genuine
 * offsets here — but it is also spelled as a date by several sources, which is
 * what PW_OFFSET_MAX below exists to separate.
 *
 * `$offset` is Socrata SODA's record offset, and its absence from this file's
 * list was the second copy of the same defect fixed in PW_PAGERS: a VJSON_KEYED
 * row reaches pw_walk, pw_walk only advances a parameter it can name, so the
 * 13 keyed Socrata rows read one page even after pw_seed_cursor wrote
 * `$offset=0` into their URL. */
#define PWC_OFF_DECL   1u   /* pw_walk: a record offset, advanced by the page size */
#define PWC_PAGE_DECL  2u   /* pw_walk: a page number, advanced by one            */
#define PWC_TOTAL_PAGE 4u   /* paired walk, declared-total path: a page number    */
#define PWC_SEED       8u   /* "the URL already carries a cursor": do not seed one */
#define PWC_GEO        16u  /* GeoJSON: advanced by the records the page held     */
typedef struct { const char *name; unsigned uses; } pw_cursor;
static const pw_cursor PW_CURSORS[] = {
  { "offset",       PWC_OFF_DECL | PWC_SEED | PWC_GEO },
  { "$offset",      PWC_OFF_DECL | PWC_SEED },
  { "$skip",        PWC_OFF_DECL | PWC_SEED },
  { "skip",         PWC_OFF_DECL | PWC_SEED },
  { "resultOffset", PWC_OFF_DECL | PWC_SEED | PWC_GEO },
  { "startIndex",   PWC_OFF_DECL | PWC_SEED | PWC_GEO },
  { "startindex",   PWC_GEO },
  { "start",        PWC_OFF_DECL | PWC_SEED },
  { "page",         PWC_PAGE_DECL | PWC_TOTAL_PAGE | PWC_SEED },
  { "pageNumber",   PWC_PAGE_DECL | PWC_TOTAL_PAGE | PWC_SEED },
  { "p",            PWC_PAGE_DECL | PWC_TOTAL_PAGE | PWC_SEED },
  { "pagina",       PWC_TOTAL_PAGE },
  { "pageNum",      PWC_TOTAL_PAGE },
  { NULL, 0 }
};

/* A record offset is a small number. Anything above this is a timestamp that
 * happens to be wholly numeric — `start=1754697600` parses as a valid integer
 * and would otherwise be "advanced" by the page size into a different time
 * window. No paged API in this fleet is walked past ten million records, so
 * this rejects only fabrication. */
#define PW_OFFSET_MAX 10000000L

const pw_pager *pw_pager_find(const char *size_param) {
  for (int i = 0; PW_PAGERS[i].size; i++)
    if (!strcmp(PW_PAGERS[i].size, size_param)) return &PW_PAGERS[i];
  return NULL;
}

/* The page size this URL declares, as the PAGERS families state it, or -1.
 * `uses` selects whose spellings count (PWP_JSON, PWP_DECL or PWP_GEO). It
 * used to consult a second list as well (EXTRA: "a size we can disclose but
 * cannot move"); every member graduated to PW_PAGERS once its cursor was
 * measured, so the list is gone. If a size spelling ever needs to be
 * declarable-but-unmovable again, the way to do it is a row here with a NULL
 * cursor — as retmax and rpp are for the declared-size walk — and not a second
 * list that only one function reads. */
long pw_declared_size_below(const char *url, unsigned uses, long below) {
  for (int i = 0; PW_PAGERS[i].size; i++) {
    if (!(PW_PAGERS[i].uses & uses)) continue;
    long v = pw_url_int(url, PW_PAGERS[i].size);
    if (v > 0 && v < below) return v;
  }
  return -1;
}
const char *pw_declared_size_param(const char *url, long *size) {
  for (int i = 0; PW_PAGERS[i].size; i++) {
    if (!(PW_PAGERS[i].uses & PWP_DECL)) continue;
    long v = pw_url_int(url, PW_PAGERS[i].size);
    if (v > 0) { if (size) *size = v; return PW_PAGERS[i].size; }
  }
  return NULL;
}
int pw_url_has_cursor(const char *url) {
  for (int i = 0; PW_CURSORS[i].name; i++)
    if ((PW_CURSORS[i].uses & PWC_SEED) && pw_url_int(url, PW_CURSORS[i].name) >= 0) return 1;
  return 0;
}
long pw_declared_size(const char *url, unsigned uses) {
  return pw_declared_size_below(url, uses, LONG_MAX);
}

/* host part of "https://a.b/c", "user@a.b" or a bare host.
 *
 * The userinfo '@' counts only INSIDE the authority — before the first '/',
 * '?' or '#' that ends it. It used to be the first '@' anywhere after the
 * scheme, so "http://evil.test/x?e=a@good.com" read as host "good.com": the
 * resume-position host check (and any host comparison built on this) could be
 * walked past by a query string. */
char *pw_host_of(const char *s) {
  if (!s) return NULL;
  const char *p = strstr(s, "://");
  p = p ? p + 3 : s;
  size_t alen = 0;                              /* authority length */
  while (p[alen] && p[alen] != '/' && p[alen] != '?' && p[alen] != '#') alen++;
  const char *at = NULL;
  for (size_t i = 0; i < alen; i++) if (p[i] == '@') at = p + i;   /* the LAST one: userinfo may not hold '@', a host never does */
  if (at) { alen -= (size_t)(at + 1 - p); p = at + 1; }
  size_t n = 0;
  if (p[0] == '[') {                            /* IPv6 literal */
    while (n < alen && p[n] != ']') n++;
    if (n < alen) n++;
  } else {
    while (n < alen && p[n] != ':' && p[n] != ' ') n++;
  }
  if (!n) return NULL;
  char *out = malloc(n + 1);
  if (!out) return NULL;
  memcpy(out, p, n);
  out[n] = 0;
  return out;
}

/* Same host, case-insensitively. Two NULLs are NOT the same host: an
 * unparseable URL is never "safe to send credentials to". This is the CROSS-HOST
 * RULE of every walk: a next link that points at another host than the row's
 * own is followed without the row's headers, body or key. */
int pw_same_host(const char *a, const char *b) {
  char *ha = pw_host_of(a), *hb = pw_host_of(b);
  int same = ha && hb && strcasecmp(ha, hb) == 0;
  free(ha); free(hb);
  return same;
}

/* Remove a secret (raw or percent-encoded) from a URL, in place. Used when an
 * upstream-supplied URL points at another host: our key is the ROW's, not that
 * host's, and must not travel with the request. */
void pw_strip_secret(char *url, const char *secret) {
  if (!url || !secret || !*secret) return;
  size_t sl = strlen(secret);
  char *at;
  while ((at = strstr(url, secret))) memmove(at, at + sl, strlen(at + sl) + 1);
}

/* Resolve `ref` against the page it was published on (RFC 3986, enough of it).
 * An absolute http(s) URL passes through; "//host/p", "/p" and "?q" are
 * resolved; with PW_RESOLVE_PATHS a bare relative path is too ("../x", "p2").
 * A next link of "/api?page=2" used to be handed to the HTTP client as written,
 * which fails, and the walk then ended as a "failed page" with the data behind
 * it unread (bio.tools answered `"next": "?page=2&format=json"`, the whole
 * reason 30,000 rows stopped at page 1 — the link WAS published, we just
 * refused to read it).
 *
 * The bare relative path is opt-in because the walks that read a link out of a
 * JSON ENVELOPE must refuse a bare token: `"next": "eyJvZmZzZXQi…"` is a
 * cursor, not a URL, and gluing it onto the base would fabricate a request.
 * hpengine resolves a link its row already declared to be one, so it asks for
 * the paths. NULL when `ref` is not a URL reference (the caller keeps what it
 * had); NULL too without a `base` for anything but an absolute URL, which is
 * how an absolute-only caller refuses to guess. */
char *pw_resolve_url(const char *base, const char *ref, unsigned flags) {
  if (!ref || !*ref) return NULL;
  if (!strncasecmp(ref, "http://", 7) || !strncasecmp(ref, "https://", 8))
    return strdup(ref);
  if (!base) return NULL;
  const char *sch = strstr(base, "://");
  if (!sch) return NULL;
  size_t schlen = (size_t)(sch - base) + 3;               /* "https://" */
  const char *auth_end = base + schlen;
  while (*auth_end && *auth_end != '/' && *auth_end != '?' && *auth_end != '#') auth_end++;
  size_t cap = strlen(base) + strlen(ref) + 8;
  char *out = malloc(cap);
  if (!out) return NULL;
  if (ref[0] == '/' && ref[1] == '/') {                   /* protocol-relative */
    snprintf(out, cap, "%.*s%s", (int)(sch - base) + 1, base, ref);
    return out;
  }
  if (ref[0] == '/') {                                    /* same origin */
    snprintf(out, cap, "%.*s%s", (int)(auth_end - base), base, ref);
    return out;
  }
  if (ref[0] == '?') {                                    /* same path, new query */
    const char *q = auth_end;
    while (*q && *q != '?' && *q != '#') q++;
    snprintf(out, cap, "%.*s%s", (int)(q - base), base, ref);
    return out;
  }
  if (ref[0] == '#' || !(flags & PW_RESOLVE_PATHS)) { free(out); return NULL; }
  {                                                       /* "scheme:" of something we do not speak */
    size_t k = 0;
    while (ref[k] && (isalnum((unsigned char)ref[k]) || ref[k] == '+' || ref[k] == '-' || ref[k] == '.')) k++;
    if (ref[k] == ':' && k > 0) { free(out); return NULL; }
  }
  {                                                       /* relative path */
    const char *pe = auth_end;
    while (*pe && *pe != '?' && *pe != '#') pe++;
    const char *ls = pe;                                  /* last '/' of the path */
    while (ls > auth_end && ls[-1] != '/') ls--;
    size_t dirlen = (size_t)(ls - base);                  /* through the '/' */
    if (ls == auth_end) {                                 /* path-less base */
      snprintf(out, cap, "%.*s/%s", (int)(auth_end - base), base, ref);
    } else snprintf(out, cap, "%.*s%s", (int)dirlen, base, ref);
    /* collapse "./" and "../" in the path part */
    char *path = out + (auth_end - base);
    char *w = path, *r = path;
    while (*r && *r != '?' && *r != '#') {
      if (!strncmp(r, "/./", 3)) { r += 2; continue; }
      if (!strncmp(r, "/../", 4)) {
        r += 3;
        if (w > path) { w--; while (w > path && *w != '/') w--; }
        continue;
      }
      *w++ = *r++;
    }
    memmove(w, r, strlen(r) + 1);
  }
  return out;
}

/* The server's own "next page" link, if it published one — THE list. It was
 * three: jsonlist's `next_link` KEYS, this file's `pw_next_link` (direct names,
 * four nested objects, the rel=next array) and geojson's `gj_next_link` (the
 * rel=next array only). Union of all three. Ordered most- to least-specific:
 * `links.next` is the JSON:API/CKAN spelling, `@odata.nextLink` is OData, the
 * rest are common house styles.
 *
 * Where they disagreed (2026-11-16): precedence follows jsonlist's order, which
 * serves the VJSON fleet; this file used to take the first STRING it met even
 * when it was empty or relative and then refuse it, which hid every later
 * candidate — so an unusable `next` no longer shadows a usable `links.next`.
 *
 * A next link that is present but null/false/empty means "this is the last
 * page" in every one of these dialects, so it is treated as absent.
 *
 * `base` selects the discipline: given the URL of the page just read, a
 * relative link is resolved against it (jsonlist and geojson always did);
 * NULL means absolute http(s) only, 8 to 4,000 bytes — pw_walk's rule, which
 * refuses to resolve a relative value against assumptions about the API. */
char *pw_next_link(const cJSON *doc, const char *base) {
  if (!cJSON_IsObject(doc)) return NULL;
  static const char *const KEYS[] = {
    "links.next", "next", "next_url", "nextUrl", "nextPageUrl",
    "meta.next", "paging.next", "@odata.nextLink", "next_page",
    "meta.pagination.next", "pagination.next", "pagination.next_page",
    "meta.pagination.next_page", "links.next_url",
    /* OCHA FTS (api.hpc.tools) puts it at meta.nextLink: ocha-fts-flow-2026
     * stopped after its first 200 of 20,161 flows (measured 2026-09-15). */
    "meta.nextLink",
    /* This file's own spellings. `next_url` and `next_page` are the
     * DRF/CKAN-adjacent ones; `nextLink`, `next_page_url` and `nextRecordsUrl`
     * (Salesforce) are direct, and every nested family it read could also say
     * `nextLink`. */
    "nextLink", "next_page_url", "nextRecordsUrl",
    "links.nextLink", "paging.nextLink", "pagination.nextLink", NULL };
  for (int i = 0; KEYS[i]; i++) {
    const cJSON *v = pw_dotted(doc, KEYS[i]);
    if (!v) continue;
    const char *u = NULL;
    if (cJSON_IsString(v)) u = v->valuestring;
    /* CKAN and some JSON:API servers nest it as {next: {href: "..."}}. */
    else if (cJSON_IsObject(v)) {
      const cJSON *h = cJSON_GetObjectItem((cJSON *)v, "href");
      if (cJSON_IsString(h)) u = h->valuestring;
    }
    if (!u || !u[0]) continue;
    if (!base) {
      size_t n = strlen(u);
      if ((strncasecmp(u, "http://", 7) && strncasecmp(u, "https://", 8)) ||
          n < 8 || n > 4000) continue;
    }
    char *out = pw_resolve_url(base, u, 0);
    if (out) return out;
  }
  /* The ARRAY form, {"links":[{"rel":"next","href":…},…]} — Oracle ORDS, HAL
   * and OGC API. br-tcu-inabilitados (ORDS) stopped at its first 25 records of
   * 766 without it (measured 2026-09-14). Selected by rel, never by position:
   * a server is free to put "self" first. */
  const cJSON *arr = cJSON_GetObjectItem((cJSON *)doc, "links");
  if (cJSON_IsArray(arr)) {
    const cJSON *l;
    cJSON_ArrayForEach(l, arr) {
      const cJSON *rel  = cJSON_GetObjectItem((cJSON *)l, "rel");
      const cJSON *href = cJSON_GetObjectItem((cJSON *)l, "href");
      if (cJSON_IsString(rel) && rel->valuestring &&
          strcasecmp(rel->valuestring, "next") == 0 &&
          cJSON_IsString(href) && href->valuestring) {
        if (!base) {
          size_t n = strlen(href->valuestring);
          if ((strncasecmp(href->valuestring, "http://", 7) &&
               strncasecmp(href->valuestring, "https://", 8)) || n < 8 || n > 4000)
            continue;
        }
        char *out = pw_resolve_url(base, href->valuestring, 0);
        if (out) return out;
      }
    }
  }
  return NULL;
}

/* The continuation decision a paired walk makes after every page, exported so
 * hpengine's `page_walk` rows make exactly the same one. Those rows are VJSON
 * collectors moved onto hpengine to walk their detail hop, and the move must
 * not change how far they page: two copies of this decision is how a row
 * silently loses every page after the first while both engines report success.
 *
 *   doc        the page just read (not consumed)
 *   page_url   the URL it was read from — the OVERRIDDEN one
 *   got        records the page's array held
 *   available  the upstream's declared total, or -1
 *   total      records FETCHED so far across the walk, this page included —
 *              not emitted: it is compared with `available`, the upstream's
 *              count of what it holds, and a record filtered out or dropped
 *              as noise was still handed over
 *
 * Returns the next URL (caller frees) or NULL, and reports through
 * `full_unadvanced` whether the walk is stopping at a page that looked full —
 * the condition the truncation notice discloses. */
char *pw_next_page(const cJSON *doc, const char *page_url, int got,
                   long available, int total, int *full_unadvanced) {
  char *next = pw_next_link(doc, page_url);
  if (!next && got > 0) {
    /* No server link. Advance a cursor only when the URL declares a page
     * size AND this page came back exactly full — a short page is the
     * upstream saying it is finished, and following it would be us
     * inventing a page that was never offered. */
    for (int i = 0; PW_PAGERS[i].size && !next; i++) {
      if (!(PW_PAGERS[i].uses & PWP_JSON)) continue;
      long size = pw_url_int(page_url, PW_PAGERS[i].size);
      if (size <= 0 || got < size) continue;
      /* Move the cursor this URL already names, when the family has two
       * spellings; otherwise the canonical one. */
      const char *cursor = PW_PAGERS[i].cursor;
      if (PW_PAGERS[i].alt_cursor && pw_url_int(page_url, PW_PAGERS[i].alt_cursor) >= 0)
        cursor = PW_PAGERS[i].alt_cursor;
      long cur = pw_url_int(page_url, cursor);
      /* A page NUMBER the URL does not state (or states as 0) is followed by
       * page 1, not 2. This used to be `cur > 0 ? cur + 1 : 2`, which is right
       * for a 1-based API and silently skips page 1 of a 0-based one (Spring
       * Data's `size`+`page`, Diavgeia): the walk read 0, 2, 3 … and
       * gr-diavgeia-decision-search lost records 101-200 on every run. Which
       * base the API uses is not knowable from the URL, so it is asked: page 1
       * is requested, and a 1-based server answers it with the page already
       * read. The walk recognises that repeat (pw_page_one_retry) and
       * continues at page 2 — one extra request, never a lost page. */
      long nextval = PW_PAGERS[i].page_numbered
                       ? (cur > 0 ? cur + 1 : 1)
                       : (cur >= 0 ? cur + size : size);
      next = pw_url_set(page_url, cursor, nextval);
    }
  }
  /* Last resort, and the only one that needs no page-size sibling: the
   * upstream published a total BIGGER than what it has handed us, and the
   * URL already carries a page number. ROR says `number_of_results: 108000`
   * and hands over 20; retsinformation.dk says `totalResultCount` and hands
   * over a screenful. There is no guessing here — the remainder is the
   * upstream's own arithmetic — and the cursor is one this URL already
   * declares, so we are turning a dial the caller wrote, not inventing one.
   * A server that ignores it re-serves page 1 and the no-progress guard
   * ends the walk on the next turn. */
  if (!next && got > 0 && available > (long)(total)) {
    for (int i = 0; PW_CURSORS[i].name && !next; i++) {
      if (!(PW_CURSORS[i].uses & PWC_TOTAL_PAGE)) continue;
      long cur = pw_url_int(page_url, PW_CURSORS[i].name);
      if (cur < 1) continue;               /* must already be declared */
      next = pw_url_set(page_url, PW_CURSORS[i].name, cur + 1);
    }
  }
  /* A FULL page with no link and no cursor this walk can move is not the end
   * of the data; it is the end of what this URL lets us ask for. That was
   * never disclosed: Socrata's `$limit` had no sibling here, so 472 such URLs
   * read one page, filed no notice and reported success — measured
   * 2026-09-15, 28 of 38 sampled hold more than one page (up to 17.6 M rows).
   * Say so. Guessing a cursor is not done here: it would change what these
   * rows fetch, which is a data-volume decision, not a disclosure. */
  if (full_unadvanced) {
    long ds = pw_declared_size(page_url, PWP_JSON);
    *full_unadvanced = (!next && got > 0 &&
                        ((ds > 0 && got >= ds) || available > (long)total));
  }
  return next;
}

/* The page-1 probe's other half. pw_next_page() follows a first page that
 * carried no page number (or page=0) with page=1, because on a 0-based API
 * that is the next page. On a 1-based API it is the page just read, so it
 * comes back as a repeat. Given the URL read before (`prev_url`) and the one
 * that repeated it (`page_url`), return the URL to continue at — page 2 — when
 * the repeat is exactly that probe, else NULL (a real repeat: stop). Caller
 * frees. Only the page-numbered families pw_next_page() advances are
 * recognised, and only when page_url still declares the family's page size. */
char *pw_page_one_retry(const char *prev_url, const char *page_url) {
  if (!prev_url || !page_url) return NULL;
  for (int i = 0; PW_PAGERS[i].size; i++) {
    if (!(PW_PAGERS[i].uses & PWP_JSON) || !PW_PAGERS[i].page_numbered) continue;
    if (pw_url_int(page_url, PW_PAGERS[i].size) <= 0) continue;
    const char *cursor = PW_PAGERS[i].cursor;
    if (pw_url_int(page_url, cursor) != 1) continue;
    if (pw_url_int(prev_url, cursor) > 0) continue;   /* absent or 0 */
    return pw_url_set(page_url, cursor, 2);
  }
  return NULL;
}

/* The URL with its page-size family's cursor written in at its starting value,
 * or NULL when nothing needs seeding: no declared size, or the URL already
 * carries a cursor of any spelling the declared-size walk can advance. Uses the
 * same table pw_next_page() walks by, so the paths agree on what a size
 * parameter implies. Caller frees. */
char *pw_seed_cursor(const char *url) {
  if (!url) return NULL;
  for (int i = 0; PW_CURSORS[i].name; i++)
    if ((PW_CURSORS[i].uses & PWC_SEED) && pw_url_int(url, PW_CURSORS[i].name) >= 0)
      return NULL;
  for (int i = 0; PW_PAGERS[i].size; i++) {
    if (!(PW_PAGERS[i].uses & PWP_JSON)) continue;
    if (pw_url_int(url, PW_PAGERS[i].size) <= 0) continue;
    /* OFFSET families only. A record offset starts at 0 on every API that
     * speaks one, so writing `offset=0` restates what the server already
     * served. A PAGE NUMBER does not have that property: Spring Data (`size` +
     * `page`) is 0-based, most others are 1-based. Seeding `page=1` made
     * eur-brreg-enheter start on Spring's SECOND page and never read the 100
     * newest entities — the records that source exists for (measured
     * 2026-09-15). A page-numbered row that wants a walk states its own first
     * page in the URL. */
    if (PW_PAGERS[i].page_numbered) return NULL;
    return pw_url_set(url, PW_PAGERS[i].cursor, 0);
  }
  return NULL;
}

/* An ArcGIS `exceededTransferLimit`, wherever the server chose to put it:
 * FeatureServer sets it at the top level, some MapServer builds nest it under
 * `properties`, and a few emit the string "true" rather than a JSON boolean. */
static int pw_exceeded(const cJSON *doc) {
  const char *K = "exceededTransferLimit";
  const cJSON *v = cJSON_GetObjectItem((cJSON *)doc, K);
  if (!v) {
    const cJSON *p = cJSON_GetObjectItem((cJSON *)doc, "properties");
    if (p) v = cJSON_GetObjectItem((cJSON *)p, K);
  }
  if (!v) return 0;
  if (cJSON_IsBool(v)) return cJSON_IsTrue(v);
  if (cJSON_IsNumber(v)) return v->valueint != 0;
  if (cJSON_IsString(v) && v->valuestring) return !strcasecmp(v->valuestring, "true");
  return 0;
}

/* The GeoJSON walk's continuation decision. Different from pw_next_page in
 * what it ADVANCES BY (the records the page held, not the declared size) and in
 * having ArcGIS's own "I held features back" flag, so it is a second decision
 * procedure — but it reads the same link list, the same size table and the same
 * cursor table, and its Socrata rule is the `$limit` row of PW_PAGERS rather
 * than a literal. `total` is what the walk has EMITTED. */
char *pw_next_page_geo(const cJSON *doc, const char *page_url, int got,
                       long available, int total, int *full_unadvanced) {
  char *next = NULL;
  if (got > 0) {
    /* 1. the server said outright that it held features back */
    if (pw_exceeded(doc)) {
      long size = pw_url_int(page_url, "resultRecordCount");
      if (size <= 0) size = got;              /* it truncated at whatever it gave */
      long cur = pw_url_int(page_url, "resultOffset");
      next = pw_url_set(page_url, "resultOffset", (cur >= 0 ? cur : 0) + size);
    }
    /* 2. a next link, followed verbatim */
    if (!next) next = pw_next_link(doc, page_url);
    /* 3. the upstream's own total exceeds what we hold, and the URL already
     *    names a cursor. No page size is required here: the remainder is the
     *    server's arithmetic, not ours. */
    if (!next && available > (long)total) {
      for (int i = 0; PW_CURSORS[i].name && !next; i++) {
        if (!(PW_CURSORS[i].uses & PWC_GEO)) continue;
        long cur = pw_url_int(page_url, PW_CURSORS[i].name);
        if (cur < 0) continue;               /* must already be declared */
        next = pw_url_set(page_url, PW_CURSORS[i].name, cur + got);
      }
    }
    /* 4. Socrata SODA: `$limit` declares the page size and `$offset` is the
     *    cursor that goes with it (Calgary air quality: 400 of 3,725,365 rows
     *    before this). Gated on a FULL page, like every other advance here — a
     *    short page is the upstream saying it is finished. */
    if (!next) {
      const pw_pager *pg = pw_pager_find("$limit");
      long size = pg ? pw_url_int(page_url, pg->size) : -1;
      if (size > 0 && got >= size) {
        long cur = pw_url_int(page_url, pg->cursor);
        next = pw_url_set(page_url, pg->cursor, (cur >= 0 ? cur : 0) + size);
      }
    }
  }
  /* A FULL page with no way to ask for the next one is not the end of the
   * data. The same disclosure the paired walk makes; no cursor is guessed. */
  if (full_unadvanced) {
    long ds = pw_declared_size(page_url, PWP_GEO);
    *full_unadvanced = (!next && got > 0 &&
                        ((ds > 0 && got >= ds) || available > (long)total));
  }
  return next;
}

/* The next URL for the declared-size walk: derived from the collector's OWN
 * parameters, or NULL when the URL carries nothing that can be legitimately
 * advanced. Any offset parameter the URL carries is moved by the page size; a
 * page number by one. */
static char *pw_advance_declared(const char *url, long page_size) {
  long v = 0; size_t ds = 0, de = 0;
  for (int i = 0; PW_CURSORS[i].name; i++) {
    if (!(PW_CURSORS[i].uses & PWC_OFF_DECL)) continue;
    if (!pw_url_find(url, PW_CURSORS[i].name, 1, &v, &ds, &de)) continue;
    if (page_size <= 0) return NULL;
    if (v < 0 || v > PW_OFFSET_MAX) return NULL;   /* a timestamp, not an offset */
    return pw_set_num(url, ds, de, v + page_size);
  }
  for (int i = 0; PW_CURSORS[i].name; i++) {
    if (!(PW_CURSORS[i].uses & PWC_PAGE_DECL)) continue;
    if (!pw_url_find(url, PW_CURSORS[i].name, 1, &v, &ds, &de)) continue;
    return pw_set_num(url, ds, de, v + 1);
  }
  return NULL;
}

/* ── (e) a later page that failed: end of data, or a walk cut short? ──────
 * `status` is what the failing request answered, or -1 for a transport
 * failure, or 200 for a response that could not be read. Does that mean the
 * upstream ran out of data, or that the walk was cut short? The ruling, in
 * order:
 *
 *   1. The previous page was SHORT: the upstream already said it had
 *      finished, so whatever the next request met is past the end. EPA
 *      Envirofacts answers a range past its last row with HTTP 500; plenty of
 *      servers answer 404 or 400.
 *   2. Statuses that never mean "no more data" — the request was not
 *      answered, it was refused or deferred: transport failure, any 3xx
 *      (curl follows redirects, so one that reaches us is a loop or has no
 *      Location), 304 (we sent no condition, so a cache is answering for a
 *      page it never fetched), 401/403/407, 408, 429, 5xx, and a 200 whose
 *      body is unreadable. A collection does not end in Forbidden.
 *   3. The rest of 4xx (400, 404, 410, 416, 422 …) is how many servers say
 *      "past the last page". It is still a walk cut short when the upstream
 *      gave evidence that more exists — its own total is larger than what
 *      was read, it published the link to the page that failed, or the
 *      previous page came back FULL. A full page followed by a 400 is the
 *      signature of a deep-paging limit (Elasticsearch's 10,000-record
 *      window, GitHub's 1,000-result search), which is a silent discard if
 *      it is read as the end. Only a collection ending exactly on a page
 *      boundary looks the same, and the notice says so.
 *
 * `prev_full`/`prev_short` are both 0 when the page size is unknown. Callers
 * that only ever ask for a later page ON the upstream's evidence (the jsonlist,
 * GeoJSON and callback walks: a link, a full page, or a declared remainder)
 * pass prev_full=1, link_given=1, declared_total=-1, which rules every status
 * a cut — their behaviour before this function was shared. Returns 1 for
 * "cut short". */
int pw_later_page_cut(long status, int prev_short, int prev_full,
                      int link_given, long declared_total, long fetched) {
  if (prev_short) return 0;
  if (status < 0 || status == 200 || (status >= 300 && status < 400) ||
      status == 401 || status == 403 || status == 407 || status == 408 ||
      status == 429 || status >= 500)
    return 1;
  if (declared_total >= 0) return declared_total > fetched;
  return prev_full || link_given;
}

/* The reason a cut-short walk files, by what the failing page answered. */
void pw_cut_reason(long status, int unreadable, char *out, size_t cap) {
  if (status < 0)
    snprintf(out, cap, "a later page failed at the transport level, so the walk "
             "stopped before the upstream ran out");
  else if (unreadable)
    snprintf(out, cap, "a later page answered HTTP 200 with a body that could "
             "not be read in this row's declared mode (an error or challenge "
             "page), so its records were never delivered");
  else if (status == 304)
    snprintf(out, cap, "a later page answered HTTP 304 Not Modified to a request "
             "that carried no conditional header (a cache or proxy answering for "
             "a page it never fetched); a 304 has no body, so the walk stopped "
             "before the upstream ran out");
  else if (status >= 300 && status < 400)
    snprintf(out, cap, "a later page answered a redirect (HTTP %ld) that could "
             "not be followed, so the walk stopped before the upstream ran out",
             status);
  else if (status == 429)
    snprintf(out, cap, "a later page was refused with HTTP 429 (rate limited), "
             "so the walk stopped before the upstream ran out");
  else if (status == 401 || status == 403 || status == 407)
    snprintf(out, cap, "a later page was refused with HTTP %ld, so the walk "
             "stopped before the upstream ran out", status);
  else if (status == 408 || status >= 500)
    snprintf(out, cap, "a later page answered a server error (HTTP %ld), so the "
             "walk stopped before the upstream ran out", status);
  else
    snprintf(out, cap, "a later page answered HTTP %ld although the upstream had "
             "given evidence of more (a full previous page, its own total, or a "
             "next link): either a deep-paging limit refused the rest, or the "
             "collection ends exactly on a page boundary", status);
}

/* The plain form, for a walk whose fetch hook reports only a status
 * (feed_last_json_status's vocabulary) and never an "unreadable 200". */
void pw_failed_page_reason(int page, long status, char *out, size_t cap) {
  char fdesc[64];
  feed_status_describe(status, fdesc, sizeof fdesc);
  snprintf(out, cap, "page %d answered %s, so the walk stopped before the "
           "upstream ran out", page, fdesc);
}

/* The remedy that goes with a cut walk, by what the failing page answered.
 * hpengine's own wording, now the one copy. */
const char *pw_cut_remedy(long status) {
  if (status == 304)
    return "re-run; the engine sends no conditional header on a later page and "
           "already retried with Cache-Control: no-cache, so a 304 that survives "
           "that is a cache or proxy in front of the upstream answering for it";
  if (status >= 400 && status < 500 && status != 401 && status != 403 &&
      status != 407 && status != 408 && status != 429)
    return "check whether the upstream caps how deep it pages; if it does, "
           "narrow each request (a date window or a filter) so every slice fits "
           "under the cap — see docs/SOURCE_EXHAUSTIVENESS.md";
  return "re-run; a rate-limited host needs a per-host minimum gap "
         "(core/hostgate.c) — see docs/SOURCE_EXHAUSTIVENESS.md";
}

/* ── (f) the disclosure ─────────────────────────────────────────────────────
 * ONE builder of `collector-truncation-notice` for every paging engine. It
 * emits a fact only when the caller supplies it (`-1`/NULL mean "not
 * known / not applicable"), under the same property name whichever engine
 * wrote it: source_id, query, endpoint|url, records_used, records_available,
 * records_available_basis, records_fetched, pages_read, page_ceiling,
 * more_pages_pending, records_dropped, failed_page, failed_page_status,
 * failed_page_url, failed_page_unreadable, reason, remedy — plus whatever
 * engine-specific facts `extra` carries (hpengine's window and cycle fields).
 *
 * What it deliberately does NOT unify, because both are data-shape decisions
 * for the consumers of published rows and not a merge resolution (jocore.h says
 * the same of its two builders): an unknown total is the string "unknown —
 * upstream declared no total" for the list walks and JSON null for the others
 * (`avail_style`), and the row's uid is "truncation" for the list walks but
 * "<id>|truncation:<query>" for pw_walk and hpengine (`key_style`) — changing
 * either would orphan or duplicate every stored notice. */
void pw_notice_emit(intel_sink *sink, pw_notice *n) {
  if (!sink || !n || !n->source_id) { if (n) cJSON_Delete(n->extra); return; }
  cJSON *p = cJSON_CreateObject();
  if (!p) { cJSON_Delete(n->extra); return; }
  cJSON_AddStringToObject(p, "source_id", n->source_id);
  if (n->query) cJSON_AddStringToObject(p, "query", n->query);
  if (n->url_prop)
    cJSON_AddStringToObject(p, n->url_prop, n->url ? n->url : "");
  cJSON_AddNumberToObject(p, "records_used", (double)n->used);
  if (n->available >= 0) {
    cJSON_AddNumberToObject(p, "records_available", (double)n->available);
    /* Where that number came from, because a reader cannot otherwise tell the
     * upstream's own count from our floor. */
    if (n->basis) cJSON_AddStringToObject(p, "records_available_basis", n->basis);
  } else if (n->avail_style == PW_AVAIL_STRING) {
    cJSON_AddStringToObject(p, "records_available", "unknown — upstream declared no total");
  } else {
    cJSON_AddNullToObject(p, "records_available");
  }
  if (n->fetched >= 0)  cJSON_AddNumberToObject(p, "records_fetched", (double)n->fetched);
  if (n->pages_read >= 0) cJSON_AddNumberToObject(p, "pages_read", n->pages_read);
  if (n->page_ceiling > 0) cJSON_AddNumberToObject(p, "page_ceiling", n->page_ceiling);
  if (n->more_pages_pending >= 0)
    cJSON_AddBoolToObject(p, "more_pages_pending", n->more_pages_pending);
  /* Records the pages held that the emitter could not turn into rows. */
  if (n->dropped >= 0)  cJSON_AddNumberToObject(p, "records_dropped", (double)n->dropped);
  /* The page that did not deliver, when one did not: which one, what it
   * answered (feed_last_json_status's vocabulary) and where it was asked. */
  if (n->failed_page > 0) {
    cJSON_AddNumberToObject(p, "failed_page", n->failed_page);
    cJSON_AddNumberToObject(p, "failed_page_status", (double)n->failed_status);
    if (n->failed_unreadable) cJSON_AddBoolToObject(p, "failed_page_unreadable", 1);
    cJSON_AddStringToObject(p, "failed_page_url", n->failed_url ? n->failed_url : "");
  }
  if (cJSON_IsObject(n->extra))
    for (const cJSON *m = n->extra->child; m; m = m->next) {
      if (!m->string || cJSON_GetObjectItem(p, m->string)) continue;
      cJSON *cp = cJSON_Duplicate(m, 1);
      if (cp) cJSON_AddItemToObject(p, m->string, cp);
    }
  cJSON_Delete(n->extra);
  n->extra = NULL;
  cJSON_AddStringToObject(p, "reason", n->reason ? n->reason : "a cap stopped the run");
  cJSON_AddStringToObject(p, "remedy",
                          n->remedy ? n->remedy : "see docs/SOURCE_EXHAUSTIVENESS.md");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char key[320], title[256];
  if (n->key_style == PW_KEY_SCOPED)
    snprintf(key, sizeof key, "%.150s|truncation:%.120s", n->source_id,
             n->query ? n->query : "");
  else
    /* remote_key is just "truncation": the sink derives the uid as
     * "<source_id>|<remote_key>", so prefixing the id would stutter it into
     * "<id>|<id>|truncation". One notice per source, upsert-keyed, so a re-run
     * updates the disclosure rather than piling up duplicates. */
    snprintf(key, sizeof key, "truncation");
  if (n->title && n->title[0])
    snprintf(title, sizeof title, "%s", n->title);
  else if (n->available >= 0)
    snprintf(title, sizeof title, "%s used %ld of %ld available records",
             n->source_id, n->used, n->available);
  else
    snprintf(title, sizeof title,
             "%s used %ld records and stopped before the upstream ran out",
             n->source_id, n->used);

  intel_item note = {0};
  note.remote_key      = key;
  note.title           = title;
  note.lang            = "en";
  note.record_type     = "collector-truncation-notice";
  note.properties_json = pj ? pj : "{}";
  note.tags_json       = n->tags_json ? n->tags_json : "[\"truncation-notice\"]";
  sink->emit(sink, &note);
  free(pj);
}

/* ── the two loops that read a JSON page at a time ────────────────────────── */

/* Shared state of a walk, and the one place it is turned into a notice. */
typedef struct {
  long total, fetched, available;
  int  pages, truncated, full_unadvanced, repeated;
  int  failed, failed_page;
  long failed_status;
  char *failed_url;
} pw_list_state;

/* A walk over a page that is a LIST of records — the jsonlist driver (VJSON
 * rows, and hpengine rows that declare page_walk) and the GeoJSON driver
 * (VGEO rows). Both used to carry their own copy of this loop; they differ in
 * what a "record array" is, what emitting one means, how the next page is
 * decided, and what counts against the declared total — and those four things
 * are the hooks. Everything else is here, once.
 *
 * What the two copies disagreed about, resolved to the one the tree documents
 * as correct (2026-11-16):
 *   - the GeoJSON copy EMITTED a page it had just recognised as a re-serve,
 *     and only then stopped — so `emitted` double-counted it. The jsonlist copy
 *     checks before it emits (CLAUDE.md, 2026-09-05: "a page the engine itself
 *     has decided is a re-serve is not a page of records"), and that is now
 *     the rule for both.
 *   - GeoJSON fingerprinted n + the first eight records, jsonlist the first
 *     eight only; one function now. The only pages that differ are two pages
 *     sharing their first eight records and not their length.
 *   - GeoJSON read $JO_GEOJSON_PAGE_MAX and ignored $JO_PAGE_MAX; it honours
 *     both now, with JO_PAGE_MAX winning as everywhere else.
 * Returns the emitted total, or -1 when the FIRST fetch failed (a dead
 * endpoint is an error; an empty one is not). */
int pw_walk_list(int flavor, intel_sink *sink, const char *source_id,
                 const char *start_url, const char *endpoint,
                 const char *legacy_env, const pw_list_ops *ops) {
  const int json = (flavor == PW_FLAVOR_JSON);
  const char *unit = json ? "records" : "features";
  int page_max = pw_page_ceiling(legacy_env, 0);
  char *page_url = strdup(start_url);
  if (!page_url) return -1;

  pw_list_state st = {0};
  st.available = -1;
  unsigned long long prev_fp = 0;
  char *prev_url = NULL;      /* the page read before this one, for the probe */
  int probed = 0;             /* the page-1 probe is tried at most once        */
  /* Page 1's record identities, for the probe's set test (pw_page_fps). */
  unsigned long long *probe_fps = NULL;
  int probe_nfps = 0;

  for (; st.pages < page_max && page_url; st.pages++) {
    feed_last_json_status_reset();
    cJSON *doc = ops->fetch(ops->ud, page_url);
    if (!doc) {
      /* A failed FIRST fetch is a dead endpoint and belongs to the caller as
       * an error. A failure mid-walk is different: we already have real
       * records, so we keep them and stop — but the walk ended early, which is
       * a shortfall and gets disclosed below. */
      if (st.pages == 0) { free(page_url); free(probe_fps); return -1; }
      long fst = feed_last_json_status();
      /* Only asked for on the upstream's evidence, so any failure is a cut. */
      if (pw_later_page_cut(fst, 0, 1, 1, -1, 0)) {
        st.truncated = 1;
        st.failed = 1;
        st.failed_page = st.pages + 1;
        st.failed_status = fst;
        st.failed_url = strdup(page_url);
      }
      break;
    }
    if (st.available < 0) st.available = pw_declared_total(doc, PW_TOT_COUNT);

    cJSON *arr = ops->records(ops->ud, doc);
    int got = arr ? cJSON_GetArraySize(arr) : 0;

    /* Did the upstream actually move? An ignored cursor parameter re-serves
     * the page we already have; that is the end of the data as far as this
     * URL is concerned, not a page we are owed.
     *
     * This check MUST run before the emit below. It used to run after, so a
     * re-served page was emitted — every record of it colliding onto a uid
     * written one page earlier — and only then recognised as a repeat. Nothing
     * was lost (the sink already held those rows) but `emitted` double-counted
     * the page, which is exactly what the registry sweep reads as a uid
     * COLLISION. Measured 2026-09-05: 129 of 882 flagged sources sat at an
     * exact 1/2, 1/3 or 2/3 stored/emitted ratio. */
    unsigned long long fp = pw_page_fp(arr);
    /* Is THIS response the 0-vs-1-based probe? Asked of the URL PAIR, which a
     * reorder cannot change, and not of the records (pw_page_fps). On a
     * 1-based API this response is page 1 again, so it is emitted MINUS what
     * page 1 already carried: anything left arrived in between and is here and
     * nowhere else. The probe is not a page read, so it does not count toward
     * the ceiling or `pages`. */
    char *p2 = (json && st.pages > 0 && got > 0 && !probed)
                 ? pw_page_one_retry(prev_url, page_url) : NULL;
    if (p2) {
      probed = 1;
      int added = ops->emit(ops->ud, doc, probe_fps, probe_nfps);
      st.total += added;
      st.fetched += added;
      cJSON_Delete(doc);
      free(prev_url);
      prev_url = page_url;
      page_url = p2;
      st.pages--;                              /* the loop's pages++ restores it */
      continue;
    }
    if (st.pages > 0 && got > 0 && fp == prev_fp) st.repeated = 1;
    prev_fp = fp;
    /* A probe can only follow the first page; keep that page's identities. */
    if (json && st.pages == 0 && got > 0) {
      free(probe_fps);
      probe_fps = pw_page_fps(arr, &probe_nfps);
    }

    int n = st.repeated ? 0 : ops->emit(ops->ud, doc, NULL, 0);
    st.total += n;
    if (!st.repeated) st.fetched += got;

    /* Compared with the upstream's declared total on what was FETCHED (the
     * paired walk) or EMITTED (GeoJSON): a record dropped as shape noise was
     * still handed over, and counting only the emitted ones read as "the
     * upstream holds more". */
    int fu = 0;
    char *next = NULL;
    if (!st.repeated)
      next = json ? pw_next_page(doc, page_url, got, st.available,
                                 (int)st.fetched, &fu)
                  : pw_next_page_geo(doc, page_url, got, st.available,
                                     (int)st.total, &fu);
    cJSON_Delete(doc);
    st.full_unadvanced = fu;

    if (got <= 0 || st.repeated) { free(next); break; }   /* upstream is exhausted */
    free(prev_url);
    prev_url = page_url;
    page_url = next;
    if (st.pages + 1 >= page_max && page_url) st.truncated = 1;   /* ceiling bit */
  }
  free(prev_url);
  free(page_url);
  free(probe_fps);

  /* A THIRD trigger, for the paired walk: the upstream's own total exceeds
   * what this walk fetched, and nothing above noticed.
   *
   * `truncated` is our ceiling and `full_unadvanced` is a full last page with
   * no cursor; neither fires when the walk ENDS for another reason — and the
   * commonest other reason is the repeat-page guard, which `break`s out of the
   * loop above before `truncated` can be set. So a row whose upstream says it
   * holds 9,079 records and which read 100 of them disclosed NOTHING (measured
   * on `gov-dataeuropa-search`: data.europa.eu answers `result.count: 9079`
   * beside the 100 records it hands over, its deeper pages are not stable
   * between reads, and every sort spelling it accepts is ignored — so one page
   * is all that can be read SAFELY, and the remainder is a fact about the
   * source that belongs in the database).
   *
   * It cannot cry wolf: a walk that read everything has `available == fetched`
   * and files nothing, which is the property that makes a notice worth
   * reading at all. */
  int declared_short = json && !st.failed && !st.truncated && !st.full_unadvanced &&
                       st.available >= 0 && st.available > st.fetched;

  if (st.truncated || st.full_unadvanced || declared_short) {
    char fdesc[64] = "", freason[256] = "";
    if (st.failed) {
      feed_status_describe(st.failed_status, fdesc, sizeof fdesc);
      pw_failed_page_reason(st.failed_page, st.failed_status, freason, sizeof freason);
    }
    char ceil_remedy[160];
    snprintf(ceil_remedy, sizeof ceil_remedy,
             "raise $%s — see docs/SOURCE_EXHAUSTIVENESS.md",
             legacy_env ? legacy_env : "JO_PAGE_MAX");
    char reason_buf[512], title[256];
    const char *reason, *remedy;
    if (st.failed) {
      reason = freason;
      remedy = "re-run; a host that refuses or rate-limits later pages needs a "
               "per-host minimum gap (core/hostgate.c) — see docs/SOURCE_EXHAUSTIVENESS.md";
    } else if (st.truncated) {
      snprintf(reason_buf, sizeof reason_buf,
               "the page ceiling stopped the walk while the upstream still had %s",
               json ? "pages" : "features");
      reason = reason_buf;
      remedy = ceil_remedy;
    } else if (declared_short) {
      reason = "the walk ended without reaching the upstream's own declared total — "
               "either it re-served a page it had already given (the repeat guard) or "
               "it ran out of pages to offer, so the remainder was never requested";
      remedy = "ask the endpoint what ordering it HONOURS before paging it, with a "
               "control (an unknown field in a well-typed sort value): where a sort "
               "is only type-checked and then ignored, a page walk re-reads and skips, "
               "and one stable page plus this notice is the honest read. Where a sort "
               "does work, give the row an ascending one on its own id and a cursor — "
               "see docs/SOURCE_EXHAUSTIVENESS.md";
    } else {
      snprintf(reason_buf, sizeof reason_buf,
               "the last page came back full (or the upstream declared more than was "
               "read) and neither the response nor this URL offers a way to ask for "
               "the next page, so %s may remain unread", json ? "records" : "features");
      reason = reason_buf;
      remedy = "give this source a cursor the walk can advance (for Socrata: $offset "
               "with a stable $order=:id) — see docs/SOURCE_EXHAUSTIVENESS.md";
    }
    if (st.failed && st.available >= 0)
      snprintf(title, sizeof title, "%s used %ld of %ld available %s — page %d answered %s",
               source_id, st.total, st.available, unit, st.failed_page, fdesc);
    else if (st.failed)
      snprintf(title, sizeof title, "%s used %ld %s — page %d answered %s",
               source_id, st.total, unit, st.failed_page, fdesc);
    else if (st.available >= 0)
      snprintf(title, sizeof title, "%s used %ld of %ld available %s",
               source_id, st.total, st.available, unit);
    else if (st.truncated)
      snprintf(title, sizeof title, "%s used %ld %s and stopped at the page ceiling",
               source_id, st.total, unit);
    else
      snprintf(title, sizeof title, "%s used %ld %s; the last page was full "
               "and this URL has no cursor to ask for more", source_id, st.total, unit);

    pw_notice n = PW_NOTICE_INIT;
    n.sink = sink;
    n.source_id = source_id;
    n.url_prop = "endpoint";
    n.url = endpoint;
    n.used = st.total;
    n.available = st.available;
    n.avail_style = PW_AVAIL_STRING;
    n.basis = "upstream declared this total";
    /* The GeoJSON notice never carried records_fetched; it counts against its
     * own emitted total. */
    n.fetched = json ? st.fetched : -1;
    n.pages_read = st.pages;
    n.page_ceiling = page_max;
    /* FALSE on the declared-short path, and that is the point: the walk held
     * no next-page URL when it stopped, so a checker keyed on this flag must
     * not read it as "more is coming". The authoritative signal is the pair
     * records_available / records_fetched. */
    n.more_pages_pending = declared_short ? 0 : 1;
    n.failed_page = st.failed ? st.failed_page : 0;
    n.failed_status = st.failed_status;
    n.failed_url = st.failed_url;
    n.reason = reason;
    n.remedy = remedy;
    n.title = title;
    n.key_style = PW_KEY_PLAIN;
    pw_notice_emit(sink, &n);
  }

  if (st.pages > 1 || st.truncated || st.full_unadvanced)
    fprintf(stderr, "[%s] emitted %ld across %d page(s)%s\n",
            source_id, st.total, st.pages,
            st.failed ? " (TRUNCATED: a later page failed)"
            : st.truncated ? " (TRUNCATED)"
                      : st.full_unadvanced ? " (TRUNCATED: full last page, no cursor)" : "");
  free(st.failed_url);
  return (int)st.total;
}

int pw_walk(const source_ctx *c, intel_sink *s, const char *id,
            const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page, void *ud) {
  return pw_walk_max(c, s, id, url, fetch, emit_page, ud, 0);
}

int pw_walk_max(const source_ctx *c, intel_sink *s, const char *id,
                const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page,
                void *ud, int page_max_declared) {
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
  long page_size = pw_declared_size(url, PWP_DECL);

  /* Does the URL carry a page NUMBER the author wrote? That is a weaker
   * requirement than a declared size, and it unlocks `?page=1` with no size
   * parameter at all — ROR is the header's own motivating example: measured
   * against the real pw_walk, `https://api.ror.org/organizations?page=1` did
   * exactly ONE fetch, because the continuation test demanded a page_size the
   * URL never states. Nine more fleet URLs are the same shape (bio.tools,
   * luchtmeetnet x4, data.gov.sg, CORDIS before `num` was recognised).
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
  int has_page_param = 0;
  for (int i = 0; PW_CURSORS[i].name && !has_page_param; i++) {
    long pn = 0; size_t a = 0, b = 0;
    if ((PW_CURSORS[i].uses & PWC_PAGE_DECL) &&
        pw_url_find(url, PW_CURSORS[i].name, 1, &pn, &a, &b))
      has_page_param = 1;
  }

  const int page_max = pw_page_ceiling(NULL, page_max_declared);
  const int may_walk = pw_walk_enabled();

  char *cur = strdup(url);
  if (!cur) return -1;

  long total = 0, available = -1, dropped = 0;
  int  pages = 0, last_seen = 0;
  int  first_seen = -1;   /* the server's own page size, when the URL states none */
  int  stopped_at_ceiling = 0, stopped_on_repeat = 0, full_last = 0;
  int  stopped_on_same_body = 0;
  /* A later page that failed. The status comes from feed_last_json_status():
   * pw_fetch_json goes through feed_get_json, and a fetch callback that does
   * not leaves it FEED_ST_UNKNOWN, said as such. */
  int  failed_page = 0;
  long failed_status = FEED_ST_UNKNOWN;
  char *failed_url = NULL;
  unsigned long long prev_fp = 0;
  char *next = NULL;

  for (;;) {
    feed_last_json_status_reset();
    cJSON *doc = fetch(c, cur, ud);
    if (!doc) {
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
      unsigned long long fp = pw_body_fp(doc);
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

    long t = pw_declared_total(doc, 0);
    if (t >= 0) available = t;

    next = pw_next_link(doc, NULL);
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
      if (full_last) next = pw_advance_declared(cur, page_size);
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
  char failwhy[320] = "";
  /* Name the ceiling that actually bit — see pw_ceiling_name(). */
  char ceilwhy[200];
  snprintf(ceilwhy, sizeof ceilwhy,
           "the page ceiling (%d page%s, %s) stopped the walk", page_max,
           page_max == 1 ? "" : "s", pw_ceiling_name(NULL, page_max_declared));
  if (failed_page > 0)
    pw_failed_page_reason(failed_page, failed_status, failwhy, sizeof failwhy);
  if (failed_page > 0 || stopped_at_ceiling || stopped_on_repeat ||
      stopped_on_same_body || more_known || full_last || dropped > 0) {
    const char *reason =
      failed_page > 0      ? failwhy
      : stopped_on_same_body ? "the upstream answered the next page with the page "
                           "just fetched, byte for byte, so it is ignoring the "
                           "paging parameter this URL carries; the walk stopped "
                           "rather than re-collecting the same records"
      : stopped_on_repeat  ? PW_REASON_NEXT_LINK_SELF
      : stopped_at_ceiling ? ceilwhy
      : more_known       ? "the upstream reports more records than were collected"
      : full_last        ? "the last page came back full and the upstream offered "
                           "no next link, nor does this source's URL carry an "
                           "offset parameter that could be advanced"
                         : "records in the pages fetched carried no field this "
                           "collector could use as a title, so they were not "
                           "emitted as rows";
    const char *remedy =
      failed_page > 0      ? "re-run — the walk starts again from the same URL; a host "
                           "that refuses or rate-limits later pages needs a per-host "
                           "minimum gap (core/hostgate.c)"
      : stopped_on_same_body ? "this endpoint does not page the way its URL implies — "
                           "check whether it needs a different parameter, or drop "
                           "the one it ignores so the bound is stated honestly"
      : stopped_on_repeat  ? "the upstream's pagination is not advancing — check whether "
                           "this endpoint needs a cursor parameter we are not sending"
      : stopped_at_ceiling ? "raise JO_PAGE_MAX, or re-run — the walk resumes from the same URL"
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
     * When those differ the gap is the discard, and it is stated as data.
     * `query` is deliberately NULL: the uid must stay one row per SOURCE, as it
     * was when this module hand-rolled the record, rather than one row per
     * url — a walk whose offset advances would otherwise leave a new notice
     * behind on every page boundary. The url is a property instead. */
    pw_notice n = PW_NOTICE_INIT;
    n.sink = s;
    n.source_id = id;
    n.url_prop = "url";
    n.url = url;
    n.used = total;
    n.available = available;
    n.avail_style = PW_AVAIL_NULL;
    n.pages_read = pages;
    n.dropped = dropped;
    n.failed_page = failed_page;
    n.failed_status = failed_status;
    n.failed_url = failed_url;
    n.reason = reason;
    n.remedy = remedy;
    n.key_style = PW_KEY_SCOPED;
    pw_notice_emit(s, &n);
    fprintf(stderr, "[%s] emitted %ld across %d page(s) — TRUNCATED (%s)\n",
            id, total, pages, reason);
  } else {
    fprintf(stderr, "[%s] emitted %ld across %d page(s)\n", id, total, pages);
  }

  free(failed_url);
  free(cur);
  return (int)total;
}
