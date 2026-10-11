/* test_fts_threads.c — per-thread MeCab taggers segment exactly what the one
 * global, mutex-guarded tagger did, and they do it concurrently.
 *
 * core/fts.c used to run every segmentation in the process through one
 * mecab_t behind one mutex. It now shares the dictionary model and gives each
 * thread its own tagger + lattice. Holds:
 *   1. fts_segment() and fts_reading() output is byte-identical to the old
 *      mecab_new2 + mecab_sparse_tostr / sparse_tonode path (reimplemented
 *      here as the reference), Japanese and Latin alike;
 *   2. 8 threads x 400 segmentations each produce the reference output every
 *      time — no torn lattice, no shared state;
 *   3. a thread that exits releases its tagger (the key destructor runs; TSan/
 *      ASan runs of this test are what see a leak or a race).
 * Skips (exit 0) when MeCab or its dictionary is not installed, which is the
 * fail-open case fts.c itself handles. */
#include "../../core/fts.c"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SAMPLES[] = {
  "東京都千代田区の観測点",
  "気象庁が発表した観測データ。地点番号 12、東京都千代田区大手町一丁目。",
  "ＮＴＴドコモ株式会社は新しいサービスを発表した",
  "山田太郎",
  "Observation station 42 — hourly values",
  "すもももももももものうち",
};
#define NS (sizeof SAMPLES / sizeof *SAMPLES)

static char *REF_SEG[NS], *REF_READ[NS];

/* The pre-change implementation, verbatim in substance. */
static char *old_segment(mecab_t *m, const char *raw) {
  char *text = jpnorm_fold_dup(raw);
  if (!fts_has_japanese(text)) return text;
  const char *out = mecab_sparse_tostr(m, text);
  char *res = strdup(out);
  free(text);
  size_t n = strlen(res);
  while (n > 0 && (res[n - 1] == ' ' || res[n - 1] == '\n' || res[n - 1] == '\r'))
    res[--n] = '\0';
  return res;
}

static char *old_reading(mecab_t *m, const char *raw) {
  char *text = jpnorm_compat_dup(raw);
  if (!fts_has_japanese(text)) return text;
  const mecab_node_t *node = mecab_sparse_tonode(m, text);
  size_t cap = strlen(text) * 3 + 16, n = 0;
  char *out = malloc(cap);
  for (; node; node = node->next) {
    if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) continue;
    size_t rl = 0;
    const char *r = node->feature ? feature_field(node->feature, 7, &rl) : NULL;
    if (!r) { r = node->surface; rl = node->length; }
    if (!rl) continue;
    if (n + rl + 2 > cap) { cap = (n + rl + 2) * 2; out = realloc(out, cap); }
    if (n) out[n++] = ' ';
    memcpy(out + n, r, rl); n += rl;
  }
  out[n] = 0;
  free(text);
  return out;
}

static void *worker(void *arg) {
  long bad = 0;
  for (int i = 0; i < 400; i++) {
    size_t k = (size_t)(i + (long)arg) % NS;
    char *s = fts_segment(SAMPLES[k]);
    if (strcmp(s, REF_SEG[k]) != 0) bad++;
    free(s);
    if (i % 8 == 0) {
      char *r = fts_reading(SAMPLES[k]);
      if (strcmp(r, REF_READ[k]) != 0) bad++;
      free(r);
    }
  }
  return (void *)bad;
}

int main(void) {
  const char *dic = getenv("MECAB_IPADIC");
  char a1[512], a2[512];
  snprintf(a1, sizeof a1, "-O wakati%s%s", dic && *dic ? " -d " : "", dic && *dic ? dic : "");
  snprintf(a2, sizeof a2, "%s%s", dic && *dic ? "-d " : "", dic && *dic ? dic : "");
  mecab_t *mw = mecab_new2(a1), *mf = mecab_new2(a2);
  if (!mw || !mf) {
    printf("  MeCab unavailable — fail-open path only, skipped\n\nall passed\n");
    return 0;
  }
  for (size_t k = 0; k < NS; k++) {
    REF_SEG[k] = old_segment(mw, SAMPLES[k]);
    REF_READ[k] = old_reading(mf, SAMPLES[k]);
    char *s = fts_segment(SAMPLES[k]);
    char *r = fts_reading(SAMPLES[k]);
    if (strcmp(s, REF_SEG[k]) || strcmp(r, REF_READ[k]))
      fprintf(stderr, "MISMATCH %s\n  seg old=[%s] new=[%s]\n  read old=[%s] new=[%s]\n",
              SAMPLES[k], REF_SEG[k], s, REF_READ[k], r);
    assert(strcmp(s, REF_SEG[k]) == 0 && "segmentation must be byte-identical");
    assert(strcmp(r, REF_READ[k]) == 0 && "readings must be byte-identical");
    free(s); free(r);
  }
  mecab_destroy(mw); mecab_destroy(mf);
  printf("  per-thread tagger output == the old global tagger's: ok\n");

  pthread_t th[8];
  for (long t = 0; t < 8; t++) assert(pthread_create(&th[t], NULL, worker, (void *)t) == 0);
  long bad = 0;
  for (int t = 0; t < 8; t++) { void *r; pthread_join(th[t], &r); bad += (long)r; }
  assert(bad == 0 && "concurrent segmentation must match the reference every time");
  printf("  8 threads x 400 segmentations, all identical to the reference: ok\n");

  for (size_t k = 0; k < NS; k++) { free(REF_SEG[k]); free(REF_READ[k]); }
  printf("\nall passed\n");
  return 0;
}
