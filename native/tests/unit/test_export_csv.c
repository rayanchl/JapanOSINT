/* test_export_csv.c — CSV export is not a spreadsheet-formula vector (API-09).
 *
 * Feed content is upstream-controlled, and Excel / LibreOffice evaluate a cell
 * that begins with = + - @ TAB or CR. csv_row() prefixes such a TEXT cell with
 * a single quote; a numeric cell (a negative number is data, not an attack),
 * a text cell that merely CONTAINS those characters, and the header row are
 * left alone. RFC 4180 quoting still applies on top. */
#include "../../core/exportapi.c"

#include <assert.h>

typedef struct { char buf[4096]; size_t len; } sinkbuf;
static int sink_fn(void *ctx, const char *b, size_t n) {
  sinkbuf *s = ctx;
  assert(s->len + n < sizeof s->buf);
  memcpy(s->buf + s->len, b, n);
  s->len += n; s->buf[s->len] = 0;
  return 0;
}

static void field(const char *v, int guard, const char *want) {
  static ostream o;
  sinkbuf sb = {{0}, 0};
  memset(&o, 0, sizeof o);
  o.fn = sink_fn; o.ctx = &sb;
  assert(csv_field_g(&o, v, guard) == 0 && ob_flush(&o) == 0);
  if (strcmp(sb.buf, want) != 0)
    fprintf(stderr, "csv_field_g(\"%s\", %d) = \"%s\", want \"%s\"\n", v, guard, sb.buf, want);
  assert(!strcmp(sb.buf, want));
}

int main(void) {
  /* the leading characters, each prefixed with one quote */
  field("=1+1",         1, "'=1+1");
  field("+cmd|' /C calc'!A0", 1, "'+cmd|' /C calc'!A0");
  field("-2+3",         1, "'-2+3");
  field("@SUM(A1)",     1, "'@SUM(A1)");
  field("\tx",          1, "'\tx");
  /* RFC 4180 quoting composes: the prefix goes INSIDE the quotes */
  field("=HYPERLINK(\"http://e\",\"x\")", 1, "\"'=HYPERLINK(\"\"http://e\"\",\"\"x\"\")\"");
  field("\r=x",         1, "\"'\r=x\"");
  field("=a,b",         1, "\"'=a,b\"");
  /* untouched: ordinary text, an embedded (not leading) trigger, unguarded cells */
  field("plain",        1, "plain");
  field("a=b",          1, "a=b");
  field("a-b",          1, "a-b");
  field("",             1, "");
  field("=1+1",         0, "=1+1");           /* header / numeric callers pass guard 0 */
  field("-5",           0, "-5");

  /* csv_row: a TEXT cell is guarded, a numeric one is not */
  sqlite3 *h = NULL;
  assert(sqlite3_open(":memory:", &h) == SQLITE_OK);
  assert(sqlite3_exec(h, "CREATE TABLE t(a TEXT, b REAL, c INTEGER, d TEXT)", 0, 0, 0) == SQLITE_OK);
  assert(sqlite3_exec(h, "INSERT INTO t VALUES ('=evil()', -1.5, -3, 'ok')", 0, 0, 0) == SQLITE_OK);
  sqlite3_stmt *s = NULL;
  assert(sqlite3_prepare_v2(h, "SELECT a,b,c,d FROM t", -1, &s, NULL) == SQLITE_OK);
  assert(sqlite3_step(s) == SQLITE_ROW);
  const ecol cols[4] = { {"a", CT_TEXT, GR_NONE}, {"b", CT_NUM, GR_NONE},
                         {"c", CT_AUTO, GR_NONE}, {"d", CT_TEXT, GR_NONE} };
  static ostream o;
  sinkbuf sb = {{0}, 0};
  memset(&o, 0, sizeof o);
  o.fn = sink_fn; o.ctx = &sb;
  assert(csv_row(&o, s, cols, 4) == 0 && ob_flush(&o) == 0);
  assert(!strcmp(sb.buf, "'=evil(),-1.5,-3,ok\r\n") && "text guarded, numbers left as numbers");
  sqlite3_finalize(s);
  sqlite3_close(h);

  printf("test_export_csv: ok\n");
  return 0;
}
