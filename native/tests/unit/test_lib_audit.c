/* test_lib_audit.c — the parsing-library fixes of the 2026-11-14 audit that
 * are checkable without a network: LIB-01/07 (xlsx), LIB-04/06/09 (csv, utf8),
 * LIB-08 (jsonbig), LIB-16 (zip iterator). The module under test is csv.c
 * (first include, as run.sh reads it); the rest link from their own objects.
 * Standalone: cc ... tests/unit/test_lib_audit.c lib/xlsx.c lib/zipread.c
 *             third_party/cJSON.c -lz */
#include "../../lib/csv.c"
#include "../../lib/utf8.h"
#include "../../lib/jsonbig.h"
#include "../../lib/xlsx.h"
#include "../../lib/zipread.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

static int fails;
static void ok(int c, const char *w) { printf("  %-4s %s\n", c ? "ok" : "FAIL", w); if (!c) fails++; }

/* A tiny STORED zip writer (no compression), enough for the iterator/xlsx. */
static void le16(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static void le32(unsigned char *p, unsigned long v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
typedef struct { char name[64]; const char *data; size_t n; } zm;
static unsigned char *mkzip(const zm *m, int nm, size_t *outn, int zip64_count) {
  size_t cap = 1 << 22, w = 0, cdoff;
  unsigned char *b = calloc(1, cap);
  unsigned long offs[64];
  for (int i = 0; i < nm; i++) {
    offs[i] = w;
    le32(b + w, 0x04034b50); le16(b + w + 4, 20); le16(b + w + 8, 0);
    le32(b + w + 18, m[i].n); le32(b + w + 22, m[i].n);
    le16(b + w + 26, strlen(m[i].name)); le16(b + w + 28, 0);
    memcpy(b + w + 30, m[i].name, strlen(m[i].name)); w += 30 + strlen(m[i].name);
    memcpy(b + w, m[i].data, m[i].n); w += m[i].n;
  }
  cdoff = w;
  for (int i = 0; i < nm; i++) {
    le32(b + w, 0x02014b50); le16(b + w + 4, 20); le16(b + w + 6, 20);
    le32(b + w + 20, m[i].n); le32(b + w + 24, m[i].n);
    le16(b + w + 28, strlen(m[i].name)); le32(b + w + 42, offs[i]);
    memcpy(b + w + 46, m[i].name, strlen(m[i].name)); w += 46 + strlen(m[i].name);
  }
  size_t cdsz = w - cdoff;
  if (zip64_count) {                       /* ZIP64 EOCD record + locator */
    size_t z64 = w;
    le32(b + w, 0x06064b50); le32(b + w + 4, 44); le32(b + w + 32, nm); le32(b + w + 40, cdsz); le32(b + w + 48, cdoff); w += 56;
    le32(b + w, 0x07064b50); le32(b + w + 8, z64); le32(b + w + 16, 1); w += 20;
  }
  le32(b + w, 0x06054b50);
  le16(b + w + 10, zip64_count ? 0xFFFF : nm); le32(b + w + 12, zip64_count ? 0xFFFFFFFFUL : cdsz);
  le32(b + w + 16, zip64_count ? 0xFFFFFFFFUL : cdoff);
  w += 22; *outn = w; return b;
}

int main(void) {
  printf("lib audit\n");

  /* LIB-04: NEC/IBM extensions decode (CP932), and a truncated trailing byte
   * keeps the converted prefix. bytes: ㈱ ① 髙 in CP932 */
  {
    const char sj[] = "\x87\x8a\x87\x40\xfb\xfc,ok";           /* ㈱ ① 髙 , ok */
    char *u = csv_decode_sjis(sj, sizeof sj - 1);
    ok(u && !strcmp(u, "\xe3\x88\xb1\xe2\x91\xa0\xe9\xab\x99,ok"), "LIB-04: CP932 extensions (㈱ ① 髙) decode");
    free(u);
    const char cut[] = "abc\x82";                                /* lead byte, no trail */
    u = csv_decode_sjis(cut, sizeof cut - 1);
    ok(u && !strncmp(u, "abc", 3) && !strcmp(u + 3, "\xef\xbf\xbd"), "LIB-04: a truncated tail keeps the prefix (+U+FFFD)");
    free(u);
  }
  /* LIB-09 */
  {
    int adv; unsigned cp;
    cp = utf8_decode((const unsigned char *)"\xC0\x80zz", &adv);
    ok(cp == 0xFFFD && adv == 1, "LIB-09: overlong C0 80 is U+FFFD, advance 1 (not U+0000)");
    cp = utf8_decode((const unsigned char *)"\xED\xA0\x80", &adv);
    ok(cp == 0xFFFD && adv == 1, "LIB-09: a surrogate is U+FFFD");
    cp = utf8_decode((const unsigned char *)"\xF4\x90\x80\x80", &adv);
    ok(cp == 0xFFFD && adv == 1, "LIB-09: > U+10FFFF is U+FFFD");
    cp = utf8_decode((const unsigned char *)"\xE6\x97\xA5", &adv);
    ok(cp == 0x65E5 && adv == 3, "LIB-09 control: valid 日 decodes");
    ok(!csv_is_utf8("a\xC0\x80", 3) && !csv_is_utf8("\xED\xA0\x80", 3) && csv_is_utf8("\xE6\x97\xA5", 3) &&
       csv_is_utf8("\xF0\x9F\x98\x80", 4), "LIB-09: csv_is_utf8 rejects overlong/surrogate, keeps valid text");
  }
  /* LIB-06 */
  {
    cJSON *j = csv_parse_d("a,a,b,a\n1,2,3,4\n", 1, ',');
    cJSON *r = cJSON_GetArrayItem(j, 0);
    ok(r && !strcmp(cJSON_GetObjectItem(r, "a")->valuestring, "1") &&
       !strcmp(cJSON_GetObjectItem(r, "a.1")->valuestring, "2") &&
       !strcmp(cJSON_GetObjectItem(r, "b")->valuestring, "3") &&
       !strcmp(cJSON_GetObjectItem(r, "a.2")->valuestring, "4"), "LIB-06: duplicate header names get .1, .2 suffixes");
    cJSON_Delete(j);
    size_t ncol = 80000; char *t = malloc(ncol * 16 + 16); size_t w = 0;
    for (size_t i = 0; i < ncol; i++) w += sprintf(t + w, "%sc%zu", i ? "," : "", i);
    w += sprintf(t + w, "\n");
    for (size_t i = 0; i < ncol; i++) w += sprintf(t + w, "%sv", i ? "," : "");
    clock_t c0 = clock();
    j = csv_parse_d(t, 1, ',');
    double secs = (double)(clock() - c0) / CLOCKS_PER_SEC;
    ok(j && cJSON_GetArraySize(j) == 1 && secs < 3.0, "LIB-06: an 80,000-column header is linear");
    cJSON_Delete(j); free(t);
  }
  /* LIB-08 */
  {
    char *q = jo_json_quote_bigints("{\"id\":1234567890123456781,\"s\":\"1234567890123456782\",\"n\":9007199254740993,\"ok\":1700000000000000000,\"f\":1.5e20,\"k\":-12345678901234567890}");
    ok(q && strstr(q, "\"id\":\"1234567890123456781\"") && strstr(q, "\"s\":\"1234567890123456782\"") &&
       strstr(q, "\"n\":\"9007199254740993\"") && strstr(q, "\"ok\":1700000000000000000") &&
       strstr(q, "\"f\":1.5e20") && strstr(q, "\"k\":\"-12345678901234567890\""),
       "LIB-08: lossy int literals are quoted; strings, exact ints and floats untouched");
    free(q);
    ok(jo_json_quote_bigints("{\"a\":[1,2,3],\"b\":\"99999999999999999999\"}") == NULL, "LIB-08: nothing to quote -> NULL");
    cJSON *d = jo_json_parse("{\"a\":1234567890123456781,\"b\":1234567890123456782}");
    ok(d && cJSON_IsString(cJSON_GetObjectItem(d, "a")) &&
       strcmp(cJSON_GetObjectItem(d, "a")->valuestring, cJSON_GetObjectItem(d, "b")->valuestring) != 0,
       "LIB-08: two ids differing in the last digit stay distinct after parsing");
    cJSON_Delete(d);
  }
  /* LIB-01 / LIB-07: xlsx */
  {
    size_t big = 1 << 20;
    char *si = malloc(big + 64);
    strcpy(si, "<sst><si><t>"); memset(si + 12, 'A', big); strcpy(si + 12 + big, "</t></si></sst>");
    char *sheet = malloc(200000); size_t sw = sprintf(sheet, "<worksheet><sheetData><row r=\"1\">");
    for (int i = 0; i < 3000; i++) sw += sprintf(sheet + sw, "<c r=\"A1\" t=\"s\"><v>0</v></c>");
    sw += sprintf(sheet + sw, "</row></sheetData></worksheet>");
    zm m[4] = {
      {"xl/workbook.xml", "<workbook><sheets><sheet name=\"a\" r:id=\"rId1\"/></sheets></workbook>", 0},
      {"xl/_rels/workbook.xml.rels", "<Relationships><Relationship Id=\"rId1\" Target=\"worksheets/s.xml\"/></Relationships>", 0},
      {"xl/sharedStrings.xml", si, 12 + big + 15},
      {"xl/worksheets/s.xml", sheet, sw}};
    m[0].n = strlen(m[0].data); m[1].n = strlen(m[1].data);
    size_t zn; unsigned char *z = mkzip(m, 4, &zn, 0);
    char *csv = NULL; size_t cl = 0; char err[256];
    int rc = xlsx_to_csv((char *)z, zn, 0, NULL, &csv, &cl, err, sizeof err);
    ok(rc != 0 && csv == NULL && strstr(err, "JO_XLSX_MAX_BYTES"), "LIB-01: shared-string reuse (3 GB of output) is refused by the byte ceiling");
    setenv("JO_XLSX_MAX_BYTES", "4000000000", 1);
    sw = sprintf(sheet, "<worksheet><sheetData><row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c></row></sheetData></worksheet>");
    m[3].n = sw; free(z); z = mkzip(m, 4, &zn, 0);
    rc = xlsx_to_csv((char *)z, zn, 0, NULL, &csv, &cl, err, sizeof err);
    ok(rc == 0 && cl == big + 1, "LIB-01 control: an honest sheet under the ceiling converts whole");
    free(csv); unsetenv("JO_XLSX_MAX_BYTES"); free(z); free(si); free(sheet);

    /* LIB-07 */
    size_t runs = 60000; char *s2 = malloc(runs * 16 + 64); size_t w2 = sprintf(s2, "<sst><si>");
    for (size_t i = 0; i < runs; i++) w2 += sprintf(s2 + w2, "<r><t>a</t></r>");
    w2 += sprintf(s2 + w2, "</si></sst>");
    zm m2[4] = {m[0], m[1], {"xl/sharedStrings.xml", s2, w2},
      {"xl/worksheets/s.xml", "<worksheet><sheetData><row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c></row></sheetData></worksheet>", 0}};
    m2[3].n = strlen(m2[3].data);
    z = mkzip(m2, 4, &zn, 0);
    clock_t c0 = clock();
    rc = xlsx_to_csv((char *)z, zn, 0, NULL, &csv, &cl, err, sizeof err);
    double secs = (double)(clock() - c0) / CLOCKS_PER_SEC;
    ok(rc == 0 && cl == runs + 1 && secs < 3.0, "LIB-07: an <si> of 60,000 runs is linear (was 70 s)");
    free(csv); free(z); free(s2);
  }
  /* LIB-16 */
  {
    zm m[3] = {{"a.json", "{\"a\":1}", 7}, {"b.json", "{\"b\":2}", 7}, {"c.json", "{\"c\":3}", 7}};
    for (int z64 = 0; z64 < 2; z64++) {
      size_t zn; unsigned char *z = mkzip(m, 3, &zn, z64);
      zip_iter it; int n = 0, bad = 0; char name[64], *d; size_t dl;
      ok(zip_iter_open(&it, (char *)z, zn) == 0 && it.declared == 3 && (z64 ? it.zip64 : !it.zip64),
         z64 ? "LIB-16: a ZIP64 end-of-directory record is read (count 3, not 0xFFFF)" : "LIB-16: iterator opens a plain archive");
      int rc;
      while ((rc = zip_iter_next(&it, name, sizeof name, &d, &dl)) != 0) {
        if (rc < 0) { bad++; continue; }
        n += (dl == 7 && d[0] == '{'); free(d);
      }
      ok(n == 3 && !bad && it.walked == 3 && zip_entry_count((char *)z, zn) == 3, "LIB-16: one pass reads all members; entry count is the real one");
      free(z);
    }
    size_t zn; unsigned char *z = mkzip(m, 3, &zn, 0);
    /* a stored member claiming more bytes than the archive holds is unreadable, not "short" */
    size_t eo = zn - 22, cd = (size_t)(z[eo + 16] | z[eo + 17] << 8 | z[eo + 18] << 16);
    le32(z + cd + 24, 100000);
    size_t l; char *got = zip_find_entry((char *)z, zn, "a.json", &l);
    ok(got == NULL, "LIB-16: a stored member larger than the archive is NULL, not silently truncated");
    free(got); free(z);
  }
  printf(fails ? "\n%d FAILURES\n" : "\nall passed\n", fails);
  return fails ? 1 : 0;
}
