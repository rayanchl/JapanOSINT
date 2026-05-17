/* core/entity_enrich.h — the ONE LLM entity enricher (faithful port of the
 * entities scope of server/src/utils/entityExtractor.js, driven like the
 * Node 5-min guarded cron). Drains the SAME intel_items the one sink fills
 * (collector AND osint-search rows) via the entity_extraction_state
 * watermark; writes through the SAME core/entitystore.h surface the OSINT
 * search ingest uses. One path. */
#ifndef JO_ENTITY_ENRICH_H
#define JO_ENTITY_ENRICH_H
#include "db.h"
#include "llm.h"

/* enrichEntities(): drain unextracted intel_items newest-first, per-item LLM
 * NER (grammar entity_extraction), upsert entities + 'body' mentions +
 * co_mention edges, advance the watermark. Returns #processed. */
int entity_enrich_extract(db_handle *db, llm_client *llm, int batch);

/* resolveEntities(): tier-2 LLM dedup over candidate pairs → entity_merges,
 * then union pass folds same=1 & confidence>=0.7. Returns #merged. */
int entity_enrich_resolve(db_handle *db, llm_client *llm, int batch);

/* runLlmEnricher() entities subset: extract then resolve. Returns 0. */
int entity_enrich_run(db_handle *db, llm_client *llm);

#endif
