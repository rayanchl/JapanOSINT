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
  int  n_body;
  int  calls;
  char seen[8][512];           /* the URL of each call, so paging is checkable */
} script_t;

static cJSON *script_fetch(const source_ctx *c, const char *url, void *ud) {
  (void)c;
  script_t *sc = (script_t *)ud;
  int i = sc->calls;
  if (i < 8) snprintf(sc->seen[i], sizeof sc->seen[i], "%s", url);
  sc->calls++;
  if (i >= sc->n_body || !sc->body[i]) return NULL;
  return cJSON_Parse(sc->body[i]);
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
    script_t sc = { .body = { Q3_a, Q3_a, Q3_a, Q3_a, Q3_a }, .n_body = 5 };
    cap_t cap; int n = run("https://x/api?page=1", &sc, &cap);
    ok(sc.calls == 2, "the repeated body stopped the walk after one wasted fetch");
    ok(n == 3, "records_used is what was seen once, not a multiple of it");
    ok(cap.notices == 1, "the endpoint that ignores its own paging is disclosed");
    ok(prop_num(cap.last_notice_props, "records_used") == 3, "the notice states 3, not 6");
    ok(prop_str_eq(cap.last_notice_props, "reason", "ignoring the paging parameter"),
       "and names the cause as an ignored parameter, not a ceiling");
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

  printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}
