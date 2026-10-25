/* test_fts_segment_body.c — a record body is indexed as text, never as JSON keys.
 *
 * 205 hand-written collectors pass the record's own JSON as `body`
 * (2026-10-25). fts_segment_body() is what the sink segments a body with,
 * for the FTS index and the simhash fingerprint alike. Holds:
 *   1. a JSON object body is segmented as its VALUES: no key reaches the index;
 *   2. nested values and numbers are kept, true/false/null are not words;
 *   3. a JSON array body is its elements' values;
 *   4. a body that is not JSON is segmented exactly as fts_segment() does —
 *      including text that only STARTS like JSON;
 *   5. NULL is the empty string, not a crash.
 * fts_segment() folds case and width before indexing, so the words are
 * checked in folded (lower) case. */
#include "../../core/fts_schema.c"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int has_word(const char *s, const char *w) {
  size_t n = strlen(w);
  for (const char *p = s; (p = strstr(p, w)); p += n) {
    int start = (p == s) || p[-1] == ' ';
    int end = p[n] == 0 || p[n] == ' ';
    if (start && end) return 1;
  }
  return 0;
}

int main(void) {
  /* 1. values, not keys */
  char *a = fts_segment_body("{\"dba_name\":\"Acme Bakery\",\"city\":\"Oakland\"}");
  assert(has_word(a, "acme") && has_word(a, "bakery") && has_word(a, "oakland"));
  assert(!strstr(a, "dba_name") && !has_word(a, "city"));
  free(a);

  /* 2. nested values and numbers kept; booleans and null are not words */
  char *b = fts_segment_body("{\"owner\":{\"name\":\"Kowalski\"},\"units\":12,"
                             "\"active\":true,\"closed\":null}");
  assert(has_word(b, "kowalski") && has_word(b, "12"));
  assert(!has_word(b, "true") && !has_word(b, "null") && !strstr(b, "owner"));
  free(b);

  /* 3. an array body */
  char *c = fts_segment_body(" [ {\"t\":\"first\"}, {\"t\":\"second\"} ] ");
  assert(has_word(c, "first") && has_word(c, "second") && !has_word(c, "t"));
  free(c);

  /* 4. not JSON: exactly fts_segment(), including JSON-looking prose */
  const char *plain[] = { "Bakery licence renewed in Oakland",
                          "{not json at all, just braces}",
                          "[draft] minutes of the meeting", NULL };
  for (int i = 0; plain[i]; i++) {
    char *x = fts_segment_body(plain[i]);
    char *y = fts_segment(plain[i]);
    assert(strcmp(x, y) == 0);
    free(x); free(y);
  }

  /* 5. NULL */
  char *d = fts_segment_body(NULL);
  assert(d && d[0] == 0);
  free(d);

  printf("fts_segment_body: values not keys; non-JSON unchanged; NULL safe — ok\n");
  return 0;
}
