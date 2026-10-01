/* tests/unit/test_xlsx.c — contract test for lib/xlsx.c.
 *
 * WHAT IS BEING PROTECTED. The ministries' registers arrive as .xlsx and are
 * handed to the engine's CSV path as text. Every shape the header promises to
 * preserve is one line of the fixture: shared and rich-text strings (with the
 * furigana <rPh> run EXCLUDED), inline strings, booleans, errors, formula
 * strings, raw numbers, a row holding only column C (A/B must come out as
 * empty cells, not vanish), a row number the sheet skips (must come out as an
 * empty LINE so line N is sheet row N), and a cell holding a comma and quotes
 * (must be RFC 4180 quoted). Every non-empty line is padded to the widest
 * row so the CSV is rectangular (the header names column D even though row 1
 * stops at C); a row the sheet skips is an EMPTY line, not a line of commas,
 * so the CSV reader does not emit a blank record for it. Each was a way for a register to be silently
 * shortened or shifted by a column, and none of them errors anywhere.
 *
 * The workbook is embedded (fixtures/xlsx_fixture.h, made by
 * fixtures/gen_xlsx_fixture.py) so the test has no file or Python dependency.
 *
 * Includes xlsx.c directly; the harness links every object EXCEPT
 * obj/lib/xlsx.o and obj/main.o. See tests/unit/run.sh. */

#include "../../lib/xlsx.c"
#include "fixtures/xlsx_fixture.h"

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}
static void eq(const char *got, const char *want, const char *what) {
  int c = got && strcmp(got, want) == 0;
  printf("%s  %s", c ? "  ok  " : "FAIL  ", what);
  if (!c) printf("  (got \"%s\", want \"%s\")", got ? got : "(null)", want);
  printf("\n");
  if (!c) g_fail++;
}

/* Line `n` (1-based) of `csv` as a malloc'd string, "" past the end. */
static char *line(const char *csv, int n) {
  const char *p = csv;
  for (int i = 1; i < n && p; i++) { p = strchr(p, '\n'); if (p) p++; }
  if (!p) return strdup("");
  const char *e = strchr(p, '\n');
  size_t len = e ? (size_t)(e - p) : strlen(p);
  char *s = malloc(len + 1); memcpy(s, p, len); s[len] = 0;
  return s;
}
static void line_eq(const char *csv, int n, const char *want, const char *what) {
  char *l = line(csv, n); eq(l, want, what); free(l);
}

static const char *XB = (const char *)FIXTURE_XLSX;

static void test_sheet_names(void) {
  printf("-- sheet names\n");
  char err[256] = "";
  char *names = xlsx_sheet_names(XB, FIXTURE_XLSX_len, err, sizeof err);
  eq(names, "登録一覧\nMemo", "tab order, newline-separated, UTF-8 name intact");
  free(names);
}

static void test_first_sheet(void) {
  printf("-- sheet 0 as CSV\n");
  char err[256] = ""; char *csv = NULL; size_t n = 0;
  int rc = xlsx_to_csv(XB, FIXTURE_XLSX_len, 0, NULL, &csv, &n, err, sizeof err);
  ok(rc == 0 && csv, "converts (err should be empty)");
  if (rc != 0) { printf("      err: %s\n", err); return; }
  ok(n == strlen(csv), "csv_len is the byte length");
  ok(n > 0 && csv[n - 1] == '\n', "last line is newline-terminated");
  line_eq(csv, 1, "商号,登録番号,備考,",
          "row 1: two shared strings + one inlineStr, padded to the widest row (D)");
  line_eq(csv, 2, "株式会社テスト,20240401,1,",
          "row 2: rich-text runs joined, <rPh> furigana NOT included; raw number; bool as stored");
  line_eq(csv, 3, ",,\"a, \"\"b\"\" & c\",",
          "row 3: only C3 exists -> A/B empty; comma+quotes quoted, &quot;/&amp; unescaped");
  line_eq(csv, 4, "",
          "row 4: absent from the sheet -> EMPTY line, so line N == sheet row N");
  line_eq(csv, 5, "xy,#N/A,,1.5E-3",
          "row 5: formula string value, error cell, empty <c/>, number verbatim");
  line_eq(csv, 6, "", "no line 6");
  int lines = 0; for (size_t i = 0; i < n; i++) lines += csv[i] == '\n';
  ok(lines == 5, "exactly 5 lines (the sheet's 5 rows)");
  free(csv);
}

