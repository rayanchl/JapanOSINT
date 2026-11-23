/* tests/pagewalk_test.c — offline proof for lib/pagewalk.c.
 *
 * The module decides, per source, whether it may keep asking an upstream for
 * more records and what it must say when it stops. Both halves are house rules:
 * continuing wrongly means fabricated requests against 2,700 real endpoints,
 * and stopping silently is the discard this module was written to end. Neither
 * is visible from a compile, and neither can be tested against live upstreams
 * from here — so the fetch is scripted and every decision is asserted.
 *
 *   cc -Ithird_party tests/pagewalk_test.c lib/pagewalk.c third_party/cJSON.c
 */
#include "../lib/pagewalk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── stubs for the two symbols pagewalk.c pulls in from feedlib ───────────── */
#include "../lib/feedlib.h"
cJSON *feed_get_json(http_client *h, const char *url, int t) {
  (void)h; (void)url; (void)t; return NULL;    /* never reached: fetch is scripted */
}
char *feed_get_text(http_client *h, const char *url, int t) {
  (void)h; (void)url; (void)t; return NULL;
}
/* feed_last_json_status(): the scripted fetch sets it for a failing call, the
 * way feed_get_json does in production, so the notice's failed-page status is
 * checkable. feed_status_describe() mirrors lib/feedlib.c's wording. */
static long g_fake_status = FEED_ST_UNKNOWN;
long feed_last_json_status(void) { return g_fake_status; }
void feed_last_json_status_reset(void) { g_fake_status = FEED_ST_UNKNOWN; }
void feed_status_describe(long st, char *out, size_t cap) {
  if (st == FEED_ST_TRANSPORT)      snprintf(out, cap, "a transport failure");
  else if (st == FEED_ST_UNPARSED)  snprintf(out, cap, "an HTTP 2xx whose body was not JSON");
  else if (st > 0)                  snprintf(out, cap, "HTTP %ld", st);
  else                              snprintf(out, cap, "an unrecorded failure");
}

/* url_override_apply: stubbed so the test does not need the DB + hostgate the
 * real one pulls in. `g_override` lets a case assert that pw_walk plans against
 * the OVERRIDDEN url — an approved pagination swap lands in that table, so a
 * walk that read the compile-time url instead would leave the swap inert. */
static const char *g_override_from, *g_override_to;
const char *url_override_apply(const char *url) {
  if (g_override_from && url && strcmp(url, g_override_from) == 0)
    return g_override_to;
  return url;
}

static int failures = 0;
static void ok(int cond, const char *what) {
  printf("  %-5s %s\n", cond ? "ok" : "FAIL", what);
  if (!cond) failures++;
}

/* ── a sink that records what was emitted ────────────────────────────────── */
typedef struct {
  int records;                 /* ordinary rows */
  int notices;                 /* collector-truncation-notice rows */
  char last_notice_props[2048];
} cap_t;

static int cap_emit(struct intel_sink *s, const intel_item *it) {
  cap_t *c = (cap_t *)s->ctx;
  if (it->record_type && !strcmp(it->record_type, "collector-truncation-notice")) {
    c->notices++;
    snprintf(c->last_notice_props, sizeof c->last_notice_props, "%s",
             it->properties_json ? it->properties_json : "");
  } else {
    c->records++;
  }
  return 1;
}

/* ── a scripted upstream ─────────────────────────────────────────────────── */
typedef struct {
  const char *body[8];         /* JSON per call, NULL entry = fetch failure */
  long status[8];              /* what a failing call answered (0 = not set) */
  int  n_body;
  int  calls;
  int  repeat_body;            /* 1 = answer every call byte-identically */
  char seen[8][512];           /* the URL of each call, so paging is checkable */
} script_t;

/* Pages are made DISTINCT by default, with a `_page` serial stamped into the
 * envelope — because that is what a real paging server does, and because
 * pw_walk stops a walk whose next page comes back byte for byte identical (see
 * the London Datastore note in lib/pagewalk.c). The fixtures spell their pages
 * as arrays of empty objects, so without the serial every full page WAS
 * byte-identical and the guard ended every multi-page test at page 1.
 *
 * `repeat_body` turns the serial off, for the tests that are specifically
 * about a server ignoring the paging parameter. It is not the default: a test
 * that silently models a broken server while meaning to model a working one
 * reads as a passing test of the wrong thing. */
static cJSON *script_fetch(const source_ctx *c, const char *url, void *ud) {
  (void)c;
  script_t *sc = (script_t *)ud;
  int i = sc->calls;
  if (i < 8) snprintf(sc->seen[i], sizeof sc->seen[i], "%s", url);
  sc->calls++;
  if (i >= sc->n_body || !sc->body[i]) {
    if (i < 8 && sc->status[i]) g_fake_status = sc->status[i];
    return NULL;
  }
  cJSON *doc = cJSON_Parse(sc->body[i]);
  if (doc && !sc->repeat_body) cJSON_AddNumberToObject(doc, "_page", i + 1);
  return doc;
}

/* Emit one row per element of the top-level "items" array.
 *
 * `g_drop_per_page` makes the emitter REFUSE that many records per page while
 * still counting them as seen — which is what a real emitter does with a
 * record it cannot label (jsonlist_emit) or that is not an object
 * (geojson_emit_features). The distinction is the whole point of the `seen`
 * out-param: driving the full-page test off the emitted count made a full page
 * look short, which stopped the walk AND suppressed its truncation notice. */
