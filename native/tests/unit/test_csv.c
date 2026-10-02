/* tests/unit/test_csv.c — contract test for lib/csv.c's tokenizer.
 *
 * WHAT IS BEING PROTECTED. Roughly 380 registered rows parse in CSV mode, and
 * a tokenizer that merges records loses them with every gate green: emit() is
 * called for each record it DID build, so `records=N` is honest about the
 * parse and silent about the file. There is no cheap external check either —
 * the upstream's own line count is not something the engine ever sees.
 *
 * The bug this file was written for (2026-09-16): `csv_rows` opened a quoted
 * field on ANY `"`, not only on one at the start of a field. That makes the
 * quote parity of the WHOLE FILE load-bearing — a single mid-field quote
 * re-pairs every quote after it, so field-start quotes that used to close each
 * other now close across records and each following line is swallowed into its
 * predecessor. ThreatView's Cobalt Strike C2 feed measured 503 records from
 * 1,180 data lines (675 detections discarded, run reporting "503 of 503
 * available"); with start-only quoting the same bytes parse to 1,174.
 *
 * Note the subtlety the fixtures below encode, because it is what made the
 * cause hard to see: ThreatView's unbalanced quotes ARE at field starts and
 * are legitimate under both readings. The loss came from OTHER quotes sitting
 * mid-field elsewhere in the file, shifting which quote closes which.
 *
 * Includes csv.c directly; the harness links every object EXCEPT obj/lib/csv.o
 * and obj/main.o. See tests/unit/run.sh. */

#include "../../lib/csv.c"

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}
static void eqi(int got, int want, const char *what) {
  int c = got == want;
  printf("%s  %s", c ? "  ok  " : "FAIL  ", what);
  if (!c) printf("  (got %d, want %d)", got, want);
  printf("\n");
  if (!c) g_fail++;
}
static void eq(const char *got, const char *want, const char *what) {
  int c = got && strcmp(got, want) == 0;
  printf("%s  %s", c ? "  ok  " : "FAIL  ", what);
  if (!c) printf("  (got \"%s\", want \"%s\")", got ? got : "(null)", want);
  printf("\n");
  if (!c) g_fail++;
}

/* cell [r][c] of a positional parse, or NULL. */
static const char *cell(cJSON *rows, int r, int c) {
  cJSON *row = cJSON_GetArrayItem(rows, r);
  if (!row) return NULL;
  cJSON *v = cJSON_GetArrayItem(row, c);
  return v ? v->valuestring : NULL;
}

/* ── the regression itself ───────────────────────────────────────────────── */

static void test_midfield_quote_is_literal(void) {
  printf("-- a quote inside a field is content, not a quoted field\n");
  /* Old behaviour: the `"` in `b"c` opened a quoted region that ran to EOF
   * (no later quote to close it), collapsing all three lines into ONE row. */
  const char *t = "a,b\"c,d\ne,f,g\nh,i,j\n";
  cJSON *rows = csv_parse(t, 0);
  eqi(cJSON_GetArraySize(rows), 3, "three lines stay three records");
  eq(cell(rows, 0, 1), "b\"c", "the quote is kept as an ordinary character");
  eq(cell(rows, 1, 0), "e", "the record after it is its own record");
  eq(cell(rows, 2, 2), "j", "and so is the one after that");
  cJSON_Delete(rows);
}

static void test_parity_does_not_cross_records(void) {
  printf("-- one mid-field quote does not re-pair the quotes after it\n");
  /* The ThreatView shape in miniature: a mid-field quote on line 1, then two
   * later lines each opening a legitimate field-start quote that never closes
   * on its own line. Under toggle-anywhere the parity shifts and lines 2-4
   * merge; each field-start quote must instead close at the next quote and
   * leave the record boundaries alone. */
  const char *t =
    "1.1.1.1,ok\"x,first\n"
    "2.2.2.2,\"qw.example.com,/ms\",second\n"
    "3.3.3.3,plain,third\n"
    "4.4.4.4,\"zz.example.com,/c\",fourth\n";
  cJSON *rows = csv_parse(t, 0);
  eqi(cJSON_GetArraySize(rows), 4, "four records survive");
  eq(cell(rows, 0, 1), "ok\"x", "mid-field quote is literal");
  eq(cell(rows, 1, 1), "qw.example.com,/ms", "field-start quote still quotes");
  eq(cell(rows, 2, 0), "3.3.3.3", "the unquoted record between them is intact");
  eq(cell(rows, 3, 1), "zz.example.com,/c", "and the last quoted field too");
  cJSON_Delete(rows);
}