static void test_sheet_selection(void) {
  printf("-- sheet selection\n");
  char err[256] = ""; char *csv = NULL; size_t n = 0;
  int rc = xlsx_to_csv(XB, FIXTURE_XLSX_len, 0, "Memo", &csv, &n, err, sizeof err);
  ok(rc == 0, "by name overrides index");
  if (rc == 0) line_eq(csv, 1, "Memo header", "second sheet's own cells");
  free(csv); csv = NULL;

  rc = xlsx_to_csv(XB, FIXTURE_XLSX_len, 1, NULL, &csv, &n, err, sizeof err);
  ok(rc == 0, "by index 1");
  if (rc == 0) line_eq(csv, 1, "Memo header", "index 1 is the Memo sheet");
  free(csv); csv = NULL;

  rc = xlsx_to_csv(XB, FIXTURE_XLSX_len, 0, "Sheet3", &csv, &n, err, sizeof err);
  ok(rc == -1 && csv == NULL, "unknown name fails, csv_out NULL");
  ok(strstr(err, "Sheet3") && strstr(err, "have:") && strstr(err, "Memo"),
     "error names the missing sheet and lists the real ones");

  rc = xlsx_to_csv(XB, FIXTURE_XLSX_len, 7, NULL, &csv, &n, err, sizeof err);
  ok(rc == -1 && csv == NULL && strstr(err, "out of range"), "index out of range fails");
}

static void test_not_xlsx(void) {
  printf("-- not an xlsx\n");
  char err[256] = ""; char *csv = NULL; size_t n = 0;
  static const char pdf[] = "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n";
  int rc = xlsx_to_csv(pdf, sizeof pdf - 1, 0, NULL, &csv, &n, err, sizeof err);
  ok(rc == -1 && !csv && strstr(err, "not a zip") && strstr(err, "%PDF"),
     "a PDF renamed .xlsx: 'not a zip' naming what was seen");

  static const unsigned char ole[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1, 0, 0};
  rc = xlsx_to_csv((const char *)ole, sizeof ole, 0, NULL, &csv, &n, err, sizeof err);
  ok(rc == -1 && !csv && strstr(err, ".xls"), "OLE2 .xls is named as such");

  rc = xlsx_to_csv((const char *)FIXTURE_PLAIN_ZIP, FIXTURE_PLAIN_ZIP_len, 0, NULL,
                   &csv, &n, err, sizeof err);
  ok(rc == -1 && !csv && strstr(err, "xl/workbook.xml"), "plain zip: no xl/workbook.xml");

  rc = xlsx_to_csv(NULL, 0, 0, NULL, &csv, &n, err, sizeof err);
  ok(rc == -1 && !csv && err[0], "NULL body fails with a reason");

  char *names = xlsx_sheet_names(pdf, sizeof pdf - 1, err, sizeof err);
  ok(names == NULL && err[0], "sheet_names on a non-workbook: NULL + reason");
}

static void test_cell_ceiling(void) {
  printf("-- JO_XLSX_MAX_CELLS\n");
  char err[256] = ""; char *csv = NULL; size_t n = 0;
  /* sheet 0 is 5 rows x 4 columns = 20 cells */
  setenv("JO_XLSX_MAX_CELLS", "19", 1);
  int rc = xlsx_to_csv(XB, FIXTURE_XLSX_len, 0, NULL, &csv, &n, err, sizeof err);
  ok(rc == -1 && csv == NULL && strstr(err, "JO_XLSX_MAX_CELLS"),
     "20-cell sheet over a 19-cell ceiling is REFUSED, never shortened");
  setenv("JO_XLSX_MAX_CELLS", "20", 1);
  rc = xlsx_to_csv(XB, FIXTURE_XLSX_len, 0, NULL, &csv, &n, err, sizeof err);
  ok(rc == 0 && csv, "exactly at the ceiling converts");
  free(csv);
  unsetenv("JO_XLSX_MAX_CELLS");
}

int main(void) {
  test_sheet_names();
  test_first_sheet();
  test_sheet_selection();
  test_not_xlsx();
  test_cell_ceiling();
  printf(g_fail ? "\n%d FAILED\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