static int g_drop_per_page = 0;

static int script_emit(const source_ctx *c, intel_sink *s, const char *id,
                       cJSON *doc, void *ud, int *seen) {
  (void)c; (void)ud;
  cJSON *arr = cJSON_GetObjectItem(doc, "items");
  int n = 0, have = 0, dropped = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    have++;
    if (dropped < g_drop_per_page) { dropped++; continue; }  /* refused, not absent */
    intel_item it = {0};
    char key[64]; snprintf(key, sizeof key, "%s|%d", id, n);
    it.remote_key = key;
    it.title = "row";
    if (s->emit(s, &it) >= 0) n++;
  }
  if (seen) *seen = have;
  return n;
}

static long prop_num(const char *props, const char *key) {
  cJSON *o = cJSON_Parse(props);
  cJSON *v = o ? cJSON_GetObjectItem(o, key) : NULL;
  long r = (cJSON_IsNumber(v)) ? (long)v->valuedouble : -12345;
  cJSON_Delete(o);
  return r;
}
static int prop_has(const char *props, const char *key) {
  cJSON *o = cJSON_Parse(props);
  int r = o && cJSON_GetObjectItem(o, key) != NULL;
  cJSON_Delete(o);
  return r;
}

/* Two rows / three rows, as an "items" array of N objects. */
static const char *P2_of3 = "{\"items\":[{},{}]}";        /* short page  */
static const char *P3_of3 = "{\"items\":[{},{},{}]}";     /* full page   */

static int run(const char *url, script_t *sc, cap_t *cap) {
  source_ctx ctx = {0};
  intel_sink sink = { cap, cap_emit };
  memset(cap, 0, sizeof *cap);
  sc->calls = 0;
  return pw_walk(&ctx, &sink, "TEST", url, script_fetch, script_emit, sc);
}

static int prop_str_eq(const char *props, const char *key, const char *want) {
  cJSON *o = cJSON_Parse(props);
  cJSON *v = o ? cJSON_GetObjectItem(o, key) : NULL;
  int r = cJSON_IsString(v) && strstr(v->valuestring, want) != NULL;
  cJSON_Delete(o);
  return r;
}

/* a scripted list upstream for pw_walk_list: page 1 and page 2 of two records
 * each, page 1 carrying a next link and a declared total of 10; `same` serves
 * page 1 again for every request; `fail_page` fails that page (1-based). */
typedef struct { int calls; int same; int fail_page; } lw_t;
static cJSON *lw_fetch(void *ud, const char *url) {
  lw_t *l = ud; (void)url;
  l->calls++;
  if (l->fail_page && l->calls == l->fail_page) { g_fake_status = 503; return NULL; }
  int pg = l->same ? 1 : l->calls;
  char b[200];
  snprintf(b, sizeof b, "{\"total_count\":%d,\"items\":[{\"id\":%d},{\"id\":%d}]%s}", l->same ? 10 : 4,
           pg * 10, pg * 10 + 1, pg == 1 ? ",\"next\":\"https://x/a?limit=3&page=2\"" : "");
  return cJSON_Parse(b);
}
static cJSON *lw_records(void *ud, cJSON *doc) { (void)ud; return cJSON_GetObjectItem(doc, "items"); }
static int lw_emit(void *ud, cJSON *doc, const unsigned long long *skip, int nskip) {
  (void)ud; (void)skip; (void)nskip;
  cJSON *a = cJSON_GetObjectItem(doc, "items");
  int n = cJSON_GetArraySize(a);
  return n;
}

