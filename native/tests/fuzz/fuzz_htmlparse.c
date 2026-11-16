/* libFuzzer harness: lib/htmlparse.c — the anchor scanner, the table reader,
 * the tag/attr helpers and html_strip, on arbitrary markup. */
#include "../../lib/htmlparse.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *html = malloc(size + 1);
  if (!html) return 0;
  memcpy(html, data, size);
  html[size] = 0;

  html_anchor a;
  const char *p = html;
  int guard = 0;
  while (p && *p && guard++ < 100000) {
    p = html_anchor_next(p, &a);
    if (!p) break;
  }

  html_table t;
  for (int idx = 0; idx < 2; idx++) {
    memset(&t, 0, sizeof t);
    if (html_table_parse(html, idx ? NULL : "", idx, &t)) html_table_free(&t);
  }

  char out[512];
  (void)html_tag(html, "title", out, sizeof out);
  (void)html_attr(html, "href", out, sizeof out);
  char *s = html_strip(html);
  free(s);
  free(html);
  return 0;
}
