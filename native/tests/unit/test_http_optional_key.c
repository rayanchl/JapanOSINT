/* test_http_optional_key.c — $OPENALEX_API_KEY is appended on api.openalex.org
 * and nowhere else, and nothing changes while it is unset.
 *
 * The API's own notice (2026-11-16) names the parameter: "add
 * api_key=YOUR_KEY to your requests". What this pins is everything AROUND
 * that: the key must not reach another host (a look-alike, a userinfo trick, a
 * plain-http downgrade), must not be added twice, and must not be built from a
 * value that could smuggle a second parameter. */
#include "../../core/httpclient.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect(const char *url, const char *want) {
  char *got = http_optional_key_url(url);
  if (want) { assert(got && strcmp(got, want) == 0); }
  else assert(got == NULL);
  free(got);
}

int main(void) {
  unsetenv("OPENALEX_API_KEY");
  expect("https://api.openalex.org/works?per-page=1", NULL);        /* unset: untouched */

  setenv("OPENALEX_API_KEY", "abc-123_X", 1);
  expect("https://api.openalex.org/works?per-page=1",
         "https://api.openalex.org/works?per-page=1&api_key=abc-123_X");
  expect("https://api.openalex.org/works",
         "https://api.openalex.org/works?api_key=abc-123_X");
  expect("https://api.openalex.org/works?x=1#frag",
         "https://api.openalex.org/works?x=1&api_key=abc-123_X#frag");
  /* controls: the key goes nowhere else */
  expect("https://api.openalex.org.evil.example/works", NULL);
  expect("https://evil.example/?u=https://api.openalex.org/works", NULL);
  expect("https://user@evil.example/://api.openalex.org/", NULL);
  expect("http://api.openalex.org/works", NULL);                    /* no key over plain http */
  expect("https://api.crossref.org/works", NULL);
  expect("https://api.openalex.org/works?api_key=other", NULL);     /* never twice */

  setenv("OPENALEX_API_KEY", "abc&mailto=x", 1);                    /* not a key */
  expect("https://api.openalex.org/works", NULL);
  setenv("OPENALEX_API_KEY", "", 1);
  expect("https://api.openalex.org/works", NULL);

  printf("test_http_optional_key: all ok\n");
  return 0;
}