/* ── RFC 4180 shapes that must NOT change ────────────────────────────────── */

static void test_quoted_field_still_holds_delimiter_and_newline(void) {
  printf("-- a properly quoted field keeps commas and line breaks\n");
  const char *t = "a,\"x,y\nz\",b\nc,d,e\n";
  cJSON *rows = csv_parse(t, 0);
  eqi(cJSON_GetArraySize(rows), 2, "the embedded newline does not split a row");
  eq(cell(rows, 0, 1), "x,y\nz", "comma and newline are content");
  eq(cell(rows, 1, 0), "c", "the next record still starts where it should");
  cJSON_Delete(rows);
}

static void test_escaped_quotes(void) {
  printf("-- \"\" inside a quoted field is one literal quote\n");
  const char *t = "a,\"he said \"\"hi\"\"\",b\n";
  cJSON *rows = csv_parse(t, 0);
  eq(cell(rows, 0, 1), "he said \"hi\"", "doubled quotes collapse to one");
  eq(cell(rows, 0, 2), "b", "and the field after it is not swallowed");
  cJSON_Delete(rows);
}

static void test_trim_mode_keeps_padded_quotes_working(void) {
  printf("-- padded quoted cells in a non-comma file still unquote\n");
  /* csv.h: with a non-comma delimiter, blanks around a cell are layout. A
   * quote after that padding must still open the field — `a; "b" ;c` reads as
   * b, exactly as it did before the start-only rule. */
  const char *t = "a; \"b, still b\" ;c\n";
  cJSON *rows = csv_parse_d(t, 0, ';');
  eq(cell(rows, 0, 1), "b, still b", "padding dropped, quote still opens");
  eq(cell(rows, 0, 2), "c", "the trailing cell is unaffected");
  cJSON_Delete(rows);
}

static void test_whitespace_mode(void) {
  printf("-- whitespace mode is unchanged\n");
  const char *t = "2497      IIJ          JP00006327\n";
  cJSON *rows = csv_parse_x(t, 0, "ws", 0, NULL);
  eqi(cJSON_GetArraySize(rows), 1, "one row");
  eq(cell(rows, 0, 0), "2497", "first column");
  eq(cell(rows, 0, 2), "JP00006327", "third column");
  cJSON_Delete(rows);
}

static void test_banner_and_header_promotion(void) {
  printf("-- a '#' banner whose last line is the header (URLhaus shape)\n");
  const char *t = "# licence blah\n# id,name\n1,alpha\n2,beta\n";
  cJSON *rows = csv_parse_x(t, 1, NULL, 0, "#");
  eqi(cJSON_GetArraySize(rows), 2, "two records, the header not among them");
  cJSON *r0 = cJSON_GetArrayItem(rows, 0);
  cJSON *name = r0 ? cJSON_GetObjectItem(r0, "name") : NULL;
  eq(name ? name->valuestring : NULL, "alpha", "columns named from the banner");
  cJSON_Delete(rows);
}

/* ── the unterminated-quote recovery and its disclosure counter ──────────── */

static void test_no_repairs_on_clean_input(void) {
  printf("-- a clean file reports zero repairs\n");
  const char *t = "a,\"x,y\nz\",b\nc,d,e\n";
  cJSON *rows = csv_parse_x(t, 0, NULL, 0, NULL);
  eqi(csv_quote_repairs(), 0, "no repair claimed for a legal multi-line cell");
  cJSON_Delete(rows);
}

static void test_runaway_quote_is_closed_and_counted(void) {
  printf("-- a quote running past its line is closed and disclosed\n");
  /* A field-start quote with no closing quote anywhere: without recovery it
   * eats the rest of the file. The bound is CSV_QUOTE_MAX_LINES physical
   * lines, so the lines after it come back as the records they are. */
  const char *t =
    "1,\"runaway starts here\n"
    "2,b\n3,c\n4,d\n5,e\n6,f\n7,g\n";
  cJSON *rows = csv_parse_x(t, 0, NULL, 0, NULL);
  ok(csv_quote_repairs() >= 1, "the repair is counted for disclosure");
  ok(cJSON_GetArraySize(rows) >= 5, "the swallowed lines come back as records");
  cJSON_Delete(rows);
}

