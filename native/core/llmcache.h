/* core/llmcache.{c,h} — an exact-match cache of LLM responses, in SQLite.
 *
 * WHY. /api/search/suggest asks a generation server the same question every
 * time a user pauses on the same prefix — and with the iOS debounce at 300 ms
 * instead of 3 s it pauses far more often. The answer to an identical prompt,
 * with identical parameters, from the identical model, is reusable for a
 * while; asking again spends the suggest server's only slot to learn nothing.
 *
 * EXACT, NOT SEMANTIC. The key is (model, sha256(prompt + "\n" + params)):
 * one byte different in the prompt, grammar, temperature or token budget is
 * a different question. A cache that answered a NEAR prompt would be serving
 * an answer to a question nobody asked.
 *
 * MODEL. The model is what the server SAYS it serves (GET /v1/models),
 * remembered per base URL for LLMCACHE_MODEL_TTL_SEC. When it cannot be
 * determined nothing is read or written — a response keyed to "unknown"
 * would be served after a model swap as if the new model had written it.
 *
 * OFF BY DEFAULT. JO_LLM_CACHE_TTL_SEC (seconds a response is reused) unset
 * or 0 = no cache, no table, no /v1/models request: the suggest path is then
 * exactly what it was. Expired rows are deleted when they are next read. */
#ifndef JO_LLMCACHE_H
#define JO_LLMCACHE_H

#include "db.h"
#include "llm.h"

#define LLMCACHE_TABLE         "llm_response_cache"
#define LLMCACHE_MODEL_TTL_SEC 60

/* JO_LLM_CACHE_TTL_SEC, or 0 (off). */
long llmcache_ttl(void);

/* The model the server at c->base_url serves (cached), into `out`. 0 / -1. */
int llmcache_model(llm_client *c, char *out, size_t cap);

/* hex sha256 of prompt + "\n" + params into out[65]. */
void llmcache_key(const char *prompt, const char *params, char out[65]);
/* llmcache_key() with the prompt's random fence id masked (prompts.c builds
 * every prompt with a per-call id; it changes nothing the model answers). An
 * out[0] == 0 means the key could not be computed: do not cache. */
void llmcache_key_prompt(const char *prompt, const char *params, char out[65]);

/* A fresh cached response (malloc'd) or NULL. */
char *llmcache_get(db_handle *db, const char *model, const char *key);
/* Store `response` (ignored when NULL/empty or the cache is off). */
void  llmcache_put(db_handle *db, const char *model, const char *key,
                   const char *response);

#endif
