/* tests/unit/test_relev_check.c — core/relevance.c's own contract
 * (relev_sample_add / relev_check) and core/qembed.c (qembed_enabled,
 * qembed_texts, qembed_index_usable, qembed_intel_knn), driven directly.
 * test_relevance.c covers them only through osint_dispatch(); the edges here
 * (bounds, echo keys, thresholds, batching, gates) are not reachable that way.
 *
 * Includes relevance.c, so run.sh links everything except
 * obj/core/relevance.o and obj/main.o. qembed.c is linked normally. */
#include "../../core/relevance.c"
#include "embed_stub.h"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

static intel_item mk(const char *title, const char *summary, const char *props) {
  intel_item it = {0};
  it.remote_key = "k";
  it.title = title;
  it.summary = summary;
  it.properties_json = props;
  it.record_type = "record";
  return it;
}
static void fill(relev_sample *s, int n, const char *fmt, const char *entity_echo) {
  for (int i = 0; i < n; i++) {
    char t[160], p[256];
    snprintf(t, sizeof t, fmt, i);
    snprintf(p, sizeof p, "{\"query\":\"%s\",\"n\":%d}", entity_echo, i);
    intel_item it = mk(t, t, p);
    relev_sample_add(s, &it);
  }
}

int main(void) {
  unsetenv("JO_EMBED_URL"); unsetenv("JO_EMBED_QUERY_URL");
  unsetenv("JO_RELEVANCE_SIM_MIN"); unsetenv("JO_RELEVANCE_CHECK");
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);

  printf("-- inert (no embedding server)\n");
  ok(!qembed_enabled(), "qembed_enabled() false with JO_EMBED_URL unset");
  relev_sample s = {0};
  intel_item it = mk("acme shipping", "x", "{}");
  relev_sample_add(&s, &it);
  ok(s.n == 0, "sample_add keeps nothing when inert");
  ok(relev_check(&s, "acme shipping", 10) == NULL, "check returns NULL when inert");
  float *v = (float *)0x1; int d = 7; llm_status st = LLM_OK;
  const char *one[1] = { "a" };
  ok(qembed_texts(one, 1, &v, &d, &st) == -1 && v == NULL && d == 0 && st == LLM_ERR_BAD_REQUEST,
     "qembed_texts inert: -1, outputs cleared, BAD_REQUEST");
  char why[200];
  ok(qembed_index_usable(&db, NULL, &d, why, sizeof why) == 0 && strstr(why, "JO_EMBED_URL"),
     "index_usable inert: refuses and says why");
  vecindex_hit *h = (vecindex_hit *)0x1;
  ok(qembed_intel_knn(&db, "legacy", "hello", 5, &h, why, sizeof why) == -1 && h == NULL && why[0],
     "intel_knn inert: -1 with a reason, never a fabricated empty");
  relev_sample_free(&s);
  relev_sample_free(NULL);

  if (estub_start(18361, 64, "stub-bow") != 0) {
    printf("test_relev_check: stub unavailable, server cases skipped\n");
    db_close(&db);
    return g_fail ? 1 : 0;
  }
  estub_use();
  ok(qembed_enabled(), "enabled once JO_EMBED_URL is set");

  printf("-- relev_sample_add\n");
  relev_sample_add(NULL, &it);
  relev_sample_add(&s, NULL);
  ok(s.n == 0, "NULL args are ignored");
  intel_item e = mk("Acme", "Acme Corp", "{\"query\":\"SECRETQUERY\",\"endpoint\":\"http://x\",\"field\":\"visible\"}");
  relev_sample_add(&s, &e);
  ok(s.n == 1, "one record sampled");
  ok(strstr(s.text[0], "visible") && !strstr(s.text[0], "secretquery"),
     "echo keys (query/endpoint) stripped, real properties kept");
  ok(strstr(s.text[0], "acme corp") != NULL, "text is jpnorm-folded (lower case)");
  ok(!strcmp(s.label[0], "Acme Acme Corp"), "label = title + summary");
  intel_item same = mk("Same", "Same", "{}");
  relev_sample_add(&s, &same);
  ok(!strcmp(s.label[1], "Same"), "summary equal to title is not repeated");
  intel_item bodyonly = {0};
  bodyonly.remote_key = "b"; bodyonly.body = "only a body here";
  relev_sample_add(&s, &bodyonly);
  ok(s.n == 3 && !strcmp(s.label[2], "only a body here"), "body is the label when title/summary are absent");
  intel_item empty = {0};
  empty.remote_key = "z";
  relev_sample_add(&s, &empty);
  ok(s.n == 4, "an all-empty record does not crash");
  intel_item notjson = mk("t", "t", "plain text, not json");
  relev_sample_add(&s, &notjson);
  ok(strstr(s.text[4], "plain text"), "non-JSON properties are used verbatim");
  relev_sample_free(&s);
  ok(s.n == 0, "free resets n");
  memset(&s, 0, sizeof s);
  for (int i = 0; i < RELEV_SAMPLE_MAX + 10; i++) relev_sample_add(&s, &same);
  ok(s.n == RELEV_SAMPLE_MAX, "sample bounded at RELEV_SAMPLE_MAX");
  relev_sample_free(&s);
  {
    char big[10000];
    memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = 0;
    memset(&s, 0, sizeof s);
    intel_item bg = mk(big, big, big);
    relev_sample_add(&s, &bg);
    ok(s.n == 1 && strlen(s.text[0]) < RELEV_TEXT_MAX && strlen(s.label[0]) < 1024, "oversize text bounded, not overrun");
    relev_sample_free(&s);
  }

  printf("-- relev_check\n");
  memset(&s, 0, sizeof s);
  fill(&s, 4, "zoning permit bakery %d", "zebra");
  ok(relev_check(&s, "zebra corp", 4) == NULL, "fewer than RELEV_MIN_RECORDS: no verdict");
  fill(&s, 20, "zoning permit bakery %d", "zebra");
  ok(relev_check(&s, "ab", 24) == NULL, "entity shorter than RELEV_MIN_ENTITY: no verdict");
  ok(relev_check(&s, NULL, 24) == NULL && relev_check(NULL, "zebra corp", 24) == NULL, "NULL entity / sample");
  setenv("JO_RELEVANCE_CHECK", "0", 1);
  ok(relev_check(&s, "zebra corp", 24) == NULL, "JO_RELEVANCE_CHECK=0 disables it");
  unsetenv("JO_RELEVANCE_CHECK");
  cJSON *r = relev_check(&s, "zebra corp", 24);
  ok(r != NULL, "records that never name the entity, nor relate to it: suspect");
  if (r) {
    ok(!strcmp(cJSON_GetObjectItem(r, "verdict")->valuestring, "relevance-suspect"), "verdict string");
    ok(cJSON_GetObjectItem(r, "sampled")->valueint == s.n, "sampled == sample size");
    ok(cJSON_GetObjectItem(r, "of_records")->valueint == 24, "of_records echoes the service total");
    ok(cJSON_GetObjectItem(r, "carrying_entity")->valueint == 0, "echoed query key did not count as carrying");
    ok(cJSON_GetObjectItem(r, "similarity_threshold")->valuedouble > 0, "threshold disclosed");
    ok(cJSON_GetObjectItem(r, "note") != NULL, "in-band note present");
    cJSON_Delete(r);
  }
  relev_sample_free(&s);

  /* lexical clear sends nothing to the server */
  memset(&s, 0, sizeof s);
  fill(&s, 10, "acme annual report %d", "zebra");
  int posts = estub_posts();
  ok(relev_check(&s, "ACME", 10) == NULL && estub_posts() == posts,
     "carrying the entity (case-folded): cleared without a request");
  relev_sample_free(&s);

  /* every word of a multi-word entity, in any order, counts */
  memset(&s, 0, sizeof s);
  fill(&s, 10, "shipping notice for acme %d", "zebra");
  posts = estub_posts();
  ok(relev_check(&s, "acme shipping", 10) == NULL && estub_posts() == posts,
     "all words of the entity present (not adjacent): cleared lexically");
  relev_sample_free(&s);

  /* exactly at the lexical pass boundary: 50% carrying clears, under it asks the model */
  memset(&s, 0, sizeof s);
  fill(&s, 5, "acme annual report %d", "zebra");
  fill(&s, 5, "zoning permit bakery %d", "zebra");
  posts = estub_posts();
  ok(relev_check(&s, "acme", 10) == NULL && estub_posts() == posts, "5 of 10 carrying == RELEV_LEXICAL_PASS: cleared");
  relev_sample_free(&s);
  memset(&s, 0, sizeof s);
  fill(&s, 4, "acme annual report %d", "zebra");
  fill(&s, 6, "zoning permit bakery %d", "zebra");
  posts = estub_posts();
  r = relev_check(&s, "acme", 10);
  ok(r == NULL && estub_posts() > posts,
     "4 of 10 carrying is below the pass share so the model is asked, but 4 > RELEV_SUSPECT_MAX still clears it");
  cJSON_Delete(r);
  relev_sample_free(&s);

  /* semantic rescue: no shared word with the entity, but the stub says related */
  memset(&s, 0, sizeof s);
  fill(&s, 10, "zebra corp registry filing %d", "x");
  ok(relev_check(&s, "zebra corp holdings", 10) == NULL, "partial word overlap related enough: not accused");
  relev_sample_free(&s);

  /* a dead embedding server gives no verdict rather than an accusation */
  estub_stop();
  setenv("JO_EMBED_URL", "http://127.0.0.1:1", 1);
  unsetenv("JO_EMBED_QUERY_URL");
  memset(&s, 0, sizeof s);
  setenv("JO_EMBED_URL", "http://127.0.0.1:1", 1);
  fill(&s, 10, "zoning permit bakery %d", "zebra");
  ok(relev_check(&s, "zebra corp", 10) == NULL, "embedding failure: no verdict (a lexical miss alone is not proof)");
  relev_sample_free(&s);
  unsetenv("JO_EMBED_URL");

  if (estub_start(18362, 64, "stub-bow") == 0) {
    estub_use();
    printf("-- qembed_texts\n");
    const char *txt[70];
    char buf[70][16];
    for (int i = 0; i < 70; i++) { snprintf(buf[i], sizeof buf[i], "word%d", i); txt[i] = buf[i]; }
    ok(qembed_texts(txt, 0, &v, &d, &st) == -1, "n=0 refused");
    ok(qembed_texts(NULL, 3, &v, &d, &st) == -1, "NULL texts refused");
    ok(qembed_texts(txt, 3, NULL, &d, &st) == -1 && qembed_texts(txt, 3, &v, NULL, &st) == -1, "NULL out/dim refused");
    int before = estub_posts();
    ok(qembed_texts(txt, 70, &v, &d, &st) == 0 && st == LLM_OK && d == 64 && v, "70 texts embed, dim 64");
    ok(estub_posts() - before == 3, "70 texts = ceil(70/QEMBED_BATCH=32) = 3 requests");
    free(v);
    /* repeat a text across a batch boundary: its row must be identical, and a
     * different text's must not be (one-hot buckets can collide by design,
     * so compare against a text known to hash elsewhere) */
    txt[40] = txt[0]; txt[65] = txt[1];
    ok(qembed_texts(txt, 70, &v, &d, &st) == 0, "re-embed with repeats");
    ok(!memcmp(v, v + 40 * 64, 64 * sizeof(float)), "row 40 (batch 2) equals row 0: offsets are right");
    ok(!memcmp(v + 64, v + 65 * 64, 64 * sizeof(float)), "row 65 (batch 3) equals row 1");
    int differs = 0;
    for (int i = 2; i < 70; i++) if (memcmp(v, v + (size_t)i * 64, 64 * sizeof(float))) differs++;
    ok(differs > 0, "rows are not all copies of one another");
    ok(vec_cosine(v + 33 * 64, v + 33 * 64, 64) > 0.99, "row 33 (second batch) is a unit vector");
    free(v);
    const char *nulls[2] = { NULL, "x" };
    ok(qembed_texts(nulls, 2, &v, &d, &st) == 0, "a NULL text embeds as empty, not a crash");
    free(v);

    printf("-- qembed_index_usable / qembed_intel_knn gates\n");
    ok(qembed_index_usable(&db, NULL, &d, why, sizeof why) == 0 && why[0], "NULL spec: refused with a reason");
    ok(qembed_index_usable(NULL, NULL, NULL, NULL, 0) == 0, "all-NULL arguments are safe");
    ok(qembed_intel_knn(&db, "legacy", "", 5, &h, why, sizeof why) == -1, "empty text refused");
    ok(qembed_intel_knn(&db, "legacy", "x", 0, &h, why, sizeof why) == -1, "k<=0 refused");
    ok(qembed_intel_knn(&db, "legacy", "x", 5, NULL, why, sizeof why) == -1, "NULL out refused");
    ok(qembed_intel_knn(&db, "legacy", "tokyo", 5, &h, why, sizeof why) == -1 && h == NULL && why[0],
       "no intel index built: -1 with a reason (unavailable is not an empty answer)");
    estub_stop();
  }
  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
