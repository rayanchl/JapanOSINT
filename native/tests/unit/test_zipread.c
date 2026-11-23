/* tests/unit/test_zipread.c — lib/zipread.c on hand-built archives.
 *
 * zip_* parses bytes an upstream (or an uploader) chose: central-directory
 * offsets, 32-bit sizes, names, deflate streams. Each case builds the archive
 * in memory with zlib's raw deflate and then breaks one field at a time. The
 * contract being pinned: never read out of bounds, never trust a declared size,
 * and return NULL (not a short/garbage buffer) on any malformity. Run under
 * ASan/UBSan (make asan) for the out-of-bounds half to bite. */
#include "../../lib/zipread.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

typedef struct { unsigned char *b; size_t n, cap; } buf_t;
static void put(buf_t *o, const void *p, size_t n) {
  if (o->n + n > o->cap) { o->cap = (o->n + n) * 2 + 64; o->b = realloc(o->b, o->cap); }
  memcpy(o->b + o->n, p, n); o->n += n;
}
static void p16(buf_t *o, unsigned v) { unsigned char c[2] = {v & 255, (v >> 8) & 255}; put(o, c, 2); }
static void p32(buf_t *o, unsigned long v) {
  unsigned char c[4] = {v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >> 24) & 255}; put(o, c, 4);
}
static void set32(unsigned char *b, size_t at, unsigned long v) {
  b[at] = v & 255; b[at+1] = (v >> 8) & 255; b[at+2] = (v >> 16) & 255; b[at+3] = (v >> 24) & 255;
}
static void set16(unsigned char *b, size_t at, unsigned v) { b[at] = v & 255; b[at+1] = (v >> 8) & 255; }

typedef struct { const char *name; const char *data; size_t dlen; int method; } ent_t;

/* Build a ZIP; cdpos[]/lhpos[] receive each entry's central / local header
 * offset so tests can corrupt a specific field. */
