#include "llm.h"
#include "llm_worker.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Generation calls are serialized per llama-server by the global LLM worker
 * (core/llm_worker.c): each base_url has one dedicated thread, so concurrent
 * pipelines/rounds/pods queue rather than piling onto the single-slot server,
 * and the request-serving thread never blocks on a model call. The main 20B
 * (LLM_BASE_URL, :8080) and the fast suggest server (:8081) get independent
 * workers, so typeahead never waits behind a 20B job. */

void llm_init(llm_client *c, http_client *http) {
  c->http = http;
  const char *b = getenv("LLM_BASE_URL");
  c->base_url = (b && *b) ? b : "http://localhost:8080";
  c->interactive = 0;          /* background by default (scheduler/pods) */
}

void llm_init_suggest(llm_client *c, http_client *http) {
  c->http = http;
  const char *s = getenv("LLM_SUGGEST_BASE_URL");
  const char *b = (s && *s) ? s : getenv("LLM_BASE_URL");
  c->base_url = (b && *b) ? b : "http://localhost:8080";
  c->interactive = 1;          /* user-facing typeahead */
}

static char *url_join(const char *base, const char *path) {
  size_t n = strlen(base);
  int trim = (n > 0 && base[n - 1] == '/') ? 1 : 0;
  char *u = malloc(n + strlen(path) + 1);
  if (!u) return NULL;
  memcpy(u, base, n - trim);
  strcpy(u + n - trim, path);
  return u;
}

const char *llm_status_code(llm_status s) {
  switch (s) {
    case LLM_OK:              return "ok";
    case LLM_ERR_BAD_REQUEST: return "llm_bad_request";
    case LLM_ERR_UNREACHABLE: return "llm_unreachable";
    case LLM_ERR_TIMEOUT:     return "llm_timeout";
    case LLM_ERR_HTTP:        return "llm_http_error";
    case LLM_ERR_EMPTY:       return "llm_empty_response";
  }
  return "llm_error";
}

/* `st`/`http` (either may be NULL) report WHY a NULL came back — see the
 * llm_status comment in llm.h. http_request's contract is the discriminator we
 * need and it was being thrown away: it returns 0 for any COMPLETED exchange
 * whatever the status, and non-zero only when no exchange happened at all. So
 * rc != 0 (or a status of 0) is "nothing is listening on base_url", which is
 * the operational condition the search pipeline has to be able to name, and a
 * non-2xx is a server that is up and refusing — a different fix entirely. */
static char *post_json(llm_client *c, const char *path, cJSON *body,
                       int timeout_ms, llm_status *st, long *http) {
  if (st)   *st = LLM_ERR_BAD_REQUEST;
  if (http) *http = 0;
  char *url = url_join(c->base_url, path);
  char *payload = cJSON_PrintUnformatted(body);
  if (!url || !payload) { free(url); free(payload); cJSON_Delete(body); return NULL; }
  const char *hdrs[] = { "Content-Type: application/json", NULL };
  http_response r = {0};
  /* Route through the per-server worker: one thread owns generation for this
   * base_url, so callers queue here instead of contending on the server (or
   * blocking the request thread). c->http is no longer used for generation. */
  int budget = timeout_ms > 0 ? timeout_ms : 30000;
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  int rc = llm_worker_request(c->base_url, "POST", url, hdrs, payload,
                              strlen(payload), budget, 1,
                              c->interactive, &r);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L
                  + (t1.tv_nsec - t0.tv_nsec) / 1000000L;
  free(url); free(payload); cJSON_Delete(body);
  if (http) *http = r.status;
  if (rc != 0 || r.status == 0) {
    /* SPLIT ON THE CLOCK, because "unreachable" and "too slow" send an
     * operator to opposite ends of the system and http_request cannot tell us
     * which it was — it reports any failed exchange as non-zero with status 0.
     * A local llama-server on CPU spends ~100 s on prompt-eval for the 18k-token
     * analysis request, blows the 60 s budget, and was reported as "llama-server
     * is not running" while it was sitting there working. The wall clock is the
     * one thing we can measure ourselves: a call that consumed essentially its
     * whole budget timed out; one that failed immediately had nothing to talk
     * to. 90% of budget, because the client's own timeout fires slightly early
     * and the queue wait before it is not free either. */
    if (st) *st = (elapsed_ms >= (long)budget * 9 / 10) ? LLM_ERR_TIMEOUT
                                                        : LLM_ERR_UNREACHABLE;
    http_response_free(&r);
    return NULL;
  }
  if (r.status < 200 || r.status >= 300) {
    if (st) *st = LLM_ERR_HTTP;
    http_response_free(&r);
    return NULL;
  }
  if (!r.body) {
    if (st) *st = LLM_ERR_EMPTY;
    http_response_free(&r);
    return NULL;
  }
  char *resp = strdup(r.body);
  http_response_free(&r);
  if (st) *st = resp ? LLM_OK : LLM_ERR_EMPTY;
  return resp;
}

/* Anti-degeneration sampler floor, applied to every generation call.
 * llama-server's default repeat_penalty is 1.0 (disabled); with GBNF masking
 * pushing a reasoning model (gpt-oss-20b) off-distribution, generation can
 * collapse onto a repeating low-entropy token — the "i....i...." reasoning and
 * {"value":"i","type":"image"} junk-entity bug. A modest repeat penalty plus a
 * top-k / nucleus / min-p floor stops the loop without hurting normal output.
 * Overridable via env so the floor can be tuned without a rebuild. */
