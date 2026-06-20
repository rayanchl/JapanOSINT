#include "llm.h"
#include "llm_worker.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static char *post_json(llm_client *c, const char *path, cJSON *body,
                       int timeout_ms) {
  char *url = url_join(c->base_url, path);
  char *payload = cJSON_PrintUnformatted(body);
  if (!url || !payload) { free(url); free(payload); cJSON_Delete(body); return NULL; }
  const char *hdrs[] = { "Content-Type: application/json", NULL };
  http_response r = {0};
  /* Route through the per-server worker: one thread owns generation for this
   * base_url, so callers queue here instead of contending on the server (or
   * blocking the request thread). c->http is no longer used for generation. */
  int rc = llm_worker_request(c->base_url, "POST", url, hdrs, payload,
                              strlen(payload),
                              timeout_ms > 0 ? timeout_ms : 30000, 1,
                              c->interactive, &r);
  free(url); free(payload); cJSON_Delete(body);
  if (rc != 0 || r.status < 200 || r.status >= 300 || !r.body) {
    http_response_free(&r);
    return NULL;
  }
  char *resp = strdup(r.body);
  http_response_free(&r);
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
  char *raw = post_json(c, "/completion", b, timeout_ms);
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
  char *raw = post_json(c, "/v1/chat/completions", b, timeout_ms);
  if (!raw) return NULL;
  cJSON *j = cJSON_Parse(raw);
  free(raw);
  if (!j) return NULL;
  char *out = NULL;
  cJSON *choices = cJSON_GetObjectItem(j, "choices");
  cJSON *ch0 = choices ? cJSON_GetArrayItem(choices, 0) : NULL;
  cJSON *msg = ch0 ? cJSON_GetObjectItem(ch0, "message") : NULL;
  cJSON *content = msg ? cJSON_GetObjectItem(msg, "content") : NULL;
  if (content && cJSON_IsString(content) && content->valuestring &&
      content->valuestring[0])
    out = strdup(content->valuestring);
  cJSON_Delete(j);
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