static void test_bom_is_not_a_column_name(void) {
  printf("-- a UTF-8 BOM is not part of the first column's name\n");
  /* The file-level byte-order mark used to land inside the first header cell,
   * so the column was stored as "\xEF\xBB\xBFid" and a row declaring
   * `id_keys = "id"` matched nothing — with the column visibly present in the
   * record, which is what made it hard to see. */
  const char *t = "\xEF\xBB\xBF" "id,name\n1,alpha\n2,beta\n";
  cJSON *rows = csv_parse_x(t, 1, NULL, 0, NULL);
  eqi(cJSON_GetArraySize(rows), 2, "two records");
  cJSON *r0 = cJSON_GetArrayItem(rows, 0);
  cJSON *idv = r0 ? cJSON_GetObjectItem(r0, "id") : NULL;
  eq(idv ? idv->valuestring : NULL, "1", "first column answers to \"id\"");
  ok(r0 && !cJSON_GetObjectItem(r0, "\xEF\xBB\xBF" "id"),
     "and NOT to the BOM-prefixed name");
  cJSON_Delete(rows);
}

static void test_wellformed_long_cell_is_not_repaired(void) {
  printf("-- a long cell in well-formed (xlsx-written) text stays one cell\n");
  /* lib/xlsx.c quotes every cell holding a line break, and Excel users write
   * long ones: the FSA notifier register carries a fund's articles of
   * incorporation, one purpose per line. The repair bound would split this
   * cell at its first line; text that is well-formed by construction must not
   * go through the repair at all. */
  const char *t =
    "id,purpose\n"
    "1,\"1. a\n2. b\n3. c\n4. d\n5. e\n6. f\"\n"
    "2,plain\n";
  cJSON *rows = csv_parse_wellformed(t, 1, NULL, 0, NULL);
  eqi(cJSON_GetArraySize(rows), 2, "two records, not eight");
  eqi(csv_quote_repairs(), 0, "and nothing was repaired");
  cJSON *r0 = cJSON_GetArrayItem(rows, 0);
  cJSON *pv = r0 ? cJSON_GetObjectItem(r0, "purpose") : NULL;
  ok(pv && cJSON_IsString(pv) && strstr(pv->valuestring, "6. f"),
     "the whole cell is kept, its last line included");
  cJSON_Delete(rows);
  /* The same text through the REPAIRING parser. This used to be pinned as
   * "csv_parse_x still repairs it" — i.e. the defect: the cell closes
   * properly (its quote is followed by a line end), so it is not malformed and
   * the repair must leave it alone in every feed, xlsx or not. */
  rows = csv_parse_x(t, 1, NULL, 0, NULL);
  eqi(cJSON_GetArraySize(rows), 2, "csv_parse_x reads the same two records");
  eqi(csv_quote_repairs(), 0, "and repairs nothing: the long cell is well-formed");
  cJSON_Delete(rows);
}

/* ── 2026-10-02: the repair must only touch a REAL malformation ──────────── */

static void test_long_wellformed_cell_in_a_feed(void) {
  printf("-- a well-formed six-line cell in an ordinary CSV feed is one cell\n");
  /* The repair bound (4 lines) used to split this into 7 rows: the real
   * record truncated to "l1", five junk records l2..l6, and a
   * csv-quote-repaired notice blaming the upstream for a correct file. */
  const char *t = "id,desc,flag\n1,\"l1\nl2\nl3\nl4\nl5\nl6\",Y\n2,ok,N\n";
  cJSON *rows = csv_parse_x(t, 1, ",", 0, NULL);
  eqi(cJSON_GetArraySize(rows), 2, "two records, not seven");
  eqi(csv_quote_repairs(), 0, "no repair is claimed");
  cJSON *r0 = cJSON_GetArrayItem(rows, 0);
  cJSON *dv = r0 ? cJSON_GetObjectItem(r0, "desc") : NULL;
  eq(dv ? dv->valuestring : NULL, "l1\nl2\nl3\nl4\nl5\nl6", "the whole cell is kept");
  cJSON *fv = r0 ? cJSON_GetObjectItem(r0, "flag") : NULL;
  eq(fv ? fv->valuestring : NULL, "Y", "and the field after it is its own");
  cJSON_Delete(rows);
  /* The same shape with a non-comma delimiter and with CRLF line ends. */
  rows = csv_parse_x("a|b\r\n1|\"x\r\ny\r\nz\r\nw\r\nv\r\nu\"\r\n2|ok\r\n", 1, "|", 0, NULL);
  eqi(cJSON_GetArraySize(rows), 2, "pipe + CRLF: two records");
  eqi(csv_quote_repairs(), 0, "pipe + CRLF: no repair");
  cJSON_Delete(rows);
}

