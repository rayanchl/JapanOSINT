/* tests/unit/test_alert_deliver_payload.c — core/alert_deliver.c's pure and
 * no-network halves: payload building, the HMAC signature, the base64 /
 * RFC 2047 subject encoder (SMTP header injection), channel parsing, the
 * delivery-outcome mapping and the strict-URL refusal on the operator test
 * path. Nothing here opens a socket: webhooks aimed at loopback/metadata are
 * refused BEFORE any request, and email is skipped when no MTA is configured.
 *
 * The injection case is the one that earned its own test: a rule named
 * "Alert\r\nBcc: attacker@example.com" used to be copied verbatim into the
 * Subject header. Includes alert_deliver.c to reach the statics. */
#include "../../core/alert_deliver.c"
#include <assert.h>
#include <openssl/evp.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

static int has_ctl(const char *s) { for (; *s; s++) if (*s == '\r' || *s == '\n') return 1; return 0; }

int main(void) {
  printf("-- payload\n");
  {
    cJSON *item = cJSON_Parse("{\"uid\":\"s|1\",\"title\":\"T \\\"quoted\\\" 東京\",\"link\":null}");
    char *p = build_payload("r1", "Rule \"A\"", "ev9", "2026-11-01 00:00:00", item);
    cJSON *j = cJSON_Parse(p);
    ok(j && !strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "rule"), "id")->valuestring, "r1")
         && !strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "rule"), "name")->valuestring, "Rule \"A\""), "rule id/name, quotes escaped, round-trips as JSON");
    ok(j && !strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "event"), "matched_at")->valuestring, "2026-11-01 00:00:00"), "event.matched_at");
    ok(j && !strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "item"), "title")->valuestring, "T \"quoted\" 東京"), "item is embedded intact (quotes, UTF-8)");
    ok(!strchr(p, '\n'), "payload is single-line JSON");
    cJSON_Delete(j); free(p);
    p = build_payload(NULL, NULL, NULL, NULL, NULL);
    j = cJSON_Parse(p);
    ok(j && cJSON_IsNull(cJSON_GetObjectItem(j, "item")) && !strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "rule"), "id")->valuestring, ""),
       "NULL fields become empty strings, NULL item becomes JSON null");
    cJSON_Delete(j); free(p); cJSON_Delete(item);
    ok(item_str(NULL, "x") == NULL, "item_str(NULL) -> NULL");
    item = cJSON_Parse("{\"a\":\"\",\"b\":3,\"c\":\"v\"}");
    ok(!item_str(item, "a") && !item_str(item, "b") && !strcmp(item_str(item, "c"), "v") && !item_str(item, "zz"), "item_str: empty / non-string / missing -> NULL");
    cJSON_Delete(item);
  }

  printf("-- HMAC-SHA256 signature\n");
  {
    char hex[65];
    hmac_hex("Jefe", "what do ya want for nothing?", hex);     /* RFC 4231 test case 2 */
    ok(!strcmp(hex, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"), "matches the RFC 4231 test vector");
    hmac_hex("k", "", hex);
    ok(strlen(hex) == 64, "empty body still yields a 64-hex-char signature");
    char h2[65]; hmac_hex("k", "x", h2);
    ok(strcmp(hex, h2) != 0, "different bodies sign differently");
  }

  printf("-- base64\n");
  {
    char o[64];
    b64((const unsigned char *)"", 0, o, sizeof o);       ok(!strcmp(o, ""), "RFC 4648 '' -> ''");
    b64((const unsigned char *)"f", 1, o, sizeof o);      ok(!strcmp(o, "Zg=="), "'f' -> Zg==");
    b64((const unsigned char *)"fo", 2, o, sizeof o);     ok(!strcmp(o, "Zm8="), "'fo' -> Zm8=");
    b64((const unsigned char *)"foo", 3, o, sizeof o);    ok(!strcmp(o, "Zm9v"), "'foo' -> Zm9v");
    b64((const unsigned char *)"foobar", 6, o, sizeof o); ok(!strcmp(o, "Zm9vYmFy"), "'foobar' -> Zm9vYmFy");
    b64((const unsigned char *)"\xff\xfe\xfd", 3, o, sizeof o); ok(!strcmp(o, "//79"), "high bytes use the + / alphabet");
    char small[10]; memset(small, 'Z', sizeof small);
    b64((const unsigned char *)"foobarfoobar", 12, small, sizeof small);
    ok(strlen(small) < sizeof small && small[strlen(small)] == 0, "output is bounded by out_sz and NUL-terminated");
  }

  printf("-- subject encoding / header injection\n");
  {
    char out[640];
    encode_subject("Plain rule", out, sizeof out);
    ok(!strcmp(out, "[JapanOSINT] Plain rule"), "ASCII subject stays readable");
    encode_subject(NULL, out, sizeof out);
    ok(!strcmp(out, "[JapanOSINT] alert"), "NULL name -> 'alert'");
    encode_subject("地震速報", out, sizeof out);
    ok(!strncmp(out, "=?UTF-8?B?", 10) && out[strlen(out) - 2] == '?' && out[strlen(out) - 1] == '=', "Japanese subject becomes an RFC 2047 encoded-word");
    { char *b = strdup(out + 10); b[strlen(b) - 2] = 0;
      unsigned char dec[512]; int n = EVP_DecodeBlock(dec, (unsigned char *)b, (int)strlen(b)); dec[n > 0 ? n : 0] = 0;
      ok(n > 0 && strstr((char *)dec, "[JapanOSINT] 地震速報") != NULL, "the encoded-word decodes back to the original subject");
      free(b); }
    encode_subject("Alert\r\nBcc: attacker@example.com", out, sizeof out);
    ok(!has_ctl(out) && !strstr(out, "Bcc"), "CRLF in a rule name cannot reach the header: encoded, no raw CR/LF, no 'Bcc' in clear");
    encode_subject("x\nSubject: spoof", out, sizeof out);
    ok(!has_ctl(out), "bare LF is neutralised");
    encode_subject("a\rb", out, sizeof out);
    ok(!has_ctl(out), "bare CR is neutralised");
    encode_subject("tab\there", out, sizeof out);
    ok(!strncmp(out, "=?UTF-8?B?", 10), "any control byte forces the encoded form");
    { char big[2000]; memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = 0;
      encode_subject(big, out, sizeof out);
      ok(strlen(out) < sizeof out && !has_ctl(out), "a 2000-byte name is truncated, not overflowed");
      char big2[2000]; for (int i = 0; i < 1999; i++) big2[i] = (i % 3 == 0) ? '\n' : 'a'; big2[1999] = 0;
      encode_subject(big2, out, sizeof out);
      ok(strlen(out) < sizeof out && !has_ctl(out), "a 2000-byte name full of control bytes is bounded and injection-free"); }
  }

  printf("-- channel parsing\n");
  {
    char *t, *g, *s;
    ok(channel_at("[{\"type\":\"webhook\",\"target\":\"http://x\",\"secret\":\"s3\"}]", 0, &t, &g, &s) && !strcmp(t, "webhook") && !strcmp(g, "http://x") && !strcmp(s, "s3"), "complete channel parsed");
    free(t); free(g); free(s);
    ok(channel_at("[{\"type\":\"email\",\"target\":\"a@b\",\"secret\":\"\"}]", 0, &t, &g, &s) && s == NULL, "empty secret -> NULL");
    free(t); free(g);
    ok(channel_at("[{\"type\":\"email\",\"target\":\"a@b\"},{\"type\":\"webhook\",\"target\":\"u\"}]", 1, &t, &g, &s) && !strcmp(t, "webhook"), "index selects the second channel");
    free(t); free(g);
    ok(!channel_at("[{\"type\":\"email\",\"target\":\"a@b\"}]", 1, &t, &g, &s) && !t && !g && !s, "index past the end (channel deleted since enqueue) -> 0, outputs NULL");
    ok(!channel_at("[{\"type\":\"email\",\"target\":\"a@b\"}]", -1, &t, &g, &s), "negative index -> 0");
    ok(!channel_at("not json", 0, &t, &g, &s), "malformed JSON -> 0");
    ok(!channel_at("{\"type\":\"email\"}", 0, &t, &g, &s), "an object instead of an array -> 0");
    ok(!channel_at("[{\"type\":\"email\"}]", 0, &t, &g, &s), "missing target -> 0");
    ok(!channel_at("[{\"type\":5,\"target\":\"x\"}]", 0, &t, &g, &s), "non-string type -> 0");
    ok(!channel_at("[\"webhook\"]", 0, &t, &g, &s), "non-object element -> 0");
    ok(!channel_at(NULL, 0, &t, &g, &s) && !channel_at("", 0, &t, &g, &s), "NULL / empty channels_json -> 0");
  }

  printf("-- delivery outcomes without a network\n");
  {
    outcome o; cJSON *item = cJSON_Parse("{\"uid\":\"u\",\"title\":\"t\"}");
    deliver_one(NULL, "x", NULL, "r", "n", "e", "m", item, &o);
    ok(!strcmp(o.status, "skipped") && o.error && !strcmp(o.error, "channel_incomplete"), "NULL type -> skipped/channel_incomplete (terminal)"); free(o.error);
    deliver_one("webhook", "", NULL, "r", "n", "e", "m", item, &o);
    ok(!strcmp(o.status, "skipped"), "empty target -> skipped"); free(o.error);
    deliver_one("sms", "123", NULL, "r", "n", "e", "m", item, &o);
    ok(!strcmp(o.status, "skipped") && strstr(o.error, "unsupported_channel_type:sms"), "unknown channel type -> skipped, type named"); free(o.error);
    const char *bad[] = { "http://127.0.0.1:9/hook", "http://169.254.169.254/latest/meta-data", "http://10.0.0.5/x", "http://[::1]/x", "file:///etc/passwd", "ftp://8.8.8.8/x" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
      deliver_one("webhook", bad[i], "sekret", "r", "n", "e", "m", item, &o);
      char w[160]; snprintf(w, sizeof w, "webhook to %s is refused before any request (failed/target_refused)", bad[i]);
      ok(!strcmp(o.status, "failed") && o.error && !strncmp(o.error, "target_refused", 14) && o.http_code == 0, w);
      free(o.error);
    }
    unsetenv("JO_SMTP_URL"); unsetenv("JO_SMTP_FROM");
    deliver_one("email", "a@b.example", NULL, "r", "n", "e", "m", item, &o);
    ok(!strcmp(o.status, "skipped") && strstr(o.error, "smtp_not_configured"), "email with no MTA configured -> skipped/smtp_not_configured (terminal, not retried)"); free(o.error);
    setenv("JO_SMTP_URL", "smtp://x", 1); unsetenv("JO_SMTP_FROM");
    const char *u, *us, *pa, *fr;
    ok(!smtp_config(&u, &us, &pa, &fr), "URL without FROM is not a configuration");
    setenv("JO_SMTP_FROM", "", 1);
    ok(!smtp_config(&u, &us, &pa, &fr), "empty FROM is not a configuration");
    setenv("JO_SMTP_FROM", "a@b", 1);
    ok(smtp_config(&u, &us, &pa, &fr), "URL + FROM is");
    unsetenv("JO_SMTP_URL"); unsetenv("JO_SMTP_FROM");
    cJSON_Delete(item);
  }

  printf("-- mail source reader\n");
  {
    mail_src m = { "abcdef", 6, 0 }; char buf[4];
    size_t a = mail_read(buf, 1, 4, &m), b2 = mail_read(buf, 1, 4, &m), c = mail_read(buf, 1, 4, &m);
    ok(a == 4 && b2 == 2 && c == 0, "reader hands out the message in bounded pieces then 0");
  }

  printf("-- operator test path\n");
  {
    const char *dbp = getenv("JO_DB");
    if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
    db_handle db = {0};
    assert(db_open(&db, NULL, NULL) == 0);
    sqlite3_exec(db.h, "INSERT OR IGNORE INTO tenants(id,slug,name) VALUES('tA','ta','A')", 0, 0, 0);
    sqlite3_exec(db.h, "INSERT INTO alert_rules(id,tenant_id,name,channels_json) VALUES('rule1','tA','Rule One',"
      "'[{\"type\":\"webhook\",\"target\":\"http://127.0.0.1:9/h\"},{\"type\":\"email\",\"target\":\"a@b.example\"},{\"type\":\"bogus\",\"target\":\"x\"},{\"type\":5}]')", 0, 0, 0);
    sqlite3_exec(db.h, "INSERT INTO alert_rules(id,tenant_id,name,channels_json) VALUES('rule2','tA','Empty','[]')", 0, 0, 0);
    int st = 0; char *r;
    r = alert_deliver_test(&db, "tA", "rule1", &st);
    cJSON *j = cJSON_Parse(r); free(r);
    cJSON *res = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "data"), "results");
    ok(cJSON_GetArraySize(res) == 3, "three well-formed channels are tried; the malformed one is skipped silently");
    ok(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(res, 0), "status")->valuestring, "failed") &&
       strstr(cJSON_GetObjectItem(cJSON_GetArrayItem(res, 0), "error")->valuestring, "target_refused"), "loopback webhook reported failed/target_refused");
    ok(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(res, 1), "status")->valuestring, "skipped"), "email reported skipped without SMTP");
    ok(cJSON_IsFalse(cJSON_GetObjectItem(j, "ok")) && cJSON_IsTrue(cJSON_GetObjectItem(j, "fired")), "ok=false when any channel is not ok; fired stays true");
    cJSON_Delete(j);
    r = alert_deliver_test(&db, "tA", "rule2", &st);
    j = cJSON_Parse(r); free(r);
    ok(cJSON_IsFalse(cJSON_GetObjectItem(j, "ok")), "a rule with no channels is NOT reported ok (nothing was delivered)");
    cJSON_Delete(j);
    r = alert_deliver_test(&db, "tB", "rule1", &st);
    ok(r && st == 404 && strstr(r, "Not found"), "another tenant's rule is 404, not tested"); free(r);
    r = alert_deliver_test(&db, "tA", "nope", &st); ok(r && st == 404, "unknown rule is 404"); free(r);
    r = alert_deliver_test(&db, NULL, "rule1", &st); ok(r && st == 400, "NULL tenant is 400"); free(r);
    r = alert_deliver_test(NULL, "tA", "rule1", &st); ok(r && st == 400, "NULL db is 400"); free(r);
    db_close(&db);
  }

  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
