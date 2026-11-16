/* tests/unit/test_jsonstream.c — the element-boundary scanner in
 * lib/jsonstream.c, driven without a network by calling js_write() (the curl
 * write callback) with hand-chosen chunks.
 *
 * jsonstream finds the boundaries of each element of a huge top-level array by
 * tracking brace depth and string state, then hands each element to cJSON. The
 * scanner is the part that can go wrong silently: a brace inside a string, an
 * escaped quote, or a chunk boundary in the middle of any of those moves an
 * element boundary and loses or splices records while the run still says "ok".
 * Every document below is therefore fed at several chunk sizes — down to ONE
 * BYTE at a time — and must give the same records at all of them.
 *
 * Includes jsonstream.c to reach the statics; run.sh links everything except
 * obj/lib/jsonstream.o and main.o. */
#include "../../lib/jsonstream.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

#define CAPN 4096
typedef struct { char key[CAPN][64]; char title[CAPN][128]; char rtype[CAPN][48]; int n; } cap_t;
static int cap_emit(intel_sink *s, const intel_item *it) {
  cap_t *c = s->ctx;
  if (c->n < CAPN) {
    snprintf(c->key[c->n], 64, "%s", it->remote_key ? it->remote_key : "");
    snprintf(c->title[c->n], 128, "%s", it->title ? it->title : "");
    snprintf(c->rtype[c->n], 48, "%s", it->record_type ? it->record_type : "");
    c->n++;
  }
  return 1;
}

/* Feed `doc` in chunks of `chunk` bytes (chunk 0 = whole). Returns the ctx. */
static void feed(js_ctx *c, intel_sink *sink, const char *path, const char *doc, size_t chunk) {
  memset(c, 0, sizeof *c);
  c->sink = sink; c->source_id = "t"; c->record_type = "rec"; c->lang = "en";
  c->tags_json = "[]"; c->path = path;
  size_t n = strlen(doc);
  if (!chunk) chunk = n ? n : 1;
  for (size_t i = 0; i < n; i += chunk) {
    size_t k = n - i < chunk ? n - i : chunk;
    size_t r = js_write((char *)doc + i, 1, k, c);
    if (r != k) { ok(0, "js_write consumed every byte"); break; }
  }
}

static int count_for(const char *path, const char *doc, size_t chunk, long *oversize, long *malformed, int *done, cap_t *keep) {
  static cap_t cap;
  memset(&cap, 0, sizeof cap);
  intel_sink sink = { &cap, cap_emit };
  js_ctx c;
  feed(&c, &sink, path, doc, chunk);
  if (oversize) *oversize = c.oversize;
  if (malformed) *malformed = c.malformed;
  if (done) *done = c.done;
  int r = (int)c.emitted;
  if (keep) *keep = cap;
  free(c.buf);
  return r;
}

/* the same document at every chunk size must give the same count */
static void stable(const char *what, const char *path, const char *doc, int want) {
  static const size_t chunks[] = {0, 1, 2, 3, 7, 64};
  int allsame = 1, got0 = -1;
  for (size_t i = 0; i < sizeof chunks / sizeof chunks[0]; i++) {
    int g = count_for(path, doc, chunks[i], NULL, NULL, NULL, NULL);
    if (i == 0) got0 = g;
    if (g != want) allsame = 0;
  }
  char w[200]; snprintf(w, sizeof w, "%s (want %d, whole-chunk got %d, same at 1/2/3/7/64-byte chunks)", what, want, got0);
  ok(allsame, w);
}

