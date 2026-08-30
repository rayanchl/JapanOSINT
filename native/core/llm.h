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

/* WHY A FAILED CALL NOW HAS A NAME.
 *
 * Every generation entry point here returns NULL on failure and returned NULL
 * for FIVE unrelated reasons: the caller handed us unparsable messages JSON,
 * nothing was listening on base_url, llama-server answered non-2xx, it answered
 * 2xx with a body we could not read, or it generated an empty completion. A
 * caller could not tell any of them apart, and core/pipeline.c did not try: a
 * NULL from the analysis call fell through to `analysis = NULL`, zero entities,
 * one fallback corpus lookup, and a run that still reported
 * gpt_analyzing → services_assigned → agents_working → completed at 100%. An
 * unreachable model host and a model that ran and found nothing were the same
 * observable outcome, which is exactly what house rule 1 forbids: a failure
 * must degrade to an explicit error, never to a silent success.
 *
 * So the transport verdict is now reported out-of-band, and the *_ex forms
 * carry it. The plain llm_chat/llm_complete wrappers are unchanged for the
 * dozen background callers (enricher, triage, repair, translate) that only ever
 * cared whether they got text back. */
typedef enum {
  LLM_OK = 0,           /* usable content returned                            */
  LLM_ERR_BAD_REQUEST,  /* caller's messages_json / prompt was unusable       */
  LLM_ERR_UNREACHABLE,  /* no HTTP exchange completed — refused, DNS, no host */
  LLM_ERR_TIMEOUT,      /* the call ran out its own timeout budget            */
  LLM_ERR_HTTP,         /* server answered, status outside 2xx               */
  LLM_ERR_EMPTY,        /* 2xx, but no usable content in the response        */
} llm_status;

/* Stable machine-readable token for a status ("ok", "llm_unreachable",
 * "llm_http_error", "llm_empty_response", "llm_bad_request"). Never NULL —
 * these strings are surfaced to the client as degradation codes, so they are
 * part of the API and must not be reworded casually. */
const char *llm_status_code(llm_status s);

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

/* llm_chat, but *out_status (may be NULL) says WHY on a NULL return and
 * *out_http (may be NULL) carries the HTTP status when one was received (0 when
 * no exchange completed). */
char *llm_chat_ex(llm_client *c, const char *messages_json,
                  const char *json_schema, int max_tokens, double temperature,
                  int timeout_ms, llm_status *out_status, long *out_http);

int  llm_healthy(llm_client *c); /* GET /health */

/* Embeddings — llama-server `/v1/embeddings` on a SEPARATE server.
 *
 * `c->base_url` is expected to be the embedding host (JO_EMBED_URL, :8082 by
 * default in scripts/start-llama.sh), never the generation server: an
 * embedding model is loaded with `--embedding` and cannot generate, and the
 * generation model answers /v1/embeddings with 501. Because the worker in
 * core/llm_worker.c is keyed on base_url, an embedding call gets its own
 * thread and never queues behind (or in front of) a search-pipeline job.
 *
 * `texts[0..n)` are embedded in ONE request (llama-server accepts an array
 * `input`). On success returns 0 and hands back a malloc'd float array of
 * n*dim values, row-major, with *out_dim set from the response; the caller
 * frees it. Returns non-zero on any failure, with *st (may be NULL) saying
 * why in the same vocabulary as llm_chat_ex. A response whose vectors do not
 * all share one dimension, or whose count differs from n, is reported as
 * LLM_ERR_EMPTY — a partial answer would be silently attributed to the wrong
 * rows, which is worse than none. */
int llm_embed(llm_client *c, const char *const *texts, int n,
              float **out_vecs, int *out_dim, int timeout_ms, llm_status *st);


#endif
