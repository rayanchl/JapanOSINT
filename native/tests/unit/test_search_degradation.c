/* tests/unit/test_search_degradation.c — the OSINT search pipeline must not
 * report a failed stage as a completed one, and its phase-1 prompt must fit in
 * a context window.
 *
 * WHAT IS BEING PROTECTED, AND WHY IT NEEDS A TEST RATHER THAN A REVIEW.
 *
 * Both defects here were invisible in normal use, which is precisely why they
 * survived. A search run with llama-server down returned HTTP 200, walked the
 * full phase ladder (gpt_analyzing → services_assigned → agents_working →
 * aggregating → completed), finished at progress_percent 100, and produced a
 * plausible one-service payload. Nothing crashed, nothing logged an error, and
 * the only way to know the analysis had never happened was to already know.
 * That is the silent success house rule 1 forbids.
 *
 * Underneath it sat the reason the analysis could not have succeeded even with
 * the model up: prompt_analysis() embeds the entity-pivot service catalogue,
 * and unbounded that catalogue was 207,353 tokens on this registry. Every
 * llama-server answered every search with
 *
 *   request (207353 tokens) exceeds the available context size (16384 tokens)
 *
 * so llm_chat returned NULL on every run, on every model, at every realistic
 * context size — and the silent-degradation bug above is what turned that into
 * "search returns nothing useful" instead of an error anyone could act on.
 *
 * A prompt-size regression is exactly the shape of defect that comes back: any
 * future batch of collectors tagged collector="osint" grows the catalogue, and
 * nothing in a code review shows you the byte count. So it is asserted.
 *
 * It includes pipeline.c directly to reach the static note_llm_stage(); the
 * harness therefore links every object EXCEPT obj/core/pipeline.o and
 * obj/main.o. See tests/unit/run.sh. */

#include "../../core/pipeline.c"

#include <assert.h>

/* A phase-1 request must leave room for the completion inside the context that
 * scripts/start-llama.sh launches with (32768 tokens, 2048 of them reserved
 * for the answer). Bytes, not tokens, because bytes are what we can measure
 * here — English/JSON on a BPE vocabulary runs about 3.2 bytes per token, so
 * 96 KB is a deliberately generous ceiling around the ~58 KB the bounded
 * prompt actually produces. It is a REGRESSION guard, not a tight budget: it
 * catches a return to six figures, which is the failure that happened. */
#define ANALYSIS_PROMPT_CEILING_BYTES (96 * 1024)

/* ── 1. severity: an error degrades the run, a notice does not ──────────── */
static void test_severity_separates_failure_from_disclosure(void) {
  osint_request *rp = progress_create("unit-degr-1", "q", 1);
  assert(rp);

  assert(progress_is_degraded(rp) == 0 && "a fresh run is not degraded");

  /* A bounded view is a disclosure the consumer is owed, not a failure. Every
   * run on this registry bounds its service catalogue, so if a notice set the
   * degraded flag the flag would be on for every search ever made and would
   * stop carrying information — the same argument CLAUDE.md rule 4b makes for
   * not letting `stored=` cry wolf on every ordinary re-run. */
  progress_stage_note(rp, "analysis", "service_catalogue_bounded", "shown 10 of 99");
  assert(progress_is_degraded(rp) == 0 && "a notice must NOT degrade the run");

  progress_stage_error(rp, "analysis", "llm_unreachable", "nothing on :8080");
  assert(progress_is_degraded(rp) == 1 && "an error MUST degrade the run");

  char *js = progress_stage_errors_json(rp);
  assert(js);
  cJSON *arr = cJSON_Parse(js);
  free(js);
  assert(arr && cJSON_IsArray(arr) && cJSON_GetArraySize(arr) == 2);

  cJSON *n0 = cJSON_GetArrayItem(arr, 0);
  cJSON *n1 = cJSON_GetArrayItem(arr, 1);
  assert(!strcmp(cJSON_GetObjectItem(n0, "severity")->valuestring, "notice"));
  assert(!strcmp(cJSON_GetObjectItem(n1, "severity")->valuestring, "error"));
  assert(!strcmp(cJSON_GetObjectItem(n1, "code")->valuestring, "llm_unreachable"));
  /* Every row carries the stage it happened in, so three failed follow-up
   * rounds read as three facts and not as one repeated one. */
  assert(cJSON_GetObjectItem(n1, "stage") &&
         !strcmp(cJSON_GetObjectItem(n1, "stage")->valuestring, "analysis"));
  cJSON_Delete(arr);
  printf("  ok: notice vs error severity\n");
}