int main(void) {
  printf("-- element boundaries\n");
  stable("flat array of three objects", NULL,
         "[{\"id\":1,\"title\":\"a\"},{\"id\":2,\"title\":\"b\"},{\"id\":3,\"title\":\"c\"}]", 3);
  stable("whitespace and newlines between elements", "",
         "[\n  {\"id\":1,\"title\":\"a\"} ,\r\n\t{\"id\":2,\"title\":\"b\"}\n]\n", 2);
  stable("braces and brackets inside strings do not move boundaries", NULL,
         "[{\"id\":1,\"title\":\"}{][ ,\"},{\"id\":2,\"title\":\"x\"}]", 2);
  stable("escaped quote inside a string", NULL,
         "[{\"id\":1,\"title\":\"he said \\\"}\\\" ok\"},{\"id\":2,\"title\":\"b\"}]", 2);
  stable("escaped backslash right before the closing quote", NULL,
         "[{\"id\":1,\"title\":\"path\\\\\"},{\"id\":2,\"title\":\"b\"}]", 2);
  stable("nested objects and arrays inside an element", NULL,
         "[{\"id\":1,\"title\":\"a\",\"x\":{\"y\":[1,2,{\"z\":[]}]}},{\"id\":2,\"title\":\"b\"}]", 2);
  stable("empty array is an honest empty", NULL, "[]", 0);
  stable("empty array with whitespace", NULL, "[ \n ]", 0);
  stable("document that is an object with no array", NULL, "{\"a\":1}", 0);
  stable("multi-byte UTF-8 inside strings", NULL,
         "[{\"id\":1,\"title\":\"東京都\"},{\"id\":2,\"title\":\"\\u6771\\u4eac\"}]", 2);

  printf("-- selecting the array by path\n");
  stable("object envelope, dotted path 'data.items'", "data.items",
         "{\"meta\":{\"n\":2},\"data\":{\"items\":[{\"id\":1,\"title\":\"a\"},{\"id\":2,\"title\":\"b\"}]}}", 2);
  stable("envelope key 'items' with whitespace around the colon", "items",
         "{ \"items\" :\n [ {\"id\":1,\"title\":\"a\"} ] }", 1);
  stable("path names a key that is not an array -> no records", "items",
         "{\"items\":{\"id\":1}}", 0);
  stable("first same-named key holds a non-array; the real one is later", "items",
         "{\"items\":3,\"x\":{\"items\":[{\"id\":9,\"title\":\"z\"}]}}", 1);
  stable("path given but document root IS an array", "items",
         "[{\"id\":1,\"title\":\"a\"}]", 0);

  printf("-- malformed and hostile elements\n");
  {
    long ov = 0, mal = 0; int done = 0;
    int n = count_for(NULL, "[{\"id\":1,\"title\":\"a\"},{\"id\":,},{\"id\":3,\"title\":\"c\"}]", 5, &ov, &mal, &done, NULL);
    ok(n == 2 && mal == 1 && done, "an unparseable element is counted malformed and skipped; neighbours survive");
    n = count_for(NULL, "[{\"id\":1,\"title\":\"a\"},{\"id\":2", 4, &ov, &mal, &done, NULL);
    ok(n == 1 && !done, "document cut mid-element: the finished record is kept, array not marked done");
    n = count_for(NULL, "[{\"id\":1,\"title\":\"a\"}", 4, &ov, &mal, &done, NULL);
    ok(n == 1 && !done, "array never closed: not done (the caller will disclose a truncation notice)");
    n = count_for(NULL, "[1,2,3]", 0, &ov, &mal, &done, NULL);
    /* Regression: a trailing scalar/string element used to take depth to -1 on the array's own ']'. */
    ok(done, "array of scalars/strings: the array closes (done) after its last element");
    {
      /* Both string elements must be SCANNED and handed to the emitter (scanned == 2) and the
       * array must close. Whether a bare string becomes a record is jsonlist_emit's rule
       * (it needs an object), not the boundary scan's, so emitted is not asserted here. */
      static cap_t cap2; memset(&cap2, 0, sizeof cap2);
      intel_sink sk = { &cap2, cap_emit };
      js_ctx c2; feed(&c2, &sk, NULL, "[\"a\",\"b\"]", 0);
      ok(c2.done && c2.scanned == 2, "array of two strings: both elements scanned (incl. the last) and the array closes");
      free(c2.buf);
      memset(&cap2, 0, sizeof cap2);
      feed(&c2, &sk, NULL, "[1,2,3]", 1);
      ok(c2.done && c2.scanned == 3, "[1,2,3] fed byte by byte: three scalars scanned, array closes");
      free(c2.buf);
    }
    n = count_for(NULL, "[{\"id\":1,\"title\":\"a\"}]garbage after the array", 3, &ov, &mal, &done, NULL);
    ok(n == 1 && done, "bytes after the closing ] are drained, not parsed");
    n = count_for(NULL, "", 0, &ov, &mal, &done, NULL);
    ok(n == 0 && !done, "empty body");
  }
  {
    /* An element over JS_MAX_ELEM must be skipped and COUNTED, not buffered without bound. */
    size_t big = JS_MAX_ELEM + 4096;
    char *doc = malloc(big + 64);
    strcpy(doc, "[{\"id\":1,\"title\":\"");
    size_t o = strlen(doc);
    memset(doc + o, 'a', big); o += big;
    strcpy(doc + o, "\"},{\"id\":2,\"title\":\"ok\"}]");
    static cap_t cap; memset(&cap, 0, sizeof cap);
    intel_sink sink = { &cap, cap_emit };
    js_ctx c; feed(&c, &sink, NULL, doc, 65536);
    ok(c.oversize >= 1, "element above the 4 MiB limit is counted as oversize");
    ok(c.len < JS_MAX_ELEM + 70000, "the window does not grow past the element limit plus one chunk");
    free(c.buf); free(doc);
  }
  {
    /* an enclosing prefix with no array must not grow the window without bound */
    size_t big = JS_MAX_PREFIX * 3;
    char *doc = malloc(big + 8);
    doc[0] = '{'; doc[1] = '"'; memset(doc + 2, 'x', big - 4); doc[big - 2] = '"'; doc[big - 1] = 0;
    static cap_t cap; memset(&cap, 0, sizeof cap);
    intel_sink sink = { &cap, cap_emit };
    js_ctx c; feed(&c, &sink, "items", doc, 65536);
    ok(c.len <= JS_MAX_PREFIX + 65536, "prefix search discards the front of the window instead of buffering 3 MiB");
    free(c.buf); free(doc);
  }

  printf("-- record content\n");
  {
    cap_t k; long ov, mal; int done;
    int n = count_for(NULL, "[{\"id\":\"A-1\",\"title\":\"first\"},{\"id\":\"A-2\",\"title\":\"second\"}]", 1, &ov, &mal, &done, &k);
    ok(n == 2 && !strcmp(k.title[0], "first") && !strcmp(k.title[1], "second"), "titles come through in order, even at one byte per write");
    ok(!strcmp(k.rtype[0], "rec"), "record_type stamped on every row");
    ok(strcmp(k.key[0], k.key[1]) != 0, "two records get distinct remote keys");
  }
  {
    /* 5,000 small records: stable across chunking and the window does not retain them */
    size_t cap = 5000 * 40 + 16; char *doc = malloc(cap); size_t o = 0;
    doc[o++] = '[';
    for (int i = 0; i < 5000; i++) o += (size_t)snprintf(doc + o, cap - o, "%s{\"id\":%d,\"title\":\"t%d\"}", i ? "," : "", i, i);
    doc[o++] = ']'; doc[o] = 0;
    static cap_t big; memset(&big, 0, sizeof big);
    intel_sink sink = { &big, cap_emit };
    js_ctx c; feed(&c, &sink, NULL, doc, 1000);
    ok(c.emitted == 5000 && c.done, "5000 records over 1000-byte chunks");
    ok(c.cap < 4 * 65536, "peak window stays small (flat memory, not O(file))");
    free(c.buf); free(doc);
  }

  printf("-- operator ceiling\n");
  {
    static cap_t cap; memset(&cap, 0, sizeof cap);
    intel_sink sink = { &cap, cap_emit };
    js_ctx c; memset(&c, 0, sizeof c);
    c.sink = &sink; c.source_id = "t"; c.record_type = "rec"; c.lang = "en"; c.tags_json = "[]";
    c.max_records = 2;
    const char *doc = "[{\"id\":1,\"title\":\"a\"},{\"id\":2,\"title\":\"b\"},{\"id\":3,\"title\":\"c\"},{\"id\":4,\"title\":\"d\"}]";
    js_write((char *)doc, 1, strlen(doc), &c);
    ok(c.emitted == 2 && c.hit_ceiling && c.done, "max_records stops the walk and flags hit_ceiling (so a notice is filed)");
    free(c.buf);
  }

  printf("-- argument refusals\n");
  {
    static cap_t cap; intel_sink sink = { &cap, cap_emit };
    ok(jsonstream_emit(NULL, "t", "http://127.0.0.1/x", 1000, NULL, "r", "en", "[]") == -1, "NULL sink -> -1");
    ok(jsonstream_emit(&sink, "t", NULL, 1000, NULL, "r", "en", "[]") == -1, "NULL url -> -1");
    ok(jsonstream_emit(&sink, "t", "", 1000, NULL, "r", "en", "[]") == -1, "empty url -> -1");
    ok(jsonstream_emit(&sink, "t", "http://127.0.0.1:1/x", 1000, NULL, "r", "en", "[]") == -1, "loopback destination refused by hostgate -> -1, never fetched");
    ok(jsonstream_emit(&sink, "t", "http://169.254.169.254/latest/meta-data", 1000, NULL, "r", "en", "[]") == -1, "cloud metadata address refused -> -1");
  }

  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