static double env_double(const char *k, double dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  double d = strtod(v, &end);
  return (end && end != v) ? d : dflt;
}
static void add_sampler_defaults(cJSON *b) {
  cJSON_AddNumberToObject(b, "repeat_penalty", env_double("LLM_REPEAT_PENALTY", 1.15));
  cJSON_AddNumberToObject(b, "repeat_last_n", env_double("LLM_REPEAT_LAST_N", 256));
  cJSON_AddNumberToObject(b, "top_k", env_double("LLM_TOP_K", 40));
  cJSON_AddNumberToObject(b, "top_p", env_double("LLM_TOP_P", 0.95));
  cJSON_AddNumberToObject(b, "min_p", env_double("LLM_MIN_P", 0.05));
}

char *llm_complete(llm_client *c, const char *prompt, const char *grammar,
                   int max_tokens, double temperature, int timeout_ms) {
  cJSON *b = cJSON_CreateObject();
  cJSON_AddStringToObject(b, "prompt", prompt);
  cJSON_AddNumberToObject(b, "n_predict", max_tokens > 0 ? max_tokens : 2048);
  cJSON_AddNumberToObject(b, "temperature", temperature >= 0 ? temperature : 0.2);
  cJSON_AddBoolToObject(b, "cache_prompt", 1);
  add_sampler_defaults(b);
  if (grammar && *grammar) cJSON_AddStringToObject(b, "grammar", grammar);
  char *raw = post_json(c, "/completion", b, timeout_ms, NULL, NULL);
  if (!raw) return NULL;
  cJSON *j = cJSON_Parse(raw);
  free(raw);
  if (!j) return NULL;
  cJSON *content = cJSON_GetObjectItem(j, "content");
  char *out = (content && cJSON_IsString(content) && content->valuestring &&
               content->valuestring[0])
                ? strdup(content->valuestring) : NULL;
  cJSON_Delete(j);
  return out;
}

char *llm_chat(llm_client *c, const char *messages_json, const char *json_schema,
               int max_tokens, double temperature, int timeout_ms) {
  return llm_chat_ex(c, messages_json, json_schema, max_tokens, temperature,
                     timeout_ms, NULL, NULL);
}

char *llm_chat_ex(llm_client *c, const char *messages_json,
                  const char *json_schema, int max_tokens, double temperature,
                  int timeout_ms, llm_status *out_status, long *out_http) {
  if (out_status) *out_status = LLM_ERR_BAD_REQUEST;
  if (out_http)   *out_http = 0;
  cJSON *msgs = cJSON_Parse(messages_json);
  if (!msgs) return NULL;
  cJSON *b = cJSON_CreateObject();
  const char *model = getenv("LLM_MODEL");
  cJSON_AddStringToObject(b, "model", model && *model ? model : "local-model");
  cJSON_AddItemToObject(b, "messages", msgs);
  cJSON_AddNumberToObject(b, "max_tokens", max_tokens > 0 ? max_tokens : 2048);
  cJSON_AddNumberToObject(b, "temperature", temperature >= 0 ? temperature : 0.1);
  add_sampler_defaults(b);
  /* Constrain the FINAL channel via response_format json_schema (NOT a raw
   * `grammar`): on a reasoning model launched with --jinja --reasoning-format,
   * llama.cpp applies the schema lazily once the harmony final channel begins,
   * so the model still reasons in its own channel — a raw grammar engages from
   * token 0 and suppresses reasoning, collapsing extraction quality. */
  if (json_schema && *json_schema) {
    cJSON *schema = cJSON_Parse(json_schema);
    if (schema) {
      cJSON *rf = cJSON_CreateObject();
      cJSON_AddStringToObject(rf, "type", "json_schema");
      cJSON *js = cJSON_CreateObject();
      cJSON_AddStringToObject(js, "name", "structured_output");
      cJSON_AddBoolToObject(js, "strict", 1);
      cJSON_AddItemToObject(js, "schema", schema);  /* takes ownership */
      cJSON_AddItemToObject(rf, "json_schema", js);
      cJSON_AddItemToObject(b, "response_format", rf);
    }
  }
  char *raw = post_json(c, "/v1/chat/completions", b, timeout_ms, out_status,
                        out_http);
  if (!raw) return NULL;
  /* From here on the exchange succeeded, so any remaining failure is a body we
   * could not use — a distinct verdict from "the host is down", and the one a
   * caller answers by fixing the prompt/schema rather than the deployment. */
  cJSON *j = cJSON_Parse(raw);
  free(raw);
  if (!j) { if (out_status) *out_status = LLM_ERR_EMPTY; return NULL; }
  char *out = NULL;
  cJSON *choices = cJSON_GetObjectItem(j, "choices");
  cJSON *ch0 = choices ? cJSON_GetArrayItem(choices, 0) : NULL;
  cJSON *msg = ch0 ? cJSON_GetObjectItem(ch0, "message") : NULL;
  cJSON *content = msg ? cJSON_GetObjectItem(msg, "content") : NULL;
  if (content && cJSON_IsString(content) && content->valuestring &&
      content->valuestring[0])
    out = strdup(content->valuestring);
  cJSON_Delete(j);
  if (out_status) *out_status = out ? LLM_OK : LLM_ERR_EMPTY;
  return out;
}

int llm_healthy(llm_client *c) {
  char *url = url_join(c->base_url, "/health");
  if (!url) return 0;
  http_response r = {0};
  int rc = http_request(c->http, "GET", url, NULL, NULL, 0, 5000, 0, &r);
  free(url);
  int ok = (rc == 0 && r.status >= 200 && r.status < 300);
  http_response_free(&r);
  return ok;
}
