/* core/prompts.h — verbatim C port of server/src/osint/prompts.js
 * (createAnalysisPrompt / createPhase2Prompt / createSuggestionsPrompt) plus
 * the GBNF grammar loader from server/src/utils/grammars.js.
 *
 * The prompt STRINGS are byte-faithful copies of the tuned JS builders — they
 * are grammar-aligned and must not drift. The `query` (and resultsJson) are
 * interpolated raw exactly as the JS template literals do (JS `${query}` is a
 * plain string substitution — NOT JSON-escaped — so neither are these).
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
 * is interpolated raw (as JS does); NULL services_list ->
 * "(service list unavailable)". Returns a heap string the CALLER must
 * free(). NULL only on allocation failure. */
char *prompt_phase2(const char *query, const char *results_json,
                    const char *services_list);

/* 9-item search-suggestions prompt. Returns a heap string the CALLER must
 * free(). NULL only on allocation failure. */
char *prompt_suggestions(const char *query);

/* Corpus-NER entity-extraction prompt (port of llmPrompts.js
 * buildEntityExtractionPrompt; reuses ENTITY_TYPES_PROMPT; body clipped to
 * 4000). Used with grammar_load("entity_extraction"). Caller frees. */
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

#endif /* JO_PROMPTS_H */
