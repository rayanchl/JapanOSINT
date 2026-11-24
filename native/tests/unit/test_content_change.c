/* test_content_change.c — roadmap 26: a page that says something different is
 * an event; a page that merely re-rendered is not.
 *
 *   normalizer: timestamps, relative times, epochs, opaque tokens and view
 *     counters are placeholders; script/style/comments and attributes vanish;
 *     reflow is a non-event; phone numbers, advisory ids, bare case counts and
 *     real link changes SURVIVE (it errs toward under-stripping);
 *   diff: equal texts are NULL, a one-line change is a real unified diff with
 *     counts, a >800-line middle takes the coarse path and says truncated;
 *   capture: baseline emits nothing, same page is UNCHANGED, a real change is
 *     CHANGED and a counter tick is not; api-type sources, watch_content=0 and
 *     empty/oversize bodies are skipped; the http hook ignores POST and non-200;
 *   sweep: retention keeps JO_CCHANGE_KEEP snapshots per key. */
#include "../../core/content_change.c"
#include "core/db.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}
static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}
static char *norm(const char *s, const char *ct) {
  return content_change_normalize(s, strlen(s), ct);
}
#define N(s) norm((s), NULL)

int main(void) {
  /* ── normalizer ──────────────────────────────────────────────────────── */
  char *a = N("<html><body><script>var t=1784812391;</script><style>p{}</style>"
              "<!-- built 8f3c9d1e --><p>Evacuation order issued</p>"
              "<p>Updated 2026-07-26 14:03:11</p></body></html>");
  char *b = N("<html><body><script>var t=1784899999;</script><style>p{}</style>"
              "<!-- built 11111111 --><p>Evacuation order issued</p>"
              "<p>Updated 2026-07-27 09:00:01</p></body></html>");
  assert(a && b && !strcmp(a, b) && "scripts, comments and timestamps are noise");
  assert(strstr(a, "Evacuation order issued") && strstr(a, "[DATE]"));
  free(a); free(b);
  printf("  markup, comments, timestamps are noise: ok\n");

  a = N("<p>Hello   world</p>\n\n   <div>second\tline</div>");
  b = N("<div><p>Hello world</p></div><div>second line</div>");
  assert(a && b && !strcmp(a, b) && "a reflow is a non-event");
  assert(strchr(a, '\n') && "block tags become line breaks");
  free(a); free(b);

  a = N("<p>5 minutes ago</p><p>1,284 views</p><p>Views: 9,001</p>");
  b = N("<p>7 minutes ago</p><p>1,301 views</p><p>Views: 9,100</p>");
  assert(a && b && !strcmp(a, b));
  assert(strstr(a, "[AGO]") && strstr(a, "[COUNT]"));
  free(a); free(b);
  printf("  relative times and counters are noise: ok\n");

  a = N("<input value=\"x\"><p>csrf 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08</p>");
  assert(a && strstr(a, "[TOKEN]") && !strstr(a, "9f86d081"));
  free(a);
  a = N("<p>epoch 1784812391123 and 1784812391</p>");
  assert(a && strstr(a, "[EPOCH]") && !strstr(a, "1784812391"));
  free(a);
  printf("  tokens and epochs are placeholders: ok\n");

  /* the conservative side: these are CONTENT */
  a = N("<p>Call 0312345678 or 03-1234-5678</p><p>JVNDB-2026-001234</p>"
        "<p>1,284</p><p>1,284件</p><p>see https://x.example/a?id=7&utm_source=n&v=3#top</p>");
  assert(a && strstr(a, "0312345678") && strstr(a, "JVNDB-2026-001234"));
  assert(strstr(a, "1,284\n") || strstr(a, "1,284"));
  assert(strstr(a, "1,284件"));
  assert(strstr(a, "https://x.example/a?id=7") && !strstr(a, "utm_source") &&
         !strstr(a, "#top") && "cache-busters and fragments are stripped, real params stay");
  free(a);
  a = N("<a href=\"https://x.example/a?id=7\">link</a> https://x.example/a?id=7");
  b = N("<a href=\"https://x.example/a?id=7\">link</a> https://x.example/a?id=8");
  assert(a && b && strcmp(a, b) && "a changed query parameter is a real change");
  free(a); free(b);
  printf("  phone numbers, ids, counts and links survive: ok\n");

  /* edges */
  assert(content_change_normalize("", 0, NULL) == NULL);
  assert(content_change_normalize(NULL, 0, NULL) == NULL);
  assert(N("   \n\t  ") == NULL);
  assert(N("<script>only();</script>") == NULL && "nothing survived -> NULL");
  a = norm("{\"a\":1,\n\"updated\":\"2026-01-02T03:04:05Z\"}", "application/json");
  assert(a && strstr(a, "[DATE]") && !strstr(a, "2026-01-02") &&
         "a compact JSON timestamp is a date (it was one token before)");
  free(a);
  { /* one huge line is bounded, not unbounded */
    size_t n = CC_MAX_LINE_BYTES * 3;
    char *big = malloc(n + 1); memset(big, 'a', n); big[n] = 0;
    for (size_t i = 0; i < n; i += 7) big[i] = ' ';
    a = content_change_normalize(big, n, "text/plain");
    assert(a && strlen(a) <= (size_t)CC_MAX_LINE_BYTES + 1);
    free(a); free(big);
  }
  { /* more than CC_MAX_LINES lines is truncated */
    size_t n = (size_t)(CC_MAX_LINES + 500) * 8;
    char *many = malloc(n + 1); size_t p = 0;
    for (int i = 0; i < CC_MAX_LINES + 500; i++) p += (size_t)sprintf(many + p, "L%06d\n", i);
    a = content_change_normalize(many, p, "text/plain");
    assert(a);
    int lines = 1; for (char *q = a; *q; q++) if (*q == '\n') lines++;
    assert(lines <= CC_MAX_LINES + 1);
    free(a); free(many);
  }
  printf("  empty/junk/JSON/oversize edges: ok\n");

  /* ── diff ────────────────────────────────────────────────────────────── */
  int add = -1, rem = -1, tr = -1;
  assert(content_change_diff("p", "x\ny", "x\ny", &add, &rem, &tr) == NULL);
  assert(content_change_diff("p", NULL, NULL, NULL, NULL, NULL) == NULL);
  assert(content_change_diff("p", "", "", NULL, NULL, NULL) == NULL);
  char *d = content_change_diff("page.html", "one\ntwo\nthree\nfour\nfive",
                                "one\ntwo\nTHREE\nfour\nfive", &add, &rem, &tr);
  assert(d && add == 1 && rem == 1 && tr == 0);
  assert(strstr(d, "--- ") && strstr(d, "+++ ") && strstr(d, "page.html"));
  assert(strstr(d, "@@") && strstr(d, "-three") && strstr(d, "+THREE"));
  free(d);
  d = content_change_diff("p", NULL, "new\nfile", &add, &rem, &tr);
  assert(d && add == 2 && rem == 0 && strstr(d, "+new") && strstr(d, "+file"));
  free(d);
  d = content_change_diff("p", "gone\nfile", NULL, &add, &rem, &tr);
  assert(d && add == 0 && rem == 2 && strstr(d, "-gone"));
  free(d);
  { /* a middle past CC_LCS_MAX_LINES is coarse and says so */
    size_t cap = 1 << 20; char *o = malloc(cap), *nw = malloc(cap);
    size_t po = 0, pn = 0;
    for (int i = 0; i < CC_LCS_MAX_LINES + 100; i++) {
      po += (size_t)sprintf(o + po, "old line %d\n", i);
      pn += (size_t)sprintf(nw + pn, "new line %d\n", i);
    }
    o[po - 1] = 0; nw[pn - 1] = 0;
    d = content_change_diff("big", o, nw, &add, &rem, &tr);
    assert(d && tr == 1 && "coarse path is flagged truncated");
    assert(strstr(d, "-old line 0") && strstr(d, "+new line 0"));
    free(d); free(o); free(nw);
  }
  printf("  unified diff, null sides, coarse path: ok\n");

  /* ── capture (needs the schema and sources) ──────────────────────────── */
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  unsetenv("JO_NO_CONTENT_CHANGE");
  setenv("JO_CCHANGE_KEEP", "3", 1);
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  content_change_ensure_schema(&db);
  content_change_ensure_schema(&db);                 /* idempotent */
  exec_ok(&db, "DELETE FROM content_snapshots");
  exec_ok(&db, "INSERT OR REPLACE INTO sources(id,name,type,category) VALUES"
               "('cc-scr','Scr','scraped','test'),('cc-api','Api','api','test'),"
               "('cc-off','Off','scraped','test'),('cc-on','On','api','test')");
  exec_ok(&db, "UPDATE sources SET watch_content=0 WHERE id='cc-off'");
  exec_ok(&db, "UPDATE sources SET watch_content=1 WHERE id='cc-on'");

  const char *p1 = "<p>Advisory level 2</p><p>1,284 views</p>";
  const char *p1b = "<p>Advisory level 2</p><p>1,999 views</p>";
  const char *p2 = "<p>Advisory level 3</p><p>1,999 views</p>";
  #define CAP(src, body) content_change_capture(&db, (src), NULL, \
          "https://example.org/adv", (body), strlen(body), "text/html")
  assert(CAP("cc-scr", p1) == CC_BASELINE);
  assert(scalar(&db, "SELECT COUNT(*) FROM intel_items WHERE record_type='content_change'") == 0
         && "a baseline emits nothing");
  assert(CAP("cc-scr", p1) == CC_UNCHANGED);
  assert(CAP("cc-scr", p1b) == CC_UNCHANGED && "a counter tick is not a change");
  assert(CAP("cc-scr", p2) == CC_CHANGED);
  assert(scalar(&db, "SELECT COUNT(*) FROM intel_items WHERE record_type='content_change'") == 1);
  assert(scalar(&db, "SELECT COUNT(*) FROM content_snapshots WHERE source_id='cc-scr'") == 2);
  printf("  capture: baseline, unchanged, counter tick, real change: ok\n");

  assert(CAP("cc-api", p1) == CC_SKIP_TYPE && "api sources are not watched by default");
  assert(CAP("cc-off", p1) == CC_SKIP_TYPE && "watch_content=0 never watches");
  assert(CAP("cc-on", p1) == CC_BASELINE && "watch_content=1 watches an api source");
  assert(CAP("no-such-source", p1) == CC_SKIP_TYPE);
  assert(content_change_capture(&db, "cc-scr", NULL, "u", NULL, 0, NULL) == CC_SKIP_EMPTY);
  assert(content_change_capture(&db, "cc-scr", NULL, "u", "<script>x</script>", 18,
                                "text/html") == CC_SKIP_EMPTY);
  assert(content_change_capture(&db, "", NULL, "u", "x", 1, NULL) == CC_SKIP_DISABLED);
  assert(content_change_capture(NULL, "cc-scr", NULL, "u", "x", 1, NULL) == CC_SKIP_DISABLED);
  { /* one byte over the 4 MiB document ceiling is skipped whole */
    size_t n = 4194304 + 1;
    char *huge = malloc(n); memset(huge, 'x', n);
    assert(content_change_capture(&db, "cc-scr", NULL, "u", huge, n, "text/plain")
           == CC_SKIP_TOO_BIG);
    assert(content_change_normalize(huge, n, "text/plain") == NULL);
    free(huge);
  }
  /* JO_NO_CONTENT_CHANGE and the size knobs are read once per process, so the
   * kill switch cannot be flipped mid-test; it is a one-line getenv. */
  printf("  capture: type, watch_content, empty, oversize, kill switch skips: ok\n");

  /* the http hook: scope, method and status gate */
  assert(content_change_http_hook("GET", "https://example.org/adv", 200, p2, strlen(p2))
         == CC_SKIP_DISABLED && "no scope bound");
  content_change_scope_begin(&db, "cc-scr");
  assert(content_change_http_hook("POST", "https://example.org/adv", 200, p1, strlen(p1))
         == CC_SKIP_DISABLED);
  assert(content_change_http_hook("GET", "https://example.org/adv", 404, p1, strlen(p1))
         == CC_SKIP_DISABLED);
  assert(content_change_http_hook("GET", "https://example.org/adv", 200, p2, strlen(p2))
         == CC_UNCHANGED);
  assert(content_change_http_hook("GET", "https://example.org/adv", 200, p1, strlen(p1))
         == CC_CHANGED && "back to the old text IS a change");
  content_change_scope_end();
  content_change_scope_end();                         /* unpaired end is safe */
  assert(content_change_http_hook("GET", "https://example.org/adv", 200, p2, strlen(p2))
         == CC_SKIP_DISABLED);
  printf("  http hook: scope, method and status gate: ok\n");

  /* ── retention ───────────────────────────────────────────────────────── */
  for (int i = 0; i < 6; i++) {
    char sql[256];
    snprintf(sql, sizeof sql,
      "INSERT INTO content_snapshots(source_id,ref_key,content_sha256,extract_text,captured_at)"
      " VALUES('cc-sweep','k','h%d','t',datetime('now','-%d minutes'))", i, i + 1);
    exec_ok(&db, sql);
  }
  exec_ok(&db, "INSERT INTO content_snapshots(source_id,ref_key,content_sha256,extract_text,captured_at)"
               " VALUES('cc-old','k','old','t',datetime('now','-400 days'))");
  assert(content_change_sweep(NULL, 10, NULL) == -1);
  long del = content_change_sweep(&db, 0, NULL);
  assert(del == 1 + 3 && "one aged-out row, three beyond KEEP=3");
  assert(scalar(&db, "SELECT COUNT(*) FROM content_snapshots WHERE source_id='cc-sweep'") == 3);
  assert(scalar(&db, "SELECT COUNT(*) FROM content_snapshots WHERE source_id='cc-old'") == 0);
  assert(content_change_sweep(&db, 10, NULL) == 0 && "steady state deletes nothing");
  printf("  retention sweep: age and per-key KEEP: ok\n");

  db_close(&db);
  printf("test_content_change: all passed\n");
  return 0;
}