static buf_t build(const ent_t *e, int n, size_t *cdoff, size_t *cdpos, size_t *lhpos) {
  buf_t o = {0};
  unsigned long clens[16];
  for (int i = 0; i < n; i++) {
    unsigned char *comp = NULL; size_t clen = e[i].dlen;
    if (e[i].method == 8) {
      z_stream s; memset(&s, 0, sizeof s);
      deflateInit2(&s, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
      clen = deflateBound(&s, e[i].dlen) + 16;
      comp = malloc(clen);
      s.next_in = (Bytef *)e[i].data; s.avail_in = (uInt)e[i].dlen;
      s.next_out = comp; s.avail_out = (uInt)clen;
      deflate(&s, Z_FINISH); clen = s.total_out; deflateEnd(&s);
    }
    clens[i] = clen; lhpos[i] = o.n;
    p32(&o, 0x04034b50UL); p16(&o, 20); p16(&o, 0); p16(&o, e[i].method);
    p32(&o, 0); p32(&o, 0);
    p32(&o, clen); p32(&o, e[i].dlen);
    p16(&o, strlen(e[i].name)); p16(&o, 0);
    put(&o, e[i].name, strlen(e[i].name));
    put(&o, comp ? (void *)comp : (void *)e[i].data, clen);
    free(comp);
  }
  *cdoff = o.n;
  for (int i = 0; i < n; i++) {
    cdpos[i] = o.n;
    p32(&o, 0x02014b50UL); p16(&o, 20); p16(&o, 20); p16(&o, 0); p16(&o, e[i].method);
    p32(&o, 0); p32(&o, 0);
    p32(&o, clens[i]); p32(&o, e[i].dlen);
    p16(&o, strlen(e[i].name)); p16(&o, 0); p16(&o, 0); p16(&o, 0); p16(&o, 0);
    p32(&o, 0); p32(&o, lhpos[i]);
    put(&o, e[i].name, strlen(e[i].name));
  }
  size_t cdlen = o.n - *cdoff;
  p32(&o, 0x06054b50UL); p16(&o, 0); p16(&o, 0); p16(&o, n); p16(&o, n);
  p32(&o, cdlen); p32(&o, *cdoff); p16(&o, 0);
  return o;
}

int main(void) {
  const char *csv = "a,b,c\n1,2,3\n";
  static char big[20000]; for (size_t i = 0; i < sizeof big; i++) big[i] = "abcdefgh"[(i * 7 + i / 13) % 8];
  ent_t es[3] = {
    {"feed/stop_times.txt", csv, strlen(csv), 8},
    {"readme.txt", "hello", 5, 0},
    {"big.bin", big, sizeof big, 8},
  };
  size_t cdoff, cdpos[3], lhpos[3];
  buf_t z = build(es, 3, &cdoff, cdpos, lhpos);
  size_t n;
  char nm[64];
  char *r;

  printf("-- happy path\n");
  r = zip_first_entry((char *)z.b, z.n, &n);
  ok(r && n == strlen(csv) && !memcmp(r, csv, n) && r[n] == 0, "first entry inflates, NUL-terminated");
  free(r);
  r = zip_find_entry((char *)z.b, z.n, "stop_times.txt", &n);
  ok(r && n == strlen(csv), "basename lookup resolves through the wrapping directory");
  free(r);
  r = zip_find_entry((char *)z.b, z.n, "feed/stop_times.txt", &n);
  ok(r != NULL, "full-path lookup");
  free(r);
  r = zip_find_entry((char *)z.b, z.n, "readme.txt", &n);
  ok(r && n == 5 && !strcmp(r, "hello"), "stored entry");
  free(r);
  r = zip_find_entry((char *)z.b, z.n, "big.bin", &n);
  ok(r && n == sizeof big && !memcmp(r, big, n), "multi-block deflate round-trips 20000 bytes");
  free(r);
  ok(zip_entry_count((char *)z.b, z.n) == 3, "entry count from EOCD");
  r = zip_entry_at((char *)z.b, z.n, 1, nm, sizeof nm, &n);
  ok(r && !strcmp(nm, "readme.txt"), "indexed entry reports its own path");
  free(r);
  r = zip_entry_at((char *)z.b, z.n, 3, nm, sizeof nm, &n);
  ok(r == NULL && nm[0] == 0 && n == 0, "index past the end -> NULL, name_out emptied");
  r = zip_entry_at((char *)z.b, z.n, -1, nm, sizeof nm, &n);
  ok(r == NULL, "negative index -> NULL");
  r = zip_entry_at((char *)z.b, z.n, 0, nm, 4, &n);
  ok(r && !strcmp(nm, "fee"), "name_out truncated to its buffer, not overrun");
  free(r);
  r = zip_entry_at((char *)z.b, z.n, 0, NULL, 0, &n);
  ok(r != NULL, "NULL name_out accepted");
  free(r);
  ok(zip_find_entry((char *)z.b, z.n, "nope.txt", &n) == NULL, "absent name -> NULL");
  ok(zip_find_entry((char *)z.b, z.n, NULL, &n) == NULL && n == 0, "NULL name -> NULL, out_len zeroed");

  printf("-- hostile input\n");
  ok(zip_first_entry(NULL, 0, &n) == NULL, "NULL buffer");
  ok(zip_first_entry("PK\3\4", 4, &n) == NULL, "shorter than a local header");
  ok(zip_entry_count("PK", 2) == -1, "no EOCD in a tiny body -> -1, not 1");
  ok(zip_entry_count(NULL, 10) == -1, "NULL -> -1");
  {
    static char junk[4096]; memset(junk, 'x', sizeof junk);
    ok(zip_entry_count(junk, sizeof junk) == -1, "non-ZIP body -> -1");
    ok(zip_find_entry(junk, sizeof junk, "a", &n) == NULL, "non-ZIP find -> NULL");
  }
  /* Every prefix of the archive must return without touching memory past `k`.
   * The buffer is copied to an exact-size heap block per k so ASan sees a
   * read past the logical end. */
  for (size_t k = 0; k < z.n; k += 1) {
    unsigned char *t = malloc(k ? k : 1); if (k) memcpy(t, z.b, k);
    char *a = zip_first_entry((char *)t, k, &n); free(a);
    a = zip_find_entry((char *)t, k, "big.bin", &n); free(a);
    a = zip_entry_at((char *)t, k, 2, nm, sizeof nm, &n); free(a);
    (void)zip_entry_count((char *)t, k);
    free(t);
  }
  ok(1, "every truncation of the archive handled (ASan/UBSan clean if built with them)");

  {
    unsigned char *c = malloc(z.n); size_t eocd = z.n - 22;
    memcpy(c, z.b, z.n); set32(c, cdoff, 0xdeadbeefUL);
    ok(zip_find_entry((char *)c, z.n, "readme.txt", &n) == NULL, "bad CD signature -> NULL");
    memcpy(c, z.b, z.n); set32(c, eocd + 16, (unsigned long)z.n + 1000);
    ok(zip_find_entry((char *)c, z.n, "readme.txt", &n) == NULL, "CD offset past end -> NULL");
    memcpy(c, z.b, z.n); set16(c, eocd + 10, 60000);
    ok(zip_find_entry((char *)c, z.n, "never", &n) == NULL, "entry count lying high stops at the CD end");
    ok(zip_entry_count((char *)c, z.n) == 60000, "count reports the EOCD's own number");
    memcpy(c, z.b, z.n); set32(c, cdpos[1] + 42, (unsigned long)z.n);
    ok(zip_find_entry((char *)c, z.n, "readme.txt", &n) == NULL, "local header offset outside buffer -> NULL");
    memcpy(c, z.b, z.n); set32(c, cdpos[1] + 42, 0xfffffff0UL);
    ok(zip_find_entry((char *)c, z.n, "readme.txt", &n) == NULL, "huge local header offset -> NULL");
    memcpy(c, z.b, z.n); set16(c, cdpos[0] + 28, 65535);
    ok(zip_find_entry((char *)c, z.n, "x", &n) == NULL, "CD name length past end -> NULL");
    memcpy(c, z.b, z.n); set16(c, cdpos[1] + 10, 99);
    ok(zip_find_entry((char *)c, z.n, "readme.txt", &n) == NULL, "unsupported compression method -> NULL");
    memcpy(c, z.b, z.n); set32(c, cdpos[1] + 24, 0xffffffffUL);
    r = zip_find_entry((char *)c, z.n, "readme.txt", &n);
    ok(r == NULL, "stored entry whose declared size exceeds the bytes present is unreadable (NULL), not returned short");
    free(r);
    memcpy(c, z.b, z.n); memset(c + lhpos[0] + 30 + strlen(es[0].name), 0xff, 4);
    ok(zip_find_entry((char *)c, z.n, "stop_times.txt", &n) == NULL, "corrupt deflate stream -> NULL");
    free(c);
  }

  printf("-- inflate ceiling\n");
  {
    size_t bn = 4u << 20; char *zeros = calloc(bn, 1);
    ent_t bomb = {"bomb", zeros, bn, 8}; size_t co, cp[1], lp[1];
    buf_t bz = build(&bomb, 1, &co, cp, lp);
    ok(bz.n < 20000, "fixture is a real bomb (compressed < 20 KB for 4 MiB)");
    setenv("JO_ZIP_MAX_OUT", "1048576", 1);
    ok(zip_find_entry((char *)bz.b, bz.n, "bomb", &n) == NULL, "output over JO_ZIP_MAX_OUT refused (CD path)");
    ok(zip_first_entry((char *)bz.b, bz.n, &n) == NULL, "output over JO_ZIP_MAX_OUT refused (first-entry path)");
    setenv("JO_ZIP_MAX_OUT", "8388608", 1);
    r = zip_find_entry((char *)bz.b, bz.n, "bomb", &n);
    ok(r && n == bn, "raised ceiling lets the same archive through intact");
    free(r);
    set32(bz.b, cp[0] + 24, 100);            /* declared size lies LOW */
    r = zip_find_entry((char *)bz.b, bz.n, "bomb", &n);
    ok(r && n == bn, "declared size too small -> geometric growth, full output");
    free(r);
    set32(bz.b, cp[0] + 24, 0xfffffff0UL);   /* declared size lies HIGH */
    setenv("JO_ZIP_MAX_OUT", "8388608", 1);
    r = zip_find_entry((char *)bz.b, bz.n, "bomb", &n);
    ok(r && n == bn, "huge declared size is clamped to the ceiling, not allocated");
    free(r);
    unsetenv("JO_ZIP_MAX_OUT");
    ok(zip_max_out() == ZIP_DEFAULT_MAX_OUT, "unset ceiling is the 256 MiB default");
    setenv("JO_ZIP_MAX_OUT", "10", 1);
    ok(zip_max_out() == ZIP_DEFAULT_MAX_OUT, "ceiling below 1 KiB ignored");
    setenv("JO_ZIP_MAX_OUT", "-5", 1);
    ok(zip_max_out() == ZIP_DEFAULT_MAX_OUT, "negative ceiling ignored");
    unsetenv("JO_ZIP_MAX_OUT");
    free(zeros); free(bz.b);
  }

  free(z.b);
  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
