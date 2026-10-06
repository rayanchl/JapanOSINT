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

  /* The strongest label for a link wins, not the first anchor's (audit
   * 2026-10-02 open item 6, the Gifu Shimbun list): an icon anchor whose label
   * was borrowed from a neighbour, then the headline anchor to the same link. */
  printf("html_label_offer: the strongest label per link, in first-seen order\n");
  {
    html_label_set ls = {0};
    ok(html_label_offer(&ls, "/a/409812", "9月26日 10:00", HTML_LABEL_CONTEXT) == 1,
       "an icon anchor's borrowed caption opens the link's entry");
    ok(html_label_offer(&ls, "/a/774427", "photo of the campus", HTML_LABEL_IMG) == 1,
       "a second link is a second entry");
    ok(html_label_offer(&ls, "/a/409812", "岐阜新聞・中学3年模試", HTML_LABEL_TEXT) == 0,
       "the headline anchor to the same link is not a new entry");
    ok(ls.n == 2 && !strcmp(ls.v[0].label, "岐阜新聞・中学3年模試") &&
       ls.v[0].strength == HTML_LABEL_TEXT,
       "…but its link text replaces the borrowed caption");
    ok(html_label_offer(&ls, "/a/774427", "大学の理系人材ニーズ高まる", HTML_LABEL_TEXT) == 0 &&
       !strcmp(ls.v[1].label, "大学の理系人材ニーズ高まる"),
       "link text also beats an image's alt text");
    ok(html_label_offer(&ls, "/a/409812", "詳しく読む", HTML_LABEL_TEXT) == 0 &&
       !strcmp(ls.v[0].label, "岐阜新聞・中学3年模試"),
       "between equal strengths the FIRST label stays (all-text links keep their titles)");
    ok(html_label_offer(&ls, "/a/409812", "an image", HTML_LABEL_IMG) == 0 &&
       !strcmp(ls.v[0].label, "岐阜新聞・中学3年模試"),
       "a weaker label never replaces a stronger one");
    ok(html_label_offer(&ls, "/a/1", "text first", HTML_LABEL_TEXT) == 1 &&
       html_label_offer(&ls, "/a/1", "alt later", HTML_LABEL_IMG) == 0 &&
       !strcmp(ls.v[2].label, "text first"),
       "text then image: the text stays");
    ok(ls.n == 3 && !strcmp(ls.v[0].key, "/a/409812") && !strcmp(ls.v[1].key, "/a/774427") &&
       !strcmp(ls.v[2].key, "/a/1"),
       "every link offered is kept once, in the order it first appeared");
    ok(ls.v[0].nothers == 2 && !strcmp(ls.v[0].others[0], "詳しく読む") &&
       !strcmp(ls.v[0].others[1], "an image"),
       "every label the page gave the link that did not win is kept as an other label");
    ok(html_label_offer(&ls, "/a/409812", "an image", HTML_LABEL_IMG) == 0 &&
       html_label_offer(&ls, "/a/409812", "10月1日", HTML_LABEL_CONTEXT) == 0 &&
       ls.v[0].nothers == 2,
       "…once each, and never a borrowed label (the replaced date was another card's)");

    /* The containment rule: a stronger label does not replace one that
     * already says everything it says (replay of 1,127 HTML rows: a camera's
     * alt with its full location over the link text naming the spot; a full
     * headline in an alt over the same headline cut short in the text). */
    ok(html_label_offer(&ls, "/cam/1", "嬉野市嬉野町　不動山　上　国道34号(75k880)",
                        HTML_LABEL_IMG) == 1 &&
       html_label_offer(&ls, "/cam/1", "不動山　上", HTML_LABEL_TEXT) == 0 &&
       !strcmp(ls.v[3].label, "嬉野市嬉野町　不動山　上　国道34号(75k880)") &&
       ls.v[3].nothers == 1 && !strcmp(ls.v[3].others[0], "不動山　上"),
       "link text contained in the alt does not displace it (and is kept)");
    ok(html_label_offer(&ls, "/n/1", "最後の「えちご・くびき野100キロマラソン」、悪天候の初回や新型コロナウイルス禍越え30年",
                        HTML_LABEL_IMG) == 1 &&
       html_label_offer(&ls, "/n/1", "最後の「えちご・くびき野100キロマラソン」、悪天候の初回や新型コロナウイル...",
                        HTML_LABEL_TEXT) == 0 &&
       !strcmp(ls.v[4].label, "最後の「えちご・くびき野100キロマラソン」、悪天候の初回や新型コロナウイルス禍越え30年"),
       "a headline cut short with an ellipsis does not displace the whole one");
    ok(html_label_offer(&ls, "/home", "愛知労働局", HTML_LABEL_IMG) == 1 &&
       html_label_offer(&ls, "/home", "ホーム", HTML_LABEL_TEXT) == 0 &&
       !strcmp(ls.v[5].label, "ホーム") && !strcmp(ls.v[5].others[0], "愛知労働局"),
       "link text that says something else still wins, and the alt is kept beside it");
    ok(html_label_covers("Report 2026 (PDF)", "Report  2026") &&
       html_label_covers("a\xE3\x80\x80" "b", "ab") && html_label_covers("abc", "ab\xE2\x80\xA6") &&
       !html_label_covers("ab", "abc") && !html_label_covers(NULL, "a") &&
       html_label_covers("x", ""),
       "html_label_covers ignores whitespace (incl. U+3000) and a trailing ellipsis");
    ok(html_label_offer(&ls, NULL, "x", HTML_LABEL_TEXT) == -1 &&
       html_label_offer(NULL, "/k", "x", HTML_LABEL_TEXT) == -1,
       "degenerate arguments are refused, not dereferenced");
    html_label_free(&ls);
    ok(ls.n == 0 && ls.v == NULL, "html_label_free leaves an empty, reusable set");
  }

  /* Both sets are hashed: PYPI_SIMPLE_INDEX is one listing of 824,355 links,
   * and a linear lookup per anchor made that page quadratic. The index is
   * rebuilt as the set grows, so check lookups across many rebuilds. */
  printf("the link sets stay exact across index rebuilds\n");
  {
    html_label_set ls = {0};
    html_seen sn = {0};
    char k[32];
    int fresh = 1, again = 1, seen_ok = 1;
    for (int i = 0; i < 20000; i++) {
      snprintf(k, sizeof k, "/simple/p%d/", i);
      if (html_label_offer(&ls, k, "img", HTML_LABEL_IMG) != 1) fresh = 0;
      if (!html_seen_add(&sn, k)) seen_ok = 0;
    }
    for (int i = 0; i < 20000; i += 7) {
      snprintf(k, sizeof k, "/simple/p%d/", i);
      if (html_label_offer(&ls, k, "text", HTML_LABEL_TEXT) != 0) again = 0;
      if (html_seen_add(&sn, k) || !html_seen_has(&sn, k)) seen_ok = 0;
    }
    ok(fresh && ls.n == 20000, "20,000 distinct links are 20,000 entries");
    ok(again && !strcmp(ls.v[7].label, "text") && !strcmp(ls.v[8].label, "img") &&
       !strcmp(ls.v[19999 - 19999 % 7].label, "text"),
       "a re-offered link is found after every rebuild, and only it is upgraded");
    ok(seen_ok && sn.n == 20000 && !html_seen_has(&sn, "/simple/absent/") &&
       !strcmp(sn.v[12345], "/simple/p12345/"),
       "the seen-set dedupes exactly, keeps insertion order, and finds no phantom");
    html_label_free(&ls);
    html_seen_free(&sn);
  }

  printf(g_fail ? "\n%d FAILURES\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
