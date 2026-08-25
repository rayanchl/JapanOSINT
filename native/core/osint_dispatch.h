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
  /* malloc'd JSON string of EVERY record the service emitted, or NULL:
   *   {"record_count":N,"records":[<payload>, …]}
   *
   * EXHAUSTIVE-USE RULE (docs/SOURCE_EXHAUSTIVENESS.md): this used to keep
   * only the LAST emitted payload, so a service that returned 40 records
   * handed exactly one of them to Phase-2 chaining, the synthesis prompt and
   * /api/search/results — 39 records were fetched, persisted, and then thrown
   * away at the seam. The capture layer is never allowed to decide what is
   * interesting; it keeps everything and lets the consumer bound its own view
   * explicitly (see results_view_for_prompt in pipeline.c). */
  char *data;
  int   records;         /* how many records `data` carries                  */
  char *error;           /* malloc'd; "not_implemented" when no such source */
  /* malloc'd JSON array of the underlying sources/providers the service hit,
   * one entry per distinct attribution: [{ "name", "status", "records",
   * ["requests"], ["detail"] }]. Derived from each emit's sub_source_id /
   * the real HTTP hosts contacted / the service name. Surfaced as
   * results.services[i].sources.
   *
   * `records` is a NUMBER only when the rows were actually attributable to
   * that source (the collector labelled its emits with sub_source_id). When
   * attribution came from the HTTP host log it is NULL and `requests` carries
   * what was really measured — the host log counts requests, not records, and
   * writing the service total into each host row claimed 11,220 records out of
   * 187 for one 60-host service. A consumer must treat null as "not measured",
   * never as zero. */
  char *sources_json;
} osint_result;

void osint_result_free(osint_result *r);

/* Canonical service name: trimmed, upper-cased into out (or 0 if empty). */
int  osint_canon(const char *name, char *out, size_t n);

/* True if a source with this (canonical) id is registered (any source). */
int  osint_is_implemented(const char *name);

/* What osint_services_list_bounded() actually put in the prompt. `total` is
 * how many entity-pivot services are registered, `shown` how many are listed,
 * `descriptions` whether each line carries its description or is a bare id,
 * `truncated` == (shown < total). The pipeline reports these to the client, so
 * a routing decision made from a partial menu is never presented as one made
 * from the whole registry. */
typedef struct {
  int total;
  int shown;
  int descriptions;
  int truncated;
} osint_catalogue_note;

/* The service catalogue fed to the analysis / phase-2 prompts: one ON-DEMAND
 * entity-pivot service per line (collector=="osint" AND
 * update_interval_sec==0 — scheduled bulk feeds cannot pivot on an entity and
 * are excluded), bounded to a character budget
 * (JO_PROMPT_SERVICE_CATALOGUE_CHARS, default 32768) with the bound STATED
 * in-band at the end of the text. `note` may be NULL. Caller frees.
 *
 * Unbounded, this text was 207,353 tokens and llama-server rejected every
 * analysis request outright — see the long comment in osint_dispatch.c. */
char *osint_services_list_bounded(osint_catalogue_note *note);

/* osint_services_list_bounded(NULL). */
char *osint_services_list(void);

/* The osint_analysis JSON schema with its service-name enums (recommended_
 * services + per-entity services) rebuilt from the LIVE registry, so the
 * analysis LLM can recommend exactly the services that exist — no manual enum
 * maintenance on registry changes. malloc'd; caller frees. NULL → fall back to
 * the static schema_load("osint_analysis"). */
char *osint_analysis_schema_dynamic(void);

/* Same, but the enums hold only the first `limit` entity-pivot services —
 * pass osint_catalogue_note.shown so what the model is ALLOWED to answer is
 * exactly what it was SHOWN. `limit` <= 0 means no limit. */
char *osint_analysis_schema_dynamic_limited(int limit);

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
