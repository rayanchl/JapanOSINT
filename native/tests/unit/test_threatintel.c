/* tests/unit/test_threatintel.c — lib/threatintel.c: the key gate, the
 * run-callback contract and the abuse.ch header builder.
 *
 * WHAT IS PROTECTED. A key-gated feed with no key must say so as DATA (a
 * credential notice, rc 0 — "gated is a state, not a failure") and must not
 * call the provider at all; with a key, the callback receives the key and its
 * features go to the sink; a callback that returns nothing / a non-array is an
 * honest 0, never a crash; and the abuse.ch `Auth-Key` header is built once,
 * refuses an over-long key rather than truncating it into a wrong credential,
 * and is absent (NULL) when no key is configured. */
#include "../../lib/threatintel.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

typedef struct { int n; char type[8][64]; char key[8][96]; } cap_t;
static int cap_emit(intel_sink *s, const intel_item *it) {
  cap_t *c = s->ctx;
  if (c->n < 8) {
    snprintf(c->type[c->n], 64, "%s", it->record_type ? it->record_type : "");
    snprintf(c->key[c->n], 96, "%s", it->remote_key ? it->remote_key : "");
    c->n++;
  }
  return 1;
}

static int g_calls; static char g_seen_key[128]; static int g_mode;
static cJSON *runner(const char *key, const source_ctx *ctx, void *ud) {
  g_calls++;
  snprintf(g_seen_key, sizeof g_seen_key, "%s", key ? key : "(null)");
  (void)ctx; (void)ud;
  if (g_mode == 0) return NULL;                         /* provider failed */
  if (g_mode == 1) return cJSON_CreateObject();         /* not an array */
  cJSON *a = cJSON_CreateArray();
  for (int i = 0; i < 3; i++) {
    cJSON *f = cJSON_Parse("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[139.7,35.6]},"
                           "\"properties\":{\"name\":\"x\",\"id\":\"PLACEHOLDER\"}}");
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    char idb[16]; snprintf(idb, sizeof idb, "id-%d", i);
    cJSON_ReplaceItemInObject(p, "id", cJSON_CreateString(idb));
    cJSON_AddItemToArray(a, f);
  }
  return a;
}

