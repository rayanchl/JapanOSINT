/* core/entitystore.h — THE one entity-graph write surface (faithful port of
 * server/src/utils/entityStore.js write fns). Used by BOTH producers via the
 * SAME path: the LLM NER enricher (collector intel_items) and the OSINT
 * search ingest (pipeline.c). Tables already in core/schema.sql; entities_fts
 * mirror mirrors core/intel.c's fts_write pattern with fts_segment().
 *
 * Tier-1 key is exact (NFKC* + ws-collapse + trim); sameness is the LLM
 * resolver's job (tier-2 es_record_merge/es_union_entities). *NFKC is a
 * documented pragmatic approximation (ws-collapse only; no ICU compat
 * decomposition) — parity abandoned, determinism is what matters; the
 * resolver adjudicates real sameness anyway. Same stance as linecolor/
 * normName elsewhere in the C port. */
#ifndef JO_ENTITYSTORE_H
#define JO_ENTITYSTORE_H
#include "db.h"
#include <stddef.h>

/* nfkcCollapse(value): ws-collapse + trim into out (caller-sized). */
void  es_norm_key(const char *value, char *out, size_t n);

/* upsertEntity({type,value}). Returns a malloc'd entity_id (caller frees) or
 * NULL on empty value. Idempotent on UNIQUE(type,norm_key); appends new
 * surface to aliases; keeps entities_fts in sync. */
char *es_upsert_entity(db_handle *db, const char *type, const char *value);

/* addMention: INSERT ON CONFLICT(entity_id,item_uid,field); bumps
 * mention_count only on a genuinely new (entity,item,field) edge. */
void  es_add_mention(db_handle *db, const char *entity_id,
                     const char *item_uid, const char *source_id,
                     const char *surface, const char *field,
                     double confidence, const char *extractor);

/* addRelationship: skip self-edge; INSERT ON CONFLICT(src,dst,rel_type)
 * accumulating weight; evidence_uid COALESCEd. */
void  es_add_relationship(db_handle *db, const char *src_id,
                          const char *dst_id, const char *rel_type,
                          double weight, const char *evidence_uid);

/* tier-2 resolver helpers (entity_merges + union). */
int   es_merge_pair_exists(db_handle *db, const char *a, const char *b);
void  es_record_merge(db_handle *db, const char *a, const char *b,
                      int same, double confidence, const char *reason);
/* Fold loser into survivor (lexically-smaller id wins); repoint mentions +
 * relationships, append loser canonical to survivor aliases, delete loser,
 * recount, fix FTS. Returns 1 on success. */
int   es_union_entities(db_handle *db, const char *id_a, const char *id_b);

#endif