static void test_runaway_before_a_later_quoted_field(void) {
  printf("-- a runaway quote whose next quote OPENS a later field is still repaired\n");
  /* The next quote after the runaway is the opening quote of row 6's field,
   * followed by `q` — not a close. Read by the letter it would swallow rows
   * 2-5 into row 1; it must be closed at its own line end instead. */
  const char *t = "1,\"runaway\n2,b\n3,c\n4,d\n5,e\n6,\"q\",f\n";
  cJSON *rows = csv_parse_x(t, 0, NULL, 0, NULL);
  eqi(csv_quote_repairs(), 1, "one repair");
  eqi(cJSON_GetArraySize(rows), 6, "six records");
  eq(cell(rows, 0, 1), "runaway", "the runaway keeps its own line");
  eq(cell(rows, 5, 1), "q", "and the later quoted field still unquotes");
  cJSON_Delete(rows);
}

static void test_repair_leaves_no_carriage_return(void) {
  printf("-- a repaired field in a CRLF file does not end in \\r\n");
  const char *t = "a,b\r\n1,\"x\r\n2,y\r\n3,z\r\n4,w\r\n5,v\r\n6,u\r\n";
  cJSON *rows = csv_parse_x(t, 1, NULL, 0, NULL);
  eqi(csv_quote_repairs(), 1, "one repair");
  eqi(cJSON_GetArraySize(rows), 6, "six records");
  cJSON *r0 = cJSON_GetArrayItem(rows, 0);
  cJSON *bv = r0 ? cJSON_GetObjectItem(r0, "b") : NULL;
  eq(bv ? bv->valuestring : NULL, "x", "the repaired value is \"x\", not \"x\\r\"");
  cJSON_Delete(rows);
}

static void test_blank_before_quote_in_comma_mode(void) {
  printf("-- comma mode: a blank before a quote still opens the quoted field\n");
  /* Regression of the start-only rule: the blank counted as content, so the
   * quote after it was literal and "Tokyo, Japan" was split in two. */
  const char *t = "\"x\", \"Tokyo, Japan\", \"2\"\n";
  cJSON *rows = csv_parse_x(t, 0, NULL, 0, NULL);
  eqi(cJSON_GetArraySize(cJSON_GetArrayItem(rows, 0)), 3, "three cells, not four");
  eq(cell(rows, 0, 1), "Tokyo, Japan", "the quoted cell keeps its comma");
  eq(cell(rows, 0, 2), "2", "and the last cell unquotes too");
  cJSON_Delete(rows);
  /* An UNQUOTED blank-led cell in comma mode is still byte-exact. */
  rows = csv_parse("a, b\n", 0);
  eq(cell(rows, 0, 1), " b", "a blank-led unquoted cell keeps its blank");
  cJSON_Delete(rows);
  /* And the repair pre-pass reads the same quote as opening: a blank-led
   * runaway is repaired, not left for csv_rows to swallow the file with. */
  rows = csv_parse_x("1, \"runaway\n2,b\n3,c\n4,d\n5,e\n6,f\n", 0, NULL, 0, NULL);
  eqi(csv_quote_repairs(), 1, "the blank-led runaway is repaired");
  eqi(cJSON_GetArraySize(rows), 6, "and the lines after it are their own records");
  cJSON_Delete(rows);
}

int main(void) {
  test_bom_is_not_a_column_name();
  test_midfield_quote_is_literal();
  test_parity_does_not_cross_records();
  test_quoted_field_still_holds_delimiter_and_newline();
  test_escaped_quotes();
  test_trim_mode_keeps_padded_quotes_working();
  test_whitespace_mode();
  test_banner_and_header_promotion();
  test_no_repairs_on_clean_input();
  test_runaway_quote_is_closed_and_counted();
  test_wellformed_long_cell_is_not_repaired();
  test_long_wellformed_cell_in_a_feed();
  test_runaway_before_a_later_quoted_field();
  test_repair_leaves_no_carriage_return();
  test_blank_before_quote_in_comma_mode();
  printf(g_fail ? "\n%d FAILED\n" : "\nall ok\n", g_fail);
  return g_fail ? 1 : 0;
}