int main(void) {
  cap_t cap; memset(&cap, 0, sizeof cap);
  intel_sink sink = { &cap, cap_emit };
  source_ctx ctx; memset(&ctx, 0, sizeof ctx); ctx.source_id = "ti-test";

  unsetenv("TI_TEST_KEY"); unsetenv("TI_TEST_ALT");
  printf("-- key gate\n");
  g_calls = 0; g_mode = 2;
  int rc = threatintel_collect(&ctx, &sink, "TI_TEST_KEY", NULL, runner, NULL);
  ok(rc == 0 && g_calls == 0, "no key: rc 0 and the provider is never called");
  ok(cap.n == 1 && strstr(cap.type[0], "notice") != NULL, "no key: one notice record is emitted (gated is data, not silence)");
  ok(threatintel_collect(&ctx, &sink, "TI_TEST_KEY", NULL, NULL, NULL) == -1, "NULL run callback -> -1");

  setenv("TI_TEST_KEY", "", 1);
  cap.n = 0;
  threatintel_collect(&ctx, &sink, "TI_TEST_KEY", NULL, runner, NULL);
  ok(g_calls == 0 && cap.n == 1, "empty-string key counts as no key");

  const char *fb[] = { "TI_TEST_ALT", NULL };
  setenv("TI_TEST_ALT", "alt-secret", 1);
  g_calls = 0; cap.n = 0;
  rc = threatintel_collect(&ctx, &sink, "TI_TEST_KEY", fb, runner, NULL);
  ok(g_calls == 1 && !strcmp(g_seen_key, "alt-secret") && rc == 3, "fallback env name supplies the key");
  ok(cap.n == 3, "three features reach the sink");

  setenv("TI_TEST_KEY", "primary", 1);
  g_calls = 0;
  threatintel_collect(&ctx, &sink, "TI_TEST_KEY", fb, runner, NULL);
  ok(!strcmp(g_seen_key, "primary"), "primary env name wins over the fallback");

  printf("-- run callback contract\n");
  g_mode = 0; g_calls = 0; cap.n = 0;
  ok(threatintel_collect(&ctx, &sink, "TI_TEST_KEY", NULL, runner, NULL) == 0 && cap.n == 0, "callback returns NULL -> honest 0");
  g_mode = 1;
  ok(threatintel_collect(&ctx, &sink, "TI_TEST_KEY", NULL, runner, NULL) == 0 && cap.n == 0, "callback returns a non-array -> honest 0, object freed");
  g_mode = 2;
  g_calls = 0; g_seen_key[0] = 'Z';
  rc = threatintel_collect(&ctx, &sink, NULL, NULL, runner, NULL);
  ok(rc == 3 && g_calls == 1 && !strcmp(g_seen_key, "(null)"), "env_key NULL means an ungated feed: runner called with a NULL key");

  printf("-- abuse.ch header\n");
  /* pthread_once is process-wide; reset it to exercise each configuration. */
  unsetenv("ABUSE_CH_AUTH_KEY"); unsetenv("THREATFOX_AUTH_KEY");
  g_abusech_once = (pthread_once_t)PTHREAD_ONCE_INIT; g_abusech_ok = 0; g_abusech_hdr[0] = 0;
  ok(abusech_auth_header() == NULL && !abusech_have_key(), "no key configured: no header, have_key 0");

  setenv("ABUSE_CH_AUTH_KEY", "k-123", 1);
  g_abusech_once = (pthread_once_t)PTHREAD_ONCE_INIT; g_abusech_ok = 0; g_abusech_hdr[0] = 0;
  ok(abusech_auth_header() && !strcmp(abusech_auth_header(), "Auth-Key: k-123") && abusech_have_key(), "header is 'Auth-Key: <key>'");
  setenv("ABUSE_CH_AUTH_KEY", "changed-later", 1);
  ok(!strcmp(abusech_auth_header(), "Auth-Key: k-123"), "read once: a later env change does not alter the built header");

  unsetenv("ABUSE_CH_AUTH_KEY"); setenv("THREATFOX_AUTH_KEY", "tf-1", 1);
  g_abusech_once = (pthread_once_t)PTHREAD_ONCE_INIT; g_abusech_ok = 0; g_abusech_hdr[0] = 0;
  ok(abusech_auth_header() && !strcmp(abusech_auth_header(), "Auth-Key: tf-1"), "THREATFOX_AUTH_KEY accepted as the fallback name");

  { char longk[400]; memset(longk, 'k', sizeof longk - 1); longk[sizeof longk - 1] = 0;
    setenv("ABUSE_CH_AUTH_KEY", longk, 1);
    g_abusech_once = (pthread_once_t)PTHREAD_ONCE_INIT; g_abusech_ok = 0; g_abusech_hdr[0] = 0;
    ok(abusech_auth_header() == NULL && !abusech_have_key(), "over-long key is IGNORED, not truncated into a wrong credential"); }
  { char fit[160]; memset(fit, 'k', 149); fit[149] = 0;       /* "Auth-Key: " is 10 chars: 159 total fits in 160 */
    setenv("ABUSE_CH_AUTH_KEY", fit, 1);
    g_abusech_once = (pthread_once_t)PTHREAD_ONCE_INIT; g_abusech_ok = 0; g_abusech_hdr[0] = 0;
    ok(abusech_auth_header() != NULL && strlen(abusech_auth_header()) == 159, "longest key that fits (159-byte header) is kept whole");
    fit[150] = 0; memset(fit + 149, 'k', 1);
    setenv("ABUSE_CH_AUTH_KEY", fit, 1);
    g_abusech_once = (pthread_once_t)PTHREAD_ONCE_INIT; g_abusech_ok = 0; g_abusech_hdr[0] = 0;
    ok(abusech_auth_header() == NULL, "one byte more (160-byte header, no room for NUL) is refused"); }

  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
