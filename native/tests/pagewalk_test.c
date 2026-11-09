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
 *
 * Since 2026-11-07 it also compiles lib/jsonlist.c and lib/geojson.c IN (they
 * are #included below, the way tests/unit/ includes the .c it tests), because
 * the three walkers now share one declared-total reader, one next-link reader
 * and one stop describer from lib/pagewalk.c, and the defects those replaced
 * lived in the OTHER two copies. Their fetch, feed_get_json, is the scripted
 * stub below, so the Makefile's link line is unchanged.
 *
 * -DPW_TEST_OLD_API compiles only the cases that use APIs which existed before
 * 2026-11-07, so the same file can be run against the previous sources to
 * show each new regression case FAILING there (how it was checked).
 */
#include "../lib/pagewalk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── stubs for the symbols pagewalk.c, jsonlist.c and geojson.c pull in ──── */
#include "../lib/feedlib.h"
#ifndef FEED_ST_DECLARED_EMPTY
#define FEED_ST_DECLARED_EMPTY (-3L)   /* PW_TEST_OLD_API: absent before 2026-11-07 */
#endif
/* jsonlist_emit_paged and geojson_emit_paged fetch through feed_get_json, so
 * it reads the same script the pw_walk cases hand pw_walk directly. */
struct script_s;
static struct script_s *g_feed_sc;
static cJSON *script_fetch(const source_ctx *c, const char *url, void *ud);
cJSON *feed_get_json(http_client *h, const char *url, int t) {
  (void)h; (void)t;
  return g_feed_sc ? script_fetch(NULL, url, g_feed_sc) : NULL;
}
/* jsonlist.c's uid hash and geojson.c's SHA-1: the uids are not under test
 * here, and the Makefile rule links no libcrypto. Deterministic, never empty. */
void feed_hash_key(char *out21, const char *const *parts, int n) {
  unsigned long h = 5381;
  for (int i = 0; i < n; i++)
    for (const char *p = parts[i] ? parts[i] : ""; *p; p++) h = h * 33 + (unsigned char)*p;
  snprintf(out21, 21, "%020lu", h);
}
unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md) {
  static unsigned char buf[20];
  unsigned char *o = md ? md : buf;
  memset(o, 0, 20);
  for (size_t i = 0; i < n; i++) o[i % 20] = (unsigned char)(o[i % 20] * 31 + d[i]);
  return o;
}
char *feed_get_text(http_client *h, const char *url, int t) {
  (void)h; (void)url; (void)t; return NULL;
}
/* feed_last_json_status(): the scripted fetch sets it for a failing call, the
 * way feed_get_json does in production, so the notice's failed-page status is
 * checkable. feed_status_describe() mirrors lib/feedlib.c's wording. */
static long g_fake_status = FEED_ST_UNKNOWN;
#ifndef PW_TEST_OLD_API
#endif
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

/* The other two walkers, compiled in (see the header comment). */
#ifndef PW_JSONLIST_C
#define PW_JSONLIST_C "../lib/jsonlist.c"
#define PW_GEOJSON_C  "../lib/geojson.c"
#endif
#include PW_JSONLIST_C
#include PW_GEOJSON_C

static int failures = 0;
static void ok(int cond, const char *what) {
  printf("  %-5s %s\n", cond ? "ok" : "FAIL", what);
  if (!cond) failures++;
}

/* ── a sink that records what was emitted ────────────────────────────────── */
typedef struct {
  int records;                 /* ordinary rows */
  int notices;                 /* collector-truncation-notice rows */
  int other_notices;           /* any other collector-*-notice (jsonlist's shape notices) */
  char last_notice_props[4096];
} cap_t;

static int cap_emit(struct intel_sink *s, const intel_item *it) {
  cap_t *c = (cap_t *)s->ctx;
  if (it->record_type && !strcmp(it->record_type, "collector-truncation-notice")) {
    c->notices++;
    snprintf(c->last_notice_props, sizeof c->last_notice_props, "%s",
             it->properties_json ? it->properties_json : "");
  } else if (it->record_type && !strncmp(it->record_type, "collector-", 10)) {
    c->other_notices++;
  } else {
    c->records++;
  }
  return 1;
}

