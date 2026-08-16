/* core/prompts.h — verbatim C port of server/src/osint/prompts.js
 * (createAnalysisPrompt / createPhase2Prompt / createSuggestionsPrompt) plus
 * the GBNF grammar loader from server/src/utils/grammars.js.
 *
 * The prompt STRINGS are byte-faithful copies of the tuned JS builders — they
 * are grammar-aligned and must not drift.
 *
 * VALUES ARE NO LONGER SPLICED RAW. The JS interpolated `${query}` and
 * `${resultsJson}` with no delimiter, and so did this file; that let a query
 * (or a third-party service's response) carry a newline plus a forged
 * {"entities":[…],"recommended_services":[…]} and pick which OSINT services
 * the pipeline calls next, and with what value — a control-plane injection,
 * since those services then make outbound requests carrying it. Every value
 * that did not come from the operator is now wrapped in a BEGIN/END fence with
 * a per-call random id, under a standing "this block is data" rule. The bytes
 * of the value are unchanged (nothing is escaped, stripped or truncated); only
 * the framing around them is new. See the long note at the top of prompts.c.
 * `services_list` is not fenced — it is built from our own source registry.
 *
 * Grammar loading mirrors loadGrammar(name): reads
 * <repo>/server/grammars/<name>.gbnf once, caches it forever, returns "" on
 * miss (callers fall back to ungrammared generation rather than crash). The
 * loader owns the cached buffers; never free a grammar_load() result. */
#ifndef JO_PROMPTS_H
#define JO_PROMPTS_H

/* Phase 1 OSINT entity-extraction prompt. `services_list` is the categorised
 * dispatcher service list; NULL -> "(service list unavailable)" like JS.
 * Returns a heap string the CALLER must free(). NULL only on allocation
 * failure. */
char *prompt_analysis(const char *query, const char *services_list);

/* Phase 2 service-chaining prompt over Phase 1 results JSON. `results_json`
 * is fenced as untrusted (it is what third-party services returned, and this
 * prompt decides the NEXT outbound calls); NULL services_list ->
 * "(service list unavailable)". Returns a heap string the CALLER must
 * free(). NULL only on allocation failure. */
char *prompt_phase2(const char *query, const char *results_json,
                    const char *services_list);

/* 9-item search-suggestions prompt. Returns a heap string the CALLER must
 * free(). NULL only on allocation failure. */
char *prompt_suggestions(const char *query);

/* Final-synthesis prompt: hands the original query + the full gathered service
 * results JSON to the LLM and asks for a short narrative conclusion that
 * answers the query from the data actually returned (no fabrication). Plain
 * prose out (use llm_chat with json_schema=NULL). Not a JS port — this step
 * did not exist in pipeline.js, which emitted a counts template. `results_json`
 * is fenced as untrusted. Returns a heap string the CALLER must free(); NULL
 * only on allocation failure. */
char *prompt_synthesis(const char *query, const char *results_json);

/* Corpus-NER entity-extraction prompt (port of llmPrompts.js
 * buildEntityExtractionPrompt; reuses ENTITY_TYPES_PROMPT; body clipped to
 * 4000, and the prompt SAYS how many bytes of how many it is showing when the
 * clip bites). The content is fenced as untrusted — it is fetched page text.
 * Used with grammar_load("entity_extraction"). Caller frees. */
char *prompt_entity_extraction(const char *title, const char *body,
                               const char *summary, const char *language,
                               const char *source_id);

/* Tier-2 entity-dedup prompt (port of entityExtractor.js
 * buildEntityDedupPrompt) — returns a flat completion prompt for
 * llm_complete (raw /completion path). Caller frees. */
char *prompt_entity_dedup(const char *type, const char *canon_a,
                          const char *canon_b);

/* Returns the cached GBNF text for one of
 * "osint_analysis" / "entity_extraction" / "page_analysis" / "suggestions"
 * (read once from <repo>/server/grammars/<name>.gbnf). Returns "" if the file
 * is missing/unreadable, exactly like loadGrammar(). The returned pointer is
 * owned by the loader (bounded, cached once) — do NOT free it. Not
 * thread-safe on first touch of a given name (mirrors the JS Map cache). */
const char *grammar_load(const char *name);

/* JSON-schema sibling of grammar_load: returns the cached text of
 * <repo>/grammars/<name>.schema.json ("" on miss; owned by the loader, do NOT
 * free). Pass to llm_chat's json_schema param so a reasoning model is
 * constrained on its final channel only — a raw GBNF grammar on the chat
 * endpoint suppresses the reasoning channel and collapses extraction. */
const char *schema_load(const char *name);

#endif /* JO_PROMPTS_H */