int main(void) {
  setenv("JO_PAGE_MAX", "5", 1);
  setenv("JO_PAGE_WALK", "1", 1);

  printf("a short page is the end of the data\n");
  {
    script_t sc = { .body = { P2_of3 }, .n_body = 1 };
    cap_t cap; int n = run("https://x/api?limit=3", &sc, &cap);
    ok(n == 2 && cap.records == 2, "two records emitted");
    ok(sc.calls == 1, "one fetch — a short page is not followed");
    ok(cap.notices == 0, "no truncation notice: nothing was left behind");
  }

  printf("a full page with no way onward is DISCLOSED, not silently dropped\n");
  {
    script_t sc = { .body = { P3_of3 }, .n_body = 1 };
    cap_t cap; int n = run("https://x/api?limit=3", &sc, &cap);
    ok(n == 3, "three records emitted");
    ok(sc.calls == 1, "no second fetch — the URL offers nothing to advance");
    ok(cap.notices == 1, "a truncation notice was emitted");
    ok(prop_num(cap.last_notice_props, "records_used") == 3,
       "the notice states how many records were used");
    ok(prop_has(cap.last_notice_props, "remedy"),
       "the notice names a remedy, not just a complaint");
  }

  printf("an offset the author already wrote IS advanced\n");
  {
    script_t sc = { .body = { P3_of3, P3_of3, P2_of3 }, .n_body = 3 };
    cap_t cap; int n = run("https://x/api?limit=3&offset=0", &sc, &cap);
    ok(n == 8, "all three pages collected (3+3+2)");
    ok(sc.calls == 3, "walked until the short page ended it");
    ok(strstr(sc.seen[1], "offset=3") != NULL, "page 2 asked for offset=3");
    ok(strstr(sc.seen[2], "offset=6") != NULL, "page 3 asked for offset=6");
    ok(cap.notices == 0, "walk completed, so nothing to disclose");
  }

  printf("a page NUMBER is advanced by one, not by the page size\n");
  {
    script_t sc = { .body = { P3_of3, P2_of3 }, .n_body = 2 };
    cap_t cap; run("https://x/api?per_page=3&page=1", &sc, &cap);
    ok(strstr(sc.seen[1], "page=2") != NULL, "page 1 -> page 2");
  }

  printf("a server-supplied next link is followed verbatim\n");
  {
    script_t sc = { .body = {
      "{\"items\":[{},{},{}],\"links\":{\"next\":\"https://x/api?cursor=abc\"}}",
      P2_of3 }, .n_body = 2 };
    cap_t cap; int n = run("https://x/api", &sc, &cap);
    ok(n == 5, "both pages collected");
    ok(strcmp(sc.seen[1], "https://x/api?cursor=abc") == 0,
       "followed the exact URL the server gave");
    ok(cap.notices == 0, "reached the end, nothing disclosed");
  }

  printf("a relative or non-http next link is REFUSED, not guessed at\n");
  {
    script_t sc = { .body = { "{\"items\":[{},{}],\"next\":\"/api?page=2\"}" },
                    .n_body = 1 };
    cap_t cap; run("https://x/api", &sc, &cap);
    ok(sc.calls == 1, "a relative next link is not resolved against assumptions");
  }

  printf("the page ceiling stops the walk and says so\n");
  {
    script_t sc = { .body = { P3_of3, P3_of3, P3_of3, P3_of3, P3_of3, P3_of3 },
                    .n_body = 6 };
    cap_t cap; int n = run("https://x/api?limit=3&offset=0", &sc, &cap);
    ok(sc.calls == 5, "stopped at JO_PAGE_MAX=5");
    ok(n == 15, "kept every record it did fetch");
    ok(cap.notices == 1, "the ceiling stop is disclosed");
    ok(prop_num(cap.last_notice_props, "pages_read") == 5,
       "the notice states how many pages were read");
  }

  printf("upstream totals are reported when the envelope carries them\n");
  {
    script_t sc = { .body = {
      "{\"number_of_results\":110000,\"items\":[{},{},{}]}" }, .n_body = 1 };
    cap_t cap; run("https://x/api?rows=3", &sc, &cap);
    ok(cap.notices == 1, "disclosed");
    ok(prop_num(cap.last_notice_props, "records_available") == 110000,
       "records_available carries the upstream's own count");
  }

  printf("failure handling distinguishes a dead endpoint from a short walk\n");
  {
    script_t sc = { .body = { NULL }, .n_body = 1 };
    cap_t cap; int n = run("https://x/api?limit=3", &sc, &cap);
    ok(n == -1, "a failed FIRST fetch is an error");
    ok(cap.records == 0 && cap.notices == 0, "and emits nothing at all");
  }
  {
    script_t sc = { .body = { P3_of3, NULL }, .n_body = 2 };
    cap_t cap; int n = run("https://x/api?limit=3&offset=0", &sc, &cap);
    ok(n == 3, "a failed LATER page keeps what was already collected");
    ok(cap.notices == 1, "and discloses that the walk was cut short");
  }
  {
    /* …naming the page that failed, what it answered and where — it used to
     * be filed as "the page ceiling (JO_PAGE_MAX) or a failed page", which
     * sent the reader to raise a ceiling that had not been reached. */
    script_t sc = { .body = { P3_of3, NULL }, .status = { 0, 304 }, .n_body = 2 };
    cap_t cap; run("https://x/api?limit=3&offset=0", &sc, &cap);
    ok(cap.notices == 1 &&
       prop_num(cap.last_notice_props, "failed_page") == 2 &&
       prop_num(cap.last_notice_props, "failed_page_status") == 304 &&
       prop_str_eq(cap.last_notice_props, "failed_page_url", sc.seen[1]) &&
       prop_num(cap.last_notice_props, "pages_read") == 1,
       "a failed later page is disclosed with its page, status and URL");
    ok(prop_str_eq(cap.last_notice_props, "reason", "page 2 answered HTTP 304") &&
       !prop_str_eq(cap.last_notice_props, "reason", "ceiling"),
       "and the reason says what happened, not that a ceiling was hit");
  }

  printf("an approved url override is what the walk plans against\n");
  {
    /* The collector's compile-time url has no offset, so on its own it is one
     * page. The override adds the offset an operator approved. */
    const char *plain = "https://x/api?limit=3";
    g_override_from = plain;
    g_override_to   = "https://x/api?limit=3&offset=0";
    script_t sc = { .body = { P3_of3, P3_of3, P2_of3 }, .n_body = 3 };
    cap_t cap; int n = run(plain, &sc, &cap);
    ok(sc.calls == 3, "walked, because the OVERRIDDEN url carries the offset");
    ok(n == 8, "all three pages collected");
    ok(strstr(sc.seen[0], "offset=0") != NULL, "page 1 used the overridden url");
    ok(strstr(sc.seen[1], "offset=3") != NULL, "page 2 advanced that offset");
    g_override_from = g_override_to = NULL;
  }

  printf("JO_PAGE_WALK=0 restores single-fetch behaviour but KEEPS disclosure\n");
  {
    setenv("JO_PAGE_WALK", "0", 1);
    script_t sc = { .body = { P3_of3, P3_of3 }, .n_body = 2 };
    cap_t cap; int n = run("https://x/api?limit=3&offset=0", &sc, &cap);
    ok(sc.calls == 1, "exactly one fetch");
    ok(n == 3, "that page's records are kept");
    ok(cap.notices == 1, "the shortfall is still stated in-band");
    setenv("JO_PAGE_WALK", "1", 1);
  }

  /* ── regressions for the three defects found in the 2026-08-10 audit ────── */

  printf("a FULL page whose emitter refused a record is still full\n");
  {
    /* The bug: fullness was tested against rows EMITTED. A full page of 3 with
     * one unlabelled record reported 2, so the walk stopped and — because
     * full_last was false — emitted NO notice at all. Silent stop plus a silent
     * claim of completeness, on ~3,960 sources. */
    g_drop_per_page = 1;
    script_t sc = { .body = { P3_of3, P2_of3 }, .n_body = 2 };
    cap_t cap; int n = run("https://x/api?limit=3&offset=0", &sc, &cap);
    ok(sc.calls == 2, "the walk continued: the page was full even though a row was refused");
    ok(n == 3, "only the rows that could be emitted are counted (2 + 1)");
    ok(cap.notices == 1, "the refused records are disclosed, not dropped in silence");
    ok(prop_num(cap.last_notice_props, "records_dropped") == 2,
       "the notice states how many records were seen but not emitted");
    g_drop_per_page = 0;
  }
  {
    /* Same shape without pagination: a single short page that dropped a record
     * must still disclose, because the drop is a discard in its own right. */
    g_drop_per_page = 1;
    script_t sc = { .body = { P2_of3 }, .n_body = 1 };
    cap_t cap; int n = run("https://x/api?limit=3", &sc, &cap);
    ok(n == 1 && cap.notices == 1, "a dropped record discloses even on a short page");
    ok(prop_num(cap.last_notice_props, "records_dropped") == 1,
       "and states the count");
    g_drop_per_page = 0;
  }

  printf("a next link pointing at the page just fetched STOPS the walk\n");
  {
    /* The bug: a self-referential next link (a real cursor-API failure at the
     * last page) was followed to the ceiling, and every re-emit was added to
     * the total the notice then published as fact — 20 fetches, 3 distinct
     * records, "records_used": 60. The disclosure itself became the fabrication. */
    static const char *SELF =
      "{\"items\":[{},{},{}],\"links\":{\"next\":\"https://x/api?cursor=abc\"}}";
    script_t sc = { .body = { SELF, SELF, SELF, SELF, SELF }, .n_body = 5 };
    cap_t cap; int n = run("https://x/api?cursor=abc", &sc, &cap);
    ok(sc.calls == 1, "the repeated link was not followed");
    ok(n == 3, "records_used is what was actually seen once, not a multiple of it");
    ok(cap.notices == 1, "the non-advancing upstream is disclosed");
    ok(prop_num(cap.last_notice_props, "records_used") == 3,
       "the notice states 3, not 15");
    ok(prop_str_eq(cap.last_notice_props, "reason", "pointed back"),
       "and names the reason as a non-advancing next link");
  }

  printf("a date-shaped value is NEVER advanced as if it were an offset\n");
  {
    /* The bug: `start` and `from` are in PW_OFF_PARAMS, and the parser accepted
     * any leading digit run — so `start=2026-08-01` parsed as 2026 and advanced
     * to 2029-08-01, a fabricated request for a different time window whose
     * records would be attributed to this source. */
    script_t sc = { .body = { P3_of3, P3_of3 }, .n_body = 2 };
    cap_t cap; run("https://x/api?start=2026-08-01&limit=3", &sc, &cap);
    ok(sc.calls == 1, "a date is not an offset, so there is nothing to advance");
    ok(cap.notices == 1, "and the bound is disclosed instead of silently walked");
  }
  {
    script_t sc = { .body = { P3_of3, P3_of3, P2_of3 }, .n_body = 3 };
    cap_t cap; run("https://x/api?start=0&limit=3", &sc, &cap);
    ok(sc.calls == 3, "a genuine integer offset named `start` still advances");
    ok(strstr(sc.seen[1], "start=3") != NULL, "by the page size");
  }
  {
    /* A bare epoch is wholly numeric, so the boundary rule alone would accept
     * it. Two independent defences: `from` is not an offset parameter at all
     * (measured: it never carries one in this fleet), and a value above
     * PW_OFFSET_MAX is a timestamp rather than a record offset. */
    script_t sc = { .body = { P3_of3, P3_of3 }, .n_body = 2 };
    cap_t cap; run("https://x/api?from=1754697600&limit=3", &sc, &cap);
    ok(sc.calls == 1, "`from` is never treated as a record offset");
    ok(cap.notices == 1, "the bound is disclosed instead");
  }
  {
    script_t sc = { .body = { P3_of3, P3_of3 }, .n_body = 2 };
    cap_t cap; run("https://x/api?start=1754697600&limit=3", &sc, &cap);
    ok(sc.calls == 1, "an epoch spelled `start` is refused on magnitude");
    ok(cap.notices == 1, "and disclosed rather than walked into a new time window");
  }

  printf("the DataTables start/length convention paginates\n");
  {
    /* HUDOC (ECHR) and GA Tech GRIP both speak `start=0&length=50`. Before
     * `length` was a recognised size parameter, pagewalk saw no page size at
     * all — so it could neither continue nor even tell that the page came back
     * full, and emitted no notice either. */
    script_t sc = { .body = { P3_of3, P3_of3, P2_of3 }, .n_body = 3 };
    cap_t cap; int n = run("https://x/api?start=0&length=3", &sc, &cap);
    ok(sc.calls == 3, "walked start=0 -> 3 -> 6");
    ok(strstr(sc.seen[1], "start=3") != NULL, "advanced by the page size");
    ok(n == 8, "every page collected");
  }
  {
    script_t sc = { .body = { "{\"resultcount\":89604,\"items\":[{},{},{}]}" },
                    .n_body = 1 };
    cap_t cap; run("https://x/api?length=3", &sc, &cap);
    ok(cap.notices == 1, "a full page with no way onward is disclosed");
    ok(prop_num(cap.last_notice_props, "records_available") == 89604,
       "resultcount is read as the upstream's own total");
  }

  printf("Socrata's $limit/$offset advances like any other offset family\n");
  {
    /* `$offset` was missing from PW_OFF_PARAMS, and it is spelled distinctly
     * enough that the bare `offset` entry never covered it: the parser compares
     * from the parameter boundary, so `$offset=0` does not match `offset`. The
     * effect was that a keyed Socrata row read one page even after its cursor
     * had been seeded for it. */
    script_t sc = { .body = { P3_of3, P3_of3, P2_of3 }, .n_body = 3 };
    cap_t cap; int n = run("https://x/resource/ab12-cd34.json?$limit=3&$offset=0",
                           &sc, &cap);
    ok(sc.calls == 3, "walked $offset=0 -> 3 -> 6");
    ok(strstr(sc.seen[1], "$offset=3") != NULL, "advanced by the declared $limit");
    ok(n == 8, "every page collected");
  }
  {
    /* And the module's own rule still holds on the way in: a URL that declares
     * only a size carries no cursor pw_walk is entitled to invent. That one is
     * seeded before the walk starts (jl_seed_cursor), not guessed here. */
    script_t sc = { .body = { P3_of3, P3_of3 }, .n_body = 2 };
    cap_t cap; run("https://x/resource/ab12-cd34.json?$limit=3", &sc, &cap);
    ok(sc.calls == 1, "a bare $limit carries no cursor pw_walk may invent");
    ok(cap.notices == 1, "so the bound is disclosed instead");
  }

  /* Distinct full pages. The fixtures above model "a full page" with one
   * canonical constant, which is right for tests about paging control flow —
   * but the inferred-size walk below also compares response bodies, so it needs
   * pages that differ the way real pages do. */
  static const char *Q3_a   = "{\"items\":[{\"id\":1},{\"id\":2},{\"id\":3}]}";
  static const char *Q3_b   = "{\"items\":[{\"id\":4},{\"id\":5},{\"id\":6}]}";
  static const char *Q3_c   = "{\"items\":[{\"id\":7},{\"id\":8},{\"id\":9}]}";
  static const char *Q2_end = "{\"items\":[{\"id\":10},{\"id\":11}]}";

  printf("a page number with NO declared size walks on the server's own size\n");
  {
    /* The case the module was named for and did not cover. ROR, bio.tools,
     * luchtmeetnet and data.gov.sg all spell it `?page=1` with no size
     * parameter anywhere, so requiring a declared size meant one fetch. */
    script_t sc = { .body = { Q3_a, Q3_b, Q2_end }, .n_body = 3 };
    cap_t cap; int n = run("https://x/api?page=1", &sc, &cap);
    ok(sc.calls == 3, "walked page=1 -> 2 -> 3 with no size parameter present");
    ok(strstr(sc.seen[1], "page=2") != NULL, "advanced the page number by one");
    ok(strstr(sc.seen[2], "page=3") != NULL, "and again");
    ok(n == 8, "every page collected");
    ok(cap.notices == 0, "a short page ended it, so there is nothing to disclose");
  }
  {
    script_t sc = { .body = { Q3_a, Q3_b, Q3_c, Q3_a, Q3_b, Q3_c }, .n_body = 6 };
    cap_t cap; run("https://x/api?page=1", &sc, &cap);
    ok(sc.calls == 5, "the ceiling still bounds an inferred-size walk");
    ok(cap.notices == 1, "and the bound is disclosed");
  }
  {
    /* An endpoint that ignores `page` hands back page 1 forever. Advancing a
     * parameter WE wrote means the URL differs every time, so the next-link
     * guard cannot see it — this is F-5 by another route, and unguarded it
     * would publish records_used = 5 x 3. */
    script_t sc = { .body = { Q3_a, Q3_a, Q3_a, Q3_a, Q3_a }, .n_body = 5,
                    .repeat_body = 1 };
    cap_t cap; int n = run("https://x/api?page=1", &sc, &cap);
    ok(sc.calls == 2, "the repeated body stopped the walk after one wasted fetch");
    ok(n == 3, "records_used is what was seen once, not a multiple of it");
    ok(cap.notices == 1, "the endpoint that ignores its own paging is disclosed");
    ok(prop_num(cap.last_notice_props, "records_used") == 3, "the notice states 3, not 6");
    ok(prop_str_eq(cap.last_notice_props, "reason", "ignoring the paging parameter"),
       "and names the cause as an ignored parameter, not a ceiling");
  }
  printf("a DECLARED size and offset the server ignores stops the walk too\n");
  {
    /* The London Datastore, measured 2026-10-12: ?rows=200&start=0 is a
     * declared page size AND a declared offset, and that server honours
     * neither — every request answers with the whole catalogue of 1,306
     * datasets. The same-body guard used to be gated on the INFERRED-size walk
     * on the reasoning that a URL declaring its own size documents an author
     * who knew the paging contract. It does not: 1,306 >= 200 reads as a full
     * page, so the walk advanced `start` to the ceiling, re-emitted the same
     * records on every page, and published the multiple in the notice as
     * fact. Byte-identical means no progress whatever the URL declares. */
    script_t sc = { .body = { Q3_a, Q3_a, Q3_a, Q3_a, Q3_a }, .n_body = 5,
                    .repeat_body = 1 };
    cap_t cap; int n = run("https://x/api?rows=3&start=0", &sc, &cap);
    ok(sc.calls == 2, "one wasted fetch, then stop — not five");
    ok(n == 3, "records_used is one page, not five times one page");
    ok(cap.notices == 1, "and the ignored parameter is disclosed");
    ok(prop_num(cap.last_notice_props, "records_used") == 3, "the notice states 3");
    ok(prop_str_eq(cap.last_notice_props, "reason", "ignoring the paging parameter"),
       "named as an ignored parameter, not as a ceiling");
  }
  {
    /* The regression this must not cause. A URL with no page parameter at all
     * is the 2,592-source case: one fetch, and a full page is NOT evidence of
     * more, because there is no yardstick and nothing to advance. If the
     * inferred-size rule leaked to these, every such source would start
     * emitting a truncation notice claiming a bound that was never hit. */
    script_t sc = { .body = { Q3_a, Q3_b }, .n_body = 2 };
    cap_t cap; int n = run("https://x/api?q=tokyo", &sc, &cap);
    ok(sc.calls == 1, "no page parameter means nothing to advance");
    ok(n == 3, "the one page is collected");
    ok(cap.notices == 0, "and a full page alone is NOT claimed as truncation");
  }

  printf("size spellings measured before being trusted\n");
  {
    /* CORDIS: `p=1&num=50`. `num` is the Google/SerpAPI spelling. */
    script_t sc = { .body = { Q3_a, Q3_b, Q2_end }, .n_body = 3 };
    cap_t cap; int n = run("https://x/api?p=1&num=3", &sc, &cap);
    ok(sc.calls == 3, "`num` is read as a page size");
    ok(strstr(sc.seen[1], "p=2") != NULL, "so `p` advances against it");
    ok(n == 8, "every page collected");
  }
  {
    /* NSF: `rpp=25&offset=1`. An offset needs a size for its stride, and this
     * was the only offset walk in the tree blocked purely on the spelling. */
    script_t sc = { .body = { Q3_a, Q3_b, Q2_end }, .n_body = 3 };
    cap_t cap; run("https://x/api?rpp=3&offset=1", &sc, &cap);
    ok(sc.calls == 3, "`rpp` is read as a page size");
    ok(strstr(sc.seen[1], "offset=4") != NULL, "so the offset advances by it");
  }

  /* One ceiling for the whole tree. There were three — 20 here under
   * $JO_PAGE_MAX, 20 in lib/jsonlist.c under $JO_JSONLIST_PAGE_MAX, and 10 in
   * lib/hpengine.c under no variable at all — so an operator who set
   * JO_PAGE_MAX moved one population, silently did not move the second, and
   * could not reach the third at any value. jo_page_max() is now the single
   * answer; these pin its precedence, because the failure of the old
   * arrangement was that nothing contradicted it. */
  {
    unsetenv("JO_PAGE_MAX");
    unsetenv("JO_JSONLIST_PAGE_MAX");
    ok(jo_page_max(NULL) == 20, "the default ceiling is 20 pages");
    ok(jo_page_max("JO_JSONLIST_PAGE_MAX") == 20, "and 20 for a legacy caller");

    setenv("JO_JSONLIST_PAGE_MAX", "7", 1);
    ok(jo_page_max("JO_JSONLIST_PAGE_MAX") == 7,
       "an older spelling is honoured, not ignored");
    ok(jo_page_max(NULL) == 20, "but only for the caller that names it");

    setenv("JO_PAGE_MAX", "9", 1);
    ok(jo_page_max(NULL) == 9, "JO_PAGE_MAX reaches every walk");
    ok(jo_page_max("JO_JSONLIST_PAGE_MAX") == 9, "and wins when both are set");

    setenv("JO_PAGE_MAX", "0", 1);
    ok(jo_page_max(NULL) == 20, "0 is not a ceiling of zero pages");
    setenv("JO_PAGE_MAX", "not-a-number", 1);
    ok(jo_page_max(NULL) == 20, "nor is garbage");
    setenv("JO_PAGE_MAX", "5", 1);          /* as the walks above expect */
    unsetenv("JO_JSONLIST_PAGE_MAX");
  }

  /* ── the paging CORE (2026-11-16): the one copy of each shared decision ── */
  printf("one declared-total list: every key any of the old three accepted\n");
  {
    struct { const char *json; long want_plain; long want_count; const char *what; } T[] = {
      { "{\"total_count\":7}", 7, 7, "total_count" },
      { "{\"count\":5}", -1, 5, "bare `count` only when the caller opts in (DRF yes, pw_walk no)" },
      { "{\"meta\":{\"count\":6}}", -1, 6, "meta.count is gated the same way" },
      { "{\"meta\":{\"total\":9}}", 9, 9, "meta.total (pagewalk's)" },
      { "{\"result\":{\"count\":9079}}", 9079, 9079, "result.count (CKAN, data.europa.eu)" },
      { "{\"result\":{\"totalResults\":71323}}", 71323, 71323, "result.totalResults (Linked Data API)" },
      { "{\"hit\":5409}", 5409, 5409, "hit (Japan Search)" },
      { "{\"response\":{\"numFound\":12}}", 12, 12, "response.numFound (Solr, jsonlist's)" },
      { "{\"numFound\":13}", 13, 13, "numFound (pagewalk's)" },
      { "{\"totalRecords\":14}", 14, 14, "totalRecords (pagewalk-only key, kept)" },
      { "{\"total_rows\":15}", 15, 15, "total_rows (pagewalk-only key, kept)" },
      { "{\"totalFeatures\":16}", 16, 16, "totalFeatures (pagewalk's and geojson's)" },
      { "{\"matchedCount\":17}", 17, 17, "matchedCount (geojson-only key, kept)" },
      { "{\"@odata.count\":18}", 18, 18, "@odata.count (a key containing a dot)" },
      { "{\"odata.count\":19}", 19, 19, "odata.count" },
      { "{\"d\":{\"__count\":\"46144\"}}", 46144, 46144, "OData v2's string total" },
      { "{\"resultcount\":20}", 20, 20, "resultcount, case-insensitively (pagewalk spelling)" },
      { "{\"nhits\":21}", 21, 21, "nhits (OpenDataSoft)" },
      { "{\"totalItems\":22}", 22, 22, "totalItems" },
      { "{\"total\":\"many\"}", -1, -1, "a non-number is no total" },
      { "{}", -1, -1, "nothing declared is -1, never a guess" },
    };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
      cJSON *d = cJSON_Parse(T[i].json);
      ok(pw_declared_total(d, 0) == T[i].want_plain &&
         pw_declared_total(d, PW_TOT_COUNT) == T[i].want_count, T[i].what);
      cJSON_Delete(d);
    }
    ok(pw_declared_total_xml("<r><SearchHits>50530</SearchHits><SearchResult/></r>", "<SearchResult") == 50530,
       "the XML list lives in the same module");
    ok(pw_declared_total_xml("<r><SearchResult><TotalCount>9</TotalCount></SearchResult></r>", "<SearchResult") == -1,
       "and only reads before the first record");
  }

  printf("one next-link reader, one resolver, one cross-host rule\n");
  {
    cJSON *d = cJSON_Parse("{\"next\":\"?page=2&format=json\"}");
    char *u = pw_next_link(d, "https://h.test/api?page=1&format=json");
    ok(u && !strcmp(u, "https://h.test/api?page=2&format=json"), "a relative link resolves against the page it came from");
    free(u);
    ok(pw_next_link(d, NULL) == NULL, "an absolute-only caller refuses a relative link");
    cJSON_Delete(d);
    d = cJSON_Parse("{\"next\":\"\",\"links\":{\"next\":{\"href\":\"https://h.test/p3\"}}}");
    u = pw_next_link(d, NULL);
    ok(u && !strcmp(u, "https://h.test/p3"), "an empty `next` does not shadow a usable links.next");
    free(u); cJSON_Delete(d);
    d = cJSON_Parse("{\"nextLink\":\"https://h.test/odata?$skip=5\"}");
    u = pw_next_link(d, NULL);
    ok(u != NULL, "nextLink (pagewalk's spelling) is read by the one list");
    free(u); cJSON_Delete(d);
    d = cJSON_Parse("{\"next\":\"eyJvZmZzZXQiOjV9\"}");
    ok(pw_next_link(d, "https://h.test/a") == NULL, "a bare cursor token is not glued onto the base");
    cJSON_Delete(d);
    u = pw_resolve_url("https://h.test/a/b?x=1", "../c?y=2", PW_RESOLVE_PATHS);
    ok(u && !strcmp(u, "https://h.test/c?y=2"), "a relative path resolves when the caller asks for paths");
    free(u);
    u = pw_resolve_url("https://h.test/a/b", "//other.test/z", 0);
    ok(u && !strcmp(u, "https://other.test/z"), "a protocol-relative link keeps the scheme");
    free(u);
    ok(pw_same_host("https://H.test/a", "https://h.TEST:8443/b") == 1 &&
       pw_same_host("https://h.test/a", "https://evil.test/x?e=a@h.test") == 0 &&
       pw_same_host(NULL, NULL) == 0, "same host is case-insensitive, ignores userinfo tricks, and NULLs are not the same host");
  }

  printf("one ceiling, one failed-page ruling\n");
  {
    unsetenv("JO_PAGE_MAX"); unsetenv("JO_GEOJSON_PAGE_MAX");
    ok(pw_page_ceiling("JO_GEOJSON_PAGE_MAX", 0) == 20, "the default");
    setenv("JO_GEOJSON_PAGE_MAX", "4", 1);
    ok(pw_page_ceiling("JO_GEOJSON_PAGE_MAX", 0) == 4, "a legacy knob is honoured");
    setenv("JO_PAGE_MAX", "2", 1);
    ok(pw_page_ceiling("JO_GEOJSON_PAGE_MAX", 0) == 2 && pw_page_ceiling(NULL, 200) == 2,
       "JO_PAGE_MAX brakes the legacy knob and a declared ceiling alike");
    ok(!strcmp(pw_ceiling_name("JO_GEOJSON_PAGE_MAX", 200), "JO_PAGE_MAX"), "and is named as the knob that bit");
    setenv("JO_PAGE_MAX", "5", 1);
    unsetenv("JO_GEOJSON_PAGE_MAX");
    ok(pw_later_page_cut(-1, 0, 0, 0, -1, 0) && pw_later_page_cut(429, 0, 0, 0, -1, 0) &&
       pw_later_page_cut(200, 0, 0, 0, -1, 0), "transport, 429 and an unreadable 200 are a cut");
    ok(!pw_later_page_cut(500, 1, 0, 0, -1, 0), "after a short page anything is the end of data");
    ok(!pw_later_page_cut(404, 0, 0, 0, -1, 0) && pw_later_page_cut(404, 0, 1, 0, -1, 0),
       "a 404 is a cut only on evidence of more");
  }

  printf("one pager table\n");
  {
    ok(pw_pager_find("$limit") && !strcmp(pw_pager_find("$limit")->cursor, "$offset"), "Socrata's pair");
    ok(pw_pager_find("rpp") && !(pw_pager_find("rpp")->uses & PWP_JSON), "a declared-size-only spelling is not moved by the paired walk");
    ok(pw_declared_size("https://x/a?rows=5&start=0", PWP_JSON) == 5, "the size a URL declares");
    char *seed = pw_seed_cursor("https://x/a?limit=100");
    ok(seed && strstr(seed, "offset=0"), "an offset family is seeded at 0");
    free(seed);
    ok(pw_seed_cursor("https://x/a?size=100") == NULL, "a page-number family is never seeded");
  }

  printf("one notice builder\n");
  {
    cap_t cap = {0};
    intel_sink sink = { .emit = cap_emit, .ctx = &cap };
    pw_notice n = PW_NOTICE_INIT;
    n.source_id = "X"; n.url_prop = "endpoint"; n.url = "https://x"; n.used = 3; n.available = 9;
    n.basis = "upstream declared this total"; n.pages_read = 1; n.page_ceiling = 20;
    n.more_pages_pending = 1; n.failed_page = 2; n.failed_status = 429; n.failed_url = "https://x/2";
    n.reason = "r"; n.remedy = "m";
    pw_notice_emit(&sink, &n);
    ok(cap.notices == 1 && prop_num(cap.last_notice_props, "failed_page") == 2 &&
       prop_num(cap.last_notice_props, "records_available") == 9 &&
       prop_str_eq(cap.last_notice_props, "records_available_basis", "upstream declared this total") &&
       prop_has(cap.last_notice_props, "page_ceiling"),
       "every field name the tools parse is present");
  }

  printf("the list walk (jsonlist / geojson drivers) over the core\n");
  {
    lw_t lw = { .same = 0 };
    cap_t cap = {0};
    intel_sink sink = { .emit = cap_emit, .ctx = &cap };
    pw_list_ops ops = { &lw, lw_fetch, lw_records, lw_emit };
    int n = pw_walk_list(PW_FLAVOR_JSON, &sink, "L", "https://x/a?limit=3", "https://x/a?limit=3", NULL, &ops);
    ok(n == 4 && lw.calls == 2 && cap.notices == 0, "a link-followed list walk collects every page and says nothing");
    lw = (lw_t){ .same = 1 };
    memset(&cap, 0, sizeof cap);
    n = pw_walk_list(PW_FLAVOR_JSON, &sink, "L", "https://x/a?limit=3", "https://x/a?limit=3", NULL, &ops);
    ok(n == 2 && cap.notices == 1 && prop_num(cap.last_notice_props, "records_available") == 10,
       "a re-served page stops the walk, is not emitted twice, and the declared remainder is disclosed");
    ok(prop_str_eq(cap.last_notice_props, "records_available_basis", "upstream declared this total"),
       "with the basis stated");
    lw = (lw_t){ .same = 0 };
    memset(&cap, 0, sizeof cap);
    setenv("JO_PAGE_MAX", "1", 1);
    n = pw_walk_list(PW_FLAVOR_JSON, &sink, "L", "https://x/a?limit=3", "https://x/a?limit=3", NULL, &ops);
    ok(n == 2 && cap.notices == 1 && prop_str_eq(cap.last_notice_props, "reason", "page ceiling"),
       "the ceiling is the operator's brake and the notice says the ceiling bit");
    setenv("JO_PAGE_MAX", "5", 1);
    lw = (lw_t){ .same = 0, .fail_page = 2 };
    memset(&cap, 0, sizeof cap);
    n = pw_walk_list(PW_FLAVOR_JSON, &sink, "L", "https://x/a?limit=3", "https://x/a?limit=3", NULL, &ops);
    ok(n == 2 && prop_num(cap.last_notice_props, "failed_page") == 2, "a failed later page is named");
  }

  printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}