/* ── a scripted upstream ─────────────────────────────────────────────────── */
typedef struct script_s {
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

/* The VJSON and VGEO walks, which fetch through the scripted feed_get_json. */
static int run_jl(const char *url, script_t *sc, cap_t *cap) {
  intel_sink sink = { cap, cap_emit };
  memset(cap, 0, sizeof *cap);
  sc->calls = 0;
  g_feed_sc = sc;
  int n = jsonlist_emit_paged(&sink, "TEST", NULL, url, 1000, "items", NULL, NULL, NULL);
  g_feed_sc = NULL;
  return n;
}
static int run_gj(const char *url, script_t *sc, cap_t *cap) {
  intel_sink sink = { cap, cap_emit };
  memset(cap, 0, sizeof *cap);
  sc->calls = 0;
  g_feed_sc = sc;
  int n = geojson_emit_paged(&sink, "TEST", NULL, url, 1000);
  g_feed_sc = NULL;
  return n;
}
#ifndef PW_TEST_OLD_API
static int run_resume(const char *url, const char *rin, char *rout, size_t rcap,
                      script_t *sc, cap_t *cap) {
  source_ctx ctx = {0};
  intel_sink sink = { cap, cap_emit };
  memset(cap, 0, sizeof *cap);
  sc->calls = 0;
  return pw_walk_max_resume(&ctx, &sink, "TEST", url, script_fetch, script_emit,
                            sc, 0, rin, rout, rcap);
}
#endif

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

  printf("a relative next link is resolved in its two unambiguous spellings only\n");
  {
    /* This used to assert that "/api?page=2" was REFUSED. That refusal was
     * the opposite of jsonlist's rule (bio.tools' `"next": "?page=2&format=json"`
     * was the whole reason 30,000 rows stopped at page 1 there), and it made
     * `nextRecordsUrl` in this file's own key list dead — Salesforce always
     * publishes it as a path. Since 2026-11-07 the three walkers share
     * jo_next_link(): "?query" and "/path" resolve against the page's own
     * URL, which takes nothing from the API but the origin we already asked;
     * a bare token, another scheme or "//host" is still refused. */
    script_t sc = { .body = { "{\"items\":[{},{}],\"next\":\"/api?page=2\"}", P2_of3 },
                    .n_body = 2 };
    cap_t cap; run("https://x/api", &sc, &cap);
    ok(sc.calls == 2 && strcmp(sc.seen[1], "https://x/api?page=2") == 0,
       "a \"/path\" next link is resolved against the page's origin");
  }
  {
    script_t sc = { .body = { "{\"items\":[{},{}],\"next\":\"abc123\"}", P2_of3 },
                    .n_body = 2 };
    cap_t cap; run("https://x/api", &sc, &cap);
    ok(sc.calls == 1, "a bare cursor token is not a URL and is not glued on");
  }
  {
    script_t sc = { .body = { "{\"items\":[{},{}],\"next\":\"//evil.example/x\"}", P2_of3 },
                    .n_body = 2 };
    cap_t cap; run("https://x/api", &sc, &cap);
    ok(sc.calls == 1, "a network-path reference (another host) is refused");
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

  /* ── 2026-11-07: one reader, one describer, three walkers ─────────────────
   * Each case below was run against the sources as they stood before that
   * day (-DPW_TEST_OLD_API, see the header) and FAILED there, except the ones
   * marked "control", which pin the half that must not move. */
  static const char *J3a = "{\"items\":[{\"title\":\"a1\"},{\"title\":\"a2\"},{\"title\":\"a3\"}]}";
  static const char *J3b = "{\"items\":[{\"title\":\"b1\"},{\"title\":\"b2\"},{\"title\":\"b3\"}]}";
  static const char *J3c = "{\"items\":[{\"title\":\"c1\"},{\"title\":\"c2\"},{\"title\":\"c3\"}]}";
#define GF(n) "{\"type\":\"Feature\",\"geometry\":null,\"properties\":{\"name\":\"" n "\"}}"
  static const char *G3x = "{\"type\":\"FeatureCollection\",\"exceededTransferLimit\":true,"
                           "\"features\":[" GF("x1") "," GF("x2") "," GF("x3") "]}";
  static const char *G3y = "{\"type\":\"FeatureCollection\",\"exceededTransferLimit\":true,"
                           "\"features\":[" GF("y1") "," GF("y2") "," GF("y3") "]}";
  static const char *G3z = "{\"type\":\"FeatureCollection\",\"exceededTransferLimit\":true,"
                           "\"features\":[" GF("z1") "," GF("z2") "," GF("z3") "]}";

  printf("[4] one declared-total reader: a bare `count` above the page is a total\n");
  {
    /* Django REST Framework's envelope (Spaceflight News: count 36,457 beside
     * 100). pw_walk excluded bare `count` outright, so two collectors whose
     * comments say they "disclose against count" disclosed nothing. */
    script_t sc = { .body = { "{\"count\":110000,\"items\":[{},{},{}]}" }, .n_body = 1 };
    cap_t cap; run("https://x/api?limit=3", &sc, &cap);
    ok(cap.notices == 1 && prop_num(cap.last_notice_props, "records_available") == 110000,
       "pw_walk reads a DRF `count` larger than the page as the upstream's total");
  }
  {
    script_t sc = { .body = { "{\"odata.count\":500,\"items\":[{},{},{}]}" }, .n_body = 1 };
    cap_t cap; run("https://x/api?rows=3", &sc, &cap);
    ok(prop_num(cap.last_notice_props, "records_available") == 500,
       "pw_walk reads OData's `odata.count` (jsonlist's spelling it lacked)");
  }
  {
    script_t sc = { .body = { "{\"count\":3,\"items\":[{},{},{}]}" }, .n_body = 1 };
    cap_t cap; run("https://x/api?limit=3", &sc, &cap);
    ok(cap.notices == 1 && prop_num(cap.last_notice_props, "records_available") == -12345,
       "control: a `count` equal to the page is NOT claimed as the total");
  }
#ifndef PW_TEST_OLD_API
  {
    char basis[48];
    cJSON *a = cJSON_Parse("{\"count\":20,\"items\":[]}");
    cJSON *b = cJSON_Parse("{\"total\":12.5}");
    cJSON *c = cJSON_Parse("{\"Total\":40}");
    cJSON *d = cJSON_Parse("{\"d\":{\"__count\":\"46144\"}}");
    cJSON *e = cJSON_Parse("{\"total\":7,\"count\":99}");
    ok(jo_declared_total(a, 20, basis, sizeof basis) == -1 &&
       jo_declared_total(a, 25, basis, sizeof basis) == -1,
       "a bare `count` at or below the page's own size is not a total (the dangerous direction)");
    ok(jo_declared_total(a, 3, basis, sizeof basis) == 20 && !strcmp(basis, "count"),
       "a bare `count` above the page is, and the basis names the key");
    ok(jo_declared_total(a, -1, NULL, 0) == -1,
       "an unknown page size never reads a bare `count`");
    ok(jo_declared_total(a, JO_TOTAL_TRUST_BARE_COUNT, NULL, 0) == 20,
       "the legacy rule hpengine's page_walk rows were measured under is kept");
    ok(jo_declared_total(b, 3, NULL, 0) == -1, "a fractional `total` is not a record count");
    ok(jo_declared_total(c, 3, NULL, 0) == 40, "matching is case-insensitive, as jsonlist's was");
    ok(jo_declared_total(d, 3, basis, sizeof basis) == 46144 && !strcmp(basis, "d.__count"),
       "OData v2's string `d.__count`");
    ok(jo_declared_total(e, 3, basis, sizeof basis) == 7 && !strcmp(basis, "total"),
       "an unambiguous spelling wins over a bare `count`");
    cJSON_Delete(a); cJSON_Delete(b); cJSON_Delete(c); cJSON_Delete(d); cJSON_Delete(e);
  }
#endif

  printf("[5] geojson discloses a walk that ended short of the declared total\n");
  {
    /* numberMatched 9, startIndex advanced (mechanism 3), and the server
     * re-serves page 1: the repeat guard ends the walk before `truncated` or
     * `full_unadvanced` can be set, so it used to file NOTHING. */
    static const char *G9 = "{\"type\":\"FeatureCollection\",\"numberMatched\":9,"
                            "\"features\":[" GF("m1") "," GF("m2") "," GF("m3") "]}";
    script_t sc = { .body = { G9, G9, G9 }, .n_body = 3, .repeat_body = 1 };
    cap_t cap; int n = run_gj("https://x/wfs?startIndex=0", &sc, &cap);
    ok(sc.calls == 2, "the re-served page 2 ended the walk");
    ok(cap.notices == 1 && prop_num(cap.last_notice_props, "records_available") == 9,
       "and the 6 the upstream says it holds beyond them are disclosed");
    ok(n == 3 && prop_num(cap.last_notice_props, "records_used") == 3,
       "the re-served page is not re-emitted or counted (jsonlist's 2026-09-05 rule)");
  }
  {
    static const char *G3n = "{\"type\":\"FeatureCollection\",\"numberMatched\":3,"
                             "\"features\":[" GF("k1") "," GF("k2") "," GF("k3") "]}";
    script_t sc = { .body = { G3n }, .n_body = 1 };
    cap_t cap; run_gj("https://x/wfs?startIndex=0", &sc, &cap);
    ok(cap.notices == 0, "control: a walk that read the whole declared total files nothing");
  }

  printf("[6] the upstream's own \"nothing matched\" on page 1 is an honest empty\n");
  {
    script_t sc = { .body = { NULL }, .status = { 204 }, .n_body = 1 };
    cap_t cap; int n = run("https://x/fdsnws/event/1/query?limit=3", &sc, &cap);
    ok(n == 0 && cap.notices == 0 && cap.records == 0, "pw_walk: a 204 is rc 0, no notice");
  }
  {
    script_t sc = { .body = { NULL }, .status = { FEED_ST_DECLARED_EMPTY }, .n_body = 1 };
    cap_t cap; int n = run("https://x/fdsnws/event/1/query?limit=3", &sc, &cap);
    ok(n == 0 && cap.notices == 0, "pw_walk: a 404 carrying the no-data wording is rc 0");
  }
  {
    script_t sc = { .body = { NULL }, .status = { 404 }, .n_body = 1 };
    cap_t cap; int n = run("https://x/fdsnws/event/1/query?limit=3", &sc, &cap);
    ok(n == -1, "control: a plain 404 (KOERI: no such service) is still a failure");
  }
  {
    script_t sc = { .body = { NULL }, .status = { 204 }, .n_body = 1 };
    cap_t cap; int n = run_jl("https://x/q?limit=3&offset=0", &sc, &cap);
    ok(n == 0 && cap.notices == 0, "jsonlist_emit_paged: a 204 is rc 0");
    script_t s2 = { .body = { NULL }, .status = { 404 }, .n_body = 1 };
    ok(run_jl("https://x/q?limit=3&offset=0", &s2, &cap) == -1,
       "control: jsonlist_emit_paged still fails a plain 404");
  }
  {
    script_t sc = { .body = { NULL }, .status = { FEED_ST_DECLARED_EMPTY }, .n_body = 1 };
    cap_t cap; int n = run_gj("https://x/fdsnws/event/1/query?format=geojson", &sc, &cap);
    ok(n == 0 && cap.notices == 0, "geojson_emit_paged: the no-data 404 is rc 0");
    script_t s2 = { .body = { NULL }, .status = { 404 }, .n_body = 1 };
    ok(run_gj("https://x/fdsnws/event/1/query?format=geojson", &s2, &cap) == -1,
       "control: geojson_emit_paged still fails a plain 404");
  }
#ifndef PW_TEST_OLD_API
  ok(feed_status_body_is_declared_empty(404, "Error 404: No data available for request.") &&
     feed_status_body_is_declared_empty(404, "No Matching events found") &&
     feed_status_body_is_declared_empty(204, NULL),
     "the predicate: 204, or a 404 with the service's own no-data words");
  ok(!feed_status_body_is_declared_empty(404,
       "The requested resource does not exist on this server") &&
     !feed_status_body_is_declared_empty(404, NULL) &&
     !feed_status_body_is_declared_empty(200, "no data available") &&
     !feed_status_body_is_declared_empty(500, "no matching"),
     "control: KOERI's wording, a bodiless 404 and other statuses are not empty");
  ok(feed_status_declared_empty(204) && feed_status_declared_empty(FEED_ST_DECLARED_EMPTY) &&
     !feed_status_declared_empty(404) && !feed_status_declared_empty(FEED_ST_UNKNOWN),
     "the status test the walkers read");
#endif

  printf("[7] a later page's remedy follows what it answered\n");
  {
    /* GLEIF refuses page[number]=51 with HTTP 400: a deep-paging cap, which
     * no per-host gap moves. All three walkers used to say "needs a per-host
     * minimum gap" for every status. */
    script_t sc = { .body = { P3_of3, NULL }, .status = { 0, 400 }, .n_body = 2 };
    cap_t cap; run("https://x/api?limit=3&offset=0", &sc, &cap);
    ok(prop_str_eq(cap.last_notice_props, "remedy", "narrow each request") &&
       !prop_str_eq(cap.last_notice_props, "remedy", "hostgate"),
       "pw_walk: a 400 after a full page names a deep-paging cap, not a gap");
    ok(prop_str_eq(cap.last_notice_props, "reason", "page 2 answered HTTP 400"),
       "and the reason still names the page and its status");
  }
  {
    script_t sc = { .body = { P3_of3, NULL }, .status = { 0, 429 }, .n_body = 2 };
    cap_t cap; run("https://x/api?limit=3&offset=0", &sc, &cap);
    ok(prop_str_eq(cap.last_notice_props, "remedy", "core/hostgate.c") &&
       prop_str_eq(cap.last_notice_props, "remedy", "measure") &&
       !prop_str_eq(cap.last_notice_props, "remedy", "narrow"),
       "pw_walk: a 429 names a MEASURED per-host gap (control for the 400 case)");
  }
  {
    script_t sc = { .body = { J3a, NULL }, .status = { 0, 400 }, .n_body = 2 };
    cap_t cap; run_jl("https://x/q?limit=3&offset=0", &sc, &cap);
    ok(cap.notices == 1 && prop_str_eq(cap.last_notice_props, "remedy", "narrow each request"),
       "jsonlist_emit_paged: the same 400 gets the same remedy");
  }
  {
    script_t sc = { .body = { G3x, NULL }, .status = { 0, 400 }, .n_body = 2 };
    cap_t cap; run_gj("https://x/arcgis/query?resultRecordCount=3&resultOffset=0", &sc, &cap);
    ok(cap.notices == 1 && prop_str_eq(cap.last_notice_props, "remedy", "narrow each request"),
       "geojson_emit_paged: and so does the GeoJSON walk");
  }

  printf("[2/3] the ceiling that bit is named, and $JO_PAGE_MAX reaches every walk\n");
  {
    setenv("JO_PAGE_MAX", "2", 1);
    script_t sc = { .body = { G3x, G3y, G3z, G3x }, .n_body = 4 };
    cap_t cap; run_gj("https://x/arcgis/query?resultRecordCount=3&resultOffset=0", &sc, &cap);
    ok(sc.calls == 2, "geojson_emit_paged stops at $JO_PAGE_MAX (it read only its own knob)");
    ok(prop_str_eq(cap.last_notice_props, "remedy", "$JO_PAGE_MAX") &&
       !prop_str_eq(cap.last_notice_props, "remedy", "JO_GEOJSON_PAGE_MAX"),
       "and its remedy names the knob that bit");
    script_t s2 = { .body = { J3a, J3b, J3c }, .n_body = 3 };
    run_jl("https://x/q?limit=3&offset=0", &s2, &cap);
    ok(s2.calls == 2 && prop_str_eq(cap.last_notice_props, "remedy", "$JO_PAGE_MAX: an operator set it") &&
       !prop_str_eq(cap.last_notice_props, "remedy", "raise $JO_JSONLIST_PAGE_MAX"),
       "jsonlist_emit_paged's ceiling remedy names $JO_PAGE_MAX, which wins");
    unsetenv("JO_PAGE_MAX");
    setenv("JO_GEOJSON_PAGE_MAX", "3", 1);
    script_t s3 = { .body = { G3x, G3y, G3z, G3x }, .n_body = 4 };
    run_gj("https://x/arcgis/query?resultRecordCount=3&resultOffset=0", &s3, &cap);
    ok(s3.calls == 3 && prop_str_eq(cap.last_notice_props, "remedy", "older $JO_GEOJSON_PAGE_MAX"),
       "the legacy $JO_GEOJSON_PAGE_MAX is still honoured, and named as what bit");
    unsetenv("JO_GEOJSON_PAGE_MAX");
    script_t s4 = { .body = { P3_of3, P3_of3, P3_of3 }, .n_body = 3 };
    source_ctx ctx = {0}; intel_sink sink = { &cap, cap_emit };
    memset(&cap, 0, sizeof cap);
    pw_walk_max(&ctx, &sink, "TEST", "https://x/api?limit=3&offset=0",
                script_fetch, script_emit, &s4, 2);
    ok(prop_str_eq(cap.last_notice_props, "reason", "own measured ceiling") &&
       prop_str_eq(cap.last_notice_props, "remedy", "re-measure"),
       "pw_walk_max: a source's own ceiling is named with its own remedy");
    setenv("JO_PAGE_MAX", "5", 1);
  }

  printf("[8] the GeoJSON walk resolves a relative next link\n");
  {
    script_t sc = { .body = {
      "{\"type\":\"FeatureCollection\",\"features\":[" GF("r1") "],"
      "\"links\":[{\"rel\":\"self\",\"href\":\"?startIndex=0\"},"
      "{\"rel\":\"next\",\"href\":\"?startIndex=1\"}]}",
      "{\"type\":\"FeatureCollection\",\"features\":[]}" }, .n_body = 2 };
    cap_t cap; run_gj("https://x/ogc/collections/c/items?limit=1", &sc, &cap);
    ok(sc.calls == 2 && !strcmp(sc.seen[1], "https://x/ogc/collections/c/items?startIndex=1"),
       "a \"?query\" href is resolved against the page, not fetched as written");
  }

  printf("[9] a #fragment ends the query\n");
  {
    char *a = jsonlist_query_set("https://x/a?limit=10&offset=0#sheet=2", "offset", 10);
    ok(a && !strcmp(a, "https://x/a?limit=10&offset=10#sheet=2"),
       "jsonlist_query_set keeps the fragment after the LAST parameter");
    free(a);
    a = jsonlist_query_set("https://x/a#f", "offset", 3);
    ok(a && !strcmp(a, "https://x/a?offset=3#f"),
       "and appends to the query, before the fragment");
    free(a);
    a = jsonlist_query_set("https://x/a?offset=0&limit=5", "offset", 5);
    ok(a && !strcmp(a, "https://x/a?offset=5&limit=5"), "control: a middle parameter");
    free(a);
    ok(jsonlist_query_int("https://x/a?offset=5#x", "offset") == 5,
       "jsonlist_query_int reads a value the fragment ends");
    ok(jsonlist_query_int("https://x/a#?offset=5", "offset") == -1,
       "and never reads a parameter inside the fragment");
    script_t sc = { .body = { P3_of3, P3_of3, P2_of3 }, .n_body = 3 };
    cap_t cap; run("https://x/api?limit=3&offset=0#frag", &sc, &cap);
    ok(sc.calls == 3 && strstr(sc.seen[1], "offset=3#frag") != NULL,
       "pw_walk advances an offset that a fragment follows");
  }

  printf("[10] a record path that IS one dotted key resolves\n");
  {
    cJSON *a = cJSON_Parse("{\"odata.value\":[{\"k\":1}]}");
    cJSON *b = cJSON_Parse("{\"odata\":{\"value\":[{\"k\":1},{\"k\":2}]}}");
    cJSON *ra = jsonlist_find_array(a, "odata.value");
    cJSON *rb = jsonlist_find_array(b, "odata.value");
    ok(ra && cJSON_GetArraySize(ra) == 1, "jsonlist_find_array tries the literal key");
    ok(rb && cJSON_GetArraySize(rb) == 2, "control: the split path still wins where it resolves");
    cJSON_Delete(a); cJSON_Delete(b);
  }

#ifndef PW_TEST_OLD_API
  printf("[11] pw_walk_max_resume reads a collection over a cycle\n");
  {
    char pos[512] = "x";
    script_t sc = { .body = { P3_of3, P3_of3, P3_of3, P3_of3, P3_of3, P3_of3 }, .n_body = 6 };
    cap_t cap; run_resume("https://x/api?limit=3&offset=0", NULL, pos, sizeof pos, &sc, &cap);
    ok(sc.calls == 5 && !strcmp(pos, "https://x/api?limit=3&offset=15"),
       "a ceiling stop stores the page it would have read next");
    ok(prop_str_eq(cap.last_notice_props, "next_run_resumes_at", "offset=15") &&
       prop_str_eq(cap.last_notice_props, "remedy", "over a cycle"),
       "and the notice says where the next run continues");
    char pos2[512] = "x";
    script_t s2 = { .body = { P3_of3, P2_of3 }, .n_body = 2 };
    run_resume("https://x/api?limit=3&offset=0", pos, pos2, sizeof pos2, &s2, &cap);
    ok(!strcmp(s2.seen[0], "https://x/api?limit=3&offset=15") &&
       strstr(s2.seen[1], "offset=18") != NULL,
       "the next run starts there and advances from it");
    ok(pos2[0] == 0, "reaching the end clears the position");
    char pos3[512] = "x";
    script_t s3 = { .body = { P2_of3 }, .n_body = 1 };
    run_resume("https://x/api?limit=3&offset=0", "https://evil.example/api?limit=3&offset=15",
               pos3, sizeof pos3, &s3, &cap);
    ok(!strcmp(s3.seen[0], "https://x/api?limit=3&offset=0"),
       "a stored position on another host is dropped");
    script_t s4 = { .body = { P2_of3 }, .n_body = 1 };
    run_resume("https://x/api?limit=3&offset=0", "https://X/api?limit=3&offset=3",
               pos3, sizeof pos3, &s4, &cap);
    ok(!strcmp(s4.seen[0], "https://X/api?limit=3&offset=3"),
       "control: host comparison is case-insensitive");
    char pos5[512] = "x";
    script_t s5 = { .body = { P3_of3, NULL }, .status = { 0, 500 }, .n_body = 2 };
    run_resume("https://x/api?limit=3&offset=0", NULL, pos5, sizeof pos5, &s5, &cap);
    ok(pos5[0] == 0 && prop_str_eq(cap.last_notice_props, "next_run_resumes_at", "NOT advanced"),
       "a failed page is not bookmarked, and the notice says so");
    char pos6[512] = "x";
    script_t s6 = { .body = { NULL }, .status = { 204 }, .n_body = 1 };
    ok(run_resume("https://x/api?limit=3&offset=0", NULL, pos6, sizeof pos6, &s6, &cap) == 0 &&
       pos6[0] == 0, "an honest empty first page clears it too");
    char tiny[8] = "x";
    script_t s7 = { .body = { P3_of3, P3_of3, P3_of3, P3_of3, P3_of3, P3_of3 }, .n_body = 6 };
    run_resume("https://x/api?limit=3&offset=0", NULL, tiny, sizeof tiny, &s7, &cap);
    ok(tiny[0] == 0, "a position that does not fit is not stored truncated");
  }
#endif

  printf("[12] UK Parliament's take/skip pager, in all three spellings\n");
  {
    /* 18 VJSON rows on *.parliament.uk read ONE `take=` page because PAGERS
     * had no take/skip family (2026-11-07). Each case FAILS against the
     * previous lib/jsonlist.c: the walk made one call and stopped. */
    static const char *J2d = "{\"items\":[{\"title\":\"d1\"},{\"title\":\"d2\"}]}";
    script_t sc = { .body = { J3a, J3b, J2d }, .n_body = 3 };
    cap_t cap; run_jl("https://x/api/v1/Bills?take=3&skip=0", &sc, &cap);
    ok(sc.calls == 3 && strstr(sc.seen[1], "skip=3") && strstr(sc.seen[2], "skip=6") &&
       cap.records == 8, "take/skip: a full page advances skip by take, a short page ends it");
    script_t s2 = { .body = { J3a, J3b, J2d }, .n_body = 3 };
    run_jl("https://x/data/divisions.json/search?queryParameters.take=3&queryParameters.skip=0", &s2, &cap);
    ok(s2.calls == 3 && strstr(s2.seen[1], "queryParameters.skip=3") &&
       !strstr(s2.seen[1], "&skip="),
       "queryParameters.take moves queryParameters.skip, never a bare skip");
    script_t s3 = { .body = { J3a, J2d }, .n_body = 2 };
    run_jl("https://x/EarlyDayMotions/list?parameters.take=3&parameters.skip=0", &s3, &cap);
    ok(s3.calls == 2 && strstr(s3.seen[1], "parameters.skip=3"),
       "parameters.take moves parameters.skip (the EDM API)");
    script_t s4 = { .body = { J2d }, .n_body = 1 };
    run_jl("https://x/api/v1/Bills?take=3&skip=0", &s4, &cap);
    ok(s4.calls == 1 && cap.notices == 0,
       "control: a short first page is the end of data, no second request, no notice");
  }

  printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}
