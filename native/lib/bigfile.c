#include "bigfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BF_MAXLINE 65536

struct bigfile {
  FILE *f;
  char buf[BF_MAXLINE];
  unsigned long long lines, skipped;
};

bigfile *bigfile_open(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  bigfile *bf = calloc(1, sizeof *bf);
  if (!bf) { fclose(f); return NULL; }
  bf->f = f;
  return bf;
}

const char *bigfile_next(bigfile *bf, size_t *len) {
  for (;;) {
    if (!fgets(bf->buf, sizeof bf->buf, bf->f)) return NULL;
    size_t n = strlen(bf->buf);
    int complete = (n > 0 && bf->buf[n - 1] == '\n');
    if (!complete && !feof(bf->f)) {
      /* Line exceeds BF_MAXLINE: discard the remainder and skip it. */
      int c;
      while ((c = fgetc(bf->f)) != EOF && c != '\n') { /* drain */ }
      bf->skipped++;
      continue;
    }
    while (n > 0 && (bf->buf[n - 1] == '\n' || bf->buf[n - 1] == '\r'))
      bf->buf[--n] = '\0';
    bf->lines++;
    if (len) *len = n;
    return bf->buf;
  }
}

void bigfile_stats(const bigfile *bf, unsigned long long *lines,
                   unsigned long long *skipped) {
  if (lines) *lines = bf->lines;
  if (skipped) *skipped = bf->skipped;
}

void bigfile_close(bigfile *bf) {
  if (!bf) return;
  if (bf->f) fclose(bf->f);
  free(bf);
}
