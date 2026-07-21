/* core/llm.h — llama.cpp llama-server client (port of llmClient.js).
 * complete(): native /completion with inline GBNF grammar (the verbatim
 * OSINT pipeline path). chat(): /v1/chat/completions. Returns malloc'd
 * string (caller frees) or NULL on any failure; never aborts the caller. */
#ifndef JO_LLM_H
#define JO_LLM_H

#include "httpclient.h"

/* Generation calls are serialized per-server by the global LLM worker
 * (core/llm_worker.c) — one dedicated thread per base_url — so this struct only
 * carries the target. `http` is retained for the lightweight /health probe;
 * generation no longer uses it (the worker owns its own client). `interactive`
 * picks the worker's priority lane: 1 = user-facing (OSINT search, suggest) so
 * it jumps ahead of background pod work; 0 = background (enricher/triage/repair,
 * the default). Set via llm_init (0) / llm_init_suggest (1) or directly. */
typedef struct {
  http_client *http;
  const char *base_url;
  int interactive;
} llm_client;

void llm_init(llm_client *c, http_client *http); /* base from LLM_BASE_URL */

/* Suggestion path: base from LLM_SUGGEST_BASE_URL, else LLM_BASE_URL, else
 * default — lets /api/search/suggest hit a dedicated small-model server so it
 * never contends with the pipeline's llama-server. */
void llm_init_suggest(llm_client *c, http_client *http);

/* /completion. grammar may be NULL. Returns raw generated string. */
char *llm_complete(llm_client *c, const char *prompt, const char *grammar,
                   int max_tokens, double temperature, int timeout_ms);

/* /v1/chat/completions. messages_json = a JSON array string. json_schema is an
 * optional JSON-schema OBJECT (as text, e.g. from schema_load); when present it
 * is sent as response_format json_schema, which constrains only the assistant's
 * FINAL channel. For reasoning models (gpt-oss) launched with --jinja
 * --reasoning-format auto, the chain-of-thought is parsed into its own channel
 * and stripped, so the returned content is the clean, schema-conformant final
 * message. (Do NOT pass a raw GBNF grammar here — it engages from the first
 * token and suppresses the reasoning channel.) Returns the assistant message
 * content string. */
char *llm_chat(llm_client *c, const char *messages_json, const char *json_schema,
               int max_tokens, double temperature, int timeout_ms);

int  llm_healthy(llm_client *c); /* GET /health */

#endif
