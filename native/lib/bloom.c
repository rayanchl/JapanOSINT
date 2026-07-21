#include "bloom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

/* On-disk: [magic "JBLM1\0\0\0"][u64 m_bits][u64 k][bytes ceil(m/8)]. */
#define BLOOM_MAGIC "JBLM1"

struct bloom {
  unsigned long long m;   /* bits */
  unsigned long long k;   /* hash count */
  unsigned char *bits;    /* ceil(m/8) bytes */
};

/* FNV-1a 64, then split into two 32-bit halves for double hashing. */
static unsigned long long fnv1a(const void *data, size_t len) {
  const unsigned char *p = data;
  unsigned long long h = 1469598103934665603ULL;
  for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}

static size_t nbytes(unsigned long long m) { return (size_t)((m + 7) / 8); }

bloom *bloom_new(unsigned long long n, double p) {
  if (n < 1) n = 1;
  if (!(p > 0.0 && p < 1.0)) p = 0.001;
  double m = ceil(-((double)n * log(p)) / (M_LN2 * M_LN2));
  double k = round((m / (double)n) * M_LN2);
  bloom *b = calloc(1, sizeof *b);
  if (!b) return NULL;
  b->m = (unsigned long long)m; if (b->m < 8) b->m = 8;
  b->k = (unsigned long long)k; if (b->k < 1) b->k = 1; if (b->k > 32) b->k = 32;
  b->bits = calloc(nbytes(b->m), 1);
  if (!b->bits) { free(b); return NULL; }
  return b;
}

static void positions(const bloom *b, const void *data, size_t len,
                      unsigned long long *out /* out[k] */) {
  unsigned long long h = fnv1a(data, len);
  unsigned long long h1 = h & 0xFFFFFFFFULL;
  unsigned long long h2 = (h >> 32) | 1; /* odd, non-zero step */
  for (unsigned long long j = 0; j < b->k; j++)
    out[j] = (h1 + j * h2) % b->m;
}

void bloom_add(bloom *b, const void *data, size_t len) {
  unsigned long long pos[32];
  positions(b, data, len, pos);
  for (unsigned long long j = 0; j < b->k; j++)
    b->bits[pos[j] >> 3] |= (unsigned char)(1u << (pos[j] & 7));
}

int bloom_maybe(const bloom *b, const void *data, size_t len) {
  unsigned long long pos[32];
  positions(b, data, len, pos);
  for (unsigned long long j = 0; j < b->k; j++)
    if (!(b->bits[pos[j] >> 3] & (unsigned char)(1u << (pos[j] & 7)))) return 0;
  return 1;
}

int bloom_save(const bloom *b, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  int ok = fwrite(BLOOM_MAGIC, 8, 1, f) == 1 &&
           fwrite(&b->m, sizeof b->m, 1, f) == 1 &&
           fwrite(&b->k, sizeof b->k, 1, f) == 1 &&
           fwrite(b->bits, nbytes(b->m), 1, f) == 1;
  fclose(f);
  return ok ? 0 : -1;
}

bloom *bloom_load(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  char magic[8] = {0};
  unsigned long long m = 0, k = 0;
  if (fread(magic, 8, 1, f) != 1 || memcmp(magic, BLOOM_MAGIC, 6) != 0 ||
      fread(&m, sizeof m, 1, f) != 1 || fread(&k, sizeof k, 1, f) != 1 ||
      m < 8 || k < 1 || k > 32) { fclose(f); return NULL; }
  bloom *b = calloc(1, sizeof *b);
  if (!b) { fclose(f); return NULL; }
  b->m = m; b->k = k;
  b->bits = calloc(nbytes(m), 1);
  if (!b->bits || fread(b->bits, nbytes(m), 1, f) != 1) {
    fclose(f); free(b->bits); free(b); return NULL;
  }
  fclose(f);
  return b;
}

void bloom_free(bloom *b) {
  if (!b) return;
  free(b->bits);
  free(b);
}
