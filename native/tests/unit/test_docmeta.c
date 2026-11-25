/* tests/unit/test_docmeta.c — core/docmeta.c: bytes-in forensic metadata.
 * Hostile-input contract first (hand-built ZIPs, truncated/garbage inputs, an
 * inflate bomb, control bytes and over-long values in PDF strings), then the
 * extraction itself (PDF Info dict + date precision, OOXML core/app props with
 * a stored and a deflated member, text/CSV/JSON/HTML), then the hint
 * cross-checks.  Buffers are exactly-sized heap copies so ASan sees any read
 * past the end.  Reached through the header. */
#include "../../core/docmeta.h"
#include "../../third_party/cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

/* extract from an exactly-sized heap copy */
static cJSON *ex(const void *buf, size_t len, const char *fn, const char *ct) {
  void *h = malloc(len ? len : 1);
  if (len) memcpy(h, buf, len);
  char *s = docmeta_extract(len ? h : NULL, len, fn, ct);
  free(h);
  assert(s);
  cJSON *j = cJSON_Parse(s);
  free(s);
  assert(j);
  return j;
}
static cJSON *exs(const char *lit, const char *fn, const char *ct) { return ex(lit, strlen(lit), fn, ct); }
static const char *sniff(const void *buf, size_t len) {
  void *h = malloc(len ? len : 1);
  if (len) memcpy(h, buf, len);
  const char *t = docmeta_sniff(len ? h : NULL, len);
  free(h);
  return t;
}
static const char *S(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return cJSON_IsString(v) ? v->valuestring : NULL;
}
static cJSON *O(cJSON *o, const char *k) { return cJSON_GetObjectItem(o, k); }

