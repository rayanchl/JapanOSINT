/* tests/htmlparse_test.c — offline unit test for lib/htmlparse.c's html_attr().
 *
 *   make htmlparsetest && ./bin/htmlparse_test
 *
 * html_attr() used to look for the attribute NAME as a bare substring with no
 * left boundary, so on lazy-loading camera markup
 *
 *     <img data-src="spinner.gif" src="snapshot.jpg">
 *
 * a lookup for "src" returned "spinner.gif" — the scrapers stored the
 * placeholder and never saw the real snapshot URL. Nothing about that is
 * visible at compile time and it needs no network to check, so it is pinned
 * here. The rest of the cases are the shapes the 13 in-tree call sites rely
 * on (href/title/src on an <a>/<img> header buffer, capitalised XML attributes
 * on a tag buffer that starts at '<', and a tag buffer that starts directly at
 * the attribute name). */
#include "../lib/htmlparse.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}

/* assert html_attr(s, attr) == want ("" means "must not match") */
static void want(const char *s, const char *attr, const char *expect,
                 const char *what) {
  char buf[256];
  int rc = html_attr(s, attr, buf, sizeof buf);
  int good = *expect ? (rc == 1 && strcmp(buf, expect) == 0)
                     : (rc == 0 && buf[0] == 0);
  if (!good)
    printf("      got rc=%d value=\"%s\", expected \"%s\"\n", rc, buf, expect);
  ok(good, what);
}

