/* core/osint_dispatch.h — the OSINT dispatcher, UNIFIED onto source.h.
 * JS port-from: server/src/osint/dispatcher.js — but instead of a second
 * REGISTRY Map, the C dispatcher IS the source registry, UNCONDITIONALLY:
 * ANY registered source (a JapanOSINT collector OR an OSINTsaas service —
 * no code distinguishes them) is dispatchable by id, pivoted on an entity.
 * One ABI, no parallel system, no source_kind branch.
 *
 * Improvement over the JS: a service persists its result LIVE through the
 * shared intel_sink as it runs (FTS/MeCab/entity/alert hooks), so completed
 * service results survive a crash/disconnect mid-run; the dispatcher ALSO
 * captures the emitted JSON so the pipeline's Phase-2 can chain on it. */
#ifndef JO_OSINT_DISPATCH_H
#define JO_OSINT_DISPATCH_H
#include "db.h"
#include "../source.h"

typedef struct {
  char  service[64];     /* canonical (upper-cased) id */
  int   success;         /* 1 if the service emitted a usable result */
  int   confidence;      /* 0..100 (JS: success?70:0 unless service set it) */
  char *data;            /* malloc'd JSON string of the emitted result, or NULL */
  char *error;           /* malloc'd; "not_implemented" when no such source */
} osint_result;

void osint_result_free(osint_result *r);

/* Canonical service name: trimmed, upper-cased into out (or 0 if empty). */
int  osint_canon(const char *name, char *out, size_t n);

/* True if a source with this (canonical) id is registered (any source). */
int  osint_is_implemented(const char *name);

/* Comma-separated list of ALL registered source ids — every source is
 * eligible, fed verbatim into the analysis/phase2 prompts (== JS
 * getServicesList(), now the full registry). Caller frees. */
char *osint_services_list(void);

/* Handler-dedup key (== JS handlerKey). Unified model: the canonical id IS
 * the key (distinct source_def per service); alias-grouping is an additive
 * refinement. Writes into out. */
int  osint_handler_key(const char *name, char *out, size_t n);

/* Dispatch one (service,entity) against the registry. Builds a source_ctx
 * (shared http/llm/db like scheduler_run_source), runs the source through a
 * dual sink that BOTH persists via `persist` (the real intel_sink → live
 * intel_items) AND captures the emitted result JSON into *out. Never throws;
 * unregistered/non-OSINT id → success=0, error="not_implemented" (graceful,
 * == JS). Returns 0 always (result in *out). */
int  osint_dispatch(db_handle *db, llm_client *llm, const char *service,
                    const char *entity, const char *entity_type,
                    intel_sink *persist, osint_result *out);

#endif