/* ---- a tiny ZIP writer: stored (0) or raw-deflated (8) members ---------- */
typedef struct { unsigned char *b; size_t n, cap; } buf;
static void put(buf *x, const void *p, size_t n) {
  if (x->n + n > x->cap) { x->cap = (x->n + n) * 2 + 64; x->b = realloc(x->b, x->cap); }
  memcpy(x->b + x->n, p, n);
  x->n += n;
}
static void p16(buf *x, unsigned v) { unsigned char c[2] = { v & 255, (v >> 8) & 255 }; put(x, c, 2); }
static void p32(buf *x, unsigned long v) { p16(x, v & 0xffff); p16(x, (v >> 16) & 0xffff); }
typedef struct { const char *name; const unsigned char *data; size_t len; int method; } zm;
static buf make_zip(const zm *m, int n, int with_eocd) {
  buf z = {0}, cd = {0};
  size_t offs[16];
  for (int i = 0; i < n; i++) {
    offs[i] = z.n;
    const unsigned char *d = m[i].data;
    size_t dl = m[i].len;
    unsigned char *tmp = NULL;
    if (m[i].method == 8) {
      z_stream s;
      memset(&s, 0, sizeof s);
      deflateInit2(&s, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
      size_t cap = deflateBound(&s, (uLong)m[i].len) + 16;
      tmp = malloc(cap);
      s.next_in = (Bytef *)m[i].data; s.avail_in = (uInt)m[i].len;
      s.next_out = tmp; s.avail_out = (uInt)cap;
      deflate(&s, Z_FINISH);
      dl = cap - s.avail_out;
      deflateEnd(&s);
      d = tmp;
    }
    size_t nl = strlen(m[i].name);
    p32(&z, 0x04034b50UL); p16(&z, 20); p16(&z, 0); p16(&z, m[i].method); p32(&z, 0); p32(&z, 0);
    p32(&z, dl); p32(&z, m[i].len); p16(&z, nl); p16(&z, 0);
    put(&z, m[i].name, nl); put(&z, d, dl);
    p32(&cd, 0x02014b50UL); p16(&cd, 20); p16(&cd, 20); p16(&cd, 0); p16(&cd, m[i].method);
    p32(&cd, 0); p32(&cd, 0); p32(&cd, dl); p32(&cd, m[i].len); p16(&cd, nl); p16(&cd, 0); p16(&cd, 0);
    p16(&cd, 0); p16(&cd, 0); p32(&cd, 0); p32(&cd, offs[i]);
    put(&cd, m[i].name, nl);
    free(tmp);
  }
  size_t cdoff = z.n;
  put(&z, cd.b, cd.n);
  free(cd.b);
  if (with_eocd) {
    p32(&z, 0x06054b50UL); p16(&z, 0); p16(&z, 0); p16(&z, n); p16(&z, n);
    p32(&z, cd.n); p32(&z, cdoff); p16(&z, 0);
  }
  return z;
}

static const char CORE_XML[] =
  "<?xml version=\"1.0\"?><cp:coreProperties>"
  "<dc:title>Q3 &amp; Budget</dc:title><dc:creator>Alice &#26481;</dc:creator>"
  "<cp:lastModifiedBy>Carol</cp:lastModifiedBy><cp:revision>7</cp:revision>"
  "<dcterms:created xsi:type=\"dcterms:W3CDTF\">2023-04-05T06:07:08Z</dcterms:created>"
  "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">2024-01-02T03:04:05Z</dcterms:modified>"
  "<cp:keywords/></cp:coreProperties>";
static const char APP_XML[] =
  "<Properties><Application>Microsoft Office Word</Application><AppVersion>16.0000</AppVersion>"
  "<Company>ACME Corp</Company><TotalTime>42</TotalTime><Words>many</Words><Pages>3</Pages></Properties>";

static void t_sniff(void) {
  printf("-- docmeta_sniff\n");
  ok(!strcmp(sniff(NULL, 0), "unknown"), "NULL/empty -> unknown");
  ok(!strcmp(sniff("%PDF-1.4", 8), "pdf"), "pdf");
  ok(!strcmp(sniff("%PDF", 4), "text") || !strcmp(sniff("%PDF", 4), "unknown"), "a 4-byte '%PDF' is not claimed to be a pdf");
  ok(!strcmp(sniff("\x89PNG\r\n\x1a\n", 8), "png"), "png");
  ok(!strcmp(sniff("\xff\xd8\xff\xe0", 4), "jpeg"), "jpeg");
  ok(!strcmp(sniff("GIF89a", 6), "gif") && !strcmp(sniff("GIF87a", 6), "gif"), "gif");
  ok(!strcmp(sniff("<!DOCTYPE HTML><p>x", 19), "html") && !strcmp(sniff("junk <body>x", 12), "html"), "html, case-insensitive, anywhere in the head");
  ok(!strcmp(sniff("{\"a\":1}\n", 8), "json") && !strcmp(sniff("  [1,2,3] ", 10), "json"), "json object/array");
  ok(strcmp(sniff("{\"a\":1", 6), "json") != 0, "an unterminated '{' is not called json");
  ok(!strcmp(sniff("a,b,c\n1,2,3\n4,5,6\n", 18), "csv"), "csv by consistent delimiter count");
  ok(!strcmp(sniff("hello world\nsecond line\n", 24), "text"), "text");
  ok(!strcmp(sniff("ab\0cd\0ef", 8), "unknown"), "NUL bytes -> unknown, not text");
  ok(!strcmp(sniff("\xff\xfeh\0i\0", 6), "text"), "BOM'd UTF-16 is text");
  buf z = make_zip((zm[]){ { "a.txt", (const unsigned char *)"x", 1, 0 } }, 1, 1);
  ok(!strcmp(sniff(z.b, z.n), "zip"), "plain zip");
  free(z.b);
  z = make_zip((zm[]){ { "word/document.xml", (const unsigned char *)"x", 1, 0 } }, 1, 1);
  ok(!strcmp(sniff(z.b, z.n), "docx"), "docx by word/ member");
  ok(!strcmp(sniff(z.b, 40), "docx") || !strcmp(sniff(z.b, 40), "zip"), "truncated docx (no EOCD) does not crash");
  free(z.b);
  z = make_zip((zm[]){ { "xl/workbook.xml", (const unsigned char *)"x", 1, 0 } }, 1, 1);
  ok(!strcmp(sniff(z.b, z.n), "xlsx"), "xlsx");
  free(z.b);
  z = make_zip((zm[]){ { "ppt/slides/s1.xml", (const unsigned char *)"x", 1, 0 } }, 1, 1);
  ok(!strcmp(sniff(z.b, z.n), "pptx"), "pptx");
  free(z.b);
  z = make_zip((zm[]){ { "word/document.xml", (const unsigned char *)"x", 1, 0 } }, 1, 0);
  ok(!strcmp(sniff(z.b, z.n), "docx"), "no EOCD: kind recovered from the first local header");
  free(z.b);
}

static void t_pdf(void) {
  printf("-- pdf\n");
  const char *pdf =
    "%PDF-1.7\n1 0 obj\n<< /Title (Quarterly \\(draft\\)) /Author (Alice) /Subject <48656C6C6F> "
    "/Creator (Word) /Producer (Ghostscript 9.5) /CreationDate (D:20240102030405+09'00') "
    "/ModDate (D:20240203) /Keywords (a, b) >>\nendobj\n"
    "<< /Type /Page >> << /Type /Page >> << /Type /Pages /Count 2 >>\n%%EOF\n";
  cJSON *j = ex(pdf, strlen(pdf), "report.pdf", "application/pdf");
  ok(!strcmp(S(j, "type"), "pdf") && O(j, "bytes")->valuedouble == (double)strlen(pdf), "type and byte count");
  ok(S(j, "sniffed") && !strncmp(S(j, "sniffed"), "255044462d312e37", 16), "hex prefix of the magic (16 bytes max)");
  cJSON *p = O(j, "pdf");
  ok(p && !strcmp(S(p, "version"), "1.7"), "header version");
  ok(!strcmp(S(p, "title"), "Quarterly (draft)"), "escaped parentheses in a literal string");
  ok(!strcmp(S(p, "author"), "Alice") && !strcmp(S(p, "creator"), "Word") && !strcmp(S(p, "producer"), "Ghostscript 9.5"), "author/creator/producer");
  ok(S(p, "subject") && !strcmp(S(p, "subject"), "Hello"), "hex string decoded");
  ok(!strcmp(S(p, "created"), "2024-01-02T03:04:05+09:00") && !strcmp(S(p, "created_raw"), "D:20240102030405+09'00'"),
     "full date: ISO with offset, raw kept");
  ok(!strcmp(S(p, "modified"), "2024-02-03"), "date-only value is NOT padded with invented zeros");
  ok(!strcmp(S(p, "dates_normalized"), "iso-8601"), "normalisation declared");
  ok(O(p, "approx_pages") && O(p, "approx_pages")->valueint == 2, "2 /Page objects counted, /Pages not");
  ok(cJSON_IsFalse(O(p, "encrypted")) && cJSON_IsFalse(O(p, "linearized")), "flags false when absent");
  ok(O(j, "extension_mismatch") == NULL && O(j, "content_type_mismatch") == NULL, "honest hints: no mismatch flags");
  cJSON_Delete(j);

  const char *bad =
    "%PDF-1.4\n<< /Author (Mallory\x1b[31m\x01 x) /Title (AAAA) /CreationDate (D:2024133199) /ModDate (garbage) "
    "/Encrypt 5 0 R /JavaScript /EmbeddedFiles /AcroForm >>\n";
  j = ex(bad, strlen(bad), NULL, NULL);
  p = O(j, "pdf");
  char *printed = cJSON_PrintUnformatted(j);
  ok(!strstr(printed, "\\u001b") && !strchr(S(p, "author"), 0x1b) && !strchr(S(p, "author"), 0x01), "control bytes scrubbed from extracted strings");
  free(printed);
  ok(O(p, "created") == NULL && S(p, "created_raw") != NULL, "an impossible date (month 13) yields no ISO field, raw kept");
  ok(O(p, "modified") == NULL, "an unparseable date yields no ISO field");
  ok(cJSON_IsTrue(O(p, "encrypted")) && S(p, "note") && strstr(S(p, "note"), "ciphertext"), "/Encrypt flagged with a ciphertext warning");
  ok(cJSON_IsTrue(O(p, "has_javascript")) && cJSON_IsTrue(O(p, "has_embedded_files")) && cJSON_IsTrue(O(p, "has_acroform")), "script/embedded/form flags");
  cJSON_Delete(j);

  /* a 600-char CJK author is cut on a codepoint boundary at DOCMETA_MAX_VALUE */
  buf b = {0};
  put(&b, "%PDF-1.4\n<< /Author (", 21);
  for (int i = 0; i < 400; i++) put(&b, "\xe6\x9d\xb1", 3);
  put(&b, ") >>\n", 5);
  j = ex(b.b, b.n, NULL, NULL);
  const char *au = S(O(j, "pdf"), "author");
  size_t al = au ? strlen(au) : 0;
  int valid = al > 0 && al < 512 && al % 3 == 0;
  ok(valid, "long author truncated to < DOCMETA_MAX_VALUE bytes on a UTF-8 boundary");
  cJSON_Delete(j);
  free(b.b);

  /* "%PDF-" followed by nothing useful */
  j = exs("%PDF-", NULL, NULL);
  ok(!strcmp(S(j, "type"), "pdf") && O(O(j, "pdf"), "version") == NULL, "'%PDF-' alone: no version invented");
  cJSON_Delete(j);

  /* windowing is disclosed: a >2 MiB PDF whose Info sits in the MIDDLE is not found, and says so */
  size_t big = 3u << 20;
  unsigned char *m = malloc(big);
  memset(m, ' ', big);
  memcpy(m, "%PDF-1.4\n", 9);
  memcpy(m + big / 2, "<< /Author (Hidden) >>", 22);
  j = ex(m, big, NULL, NULL);
  ok(O(O(j, "pdf"), "author") == NULL && cJSON_IsTrue(O(O(j, "pdf"), "scan_windowed")),
     "an Info value only in the unscanned middle is absent and scan_windowed=true says why");
  cJSON_Delete(j);
  memcpy(m + big - 40, "<< /Author (Tail) >>", 20);
  j = ex(m, big, NULL, NULL);
  ok(S(O(j, "pdf"), "author") && !strcmp(S(O(j, "pdf"), "author"), "Tail"), "the same value in the tail window is found");
  cJSON_Delete(j);
  free(m);
}

static void t_office(void) {
  printf("-- ooxml\n");
  const zm mem[] = {
    { "[Content_Types].xml", (const unsigned char *)"<x/>", 4, 0 },
    { "word/document.xml", (const unsigned char *)"<w/>", 4, 0 },
    { "docProps/core.xml", (const unsigned char *)CORE_XML, sizeof CORE_XML - 1, 0 },
    { "docProps/app.xml", (const unsigned char *)APP_XML, sizeof APP_XML - 1, 8 },
  };
  buf z = make_zip(mem, 4, 1);
  cJSON *j = ex(z.b, z.n, "budget.docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
  ok(!strcmp(S(j, "type"), "docx"), "docx recognised");
  cJSON *o = O(j, "office");
  ok(!strcmp(S(o, "format"), "docx") && O(o, "entries")->valueint == 4, "format and entry count");
  cJSON *c = O(o, "core"), *a = O(o, "app");
  ok(c && !strcmp(S(c, "creator"), "Alice 東"), "dc:creator with a numeric character reference decoded to UTF-8");
  ok(!strcmp(S(c, "title"), "Q3 & Budget"), "&amp; unescaped");
  ok(!strcmp(S(c, "last_modified_by"), "Carol"), "cp:lastModifiedBy kept apart from the author");
  ok(!strcmp(S(c, "revision"), "7") && !strcmp(S(c, "created"), "2023-04-05T06:07:08Z"), "revision and created (attribute-bearing tag)");
  ok(O(c, "keywords") == NULL, "a self-closing <cp:keywords/> yields no empty string");
  ok(a && !strcmp(S(a, "company"), "ACME Corp") && !strcmp(S(a, "application"), "Microsoft Office Word"), "app.xml (deflated member) read");
  ok(O(a, "total_edit_minutes") && O(a, "total_edit_minutes")->valueint == 42 && O(a, "pages")->valueint == 3, "numeric fields as numbers");
  ok(O(a, "words") == NULL, "a non-numeric <Words>many</Words> is absent, not 0");
  ok(O(j, "extension_mismatch") == NULL && O(j, "content_type_mismatch") == NULL, "matching hints: no flags");
  cJSON_Delete(j);

  j = ex(z.b, z.n, "budget.pdf", "application/pdf");
  ok(cJSON_IsTrue(O(j, "extension_mismatch")) && cJSON_IsTrue(O(j, "content_type_mismatch")), "a docx claiming to be a PDF (name and type) is flagged twice");
  cJSON_Delete(j);
  j = ex(z.b, z.n, "x.bin", "application/octet-stream");
  ok(O(j, "extension_mismatch") == NULL && O(j, "content_type_mismatch") == NULL, "unknown extension / octet-stream imply nothing");
  cJSON_Delete(j);
  free(z.b);

  const zm plain[] = { { "a.txt", (const unsigned char *)"hi", 2, 0 }, { "b.txt", (const unsigned char *)"yo", 2, 0 } };
  z = make_zip(plain, 2, 1);
  j = ex(z.b, z.n, NULL, NULL);
  o = O(j, "office");
  ok(!strcmp(S(j, "type"), "zip") && !strcmp(S(o, "format"), "zip") && O(o, "entries")->valueint == 2, "plain zip: container + entry count");
  ok(!strcmp(S(o, "docprops"), "absent-or-unsupported-compression"), "no docProps: says why, invents nothing");
  cJSON_Delete(j);
  free(z.b);

  /* corrupt deflate member and an EOCD-less archive */
  unsigned char junk[64];
  memset(junk, 0xAB, sizeof junk);
  const zm cm[] = { { "word/document.xml", (const unsigned char *)"x", 1, 0 }, { "docProps/core.xml", junk, sizeof junk, 0 } };
  z = make_zip(cm, 2, 1);
  /* flip the core.xml member to method 8 in both headers: bytes are not a valid stream */
  for (size_t i = 0; i + 4 < z.n; i++) {
    if (!memcmp(z.b + i, "PK\x03\x04", 4) && !memcmp(z.b + i + 30, "docProps/core.xml", 17)) z.b[i + 8] = 8;
    if (!memcmp(z.b + i, "PK\x01\x02", 4) && !memcmp(z.b + i + 46, "docProps/core.xml", 17)) z.b[i + 10] = 8;
  }
  j = ex(z.b, z.n, NULL, NULL);
  c = O(O(j, "office"), "core");
  ok(!strcmp(S(j, "type"), "docx") && (c == NULL || (O(c, "creator") == NULL && O(c, "last_modified_by") == NULL)),
     "a corrupt deflate member yields no invented core props (and no crash)");
  cJSON_Delete(j);
  j = ex(z.b, z.n / 2, NULL, NULL);
  ok(!strcmp(S(j, "type"), "docx") && S(O(j, "office"), "docprops") != NULL, "archive cut in half: still typed, docprops reason given");
  cJSON_Delete(j);
  free(z.b);

  /* the inflate bomb: 8 MiB of zeros declared in a few KB. Output is capped at
   * DOCMETA_ZIP_MAX_UNCOMP and the cap is reported. */
  size_t bomb = 8u << 20;
  unsigned char *zeros = calloc(1, bomb);
  memcpy(zeros, "<dc:creator>Eve</dc:creator>", 28);
  const zm bm[] = { { "word/document.xml", (const unsigned char *)"x", 1, 0 }, { "docProps/core.xml", zeros, bomb, 8 } };
  z = make_zip(bm, 2, 1);
  ok(z.n < (64u << 10), "(fixture) the bomb archive is tiny");
  j = ex(z.b, z.n, NULL, NULL);
  c = O(O(j, "office"), "core");
  ok(c && !strcmp(S(c, "creator"), "Eve") && cJSON_IsTrue(O(c, "truncated")),
     "bomb member: the early element is read, output capped at 1 MiB, truncated=true disclosed");
  cJSON_Delete(j);
  free(z.b);
  free(zeros);
}

static void t_text(void) {
  printf("-- text / csv / json / html\n");
  cJSON *j = exs("a,b,c\r\n1,2,3\r\n4,5,6\r\n", NULL, NULL);
  cJSON *t = O(j, "text");
  ok(!strcmp(S(j, "type"), "csv") && !strcmp(S(t, "delimiter"), ",") && O(t, "columns")->valueint == 3, "csv delimiter and column count");
  ok(!strcmp(S(t, "line_endings"), "crlf") && O(t, "lines")->valueint == 3, "crlf and 3 lines");
  ok(!strcmp(S(t, "detection"), "heuristic"), "csv result labelled heuristic");
  cJSON_Delete(j);
  j = exs("a\tb\n1\t2\n", NULL, NULL);
  ok(!strcmp(S(O(j, "text"), "delimiter"), "\\t"), "tab delimiter");
  cJSON_Delete(j);
  j = exs("name;\"x;y\"\nfoo;\"1;2\"\n", NULL, NULL);
  ok(!strcmp(S(j, "type"), "csv") && O(O(j, "text"), "columns")->valueint == 2, "a quoted delimiter does not inflate the column count");
  cJSON_Delete(j);
  j = exs("\xef\xbb\xbfhello\nworld", NULL, NULL);
  t = O(j, "text");
  ok(cJSON_IsTrue(O(t, "bom")) && !strcmp(S(t, "encoding"), "utf-8") && O(t, "lines")->valueint == 2, "utf-8 BOM, trailing line without newline counted");
  cJSON_Delete(j);
  j = exs("caf\xe9 au lait\nline two\n", NULL, NULL);
  ok(!strcmp(S(O(j, "text"), "encoding"), "unknown-8bit"), "invalid UTF-8 high bytes reported as unknown-8bit, not utf-8");
  cJSON_Delete(j);
  j = exs("{\"a\": [1,2]}", "x.json", "application/json");
  ok(!strcmp(S(O(j, "text"), "structure"), "object") && !strcmp(S(O(j, "text"), "detection"), "heuristic-not-validated"), "json structure, honestly labelled not validated");
  cJSON_Delete(j);
  j = exs("[1,2]", NULL, NULL);
  ok(!strcmp(S(O(j, "text"), "structure"), "array"), "json array");
  cJSON_Delete(j);
  const char *html = "<html><head><TITLE> A &lt;b&gt; page </TITLE><meta name=\"generator\" content=\"WordPress 6.4\">"
                     "<meta content='Jane Doe' name='author'><meta name=\"description\" content=\"d &amp; e\"></head><body>x</body></html>";
  j = ex(html, strlen(html), "p.html", "text/html");
  t = O(j, "text");
  ok(!strcmp(S(j, "type"), "html") && !strcmp(S(t, "title"), "A <b> page"), "html title trimmed and unescaped (case-insensitive tag)");
  ok(!strcmp(S(t, "generator"), "WordPress 6.4") && !strcmp(S(t, "author"), "Jane Doe") && !strcmp(S(t, "description"), "d & e"),
     "meta generator/author/description, either attribute order, case preserved");
  cJSON_Delete(j);
  j = exs("<html><title>unterminated", NULL, NULL);
  ok(S(O(j, "text"), "title") != NULL, "unterminated title still bounded and returned");
  cJSON_Delete(j);
}

static void t_misc(void) {
  printf("-- labelled-only types, hostile and empty input\n");
  cJSON *j = ex(NULL, 0, NULL, NULL);
  ok(!strcmp(S(j, "type"), "unknown") && O(j, "bytes")->valuedouble == 0 && O(j, "sniffed") == NULL, "NULL/empty: unknown, no hex prefix");
  cJSON_Delete(j);
  j = exs("\x89PNG\r\n\x1a\n....", "x.png", "image/png");
  ok(!strcmp(S(j, "type"), "png") && S(j, "note") && strstr(S(j, "note"), "media"), "png is labelled and routed to media.h, no fabricated fields");
  cJSON_Delete(j);
  j = exs("\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1rest", "old.doc", NULL);
  ok(!strcmp(S(j, "type"), "unknown") && S(j, "magic_note") && strstr(S(j, "magic_note"), "ole2"), "OLE2 identified by magic_note, not parsed");
  cJSON_Delete(j);
  const unsigned char elf[] = { 0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0 };
  j = ex(elf, sizeof elf, NULL, NULL);
  ok(S(j, "magic_note") && strstr(S(j, "magic_note"), "elf"), "ELF magic noted");
  cJSON_Delete(j);
  unsigned char rnd[200];
  for (size_t i = 0; i < sizeof rnd; i++) rnd[i] = (unsigned char)(i * 37 + 11);
  rnd[0] = 0x01;
  j = ex(rnd, sizeof rnd, NULL, NULL);
  ok(!strcmp(S(j, "type"), "unknown") && strlen(S(j, "sniffed")) == 32, "random bytes: unknown + a 16-byte hex prefix");
  cJSON_Delete(j);
  char longname[3000];
  memset(longname, 'a', sizeof longname - 1);
  longname[sizeof longname - 1] = 0;
  strcpy(longname + 2900, ".pdf");
  j = exs("hello\n", longname, "text/plain; charset=utf-8");
  ok(S(j, "filename") && strlen(S(j, "filename")) < 512, "an oversize filename hint is bounded");
  ok(O(j, "content_type_mismatch") == NULL, "text/plain over text is not a mismatch");
  cJSON_Delete(j);
  /* every prefix of a valid docx and pdf is survivable */
  buf z = make_zip((zm[]){ { "word/document.xml", (const unsigned char *)"x", 1, 0 },
                           { "docProps/core.xml", (const unsigned char *)CORE_XML, sizeof CORE_XML - 1, 0 } }, 2, 1);
  int crashed = 0;
  for (size_t n = 0; n <= z.n; n++) { cJSON *q = ex(z.b, n, NULL, NULL); if (!q) crashed = 1; cJSON_Delete(q); }
  ok(!crashed, "every truncation of a docx parses to a document (ASan-checked for over-reads)");
  free(z.b);
  const char *pdf = "%PDF-1.7\n<< /Author (A) /CreationDate (D:20240102) >>\n/Type /Page\n";
  for (size_t n = 0; n <= strlen(pdf); n++) { cJSON *q = ex(pdf, n, NULL, NULL); if (!q) crashed = 1; cJSON_Delete(q); }
  ok(!crashed, "every truncation of a pdf parses to a document");
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  t_sniff();
  t_pdf();
  t_office();
  t_text();
  t_misc();
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