int main(void) {
  /* 1. THE BUG: a prefixed attribute must not answer for the bare name. */
  want("<img data-src=\"LAZY.jpg\" src=\"REAL.jpg\">", "src", "REAL.jpg",
       "data-src does not satisfy src (lazy-load markup)");
  want("<img data-src=\"LAZY.jpg\">", "src", "",
       "data-src alone is not a src at all");
  want("<div data-id=\"7\" id=\"real\">", "id", "real",
       "data-id does not satisfy id");
  want("<img ng-src=\"LAZY.jpg\" src=\"REAL.jpg\">", "src", "REAL.jpg",
       "ng-src does not satisfy src");

  /* 2. the genuine attribute still resolves, wherever it sits in the tag. */
  want("<img src=\"REAL.jpg\" alt=\"x\">", "src", "REAL.jpg",
       "a genuine src resolves (first attribute)");
  want("<a class=\"c\" href=\"/en/view/12/\">", "href", "/en/view/12/",
       "a genuine href resolves (later attribute)");
  want("<a\n  href=\"/x\">", "href", "/x",
       "newline before the attribute is a boundary");
  want("<a\thref=\"/x\">", "href", "/x",
       "tab before the attribute is a boundary");

  /* 3. buffer that begins AT the attribute name (tdnet_disclosure.c builds
   *    such tag buffers) — start of buffer is a boundary. */
  want("src=\"REAL.jpg\">", "src", "REAL.jpg",
       "attribute at the very start of the buffer resolves");
  want("<src=\"REAL.jpg\">", "src", "REAL.jpg",
       "attribute right after '<' resolves");

  /* 4. all three quoting styles. */
  want("<img src=\"double.jpg\">", "src", "double.jpg",
       "double-quoted value");
  want("<img src='single.jpg'>", "src", "single.jpg",
       "single-quoted value");
  want("<img src=unquoted.jpg>", "src", "unquoted.jpg",
       "unquoted value (terminated by '>')");
  want("<img src=unquoted.jpg alt=\"x\">", "src", "unquoted.jpg",
       "unquoted value (terminated by whitespace)");
  want("<img src = \"spaced.jpg\">", "src", "spaced.jpg",
       "spaces around '=' tolerated");

  /* 5. the name appearing INSIDE a value must not confuse the scan. */
  want("<a href=\"/img/src/photo.jpg\" title=\"t\">", "href",
       "/img/src/photo.jpg", "value containing the attr name is unaffected");
  want("<a href=\"/img/src/photo.jpg\">", "src", "",
       "attr name inside another attribute's VALUE is not a match");
  want("<a title=\"see href=/x\" href=\"/real\">", "href", "/real",
       "a fake 'href=' inside a quoted value loses to the real one");

  /* 6. a longer name must not be answered by its own prefix, and vice versa
   *    (the right boundary — `srcset` is a different attribute from `src`). */
  want("<img srcset=\"a.jpg 1x\" src=\"REAL.jpg\">", "src", "REAL.jpg",
       "srcset does not satisfy src");
  want("<img srcset=\"a.jpg 1x\">", "src", "",
       "srcset alone is not a src");

  /* 7. case-insensitive names, as the header promises and
   *    cert_mitre_cwe_capec.c relies on (`ID=`, `Name=`, `Status=`). */
  want("<Attack_Pattern ID=\"66\" Name=\"SQL Injection\" Status=\"Stable\">",
       "Name", "SQL Injection", "capitalised XML attribute resolves");
  want("<Attack_Pattern ID=\"66\" Abstraction=\"Standard\">", "ID", "66",
       "short capitalised attribute resolves");
  want("<IMG SRC=\"REAL.jpg\">", "src", "REAL.jpg",
       "attribute name match is case-insensitive");

  /* 8. degenerate inputs return an honest 0 rather than reading past the end. */
  want("<img src=\"unterminated.jpg>", "src", "", "unterminated quote → no match");
  want("", "src", "", "empty buffer → no match");
  want("<img>", "src", "", "no such attribute → no match");
  {
    char buf[8];
    int rc = html_attr(NULL, "src", buf, sizeof buf);
    ok(rc == 0 && buf[0] == 0, "NULL haystack → no match, out cleared");
    rc = html_attr("<img src=\"0123456789abcdef\">", "src", buf, sizeof buf);
    ok(rc == 1 && strcmp(buf, "0123456") == 0,
       "oversized value truncates to the caller's buffer");
  }

  /* Regression: a name=value fragment inside ANOTHER attribute's quoted text
   * used to beat the tag's real attribute, because the bad-character filter
   * only fires when the closing quote is glued to the value. MITRE's CWE/CAPEC
   * XML — which cert_mitre_cwe_capec.c passes whole blocks of — has exactly
   * this shape. Quoted values now win outright over unquoted ones. */
  printf("a quoted value beats a name=value fragment inside another value\n");
  {
    char v[64];
    ok(html_attr("<Weakness Description=\"compare Name=Other here\" "
                 "Name=\"Real Name\">", "Name", v, sizeof v) &&
       strcmp(v, "Real Name") == 0,
       "the tag's real quoted Name wins over the one inside Description");
    ok(html_attr("Contact us: href=mailto:a@b.c <a href=\"/real\">",
                 "href", v, sizeof v) && strcmp(v, "/real") == 0,
       "a quoted href wins over a bare one sitting in preceding prose");
    ok(html_attr("<img src=snapshot.jpg>", "src", v, sizeof v) &&
       strcmp(v, "snapshot.jpg") == 0,
       "a genuinely unquoted value still resolves when no quoted one exists");
    ok(html_attr("<img data-src=\"spinner.gif\" src=\"snapshot.jpg\">",
                 "src", v, sizeof v) && strcmp(v, "snapshot.jpg") == 0,
       "and the original data-src boundary fix still holds");
  }

  printf("html_table_parse: a rectangular grid, whatever the markup\n");
  {
    const char *doc =
      "<p>intro</p><table id=nav><tr><td>menu</td></tr></table>"
      "<TABLE class=\"reg\"><caption>登録業者一覧</caption>"
      "<thead><tr><th>番号</th><th colspan=2>名称</th><th>所在地</th></tr></thead>"
      "<tbody>"
      "<tr><td rowspan=2>関東</td><td><a href=\"/d/1\">A&amp;B 株式会社</a></td><td>A&nbsp;B</td><td>東京都<br>千代田区</td></tr>"
      "<tr><td>C&#x6CB3;社</td><td>&#67;</td><td>横浜市</td></tr>"
      "<tr><td>近畿</td><td colspan=3>大阪 <table><tr><td>nested</td></tr></table> 支店</td></tr>"
      "<tr><td></td><td> </td><td></td><td></td></tr>"
      "</tbody></TABLE>";
    html_table tb;
    int found = html_table_parse(doc, "登録業者", 0, &tb);
    ok(found && tb.nrows == 5 && tb.width == 4, "the matching table, 5 rows x 4 columns (the nav table skipped)");
    if (found && tb.nrows == 5) {
      ok(!strcmp(tb.rows[0].cells[1].text, "名称") && !strcmp(tb.rows[0].cells[2].text, ""),
         "colspan: the extra slot is empty, so columns stay aligned");
      ok(!strcmp(tb.rows[1].cells[1].text, "A&B 株式会社") && tb.rows[1].cells[1].href &&
         !strcmp(tb.rows[1].cells[1].href, "/d/1"), "entities decoded, the cell's link kept raw");
      ok(!strcmp(tb.rows[1].cells[2].text, "A B") && !strcmp(tb.rows[1].cells[3].text, "東京都 千代田区"),
         "&nbsp; and <br> are spaces");
      ok(!strcmp(tb.rows[2].cells[0].text, "関東"), "rowspan carries the cell down");
      ok(!strcmp(tb.rows[2].cells[1].text, "C\xe6\xb2\xb3\xe7\xa4\xbe") && !strcmp(tb.rows[2].cells[2].text, "C"),
         "numeric entities, hex and decimal, become UTF-8");
      ok(!strcmp(tb.rows[3].cells[1].text, "大阪 nested 支店") && !strcmp(tb.rows[3].cells[3].text, ""),
         "a nested table is text in its cell, never rows of its own");
      ok(tb.rows[4].ncells == 4 && !strcmp(tb.rows[4].cells[1].text, ""), "an empty row is still the table's width");
    }
    html_table_free(&tb);
    ok(html_table_parse(doc, NULL, 1, &tb) && tb.nrows == 5, "no `must`: table_index counts every table");
    html_table_free(&tb);
    ok(!html_table_parse(doc, "no such text", 0, &tb), "no matching table -> 0, not an empty success");
    ok(html_table_parse("<table><td>a<td>b<tr><td>c<td>d</table>", NULL, 0, &tb) &&
       tb.nrows == 2 && !strcmp(tb.rows[0].cells[1].text, "b") && !strcmp(tb.rows[1].cells[0].text, "c"),
       "omitted </td> and </tr>, and a cell before any <tr>");
    html_table_free(&tb);
  }

  printf(g_fail ? "\n%d FAILURES\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