/* ── 2. the snapshot the SSE stream and /api/search/results serve ───────── */
static void test_snapshot_always_states_degradation(void) {
  osint_request *ok = progress_create("unit-degr-ok", "q", 1);
  assert(ok);
  char *js = progress_to_json(ok);
  assert(js);
  cJSON *j = cJSON_Parse(js);
  free(js);
  assert(j);
  /* Present even when false: a client must be able to read a MISSING key as
   * "this server is too old to tell me", never as "the run was fine". */
  cJSON *d = cJSON_GetObjectItem(j, "degraded");
  assert(d && cJSON_IsBool(d) && !cJSON_IsTrue(d));
  cJSON *se = cJSON_GetObjectItem(j, "stage_errors");
  assert(se && cJSON_IsArray(se) && cJSON_GetArraySize(se) == 0);
  cJSON_Delete(j);

  osint_request *bad = progress_create("unit-degr-bad", "q", 1);
  progress_stage_error(bad, "synthesis", "llm_timeout", "ran out of budget");
  js = progress_to_json(bad);
  j = cJSON_Parse(js);
  free(js);
  assert(j);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(j, "degraded")));
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(j, "stage_errors")) == 1);
  cJSON_Delete(j);
  printf("  ok: degraded + stage_errors always in the snapshot\n");
}

/* ── 3. every llm_status reaches the record under its own name ──────────── */
static void test_llm_failures_are_named_not_collapsed(void) {
  /* The whole point of llm_chat_ex: NULL used to mean five different things,
   * and "llama-server is not running" and "llama-server is up but this prompt
   * took longer than the budget" send an operator to opposite ends of the
   * system. Each must arrive as its own code. */
  const struct { llm_status st; const char *code; } cases[] = {
    { LLM_ERR_UNREACHABLE, "llm_unreachable" },
    { LLM_ERR_TIMEOUT,     "llm_timeout"     },
    { LLM_ERR_HTTP,        "llm_http_error"  },
    { LLM_ERR_EMPTY,       "llm_empty_response" },
    { LLM_ERR_BAD_REQUEST, "llm_bad_request" },
  };
  for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
    char id[64];
    snprintf(id, sizeof id, "unit-degr-llm-%u", i);
    osint_request *rp = progress_create(id, "q", 1);
    int usable = note_llm_stage(rp, "analysis", "http://localhost:8080",
                                cases[i].st, 503, NULL, 1);
    assert(usable == 0 && "a failed call is never 'usable'");
    char *js = progress_stage_errors_json(rp);
    cJSON *arr = cJSON_Parse(js);
    free(js);
    assert(arr && cJSON_GetArraySize(arr) == 1);
    cJSON *row = cJSON_GetArrayItem(arr, 0);
    assert(!strcmp(cJSON_GetObjectItem(row, "code")->valuestring, cases[i].code));
    /* The detail must name the host, or the verdict is unactionable. */
    assert(strstr(cJSON_GetObjectItem(row, "detail")->valuestring,
                  "http://localhost:8080"));
    cJSON_Delete(arr);
  }

  /* A model that ANSWERED but produced nothing we could use is a different
   * fault from one that never answered, and it must carry an excerpt of what
   * it actually said — otherwise the verdict is "unparsable" with zero
   * evidence. */
  osint_request *rp = progress_create("unit-degr-junk", "q", 1);
  int usable = note_llm_stage(rp, "analysis", "http://localhost:8080", LLM_OK, 200,
                              "i....i....i....", 0 /* nothing parsed out */);
  assert(usable == 0);
  char *js = progress_stage_errors_json(rp);
  cJSON *arr = cJSON_Parse(js);
  free(js);
  cJSON *row = cJSON_GetArrayItem(arr, 0);
  assert(!strcmp(cJSON_GetObjectItem(row, "code")->valuestring,
                 "llm_unusable_output"));
  assert(strstr(cJSON_GetObjectItem(row, "detail")->valuestring, "i....i"));
  cJSON_Delete(arr);

  /* And a call that worked records nothing at all. */
  osint_request *good = progress_create("unit-degr-good", "q", 1);
  assert(note_llm_stage(good, "analysis", "http://localhost:8080",
                        LLM_OK, 200, "{\"entities\":[]}", 1) == 1);
  assert(progress_is_degraded(good) == 0);
  printf("  ok: each llm failure reaches the record under its own code\n");
}

