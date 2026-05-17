/* core/pipeline.h — the OSINT search pipeline (port of
 * server/src/osint/pipeline.js). Phase-1 LLM analyze → task build
 * (handler/entity dedup + always-on JP_CORPUS_LOOKUP per entity) → dispatch
 * round 0 (osint_dispatch → results persist LIVE via the shared intel_sink)
 * → Phase-2 ≤max_rounds entity-pivot chain loop → aggregate. Progress is
 * tracked via core/progress.h (SSE reads its snapshot). On finish a
 * run-summary row is written through the intel_sink (osint-search|run:<id>);
 * per-service rows already persisted live during dispatch (improvement over
 * the JS ingest-at-done). Entities flow into the graph via the sink's
 * standard NER hooks; the explicit seed→discovered pivot_discovered edge
 * (searchIngest.js) needs an entity-WRITE api (entityapi.h is read-only) —
 * documented follow-up. */
#ifndef JO_PIPELINE_H
#define JO_PIPELINE_H
#include "db.h"
#include "llm.h"

/* Synchronous full run (caller may run it on a worker thread — searchapi
 * does). request_id must already exist via progress_create(). */
void osint_pipeline_run(db_handle *db, llm_client *llm,
                        const char *request_id, const char *query,
                        int max_rounds);

/* Suggestions path (port of getSuggestions): returns a malloc'd JSON array
 * string of ≤9 strings (or "[]"). Caller frees. */
char *osint_suggest(llm_client *llm, const char *query);

#endif
