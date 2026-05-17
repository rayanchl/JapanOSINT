/* core/llm.h — llama.cpp llama-server client (port of llmClient.js).
 * complete(): native /completion with inline GBNF grammar (the verbatim
 * OSINT pipeline path). chat(): /v1/chat/completions. Returns malloc'd
 * string (caller frees) or NULL on any failure; never aborts the caller. */
#ifndef JO_LLM_H
#define JO_LLM_H

#include "httpclient.h"

typedef struct { http_client *http; const char *base_url; } llm_client;

void llm_init(llm_client *c, http_client *http); /* base from LLM_BASE_URL */

/* /completion. grammar may be NULL. Returns raw generated string. */
char *llm_complete(llm_client *c, const char *prompt, const char *grammar,
                   int max_tokens, double temperature, int timeout_ms);

/* /v1/chat/completions. messages_json = a JSON array string. grammar
 * optional. Returns the assistant message content string. */
char *llm_chat(llm_client *c, const char *messages_json, const char *grammar,
               double temperature, int timeout_ms);

int  llm_healthy(llm_client *c); /* GET /health */

#endif