/* ── 4. the catalogue lists entity pivots only, and says what it bounded ── */
static void test_catalogue_excludes_scheduled_feeds(void) {
  osint_catalogue_note note = {0};
  char *list = osint_services_list_bounded(&note);
  assert(list);
  assert(note.total > 0 && "no entity-pivot services registered?");

  /* The filter used to be collector=="osint" alone, which swept in thousands
   * of SCHEDULED bulk feeds that fetch the same body whatever entity you hand
   * them. House rule 3: interval > 0 means scheduled, interval == 0 means
   * on-demand pivot — and only the second kind can be routed an entity. */
  int pivots = 0, scheduled_osint = 0;
  const source_def **all = registry_all();
  for (int i = 0; i < registry_count(); i++) {
    if (!all[i]->collector || strcmp(all[i]->collector, "osint")) continue;
    if (all[i]->update_interval_sec == 0) pivots++;
    else scheduled_osint++;
  }
  assert(note.total == pivots &&
         "catalogue total must be the on-demand pivots, not every osint row");
  if (scheduled_osint > 0) {
    /* Spot-check that a known scheduled row is absent from the text. */
    for (int i = 0; i < registry_count(); i++) {
      const source_def *d = all[i];
      if (!d->collector || strcmp(d->collector, "osint")) continue;
      if (d->update_interval_sec == 0) continue;
      assert(!strstr(list, d->id) &&
             "a scheduled feed must not be offered as an entity pivot");
      break;
    }
  }

  /* Rule 2: a bounded view states its bound IN BAND, where the consumer that
   * is being bounded can read it. */
  if (note.truncated || !note.descriptions)
    assert(strstr(list, "CATALOGUE BOUNDED") &&
           "a bounded catalogue must say so inside the prompt text");
  assert(note.shown <= note.total);
  assert(note.shown > 0 && "bounding must not empty the catalogue");
  printf("  ok: catalogue = %d of %d entity pivots (%d scheduled rows excluded)\n",
         note.shown, note.total, scheduled_osint);
  free(list);
}

/* ── 5. the enum the model may answer from == what it was shown ─────────── */
static void test_schema_enum_matches_the_catalogue(void) {
  osint_catalogue_note note = {0};
  char *list = osint_services_list_bounded(&note);
  assert(list);
  free(list);

  char *schema = osint_analysis_schema_dynamic_limited(note.shown);
  if (!schema) { printf("  skip: no osint_analysis schema on disk\n"); return; }
  cJSON *s = cJSON_Parse(schema);
  free(schema);
  assert(s);
  cJSON *props = cJSON_GetObjectItem(s, "properties");
  cJSON *rs = props ? cJSON_GetObjectItem(props, "recommended_services") : NULL;
  cJSON *rsi = rs ? cJSON_GetObjectItem(rs, "items") : NULL;
  cJSON *en = rsi ? cJSON_GetObjectItem(rsi, "enum") : NULL;
  assert(en && cJSON_IsArray(en));
  /* Letting these disagree means either offering the model names it was never
   * shown the meaning of, or rejecting names it was explicitly offered. */
  assert(cJSON_GetArraySize(en) == note.shown &&
         "schema enum must be exactly the services the prompt listed");
  cJSON_Delete(s);
  printf("  ok: schema enum == %d listed services\n", note.shown);
}

/* ── 6. THE regression: the phase-1 prompt has to fit a context window ──── */
static void test_analysis_prompt_fits_a_context_window(void) {
  osint_catalogue_note note = {0};
  char *list = osint_services_list_bounded(&note);
  assert(list);
  char *p = prompt_analysis("who owns example.com", list);
  assert(p);
  size_t bytes = strlen(p);
  printf("  analysis prompt: %zu bytes (%d of %d services, %s)\n",
         bytes, note.shown, note.total,
         note.descriptions ? "with descriptions" : "ids only");
  assert(bytes < ANALYSIS_PROMPT_CEILING_BYTES &&
         "phase-1 prompt is back over the ceiling — llama-server will reject "
         "every search with 'exceeds the available context size'; lower "
         "JO_PROMPT_SERVICE_CATALOGUE_CHARS or check what grew");
  free(p);
  free(list);
  printf("  ok: analysis prompt within the ceiling\n");
}

