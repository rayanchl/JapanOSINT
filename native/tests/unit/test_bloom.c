/* tests/unit/test_bloom.c — lib/bloom.c, the breach-ingest dedup prefilter.
 *
 * A bloom hit means "already seen" and the row is SKIPPED, so the two
 * properties that matter are (1) no false negatives, ever, including across a
 * save/load round trip, and (2) saturation is reported, because a saturated
 * filter silently discards real rows. Also pins the loader's refusal of
 * malformed files and the v1 on-disk format that carried no counters. */
#include "../../lib/bloom.c"
#include <assert.h>
#include <unistd.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

static void wr(const char *path, const void *p, size_t n) {
  FILE *f = fopen(path, "wb"); assert(f); fwrite(p, 1, n, f); fclose(f);
}

int main(void) {
  char path[256];
  snprintf(path, sizeof path, "%s/jo-bloom-%d.bin", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());

  printf("-- membership\n");
  bloom *b = bloom_new(10000, 0.001);
  ok(b != NULL, "bloom_new(10000, 0.001)");
  ok(bloom_capacity(b) == 10000 && bloom_count(b) == 0, "capacity recorded, count starts at 0");
  ok(b->k >= 8 && b->k <= 12, "k for p=0.001 is ~10");
  char key[32];
  for (int i = 0; i < 10000; i++) { snprintf(key, sizeof key, "row-%d", i); bloom_add(b, key, strlen(key)); }
  int fn = 0;
  for (int i = 0; i < 10000; i++) { snprintf(key, sizeof key, "row-%d", i); if (!bloom_maybe(b, key, strlen(key))) fn++; }
  ok(fn == 0, "no false negatives over 10000 inserted keys");
  int fp = 0;
  for (int i = 0; i < 20000; i++) { snprintf(key, sizeof key, "other-%d", i); if (bloom_maybe(b, key, strlen(key))) fp++; }
  ok(fp < 20000 * 0.005, "false-positive rate at design capacity is within 5x of the 0.1% target");
  ok(bloom_count(b) == 10000 && !bloom_saturated(b), "at capacity is not yet saturated");
  double est = bloom_fp_estimate(b);
  ok(est > 0.0003 && est < 0.003, "fp estimate at capacity ~ target");
  bloom_add(b, "one-more", 8);
  ok(bloom_saturated(b), "one insert past capacity reports saturated");
  ok(bloom_count(b) == 10001, "count tracks adds (including duplicates)");

  printf("-- binary keys and the empty key\n");
  bloom *c = bloom_new(100, 0.01);
  const unsigned char bin1[4] = {0, 1, 0, 2}, bin2[4] = {0, 1, 0, 3};
  bloom_add(c, bin1, 4);
  ok(bloom_maybe(c, bin1, 4), "key with embedded NULs found");
  ok(!bloom_maybe(c, bin2, 4), "key differing after a NUL is not conflated (length-based, not strlen)");
  ok(!bloom_maybe(c, "", 0), "empty key not present before insert");
  bloom_add(c, "", 0);
  ok(bloom_maybe(c, "", 0), "empty key present after insert");
  bloom_free(c);

  printf("-- sizing guards\n");
  c = bloom_new(0, 0.001);
  ok(c && bloom_capacity(c) == 1, "expected_items=0 clamps to 1");
  bloom_free(c);
  c = bloom_new(1000, 5.0);
  ok(c && c->k >= 8, "fp_rate >= 1 falls back to 0.001");
  bloom_free(c);
  c = bloom_new(1000, -1.0);
  ok(c && c->k >= 8, "negative fp_rate falls back to 0.001");
  bloom_free(c);
  c = bloom_new(1000, 1e-300);
  ok(c && c->k <= 32, "absurdly small fp rate: k capped at 32 so positions[] cannot overflow");
  bloom_add(c, "x", 1); ok(bloom_maybe(c, "x", 1), "capped-k filter still works");
  bloom_free(c);
  ok(bloom_capacity(NULL) == 0 && bloom_count(NULL) == 0 && !bloom_saturated(NULL) && bloom_fp_estimate(NULL) == 1.0,
     "NULL accessors are safe");
  bloom_free(NULL);

  printf("-- persistence\n");
  ok(bloom_save(b, path) == 0, "save");
  bloom *l = bloom_load(path);
  ok(l && l->m == b->m && l->k == b->k && bloom_count(l) == 10001 && bloom_capacity(l) == 10000,
     "load restores geometry and counters");
  ok(l && bloom_saturated(l), "saturation survives the round trip");
  fn = 0;
  for (int i = 0; i < 10000; i++) { snprintf(key, sizeof key, "row-%d", i); if (!bloom_maybe(l, key, strlen(key))) fn++; }
  ok(fn == 0, "no false negatives after reload");
  bloom_free(l);
  ok(bloom_save(b, "/nonexistent-dir/x.bin") == -1, "save to unwritable path -> -1");
  ok(bloom_load("/nonexistent-dir/x.bin") == NULL, "load of missing file -> NULL");

  printf("-- malformed files\n");
  unsigned char hdr[8 + 8 * 4 + 16];
  memset(hdr, 0, sizeof hdr);
  wr(path, hdr, 0);
  ok(bloom_load(path) == NULL, "empty file");
  memcpy(hdr, "JBLM2", 5);
  wr(path, hdr, 8);
  ok(bloom_load(path) == NULL, "magic only");
  unsigned long long m = 1024, k = 7, nd = 100, cnt = 3;
  memcpy(hdr + 8, &m, 8); memcpy(hdr + 16, &k, 8); memcpy(hdr + 24, &nd, 8); memcpy(hdr + 32, &cnt, 8);
  wr(path, hdr, 40);
  ok(bloom_load(path) == NULL, "valid header but bit array truncated to nothing");
  { unsigned char full[40 + 128]; memset(full, 0, sizeof full); memcpy(full, hdr, 40);
    wr(path, full, 40 + 127); ok(bloom_load(path) == NULL, "bit array one byte short");
    wr(path, full, 40 + 128); l = bloom_load(path); ok(l && l->m == 1024 && bloom_count(l) == 3, "exact-size file loads");
    bloom_free(l);
    k = 0; memcpy(full + 16, &k, 8); wr(path, full, sizeof full); ok(bloom_load(path) == NULL, "k = 0 refused (would divide by zero in positions)");
    k = 33; memcpy(full + 16, &k, 8); wr(path, full, sizeof full); ok(bloom_load(path) == NULL, "k = 33 refused (positions[32] overflow)");
    k = 7; m = 4; memcpy(full + 16, &k, 8); memcpy(full + 8, &m, 8); wr(path, full, sizeof full); ok(bloom_load(path) == NULL, "m < 8 refused");
    m = 0xFFFFFFFFFFFFFFFFULL; memcpy(full + 8, &m, 8); wr(path, full, sizeof full);
    ok(bloom_load(path) == NULL, "m = 2^64-1 (ceil(m/8) wraps to 0) refused, not accepted with a 0-byte array");
    m = 1024; memcpy(full + 8, &m, 8); memcpy(full, "JBLM9", 5); wr(path, full, sizeof full);
    ok(bloom_load(path) == NULL, "unknown version digit");
    memcpy(full, "XXXX2", 5); wr(path, full, sizeof full);
    ok(bloom_load(path) == NULL, "wrong magic");
  }
  /* v1: magic, m, k, bits — no counters. n_design is back-derived. */
  { unsigned char v1[8 + 16 + 128]; memset(v1, 0, sizeof v1); memcpy(v1, "JBLM1", 5);
    unsigned long long m1 = 1024, k1 = 7; memcpy(v1 + 8, &m1, 8); memcpy(v1 + 16, &k1, 8);
    wr(path, v1, sizeof v1); l = bloom_load(path);
    ok(l && bloom_count(l) == 0 && bloom_capacity(l) == (unsigned long long)(1024 * M_LN2 / 7) && !bloom_saturated(l),
       "v1 file loads: count 0, capacity back-derived from m*ln2/k, unsaturated");
    bloom_free(l); }

  unlink(path);
  bloom_free(b);
  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
