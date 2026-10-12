/* tests/unit/test_prompt_rank.c — which records the bounded prompt view shows
 * (core/prompt_rank.c, results_view_for_prompt in core/pipeline.c).
 *
 * WHAT IS BEING PROTECTED.
 *   1. INERT: with no JO_EMBED_URL the view is the positional one byte for
 *      byte — no ranker, no new keys.
 *   2. With a server, the 8 shown are the 8 most similar to the query, and the
 *      view SAYS so (records_selected_by, records_ranked) — a reader must not
 *      mistake a similarity pick for the upstream's first page.
 *   3. The stored result is untouched: only the view is reordered.
 *   4. A record embedded for round 1 is not sent again for round 2 or the
 *      synthesis (one batch per run, cached).
 *   5. A failing server turns the ranker off and the view falls back to
 *      positional, still labelled as such.
 *
 * Includes pipeline.c directly to reach the static view builder, so run.sh
 * links every object EXCEPT obj/core/pipeline.o and obj/main.o. */

#include "../../core/pipeline.c"
#include "embed_stub.h"

#include <assert.h>

/* 20 records; only #13, #15 and #17 talk about the query's words. */
static cJSON *make_results(void) {
  cJSON *results = cJSON_CreateArray();
  cJSON *svc = cJSON_CreateObject();
  cJSON_AddStringToObject(svc, "name", "TEST_SVC");
  cJSON *data = cJSON_AddObjectToObject(svc, "data");
  cJSON *recs = cJSON_AddArrayToObject(data, "records");
  for (int i = 0; i < 20; i++) {
    cJSON *r = cJSON_CreateObject();
    char t[96];
    if (i == 13 || i == 15 || i == 17)
      snprintf(t, sizeof t, "acme shipping holdings filing %d", i);
    else
      snprintf(t, sizeof t, "zoning permit bakery %d", i);
    cJSON_AddStringToObject(r, "title", t);
    cJSON_AddNumberToObject(r, "n", i);
    cJSON_AddItemToArray(recs, r);
  }
  cJSON_AddNumberToObject(data, "record_count", 20);
  cJSON_AddItemToArray(results, svc);
  /* a small service (under the bound) is never touched */
  cJSON *small = cJSON_CreateObject();
  cJSON_AddStringToObject(small, "name", "SMALL_SVC");
  cJSON *sd = cJSON_AddObjectToObject(small, "data");
  cJSON *sr = cJSON_AddArrayToObject(sd, "records");
  cJSON_AddItemToArray(sr, cJSON_CreateString("one"));
  cJSON_AddItemToArray(results, small);
  return results;
}

static cJSON *view_data(const char *view) {
  cJSON *v = cJSON_Parse(view);
  assert(v);
  return v;
}

static int shown_n(cJSON *v, int k) {
  cJSON *svc = cJSON_GetArrayItem(cJSON_GetObjectItem(v, "services"), 0);
  cJSON *r = cJSON_GetArrayItem(cJSON_GetObjectItem(cJSON_GetObjectItem(svc, "data"), "records"), k);
  return cJSON_GetObjectItem(r, "n")->valueint;
}

int main(void) {
  unsetenv("JO_EMBED_URL"); unsetenv("JO_EMBED_QUERY_URL"); unsetenv("JO_EMBED_MODEL");
  unsetenv("JO_PROMPT_RECORDS_PER_SERVICE");
  cJSON *results = make_results();
  char *before = cJSON_PrintUnformatted(results);

  /* 1. inert */
  prompt_rank *pr = prompt_rank_new("acme shipping");
  assert(pr == NULL);
  char *pos = results_view_for_prompt(results, pr);
  assert(!strstr(pos, "records_selected_by"));
  assert(strstr(pos, "only the first records are shown here"));
  cJSON *v = view_data(pos);
  for (int k = 0; k < 8; k++) assert(shown_n(v, k) == k);
  cJSON_Delete(v);
  printf("  inert: positional view, no new keys: ok\n");

  if (estub_start(18340, 256, "stub-bow") != 0) {
    printf("test_prompt_rank: stub unavailable, ranked cases skipped\n");
    free(pos); free(before); cJSON_Delete(results); estub_stop();
    return 0;
  }
  estub_use();

  /* 2 + 3 */
  pr = prompt_rank_new("acme shipping");
  assert(pr && prompt_rank_active(pr));
  char *ranked = results_view_for_prompt(results, pr);
  v = view_data(ranked);
  cJSON *d0 = cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(v, "services"), 0), "data");
  assert(!strcmp(cJSON_GetObjectItem(d0, "records_selected_by")->valuestring,
                 "similarity_to_query"));
  assert(cJSON_GetObjectItem(d0, "records_ranked")->valueint == 20);
  assert(cJSON_GetObjectItem(d0, "records_shown")->valueint == 8);
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(d0, "records")) == 8);
  /* the three that mention the query lead, in the upstream's order on ties */
  assert(shown_n(v, 0) == 13 && shown_n(v, 1) == 15 && shown_n(v, 2) == 17);
  cJSON *d1 = cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(v, "services"), 1), "data");
  assert(!cJSON_GetObjectItem(d1, "records_selected_by"));
  cJSON_Delete(v);
  char *after = cJSON_PrintUnformatted(results);
  assert(!strcmp(before, after));            /* stored result untouched */
  free(after);
  printf("  ranked: query-similar records lead, selection disclosed, stored "
         "result untouched: ok\n");

  /* 4. cached across views */
  long sent = prompt_rank_sent(pr);
  assert(sent == 1 + 20);                    /* the query + 20 distinct titles */
  int posts = estub_posts();
  char *again = results_view_for_prompt(results, pr);
  assert(!strcmp(again, ranked));
  assert(prompt_rank_sent(pr) == sent && estub_posts() == posts);
  free(again);
  printf("  second view (phase 2 / synthesis) sends nothing new: ok\n");
  prompt_rank_free(pr);

  /* 5. a server that stops answering: positional, ranker off */
  pr = prompt_rank_new("acme shipping");
  setenv("JO_EMBED_URL", "http://127.0.0.1:1", 1);
  char *fb = results_view_for_prompt(results, pr);
  assert(!prompt_rank_active(pr));
  assert(!strcmp(fb, pos));                  /* exactly the positional view */
  free(fb);
  prompt_rank_free(pr);
  printf("  embed failure: positional fallback, byte for byte: ok\n");

  free(ranked); free(pos); free(before);
  cJSON_Delete(results);
  estub_stop();
  printf("test_prompt_rank: ok\n");
  return 0;
}
