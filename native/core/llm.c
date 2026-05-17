#include "llm.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void llm_init(llm_client *c, http_client *http) {
  c->http = http;
  const char *b = getenv("LLM_BASE_URL");
  c->base_url = (b && *b) ? b : "http://localhost:8080";
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
  int rc = http_request(c->http, "POST", url, hdrs, payload, strlen(payload),
                        timeout_ms > 0 ? timeout_ms : 30000, 1, &r);
  free(url); free(payload); cJSON_Delete(body);
  if (rc != 0 || r.status < 200 || r.status >= 300 || !r.body) {
    http_response_free(&r);
    return NULL;
  }
  char *resp = strdup(r.body);
  http_response_free(&r);
  return resp;
}

char *llm_complete(llm_client *c, const char *prompt, const char *grammar,
                   int max_tokens, double temperature, int timeout_ms) {
  cJSON *b = cJSON_CreateObject();
  cJSON_AddStringToObject(b, "prompt", prompt);
  cJSON_AddNumberToObject(b, "n_predict", max_tokens > 0 ? max_tokens : 2048);
  cJSON_AddNumberToObject(b, "temperature", temperature >= 0 ? temperature : 0.2);
  cJSON_AddBoolToObject(b, "cache_prompt", 1);
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

char *llm_chat(llm_client *c, const char *messages_json, const char *grammar,
               double temperature, int timeout_ms) {
  cJSON *msgs = cJSON_Parse(messages_json);
  if (!msgs) return NULL;
  cJSON *b = cJSON_CreateObject();
  const char *model = getenv("LLM_MODEL");
  cJSON_AddStringToObject(b, "model", model && *model ? model : "local-model");
  cJSON_AddItemToObject(b, "messages", msgs);
  cJSON_AddNumberToObject(b, "temperature", temperature >= 0 ? temperature : 0.1);
  if (grammar && *grammar) cJSON_AddStringToObject(b, "grammar", grammar);
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
