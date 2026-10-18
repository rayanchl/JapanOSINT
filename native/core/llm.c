#include "llm.h"
#include "llm_worker.h"
#include "httpclient.h"
#include "../third_party/cJSON.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Pessimistic on purpose — see llm_ctx_chars() in llm.h. Ids, JSON and
 * Japanese all tokenize worse than the ~4 bytes/token English average, and
 * underestimating costs the whole stage while overestimating costs a few menu
 * entries. */
#define LLM_BYTES_PER_TOKEN 3

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

static __thread long g_last_queued_ms, g_last_http_ms;

void llm_last_call_timing(long *queued_ms, long *http_ms) {
  if (queued_ms) *queued_ms = g_last_queued_ms;
  if (http_ms)   *http_ms = g_last_http_ms;
}

const char *llm_status_code(llm_status s) {
  switch (s) {
    case LLM_OK:              return "ok";
    case LLM_ERR_BAD_REQUEST: return "llm_bad_request";
    case LLM_ERR_UNREACHABLE: return "llm_unreachable";
    case LLM_ERR_TIMEOUT:     return "llm_timeout";
    case LLM_ERR_HTTP:        return "llm_http_error";
    case LLM_ERR_EMPTY:       return "llm_empty_response";
    case LLM_ERR_QUEUE_TIMEOUT: return "llm_queue_timeout";
    case LLM_ERR_QUEUE_FULL:  return "llm_queue_full";
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
  llm_worker_req q = {
    .method = "POST", .url = url, .headers = hdrs, .body = payload,
    .body_len = strlen(payload), .timeout_ms = budget,
    /* One retry, and only after a failure that cost nothing (refused
     * connection, 5xx/429) — never after a timeout, for any caller. */
    .retry = 1,
    .high_priority = c->interactive,
    .max_wait_ms = c->bound_queue_wait ? budget : 0,
  };
  llm_worker_info wi;
  int rc = llm_worker_call(c->base_url, &q, &r, &wi);
  g_last_queued_ms = wi.queued_ms;
  g_last_http_ms = wi.http_ms;
  free(url); free(payload); cJSON_Delete(body);
  if (http) *http = r.status;
  if (wi.verdict == LLM_Q_TIMEOUT || wi.verdict == LLM_Q_FULL) {
    if (st) *st = wi.verdict == LLM_Q_FULL ? LLM_ERR_QUEUE_FULL
                                           : LLM_ERR_QUEUE_TIMEOUT;
    fprintf(stderr, "[llm] %s%s: %s after %ld ms queued (never sent)\n",
            c->base_url, path, wi.verdict == LLM_Q_FULL ? "queue full"
                                                        : "queue deadline",
            wi.queued_ms);
    return NULL;
  }
  if (rc != 0 || r.status == 0) {
    /* SPLIT ON WHAT FAILED, because "unreachable" and "too slow" send an
     * operator to opposite ends of the system. A local llama-server on CPU
     * spends ~100 s on prompt-eval for the 18k-token analysis request, blows
     * the 60 s budget, and was reported as "llama-server is not running" while
     * it was sitting there working. This used to be decided by the wall clock
     * around the whole call — which included the QUEUE wait, so a refused
     * connection after a long queue read as a timeout. The worker now reports
     * the transport's own verdict for the exchange alone: a connected
     * exchange that ran out its timeout is a timeout; a connect that never
     * completed (refused, unresolvable, black-holed) is unreachable. The
     * clock on the HTTP time alone backs up an "other" failure. */
    int timed_out = wi.transport == HTTP_TE_TIMEOUT ||
                    (wi.transport == HTTP_TE_OTHER &&
                     wi.http_ms >= (long)budget * 9 / 10);
    if (st) *st = timed_out ? LLM_ERR_TIMEOUT : LLM_ERR_UNREACHABLE;
    fprintf(stderr, "[llm] %s%s: %s — %ld ms queued + %ld ms HTTP over %d "
                    "attempt(s)\n", c->base_url, path,
            timed_out ? "timed out" : "unreachable", wi.queued_ms, wi.http_ms,
            wi.attempts);
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

/* See llm.h. Cached per process: n_ctx is fixed for the life of a
 * llama-server, and this is called on a request path. */
size_t llm_ctx_chars(llm_client *c, int reserve_tokens) {
  static int cached_ctx = -1;                 /* -1 = not asked, 0 = unknown */
  static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mu);
  if (cached_ctx < 0) {
    cached_ctx = 0;
    char url[512];
    snprintf(url, sizeof url, "%s/props",
             c && c->base_url ? c->base_url : "http://127.0.0.1:8080");
    http_response r = {0};
    /* Short timeout and no retry: this is a hint, not the work. A server that
     * will not answer /props in two seconds leaves the caller on its default. */
    if (http_request(c ? c->http : NULL, "GET", url, NULL, NULL, 0, 2000, 0, &r) == 0
        && r.status >= 200 && r.status < 300 && r.body) {
      cJSON *j = cJSON_Parse(r.body);
      if (j) {
        /* llama-server reports the PER-SLOT context here, which is the number
         * that actually applies to one request — not the --ctx-size it was
         * started with, which it may divide across parallel slots. */
        const cJSON *g = cJSON_GetObjectItem(j, "default_generation_settings");
        const cJSON *n = g ? cJSON_GetObjectItem(g, "n_ctx") : NULL;
        if (cJSON_IsNumber(n) && n->valueint > 0) cached_ctx = n->valueint;
        cJSON_Delete(j);
      }
    }
    http_response_free(&r);
    if (cached_ctx > 0)
      fprintf(stderr, "[llm] %s reports n_ctx=%d tokens per slot\n",
              c && c->base_url ? c->base_url : "(default)", cached_ctx);
    else
      fprintf(stderr, "[llm] could not read n_ctx from /props — prompt budgets "
                      "stay on their built-in defaults\n");
  }
  int ctx = cached_ctx;
  pthread_mutex_unlock(&mu);
  if (ctx <= 0) return 0;                     /* unknown: caller keeps its own */
  if (reserve_tokens < 0) reserve_tokens = 0;
  int usable = ctx - reserve_tokens;
  if (usable < 256) usable = 256;             /* never return a useless budget */
  return (size_t)usable * LLM_BYTES_PER_TOKEN;
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

int llm_embed(llm_client *c, const char *const *texts, int n,
              float **out_vecs, int *out_dim, int timeout_ms, llm_status *st) {
  return llm_embed_ex(c, texts, n, out_vecs, out_dim, timeout_ms, st, NULL);
}

int llm_embed_ex(llm_client *c, const char *const *texts, int n,
                 float **out_vecs, int *out_dim, int timeout_ms,
                 llm_status *st, long *out_http) {
  if (st) *st = LLM_ERR_BAD_REQUEST;
  if (out_http) *out_http = 0;
  if (out_vecs) *out_vecs = NULL;
  if (out_dim) *out_dim = 0;
  if (!c || !texts || n <= 0 || !out_vecs || !out_dim) return -1;
  cJSON *b = cJSON_CreateObject();
  cJSON *in = cJSON_CreateArray();
  for (int i = 0; i < n; i++)
    cJSON_AddItemToArray(in, cJSON_CreateString(texts[i] ? texts[i] : ""));
  cJSON_AddItemToObject(b, "input", in);
  const char *model = getenv("JO_EMBED_MODEL");
  cJSON_AddStringToObject(b, "model", model && *model ? model : "embedding-model");
  /* Anything the server returns is a float array we copy out, so we do not
   * ask for base64 — the default `float` encoding keeps the parse trivial. */
  char *raw = post_json(c, "/v1/embeddings", b, timeout_ms, st, out_http);
  if (!raw) return -1;
  cJSON *j = cJSON_Parse(raw);
  free(raw);
  if (!j) { if (st) *st = LLM_ERR_EMPTY; return -1; }
  cJSON *data = cJSON_GetObjectItem(j, "data");
  int rc = -1, dim = 0;
  float *vecs = NULL;
  /* Which inputs have been answered. Count alone is not enough: n entries
   * that name index 0 twice and never index 1 pass a size check, and the row
   * nobody answered keeps calloc's zero vector — stored as if embedded. */
  unsigned char *seen = calloc((size_t)n, 1);
  if (seen && cJSON_IsArray(data) && cJSON_GetArraySize(data) == n) {
    rc = 0;
    /* OpenAI shape: data[i].index says which input it answers; llama-server
     * emits them in order but honouring `index` costs nothing and guards
     * against a reordering server attributing vectors to the wrong rows. */
    cJSON *e;
    cJSON_ArrayForEach(e, data) {
      cJSON *emb = cJSON_GetObjectItem(e, "embedding");
      cJSON *idx = cJSON_GetObjectItem(e, "index");
      int i = cJSON_IsNumber(idx) ? (int)idx->valuedouble : -1;
      if (!cJSON_IsArray(emb) || i < 0 || i >= n || seen[i]) { rc = -1; break; }
      seen[i] = 1;
      int d = cJSON_GetArraySize(emb);
      if (d <= 0 || (dim && d != dim)) { rc = -1; break; }
      if (!dim) {
        dim = d;
        vecs = calloc((size_t)n * (size_t)dim, sizeof(float));
        if (!vecs) { rc = -1; break; }
      }
      int k = 0;
      cJSON *v;
      cJSON_ArrayForEach(v, emb) {
        /* isfinite on the STORED float: an overflowing literal (1e999, or
         * anything past FLT_MAX) becomes inf, and one inf component turns
         * every distance to this row into inf/NaN. */
        float f = cJSON_IsNumber(v) ? (float)v->valuedouble : NAN;
        if (!isfinite(f)) { rc = -1; break; }
        vecs[(size_t)i * dim + k++] = f;
      }
      if (rc) break;
    }
  }
  cJSON_Delete(j);
  free(seen);
  if (rc != 0 || !vecs) {
    free(vecs);
    if (st) *st = LLM_ERR_EMPTY;
    return -1;
  }
  *out_vecs = vecs;
  *out_dim = dim;
  if (st) *st = LLM_OK;
  return 0;
}