/* ── 7. attribution never states a count it did not measure ─────────────── */
static void test_source_attribution_states_only_what_was_measured(void) {
  /* osint_dispatch builds results.services[i].sources two ways. When the
   * collector labelled its emits (sub_source_id) the per-source record count
   * is real. When it did not, all we have is the HTTP host log — which counts
   * REQUESTS, not records — and the code used to write the service's total
   * into every host row: SOCIAL_EMAIL contacted 60 hosts, emitted 187 records,
   * and its attribution claimed 187 records for each of the 60, including the
   * ones whose status was "error". 11,220 records asserted out of 187 real.
   *
   * The invariant, whichever branch produced a row: a numeric `records` means
   * it was measured, and an unmeasured one is null with the `requests` we did
   * measure beside it. Never a number we invented. */
  /* db_open (not db_attach): the scratch database the harness points JO_DB at
   * does not exist yet, and attach opens without creating. */
  db_handle db = {0};
  if (db_open(&db, NULL, NULL) != 0) { printf("  skip: no scratch db\n"); return; }
  http_client *http = http_client_new();
  llm_client llm; llm_init(&llm, http);

  /* Several services, because the two attribution branches are chosen by
   * whether the COLLECTOR labels its emits — which of them does is not this
   * test's business, and pinning one service would make the test a hostage to
   * that collector's implementation. The invariant holds for every row of
   * every service either way. */
  static const struct { const char *svc, *entity, *type; } CASES[] = {
    { "DNS_RECORDS",    "example.com", "domain" },
    { "IP_GEOLOCATION", "8.8.8.8",     "ip"     },
    { "REVERSE_DNS",    "8.8.8.8",     "ip"     },
  };
  int rows = 0, host_rows = 0;
  for (unsigned ci = 0; ci < sizeof CASES / sizeof *CASES; ci++) {
    osint_result r;
    osint_dispatch(&db, &llm, CASES[ci].svc, CASES[ci].entity, CASES[ci].type,
                   NULL, &r);
    assert(r.sources_json && "dispatch must always attribute, even on failure");
    cJSON *arr = cJSON_Parse(r.sources_json);
    assert(arr && cJSON_IsArray(arr));
    cJSON *row;
    cJSON_ArrayForEach(row, arr) {
      cJSON *name = cJSON_GetObjectItem(row, "name");
      cJSON *recs = cJSON_GetObjectItem(row, "records");
      cJSON *reqs = cJSON_GetObjectItem(row, "requests");
      assert(name && cJSON_IsString(name) && name->valuestring[0]);
      assert(recs && "every attribution row carries a records key");
      rows++;
      if (cJSON_IsNull(recs)) {
        host_rows++;
        assert(reqs && cJSON_IsNumber(reqs) &&
               "an unmeasured record count must still report the requests we "
               "DID count");
      } else {
        assert(cJSON_IsNumber(recs));
        /* A measured count belongs to one labelled source, so it can never
         * exceed what the service emitted in total — which is the arithmetic
         * the old host branch broke (187 × 60). */
        assert(recs->valuedouble <= (double)r.records &&
               "a per-source count above the service total is not a count");
      }
    }
    cJSON_Delete(arr);
    osint_result_free(&r);
  }
  printf("  ok: %d attribution row(s) across %u services, "
         "%d host-attributed and honestly null\n",
         rows, (unsigned)(sizeof CASES / sizeof *CASES), host_rows);
  http_client_free(http);
  db_close(&db);
}

int main(void) {
  printf("test_search_degradation\n");
  test_severity_separates_failure_from_disclosure();
  test_snapshot_always_states_degradation();
  test_llm_failures_are_named_not_collapsed();
  test_catalogue_excludes_scheduled_feeds();
  test_schema_enum_matches_the_catalogue();
  test_analysis_prompt_fits_a_context_window();
  test_source_attribution_states_only_what_was_measured();
  printf("test_search_degradation: OK\n");
  return 0;
}
