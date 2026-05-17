/* core/progress.h — faithful port of server/src/osint/progressTracker.js.
 *
 * In-memory per-request OSINT-search progress. A process-global registry
 * keyed by request_id holds one `osint_request` per in-flight search; the
 * pipeline (worker thread) mutates it through the setters below and the SSE
 * handler (httpd thread) snapshots it with progress_to_json().
 *
 * THREAD SAFETY: a single global pthread_mutex guards the registry AND every
 * field of every request (one lock, coarse — the mutation rate is low and the
 * critical sections are tiny). progress_get() returns a stable pointer (entries
 * are never freed for the process lifetime, only the oldest *finished* one is
 * dropped past a 200-cap exactly like the JS Map eviction), so callers may hold
 * it across calls; all access goes back through the lock inside each function.
 *
 * NO EVENT EMITTER. The JS RequestProgress is an EventEmitter that emits
 * 'update'/'done'. In C there is no push: the mongoose SSE handler polls
 * progress_to_json() on MG_EV_POLL and diffs/streams the fresh snapshot
 * itself (and treats progress_is_done()!=0 as the JS 'done'/close event).
 *
 * progress_to_json() output matches progressTracker.js toJSON() field-for-
 * field (request_id, query, phase, progress_percent, gpt_thinking,
 * total_results, preliminary_findings, created_at, updated_at, entities,
 * services_assigned, services, stats, phase_history, current_round,
 * max_rounds, awaiting_user_action, discovered_entities, confirmed_entities,
 * all_entities, and `results` ONLY when set) — the exact shape the SSE route
 * and utils/searchIngest.js consume. Returned string is heap-allocated; the
 * caller frees() it. */
#ifndef JO_PROGRESS_H
#define JO_PROGRESS_H

typedef struct osint_request osint_request;

/* createRequest(requestId, query, maxRounds). max_rounds<=0 -> default 5
 * (mirrors the JS `maxRounds = 5` default). If request_id already exists the
 * existing request is returned unchanged. Never returns NULL except OOM. */
osint_request *progress_create(const char *request_id, const char *query,
                               int max_rounds);

/* getRequest(requestId) -> the request, or NULL if unknown. */
osint_request *progress_get(const char *request_id);

/* setPhase(phase, percent): unknown phase -> "unknown"; pass percent<0 to
 * skip the percent update (JS `Number.isFinite(percent)` guard). Appends a
 * phase_history entry. */
void progress_set_phase(osint_request *r, const char *phase, int percent);

/* setThinking(t): NULL -> "" (JS `String(t || '')`). */
void progress_set_thinking(osint_request *r, const char *thinking);

/* setEntities(list): replaces entities[] with `n` {value,type} pairs. The
 * arrays are borrowed (copied internally). */
void progress_set_entities(osint_request *r, const char *const *values,
                           const char *const *types, int n);

/* assignServices(names): sets services_assigned[] and adds any missing
 * service rows (status="pending", is_followup = current_round>0). */
void progress_assign_services(osint_request *r, const char *const *names,
                              int n);

/* serviceStatus(name, status, message, results_count, entities): updates (or
 * creates) the service row, then recomputes stats. Pass message/entities
 * NULL to leave them unchanged; results_count<0 to leave it unchanged
 * (JS `!= null` / `Number.isFinite` guards). */
void progress_service_status(osint_request *r, const char *name,
                             const char *status, const char *message,
                             int results_count, const char *entities);

/* setRound(r). */
void progress_set_round(osint_request *r, int round);

/* addDiscovered(value, type, discoveredBy): dedup on (value,type) — exact
 * match like the JS — then rebuilds all_entities (case-insensitive dedup on
 * `type|lower(value)` over entities ++ discovered_entities). discovered_by
 * NULL -> "". */
void progress_add_discovered(osint_request *r, const char *value,
                             const char *type, const char *discovered_by);

/* setResults(obj_json): `obj_json` is a JSON document string (the results
 * object). It is parsed/stored; total_results = obj.services.length when
 * that is an array, else 0. Invalid JSON clears results (JS try/catch). */
void progress_set_results(osint_request *r, const char *obj_json);

/* finish(phase): setPhase(phase, phase=="completed"?100:current) then marks
 * done. phase NULL -> "completed". */
void progress_finish(osint_request *r, const char *phase);

/* toJSON() snapshot. Heap-allocated JSON; CALLER FREES with free(). NULL on
 * OOM or r==NULL. */
char *progress_to_json(osint_request *r);

/* the `done` flag (1 finished, 0 in-flight; 0 if r==NULL). */
int progress_is_done(osint_request *r);

#endif
